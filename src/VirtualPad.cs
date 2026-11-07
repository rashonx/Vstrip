using Nefarius.ViGEm.Client;
using Nefarius.ViGEm.Client.Targets;
using Nefarius.ViGEm.Client.Targets.Xbox360;

namespace WheelBridge;

// The virtual Xbox 360 controller the game sees (needs the free ViGEmBus driver).
public sealed class VirtualPad : IDisposable
{
    public static readonly (string Name, Xbox360Button Button)[] ButtonList =
    {
        ("A", Xbox360Button.A), ("B", Xbox360Button.B), ("X", Xbox360Button.X), ("Y", Xbox360Button.Y),
        ("LB", Xbox360Button.LeftShoulder), ("RB", Xbox360Button.RightShoulder),
        ("Back", Xbox360Button.Back), ("Start", Xbox360Button.Start),
        ("Left stick click", Xbox360Button.LeftThumb), ("Right stick click", Xbox360Button.RightThumb),
        ("D-pad Up", Xbox360Button.Up), ("D-pad Down", Xbox360Button.Down),
        ("D-pad Left", Xbox360Button.Left), ("D-pad Right", Xbox360Button.Right),
    };

    ViGEmClient? client;
    IXbox360Controller? pad;

    public float LargeMotor { get; private set; }
    public float SmallMotor { get; private set; }
    public bool IsConnected => pad != null;

    public static bool DriverInstalled()
    {
        try { using var c = new ViGEmClient(); return true; }
        catch { return false; }
    }

    public string Connect()
    {
        try
        {
            client = new ViGEmClient();
            pad = client.CreateXbox360Controller();
            pad.AutoSubmitReport = false;
            pad.FeedbackReceived += (_, e) =>
            {
                LargeMotor = e.LargeMotor / 255f;
                SmallMotor = e.SmallMotor / 255f;
            };
            pad.Connect();
            return "";
        }
        catch (Exception e)
        {
            Disconnect();
            return e is Nefarius.ViGEm.Client.Exceptions.VigemBusNotFoundException
                ? "ViGEmBus driver is not installed"
                : e.Message;
        }
    }

    public void Send (short steering, byte throttle, byte brake, IEnumerable<(Xbox360Button Button, bool Pressed)> buttons)
    {
        if (pad == null) return;
        pad.SetAxisValue (Xbox360Axis.LeftThumbX, steering);
        pad.SetAxisValue (Xbox360Axis.LeftThumbY, 0);
        pad.SetSliderValue (Xbox360Slider.RightTrigger, throttle);
        pad.SetSliderValue (Xbox360Slider.LeftTrigger, brake);
        foreach (var (button, pressed) in buttons)
            pad.SetButtonState (button, pressed);
        pad.SubmitReport();
    }

    public void Disconnect()
    {
        try { pad?.Disconnect(); } catch { }
        pad = null;
        client?.Dispose();
        client = null;
        LargeMotor = SmallMotor = 0;
    }

    public void Dispose() => Disconnect();
}
