using SharpDX.DirectInput;

namespace WheelBridge;

// The real Moza wheel: reads its axes and buttons and drives its motor (centring spring, damper, rumble).
public sealed class Wheel : IDisposable
{
    public static readonly string[] AxisNames = { "None", "X", "Y", "Z", "RotationX", "RotationY", "RotationZ", "Slider1", "Slider2" };

    readonly DirectInput directInput = new();
    Joystick? joystick;
    Effect? spring, damper, rumble;
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

    // strength values are 0..1
    public string UpdateForces (bool centeringOn, float centering, float damping, float rumbleLevel)
    {
        lock (gate)
        {
            if (joystick == null || !HasForceFeedback) return "This wheel did not report force feedback";
            try
            {
                var springStrength = centeringOn ? (int) (Math.Clamp (centering, 0f, 1f) * 10000) : 0;
                var damperStrength = centeringOn ? (int) (Math.Clamp (damping, 0f, 1f) * 10000) : 0;

                if (spring == null)
                {
                    spring = new Effect (joystick, EffectGuid.Spring, BaseParameters (Condition (springStrength, 10000)));
                    spring.Start (1);
                }
                else
                    spring.SetParameters (BaseParameters (Condition (springStrength, 10000)), EffectParameterFlags.TypeSpecificParameters);

                if (damper == null)
                {
                    damper = new Effect (joystick, EffectGuid.Damper, BaseParameters (Condition (damperStrength, 10000)));
                    damper.Start (1);
                }
                else
                    damper.SetParameters (BaseParameters (Condition (damperStrength, 10000)), EffectParameterFlags.TypeSpecificParameters);

                var shake = new PeriodicForce { Magnitude = (int) (Math.Clamp (rumbleLevel, 0f, 1f) * 10000), Offset = 0, Phase = 0, Period = 45000 };
                if (rumble == null)
                {
                    rumble = new Effect (joystick, EffectGuid.Sine, BaseParameters (shake));
                    rumble.Start (1);
                }
                else
                    rumble.SetParameters (BaseParameters (shake), EffectParameterFlags.TypeSpecificParameters);
                return "";
            }
            catch (Exception e)
            {
                return e.Message;
            }
        }
    }

    public void StopForces()
    {
        lock (gate)
        {
            foreach (var e in new[] { spring, damper, rumble })
            {
                try { e?.Stop(); e?.Dispose(); } catch { }
            }
            spring = damper = rumble = null;
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
