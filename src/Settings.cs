using System.Text.Json;

namespace WheelBridge;

// Everything the user can change, saved to %AppData%\Dodge City Plugins\Wheel Bridge\settings.json
public class BridgeSettings
{
    public string DeviceName { get; set; } = "";
    public int WheelRotation { get; set; } = 900;   // the rotation set in Moza Pit House
    public int SteeringLock { get; set; } = 360;    // wheel degrees that give full stick in the game
    public int DeadzonePct { get; set; } = 0;
    public int AntiDeadzonePct { get; set; } = 12;  // jumps past the game's own stick deadzone

    public string ThrottleAxis { get; set; } = "Y";
    public string BrakeAxis { get; set; } = "Z";
    public string ClutchAxis { get; set; } = "None";

    // v1.3: combined pedal axes
    public int PedalOutputMode { get; set; } = 0;        // see PedalMix.OutputModes
    public bool SharedPedalAxis { get; set; } = false;   // gas and brake come in on one axis (the Throttle axis)
    public bool SwapSharedPedals { get; set; } = false;

    public bool CenteringOn { get; set; } = true;
    public int CenteringPct { get; set; } = 35;
    public int DamperPct { get; set; } = 10;
    public int RumblePct { get; set; } = 30;

    // Force feedback effects (v1.1)
    public bool CrashOn { get; set; } = true;
    public int CrashPct { get; set; } = 60;
    public bool TextureOn { get; set; } = true;
    public int TexturePct { get; set; } = 40;
    public bool SpeedWeightOn { get; set; } = true;
    public int SpeedWeightPct { get; set; } = 50;
    public bool KickbackOn { get; set; } = true;
    public int KickbackPct { get; set; } = 40;
    public bool FlipForce { get; set; } = false;

    // v1.2: hide the real wheel from games automatically while the bridge runs
    public bool AutoHide { get; set; } = true;
    public bool WheelLeftHidden { get; set; } = false;

    public Dictionary<string, int> Buttons { get; set; } = new();

    static string Folder => Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "Dodge City Plugins", "Wheel Bridge");
    static string FilePath => Path.Combine(Folder, "settings.json");

    public static BridgeSettings Load()
    {
        try
        {
            if (File.Exists(FilePath))
                return JsonSerializer.Deserialize<BridgeSettings>(File.ReadAllText(FilePath)) ?? new BridgeSettings();
        }
        catch { /* corrupt file: start fresh */ }
        return new BridgeSettings();
    }

    public void Save()
    {
        try
        {
            Directory.CreateDirectory(Folder);
            File.WriteAllText(FilePath, JsonSerializer.Serialize(this, new JsonSerializerOptions { WriteIndented = true }));
        }
        catch { /* settings are a convenience; never crash over them */ }
    }
}
