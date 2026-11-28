using System.Diagnostics;
using System.Runtime.InteropServices;

namespace WheelBridge;

// Runs about 1000 times a second: reads the wheel, feeds the virtual Xbox controller, updates the motor forces.
public sealed class Bridge : IDisposable
{
    [DllImport ("winmm.dll")] static extern uint timeBeginPeriod (uint ms);
    [DllImport ("winmm.dll")] static extern uint timeEndPeriod (uint ms);

    public readonly Wheel Wheel = new();
    public readonly VirtualPad Pad = new();
    public BridgeSettings Settings;

    Thread? thread;
    volatile bool looping, bridging;

    // Live values for the window
    public volatile float SteeringDeg, StickOut, Throttle, Brake, Clutch, RumbleOut;
    public volatile float SpeedFeel, KickOut, TextureOut;
    public readonly FfbEngine Ffb = new();
    public readonly bool[] ButtonsDown = new bool[128];
    public readonly int[] AxisRaw = new int[Wheel.AxisNames.Length];
    public volatile string ForceError = "";

    // Pedal calibration: resting value and the farthest value seen from it, per axis
    readonly int[] rest = new int[Wheel.AxisNames.Length];
    readonly int[] far = new int[Wheel.AxisNames.Length];
    readonly int[] lo = new int[Wheel.AxisNames.Length], hi = new int[Wheel.AxisNames.Length]; // shared gas/brake axis
    volatile bool captureRest = true;

    public bool IsBridging => bridging;

    public Bridge (BridgeSettings settings) { Settings = settings; }

    public void RecalibratePedals() => captureRest = true;

    public void StartLoop()
    {
        if (looping) return;
        looping = true;
        captureRest = true;
        thread = new Thread (Loop) { IsBackground = true, Priority = ThreadPriority.AboveNormal, Name = "Wheel Bridge" };
        thread.Start();
    }

    public void StopLoop()
    {
        looping = false;
        thread?.Join (500);
        thread = null;
        Wheel.StopForces();
    }

    public string StartBridging()
    {
        var error = Pad.Connect();
        bridging = error.Length == 0;
        return error;
    }

    public void StopBridging()
    {
        bridging = false;
        Pad.Disconnect();
    }

    float PedalValue (SharpDX.DirectInput.JoystickState s, string axis)
    {
        var i = Array.IndexOf (Wheel.AxisNames, axis);
        if (i <= 0) return 0f;
        var v = Wheel.AxisValue (s, axis);
        var travel = Math.Abs (v - rest[i]);
        if (travel > Math.Abs (far[i] - rest[i])) far[i] = v;
        var full = Math.Abs (far[i] - rest[i]);
        return full < 3000 ? 0f : Math.Clamp (travel / (float) full, 0f, 1f);
    }

    void Loop()
    {
        timeBeginPeriod (1);
        var clock = Stopwatch.StartNew();
        long lastForce = -1000;
        FfbOutput lastForces = new() { Spring = -1 };
        var lastFlip = false;
        try
        {
            while (looping)
            {
                var s = Wheel.Read();
                if (s == null)
                {
                    Thread.Sleep (5);
                    continue;
                }

                for (var i = 1; i < AxisRaw.Length; ++i)
                    AxisRaw[i] = Wheel.AxisValue (s, Wheel.AxisNames[i]);
                if (captureRest)
                {
                    for (var i = 1; i < AxisRaw.Length; ++i) { rest[i] = AxisRaw[i]; far[i] = AxisRaw[i]; lo[i] = AxisRaw[i]; hi[i] = AxisRaw[i]; }
                    captureRest = false;
                }

                var cfg = Settings;
                // Steering: wheel angle -> stick, with deadzone and anti-deadzone
                var wheelPos = (s.X - 32767.5f) / 32767.5f;
                SteeringDeg = wheelPos * cfg.WheelRotation * 0.5f;
                var stick = Math.Clamp (SteeringDeg / Math.Max (1f, cfg.SteeringLock * 0.5f), -1f, 1f);
                var mag = Math.Abs (stick);
                var dz = cfg.DeadzonePct / 100f;
                var adz = cfg.AntiDeadzonePct / 100f;
                mag = mag <= dz ? 0f : (mag - dz) / (1f - dz);
                if (mag > 0.0005f) mag = adz + (1f - adz) * mag;
                StickOut = Math.Sign (stick) * Math.Min (1f, mag);

                var sharedIndex = Array.IndexOf (Wheel.AxisNames, cfg.ThrottleAxis);
                if (cfg.SharedPedalAxis && sharedIndex > 0)
                {
                    // Gas and brake on one axis that rests in the middle
                    var (gas, brk) = PedalMix.SplitShared (AxisRaw[sharedIndex], rest[sharedIndex], ref lo[sharedIndex], ref hi[sharedIndex], cfg.SwapSharedPedals);
                    Throttle = gas;
                    Brake = brk;
                }
                else
                {
                    Throttle = PedalValue (s, cfg.ThrottleAxis);
                    Brake = PedalValue (s, cfg.BrakeAxis);
                }
                Clutch = PedalValue (s, cfg.ClutchAxis);

                var buttons = s.Buttons;
                for (var b = 0; b < ButtonsDown.Length; ++b)
                    ButtonsDown[b] = b < buttons.Length && buttons[b];

                if (bridging)
                {
                    var mapped = VirtualPad.ButtonList.Select (x =>
                        (x.Button, cfg.Buttons.TryGetValue (x.Name, out var idx) && idx >= 0 && idx < ButtonsDown.Length && ButtonsDown[idx]));
                    Pad.Send ((short) Math.Round (StickOut * 32767f), PedalMix.Map (cfg.PedalOutputMode, Throttle, Brake), mapped);
                }

                // Motor forces, refreshed 100 times a second (only sent when something changed)
                var ms = clock.ElapsedMilliseconds;
                if (ms - lastForce >= 10)
                {
                    var input = new FfbInput
                    {
                        Stick = stick, Throttle = Throttle, Brake = Brake, Dt = (ms - lastForce) / 1000f,
                        LargeMotor = bridging ? Pad.LargeMotor : 0f, SmallMotor = bridging ? Pad.SmallMotor : 0f,
                    };
                    lastForce = ms;
                    var f = Ffb.Step (cfg, input);
                    SpeedFeel = f.Speed;
                    RumbleOut = f.Rumble / 10000f;
                    TextureOut = f.Texture / 10000f;
                    KickOut = f.Constant / 10000f;
                    if ((f.Spring, f.Damper, f.Rumble, f.Texture, f.Constant) != (lastForces.Spring, lastForces.Damper, lastForces.Rumble, lastForces.Texture, lastForces.Constant)
                        || cfg.FlipForce != lastFlip)
                    {
                        ForceError = Wheel.UpdateForces (f, cfg.FlipForce);
                        lastForces = f;
                        lastFlip = cfg.FlipForce;
                    }
                }

                Thread.Sleep (1);
            }
        }
        finally
        {
            timeEndPeriod (1);
        }
    }

    public void Dispose()
    {
        StopBridging();
        StopLoop();
        Wheel.Dispose();
        Pad.Dispose();
    }
}
