using System.Diagnostics;

namespace WheelBridge;

// Hides the real wheel from games (but not from Wheel Bridge) using the free HidHide tool.
public static class HidHide
{
    public static string? FindCli()
    {
        var root = Environment.GetFolderPath (Environment.SpecialFolder.ProgramFiles);
        foreach (var sub in new[] { "x64", "x86", "" })
        {
            var p = Path.Combine (root, "Nefarius Software Solutions", "HidHide", sub, "HidHideCLI.exe");
            if (File.Exists (p)) return p;
        }
        return null;
    }

    public static bool Installed() => FindCli() != null;

    // "\\?\hid#vid_346e&pid_0006#7&abc&0&0000#{guid}"  ->  "HID\VID_346E&PID_0006\7&ABC&0&0000"
    public static string InstancePathFrom (string interfacePath)
    {
        var p = interfacePath;
        if (p.StartsWith (@"\\?\")) p = p[4..];
        var brace = p.LastIndexOf ("#{", StringComparison.Ordinal);
        if (brace > 0) p = p[..brace];
        return p.Replace ('#', '\\').ToUpperInvariant();
    }

    // Runs HidHideCLI with admin rights (Windows will ask for permission once).
    static string Run (string arguments)
    {
        var cli = FindCli();
        if (cli == null) return "HidHide is not installed";
        try
        {
            using var proc = Process.Start (new ProcessStartInfo (cli, arguments) { UseShellExecute = true, Verb = "runas", WindowStyle = ProcessWindowStyle.Hidden });
            proc?.WaitForExit (15000);
            return proc == null ? "Could not start HidHide" : proc.ExitCode == 0 ? "" : $"HidHide returned code {proc.ExitCode}";
        }
        catch (System.ComponentModel.Win32Exception)
        {
            return "Permission was declined";
        }
        catch (Exception e)
        {
            return e.Message;
        }
    }

    public static string Hide (string interfacePath)
    {
        var exe = Environment.ProcessPath ?? Application.ExecutablePath;
        return Run ($"--app-reg \"{exe}\" --dev-hide \"{InstancePathFrom (interfacePath)}\" --cloak-on");
    }

    public static string Unhide (string interfacePath) => Run ($"--dev-unhide \"{InstancePathFrom (interfacePath)}\"");
}
