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

    public bool CenteringOn { get; set; } = true;
    public int CenteringPct { get; set; } = 35;
    public int DamperPct { get; set; } = 10;
    public int RumblePct { get; set; } = 30;

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
