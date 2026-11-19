using SharpDX.DirectInput;

namespace WheelBridge;

// The real Moza wheel: reads its axes and buttons and drives its motor (centring spring, damper, rumble, road texture and kicks).
public sealed class Wheel : IDisposable
{
    public static readonly string[] AxisNames = { "None", "X", "Y", "Z", "RotationX", "RotationY", "RotationZ", "Slider1", "Slider2" };

    readonly DirectInput directInput = new();
    Joystick? joystick;
    Effect? spring, damper, rumble, texture, constant;
    int[] actuatorAxes = Array.Empty<int>();
    readonly object gate = new();

    public string Name { get; private set; } = "";
    public string InterfacePath { get; private set; } = "";
    public bool HasForceFeedback => actuatorAxes.Length > 0;
    public bool IsOpen => joystick != null;

    public List<DeviceInstance> ListDevices() =>
        directInput.GetDevices (DeviceClass.GameControl, DeviceEnumerationFlags.AttachedOnly).ToList();

    public string Open (DeviceInstance device, IntPtr windowHandle)
    {
        Close();
        try
        {
            var js = new Joystick (directInput, device.InstanceGuid);
            // Exclusive + background: we own the motor even while the game window has focus
            js.SetCooperativeLevel (windowHandle, CooperativeLevel.Exclusive | CooperativeLevel.Background);
            js.Properties.Range = new InputRange (0, 65535);
            try { js.Properties.AutoCenter = false; } catch { }
            try { js.Properties.ForceFeedbackGain = 10000; } catch { }
            js.Acquire();
            joystick = js;
            Name = device.InstanceName.Trim();
            try { InterfacePath = js.Properties.InterfacePath; } catch { InterfacePath = ""; }

            actuatorAxes = js.GetObjects()
                .Where (o => (o.ObjectId.Flags & DeviceObjectTypeFlags.ForceFeedbackActuator) != 0)
                .Select (o => o.Offset)
                .Take (1)
                .ToArray();
            return "";
        }
        catch (Exception e)
        {
            Close();
            return e.Message;
        }
    }

    public JoystickState? Read()
    {
        lock (gate)
        {
            if (joystick == null) return null;
            try
            {
                joystick.Poll();
                return joystick.GetCurrentState();
            }
            catch
            {
                try { joystick.Acquire(); } catch { }
                return null;
            }
        }
    }

    public static int AxisValue (JoystickState s, string axis) => axis switch
    {
        "X" => s.X, "Y" => s.Y, "Z" => s.Z,
        "RotationX" => s.RotationX, "RotationY" => s.RotationY, "RotationZ" => s.RotationZ,
        "Slider1" => s.Sliders.Length > 0 ? s.Sliders[0] : 0,
        "Slider2" => s.Sliders.Length > 1 ? s.Sliders[1] : 0,
        _ => 0,
    };

    EffectParameters BaseParameters (TypeSpecificParameters specific)
    {
        var p = new EffectParameters
        {
            Flags = EffectFlags.Cartesian | EffectFlags.ObjectOffsets,
            Duration = -1,               // infinite
            SamplePeriod = 0,
            Gain = 10000,
            TriggerButton = -1,
            TriggerRepeatInterval = 0,
            StartDelay = 0,
            Parameters = specific,
        };
        p.SetAxes (actuatorAxes, new int[actuatorAxes.Length]);
        return p;
    }

    static ConditionSet Condition (int coefficient, int saturation) => new()
    {
        Conditions = new[]
        {
            new Condition
            {
                Offset = 0, DeadBand = 0,
                PositiveCoefficient = coefficient, NegativeCoefficient = coefficient,
                PositiveSaturation = saturation, NegativeSaturation = saturation,
            }
        }
    };

    // Starts an effect the first time, then just updates its strength. Returns an error message or "".
    string Drive (ref Effect? effect, Guid kind, TypeSpecificParameters p)
    {
        try
        {
            if (effect == null)
            {
                effect = new Effect (joystick!, kind, BaseParameters (p));
                effect.Start (1);
            }
            else
                effect.SetParameters (BaseParameters (p), EffectParameterFlags.TypeSpecificParameters);
            return "";
        }
        catch (Exception e)
        {
            return e.Message;
        }
    }

    // Strengths are DirectInput units 0..10000; constant is -10000..10000 (positive = push right unless flipped)
    public string UpdateForces (in FfbOutput f, bool flipDirection)
    {
        lock (gate)
        {
            if (joystick == null || !HasForceFeedback) return "This wheel did not report force feedback";
            // The spring and damper matter most: report their errors first
            var error = Drive (ref spring, EffectGuid.Spring, Condition (f.Spring, 10000));
            var e2 = Drive (ref damper, EffectGuid.Damper, Condition (f.Damper, 10000));
            // Others are extras: a wheel that refuses one still gets the rest
            Drive (ref rumble, EffectGuid.Sine, new PeriodicForce { Magnitude = f.Rumble, Period = 45000 });
            Drive (ref texture, EffectGuid.Sine, new PeriodicForce { Magnitude = f.Texture, Period = 16000 });
            var push = flipDirection ? -f.Constant : f.Constant;
            Drive (ref constant, EffectGuid.ConstantForce, new ConstantForce { Magnitude = push });
            return error.Length > 0 ? error : e2;
        }
    }

    public void StopForces()
    {
        lock (gate)
        {
            foreach (var e in new[] { spring, damper, rumble, texture, constant })
            {
                try { e?.Stop(); e?.Dispose(); } catch { }
            }
            spring = damper = rumble = texture = constant = null;
        }
    }

    public void Close()
    {
        StopForces();
        lock (gate)
        {
            try { joystick?.Unacquire(); joystick?.Dispose(); } catch { }
            joystick = null;
            actuatorAxes = Array.Empty<int>();
        }
    }

    public void Dispose()
    {
        Close();
        directInput.Dispose();
    }
}
