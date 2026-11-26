using System.Diagnostics;

namespace WheelBridge;

// Installs the two helper drivers Wheel Bridge needs, from installers built into WheelBridge.exe:
//   ViGEmBus - lets Windows show the virtual Xbox controller
//   HidHide  - lets Wheel Bridge hide the real wheel from games
// Windows only allows drivers to do these two jobs, so they can't be skipped - but you never have to fetch them.
public static class DriverSetup
{
    public const string ViGEmResource = "drivers.ViGEmBusSetup.exe";
    public const string HidHideResource = "drivers.HidHideSetup.exe";

    public static bool Bundled (string resource) =>
        typeof (DriverSetup).Assembly.GetManifestResourceInfo (resource) != null;

    public static bool AllInstalled => VirtualPad.DriverInstalled() && HidHide.Installed();

    // Installs whatever is missing. Wheel Bridge already runs as administrator, so no extra prompts.
    // Runs each installer quietly; if a quiet install doesn't work it opens the installer's normal window instead.
    // Returns a message for the status line.
    public static string InstallMissing (Action<string> progress)
    {
        var problems = new List<string>();
        var needsRestart = false;

        if (! VirtualPad.DriverInstalled())
        {
            progress ("Installing ViGEmBus (virtual Xbox controller driver)...");
            var r = Install (ViGEmResource, "ViGEmBus");
            if (r.Error.Length > 0) problems.Add (r.Error);
            needsRestart |= r.RestartNeeded;
        }
        if (! HidHide.Installed())
        {
            progress ("Installing HidHide (hides the wheel from games)...");
            var r = Install (HidHideResource, "HidHide");
            if (r.Error.Length > 0) problems.Add (r.Error);
            needsRestart |= r.RestartNeeded;
        }

        if (problems.Count > 0) return string.Join (" ", problems);
        return needsRestart || ! AllInstalled
            ? "Drivers installed. Restart your PC once, then open Wheel Bridge again."
            : "Drivers installed. Unplug the wheel's USB cable and plug it back in, then click Refresh.";
    }

    static (string Error, bool RestartNeeded) Install (string resource, string name)
    {
        try
        {
            using var stream = typeof (DriverSetup).Assembly.GetManifestResourceStream (resource);
            if (stream == null) return ($"{name} isn't built into this copy of Wheel Bridge.", false);

            var folder = Path.Combine (Path.GetTempPath(), "WheelBridgeSetup");
            Directory.CreateDirectory (folder);
            var file = Path.Combine (folder, name + "Setup.exe");
            using (var output = File.Create (file))
                stream.CopyTo (output);

            // 0 = done, 3010 / 1641 = done but Windows wants a restart
            var code = Run (file, "/quiet /norestart");
            if (code != 0 && code != 3010 && code != 1641)
                code = Run (file, ""); // quiet install refused: show the installer's own window
            try { File.Delete (file); } catch { }

            return code is 0 or 3010 or 1641
                ? ("", code != 0)
                : ($"{name} setup ended with code {code}.", false);
        }
        catch (Exception e)
        {
            return ($"{name} setup failed: {e.Message}", false);
        }
    }

    static int Run (string file, string arguments)
    {
        using var p = Process.Start (new ProcessStartInfo (file, arguments) { UseShellExecute = false });
        if (p == null) return -1;
        p.WaitForExit();
        return p.ExitCode;
    }
}
