namespace WheelBridge;

// What the force feedback engine reads each step
public struct FfbInput
{
    public float Stick;          // steering, -1 (full left) .. 1 (full right), before deadzones
    public float Throttle, Brake; // 0..1
    public float LargeMotor, SmallMotor; // the game's Xbox rumble, 0..1 (0 when not bridging)
    public float Dt;             // seconds since the last step
}

// What the wheel motor should do right now. Strengths are DirectInput units (0..10000).
public struct FfbOutput
{
    public int Spring, Damper, Rumble, Texture;
    public int Constant;         // -10000..10000, positive pushes the wheel to the right
    public float Speed;          // estimated speed 0..1, for the window
}

// Turns the game's two rumble motors plus your pedals and steering into wheel forces:
// crash kicks, road texture, speed-feel weight and kickback, on top of centring, damper and rumble.
// No game telemetry is needed; everything is worked out from what Wheel Bridge can see.
public sealed class FfbEngine
{
    float speed;                      // estimated from the pedals
    float largeSlow;                  // slow-moving copy of the big motor, to spot sudden hits
    float prevStick, stickVel;
    bool kickbackArmed;
    float crashAmp, crashT = 99f, crashDir = 1f, crashCooldown;
    float backAmp, backT = 99f, backDir;
    float testT = 99f;
    readonly Random rng = new();

    const float CrashLength = 0.16f, BackLength = 0.20f, TestLength = 0.35f;

    public float Speed => speed;

    // A short push to the right, so you can check which way the wheel kicks
    public void TestKick() => testT = 0f;

    public FfbOutput Step (BridgeSettings cfg, in FfbInput input)
    {
        var dt = Math.Clamp (input.Dt, 0.0005f, 0.1f);
        var o = new FfbOutput();

        // Speed estimate: throttle speeds the "car" up, brake slows it hard, drag slows it gently.
        // Takes about 5 seconds of full throttle to reach top speed.
        speed += dt * (input.Throttle * 0.30f - input.Brake * 0.90f - 0.05f * speed - 0.015f);
        speed = Math.Clamp (speed, 0f, 1f);
        o.Speed = speed;

        // 1. Centring spring, made heavier with speed (and lighter when slow) by Speed weight
        var weight = cfg.SpeedWeightOn ? cfg.SpeedWeightPct / 100f : 0f;
        var springScale = 1f + weight * (-0.5f + 1.6f * speed);   // 0.5x parked .. 2.1x flat out at 100%
        var damperScale = 1f + weight * 0.6f * speed;
        o.Spring = cfg.CenteringOn ? ToUnits (cfg.CenteringPct / 100f * springScale) : 0;
        o.Damper = cfg.CenteringOn ? ToUnits (cfg.DamperPct / 100f * damperScale) : 0;
        o.Spring -= o.Spring % 50;   // small steps so the motor isn't re-sent every millisecond
        o.Damper -= o.Damper % 50;

        // 2. Rumble (big motor) and road texture (small motor)
        var rumbleSource = cfg.TextureOn ? input.LargeMotor : Math.Max (input.LargeMotor, input.SmallMotor);
        o.Rumble = ToUnits (cfg.RumblePct / 100f * rumbleSource);
        o.Texture = cfg.TextureOn ? ToUnits (cfg.TexturePct / 100f * input.SmallMotor * (0.6f + 0.4f * speed)) : 0;

        // 3. Crash kicks: a sudden jump in the big motor is a hit -> sharp jolt then a smaller rebound
        crashCooldown -= dt;
        var jump = input.LargeMotor - largeSlow;
        largeSlow += (input.LargeMotor - largeSlow) * Math.Min (1f, dt * 6f);
        if (cfg.CrashOn && jump > 0.2f && crashCooldown <= 0f)
        {
            crashAmp = Math.Clamp (jump * 1.4f, 0.35f, 1f) * cfg.CrashPct / 100f;
            crashDir = Math.Abs (input.Stick) > 0.05f ? -Math.Sign (input.Stick) : (rng.Next (2) == 0 ? -1f : 1f);
            crashT = 0f;
            crashCooldown = 0.3f;
        }
        float constant = 0f;
        if (crashT < CrashLength)
        {
            var env = crashT < 0.05f ? 1f : -0.5f * (1f - (crashT - 0.05f) / (CrashLength - 0.05f));
            constant += crashDir * crashAmp * env;
            crashT += dt;
        }

        // 4. Kickback: after a big turn, letting the wheel come back fast snaps it to centre
        var vel = (input.Stick - prevStick) / dt;
        stickVel += (vel - stickVel) * Math.Min (1f, dt * 30f);
        prevStick = input.Stick;
        var mag = Math.Abs (input.Stick);
        if (mag > 0.3f) kickbackArmed = true;
        if (cfg.KickbackOn && kickbackArmed && mag > 0.12f
            && Math.Sign (stickVel) == -Math.Sign (input.Stick) && Math.Abs (stickVel) > 1.5f)
        {
            backAmp = cfg.KickbackPct / 100f * (0.4f + 0.6f * speed) * Math.Min (1f, Math.Abs (stickVel) / 4f);
            backDir = -Math.Sign (input.Stick);
            backT = 0f;
            kickbackArmed = false;
        }
        if (backT < BackLength)
        {
            constant += backDir * backAmp * (1f - backT / BackLength);
            backT += dt;
        }

        if (testT < TestLength)
        {
            constant += 0.6f;
            testT += dt;
        }

        o.Constant = (int) Math.Round (Math.Clamp (constant, -1f, 1f) * 10000f);
        return o;
    }

    static int ToUnits (float v) => (int) Math.Round (Math.Clamp (v, 0f, 1f) * 10000f);
}
