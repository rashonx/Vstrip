namespace WheelBridge;

// Combined pedal axes, both ways:
//  - OUT: how gas and brake reach the game (separate triggers, or one combined axis)
//  - IN:  pedals that report gas and brake on one shared axis (resting in the middle)
public static class PedalMix
{
    public static readonly string[] OutputModes =
    {
        "Separate (RT gas, LT brake)",
        "Combined on triggers",
        "Combined on right stick",
        "Combined on left stick",
    };

    public struct Output
    {
        public byte RightTrigger, LeftTrigger;
        public short LeftY, RightY;
    }

    // gas and brake are 0..1
    public static Output Map (int mode, float gas, float brake)
    {
        gas = Math.Clamp (gas, 0f, 1f);
        brake = Math.Clamp (brake, 0f, 1f);
        var combined = gas - brake; // -1 (full brake) .. 1 (full gas); both pressed cancel out
        var o = new Output();
        switch (mode)
        {
            case 1:
                o.RightTrigger = ToByte (Math.Max (0f, combined));
                o.LeftTrigger = ToByte (Math.Max (0f, -combined));
                break;
            case 2: o.RightY = ToShort (combined); break;
            case 3: o.LeftY = ToShort (combined); break;
            default:
                o.RightTrigger = ToByte (gas);
                o.LeftTrigger = ToByte (brake);
                break;
        }
        return o;
    }

    // One shared input axis: resting value in the middle, gas on one side, brake on the other.
    // lo / hi remember the furthest values seen on each side (calibrates as you press).
    // Without swap, values below rest are gas and above are brake.
    public static (float Gas, float Brake) SplitShared (int value, int rest, ref int lo, ref int hi, bool swap)
    {
        lo = Math.Min (lo, value);
        hi = Math.Max (hi, value);
        float below = rest - lo < 3000 ? 0f : Math.Clamp ((rest - value) / (float) (rest - lo), 0f, 1f);
        float above = hi - rest < 3000 ? 0f : Math.Clamp ((value - rest) / (float) (hi - rest), 0f, 1f);
        // ignore tiny wobble around the resting point
        if (below < 0.02f) below = 0f;
        if (above < 0.02f) above = 0f;
        return swap ? (above, below) : (below, above);
    }

    static byte ToByte (float v) => (byte) Math.Round (Math.Clamp (v, 0f, 1f) * 255f);
    static short ToShort (float v) => (short) Math.Round (Math.Clamp (v, -1f, 1f) * 32767f);
}
