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
    public readonly bool[] ButtonsDown = new bool[128];
    public readonly int[] AxisRaw = new int[Wheel.AxisNames.Length];
    public volatile string ForceError = "";

    // Pedal calibration: resting value and the farthest value seen from it, per axis
    readonly int[] rest = new int[Wheel.AxisNames.Length];
    readonly int[] far = new int[Wheel.AxisNames.Length];
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
        (bool, float, float, float) lastForces = (false, -1, -1, -1);
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
                    for (var i = 1; i < AxisRaw.Length; ++i) { rest[i] = AxisRaw[i]; far[i] = AxisRaw[i]; }
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

                Throttle = PedalValue (s, cfg.ThrottleAxis);
                Brake = PedalValue (s, cfg.BrakeAxis);
                Clutch = PedalValue (s, cfg.ClutchAxis);

                var buttons = s.Buttons;
                for (var b = 0; b < ButtonsDown.Length; ++b)
                    ButtonsDown[b] = b < buttons.Length && buttons[b];

                if (bridging)
                {
                    var mapped = VirtualPad.ButtonList.Select (x =>
                        (x.Button, cfg.Buttons.TryGetValue (x.Name, out var idx) && idx >= 0 && idx < ButtonsDown.Length && ButtonsDown[idx]));
                    Pad.Send ((short) Math.Round (StickOut * 32767f), (byte) Math.Round (Throttle * 255f), (byte) Math.Round (Brake * 255f), mapped);
                }

                // Motor forces, refreshed about 50 times a second when something changed
                if (clock.ElapsedMilliseconds - lastForce >= 20)
                {
                    lastForce = clock.ElapsedMilliseconds;
                    RumbleOut = bridging ? cfg.RumblePct / 100f * Math.Max (Pad.LargeMotor, Pad.SmallMotor) : 0f;
                    var now = (cfg.CenteringOn, cfg.CenteringPct / 100f, cfg.DamperPct / 100f, RumbleOut);
                    if (now != lastForces)
                    {
                        ForceError = Wheel.UpdateForces (now.Item1, now.Item2, now.Item3, now.Item4);
                        lastForces = now;
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
