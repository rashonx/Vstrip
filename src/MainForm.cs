using System.Diagnostics;
using SharpDX.DirectInput;

namespace WheelBridge;

public sealed class MainForm : Form
{
    const string Version = "1.0.0";
    readonly BridgeSettings settings = BridgeSettings.Load();
    readonly Bridge bridge;
    readonly System.Windows.Forms.Timer uiTimer = new() { Interval = 33 };

    // Setup
    readonly ComboBox deviceBox = new() { DropDownStyle = ComboBoxStyle.DropDownList };
    readonly Button refreshButton = MakeButton ("Refresh");
    readonly Label wheelStatus = MakeLabel ("");
    readonly Label vigemStatus = MakeLabel ("");
    readonly Label hidHideStatus = MakeLabel ("");
    readonly Button hideButton = MakeButton ("Hide wheel from games");
    readonly Button showButton = MakeButton ("Show wheel to games");
    readonly Button startButton = new() { FlatStyle = FlatStyle.Flat, Font = new Font ("Segoe UI", 15f, FontStyle.Bold) };
    readonly Label statusLine = MakeLabel ("");
    readonly LiveView live = new();

    // Settings controls
    readonly NumericUpDown rotationBox = new() { Minimum = 90, Maximum = 2700, Increment = 10 };
    readonly CheckBox centeringBox = new() { Text = "Constant centring force", AutoSize = true, ForeColor = Theme.Text };
    readonly ComboBox throttleBox = AxisBox(), brakeBox = AxisBox(), clutchBox = AxisBox();
    readonly Dictionary<string, Label> buttonLabels = new();

    List<DeviceInstance> devices = new();
    string? learnTarget;
    bool[] learnStart = Array.Empty<bool>();
    (ComboBox Box, int[] Baseline, DateTime Until)? detecting;

    public MainForm()
    {
        bridge = new Bridge (settings);
        Text = $"Wheel Bridge v{Version} - Dodge City Plugins";
        BackColor = Theme.Back;
        ForeColor = Theme.Text;
        Font = new Font ("Segoe UI", 9.5f);
        ClientSize = new Size (1000, 720);
        FormBorderStyle = FormBorderStyle.FixedSingle;
        MaximizeBox = false;
        StartPosition = FormStartPosition.CenterScreen;

        BuildHeader();
        BuildSetupCard();
        BuildLive();
        BuildSteeringCard();
        BuildPedalsAndButtonsCard();

        statusLine.SetBounds (16, 692, 968, 20);
        statusLine.ForeColor = Theme.Dim;
        Controls.Add (statusLine);

        uiTimer.Tick += (_, _) => Tick();
        Load += (_, _) => { RefreshDevices (true); uiTimer.Start(); };
        FormClosing += (_, _) => { settings.Save(); uiTimer.Stop(); bridge.Dispose(); };
    }

    //==========================================================================
    // Building the window

    static Button MakeButton (string text) => new()
    {
        Text = text, FlatStyle = FlatStyle.Flat, BackColor = Theme.Track, ForeColor = Theme.Text, Cursor = Cursors.Hand,
        FlatAppearance = { BorderSize = 0 },
    };

    static Label MakeLabel (string text, bool bold = false) => new()
    {
        Text = text, AutoSize = false, ForeColor = Theme.Text, BackColor = Color.Transparent,
        Font = new Font ("Segoe UI", bold ? 10f : 9.5f, bold ? FontStyle.Bold : FontStyle.Regular),
    };

    static ComboBox AxisBox()
    {
        var box = new ComboBox { DropDownStyle = ComboBoxStyle.DropDownList, BackColor = Theme.Track, ForeColor = Theme.Text, FlatStyle = FlatStyle.Flat };
        box.Items.AddRange (Wheel.AxisNames);
        return box;
    }

    Panel Card (int x, int y, int w, int h, string title)
    {
        var p = new Panel { BackColor = Theme.Card, Bounds = new Rectangle (x, y, w, h) };
        var t = MakeLabel (title.ToUpperInvariant(), true);
        t.SetBounds (12, 8, w - 24, 20);
        t.ForeColor = Theme.Gold;
        p.Controls.Add (t);
        Controls.Add (p);
        return p;
    }

    void BuildHeader()
    {
        var logo = new PictureBox { SizeMode = PictureBoxSizeMode.Zoom, Bounds = new Rectangle (12, 8, 230, 70), BackColor = Color.Transparent };
        using (var s = typeof (MainForm).Assembly.GetManifestResourceStream ("WheelBridge.logo.png"))
            if (s != null) logo.Image = Image.FromStream (s);
        Controls.Add (logo);

        var title = MakeLabel ("Wheel Bridge");
        title.Font = new Font ("Segoe UI", 24f, FontStyle.Bold);
        title.ForeColor = Theme.Gold;
        title.SetBounds (256, 10, 400, 44);
        Controls.Add (title);

        var sub = MakeLabel ($"MOZA WHEEL  →  XBOX CONTROLLER   ·   CONSTANT CENTRING   ·   v{Version}");
        sub.ForeColor = Theme.Dim;
        sub.SetBounds (259, 54, 600, 20);
        Controls.Add (sub);
    }

    void BuildSetupCard()
    {
        var card = Card (16, 90, 740, 132, "1. Setup");
        var l = MakeLabel ("Wheel:"); l.SetBounds (12, 38, 50, 22); card.Controls.Add (l);
        deviceBox.SetBounds (64, 36, 380, 24);
        deviceBox.BackColor = Theme.Track; deviceBox.ForeColor = Theme.Text; deviceBox.FlatStyle = FlatStyle.Flat;
        deviceBox.SelectedIndexChanged += (_, _) => ConnectSelected();
        card.Controls.Add (deviceBox);
        refreshButton.SetBounds (452, 35, 80, 26);
        refreshButton.Click += (_, _) => RefreshDevices (false);
        card.Controls.Add (refreshButton);
        wheelStatus.SetBounds (540, 38, 190, 22);
        card.Controls.Add (wheelStatus);

        vigemStatus.SetBounds (12, 72, 360, 22);
        vigemStatus.Cursor = Cursors.Hand;
        vigemStatus.Click += (_, _) => { if (!VirtualPad.DriverInstalled()) OpenUrl ("https://github.com/nefarius/ViGEmBus/releases"); };
        card.Controls.Add (vigemStatus);
        hidHideStatus.SetBounds (12, 98, 360, 22);
        hidHideStatus.Cursor = Cursors.Hand;
        hidHideStatus.Click += (_, _) => { if (!HidHide.Installed()) OpenUrl ("https://github.com/nefarius/HidHide/releases"); };
        card.Controls.Add (hidHideStatus);

        hideButton.SetBounds (380, 72, 170, 48);
        hideButton.Click += (_, _) => RunHidHide (true);
        card.Controls.Add (hideButton);
        showButton.SetBounds (558, 72, 170, 48);
        showButton.Click += (_, _) => RunHidHide (false);
        card.Controls.Add (showButton);

        startButton.SetBounds (768, 90, 216, 132);
        startButton.FlatAppearance.BorderSize = 0;
        startButton.Cursor = Cursors.Hand;
        startButton.Click += (_, _) => ToggleBridge();
        Controls.Add (startButton);
        UpdateStartButton();
    }

    void BuildLive()
    {
        live.Bounds = new Rectangle (16, 234, 300, 450);
        live.Bridge = bridge;
        Controls.Add (live);
    }

    // A labelled slider row: "Name ........ value" with a track bar underneath
    void SliderRow (Panel card, ref int y, string name, int min, int max, int value, string unit, Action<int> onChange)
    {
        var label = MakeLabel (name); label.SetBounds (12, y, 200, 20); label.ForeColor = Theme.Dim; card.Controls.Add (label);
        var valueLabel = MakeLabel ($"{value}{unit}"); valueLabel.SetBounds (card.Width - 90, y, 78, 20); valueLabel.TextAlign = ContentAlignment.MiddleRight; card.Controls.Add (valueLabel);
        var bar = new TrackBar { Minimum = min, Maximum = max, Value = Math.Clamp (value, min, max), TickStyle = TickStyle.None, BackColor = Theme.Card, AutoSize = false };
        bar.SetBounds (6, y + 20, card.Width - 12, 28);
        bar.ValueChanged += (_, _) => { valueLabel.Text = $"{bar.Value}{unit}"; onChange (bar.Value); };
        card.Controls.Add (bar);
        y += 54;
    }

    void BuildSteeringCard()
    {
        var card = Card (328, 234, 320, 450, "2. Steering & force");
        int y = 38;
        var rl = MakeLabel ("Wheel rotation set in Moza Pit House"); rl.SetBounds (12, y, 220, 20); rl.ForeColor = Theme.Dim; card.Controls.Add (rl);
        rotationBox.SetBounds (236, y - 2, 72, 24);
        rotationBox.BackColor = Theme.Track; rotationBox.ForeColor = Theme.Text;
        rotationBox.Value = Math.Clamp (settings.WheelRotation, 90, 2700);
        rotationBox.ValueChanged += (_, _) => settings.WheelRotation = (int) rotationBox.Value;
        card.Controls.Add (rotationBox);
        y += 32;

        SliderRow (card, ref y, "Full lock in game at (wheel degrees)", 90, 1080, settings.SteeringLock, "°", v => settings.SteeringLock = v);
        SliderRow (card, ref y, "Centre deadzone", 0, 30, settings.DeadzonePct, "%", v => settings.DeadzonePct = v);
        SliderRow (card, ref y, "Anti-deadzone (beat the game's)", 0, 40, settings.AntiDeadzonePct, "%", v => settings.AntiDeadzonePct = v);

        y += 6;
        centeringBox.Checked = settings.CenteringOn;
        centeringBox.Location = new Point (12, y);
        centeringBox.CheckedChanged += (_, _) => settings.CenteringOn = centeringBox.Checked;
        card.Controls.Add (centeringBox);
        y += 28;
        SliderRow (card, ref y, "Centring strength", 0, 100, settings.CenteringPct, "%", v => settings.CenteringPct = v);
        SliderRow (card, ref y, "Damper (smooths the wheel)", 0, 100, settings.DamperPct, "%", v => settings.DamperPct = v);
        SliderRow (card, ref y, "Game rumble to wheel", 0, 100, settings.RumblePct, "%", v => settings.RumblePct = v);
    }

    void BuildPedalsAndButtonsCard()
    {
        var card = Card (660, 234, 324, 450, "3. Pedals & buttons");
        int y = 38;
        void PedalRow (string name, ComboBox box, string current, Action<string> set)
        {
            var l = MakeLabel (name); l.SetBounds (12, y + 2, 70, 20); l.ForeColor = Theme.Dim; card.Controls.Add (l);
            box.SetBounds (84, y, 130, 24);
            box.SelectedItem = Wheel.AxisNames.Contains (current) ? current : "None";
            box.SelectedIndexChanged += (_, _) => { set ((string) box.SelectedItem!); bridge.RecalibratePedals(); };
            card.Controls.Add (box);
            var detect = MakeButton ("Detect");
            detect.SetBounds (222, y - 1, 90, 26);
            detect.Click += (_, _) =>
            {
                detecting = (box, (int[]) bridge.AxisRaw.Clone(), DateTime.Now.AddSeconds (8));
                Status ($"Press the {name.ToLowerInvariant()} pedal all the way down now...");
            };
            card.Controls.Add (detect);
            y += 32;
        }
        PedalRow ("Throttle", throttleBox, settings.ThrottleAxis, v => settings.ThrottleAxis = v);
        PedalRow ("Brake", brakeBox, settings.BrakeAxis, v => settings.BrakeAxis = v);
        PedalRow ("Clutch", clutchBox, settings.ClutchAxis, v => settings.ClutchAxis = v);

        var recal = MakeButton ("Re-calibrate pedals (feet off)");
        recal.SetBounds (12, y, 300, 26);
        recal.Click += (_, _) => { bridge.RecalibratePedals(); Status ("Pedals calibrated. Press each pedal all the way once."); };
        card.Controls.Add (recal);
        y += 38;

        var bl = MakeLabel ("Xbox button  ←  wheel button (click Set, then press)"); bl.SetBounds (12, y, 300, 20); bl.ForeColor = Theme.Dim; card.Controls.Add (bl);
        y += 22;
        var list = new Panel { AutoScroll = true, BackColor = Theme.Card };
        list.SetBounds (6, y, card.Width - 12, card.Height - y - 8);
        card.Controls.Add (list);
        int ry = 0;
        foreach (var (name, _) in VirtualPad.ButtonList)
        {
            var n = MakeLabel (name); n.SetBounds (6, ry + 3, 120, 20); list.Controls.Add (n);
            var assigned = MakeLabel (""); assigned.SetBounds (128, ry + 3, 76, 20); assigned.ForeColor = Theme.Gold; list.Controls.Add (assigned);
            buttonLabels[name] = assigned;
            var set = MakeButton ("Set"); set.SetBounds (206, ry, 44, 24);
            set.Click += (_, _) =>
            {
                learnTarget = name;
                learnStart = (bool[]) bridge.ButtonsDown.Clone();
                Status ($"Press the wheel button you want for Xbox {name}...");
                RefreshButtonLabels();
            };
            list.Controls.Add (set);
            var clear = MakeButton ("×"); clear.SetBounds (254, ry, 30, 24);
            clear.Click += (_, _) => { settings.Buttons.Remove (name); RefreshButtonLabels(); };
            list.Controls.Add (clear);
            ry += 28;
        }
        RefreshButtonLabels();
    }

    //==========================================================================
    // Behaviour

    void Status (string text) => statusLine.Text = text;

    static void OpenUrl (string url)
    {
        try { Process.Start (new ProcessStartInfo (url) { UseShellExecute = true }); } catch { }
    }

    void RefreshDevices (bool autoConnect)
    {
        try { devices = bridge.Wheel.ListDevices(); }
        catch (Exception e) { devices = new(); Status ("Could not list game controllers: " + e.Message); }
        deviceBox.Items.Clear();
        foreach (var d in devices) deviceBox.Items.Add (d.InstanceName.Trim());

        var pick = devices.FindIndex (d => d.InstanceName.Trim() == settings.DeviceName);
        if (pick < 0) pick = devices.FindIndex (d => d.InstanceName.Contains ("MOZA", StringComparison.OrdinalIgnoreCase) || d.InstanceName.Contains ("R5", StringComparison.OrdinalIgnoreCase));
        if (pick < 0 && devices.Count > 0) pick = 0;
        if (pick >= 0 && autoConnect) deviceBox.SelectedIndex = pick;
        else if (pick >= 0) deviceBox.SelectedIndex = pick;
        if (devices.Count == 0)
            Status ("No wheel found. Plug in the Moza base, close Pit House if it is blocking, then click Refresh. If the wheel is hidden by HidHide, click 'Show wheel to games' first.");
        UpdateSetupStatus();
    }

    void ConnectSelected()
    {
        var i = deviceBox.SelectedIndex;
        if (i < 0 || i >= devices.Count) return;
        bridge.StopLoop();
        var error = bridge.Wheel.Open (devices[i], Handle);
        if (error.Length > 0)
        {
            Status ("Could not open the wheel: " + error);
            UpdateSetupStatus();
            return;
        }
        settings.DeviceName = devices[i].InstanceName.Trim();
        bridge.StartLoop();
        Status (bridge.Wheel.HasForceFeedback
            ? "Wheel connected. Centring force is active. Keep your feet off the pedals for a second while it calibrates."
            : "Wheel connected, but it did not report force feedback, so centring can't be applied.");
        UpdateSetupStatus();
    }

    void RunHidHide (bool hide)
    {
        if (!bridge.Wheel.IsOpen || bridge.Wheel.InterfacePath.Length == 0)
        {
            Status ("Connect the wheel first.");
            return;
        }
        var error = hide ? HidHide.Hide (bridge.Wheel.InterfacePath) : HidHide.Unhide (bridge.Wheel.InterfacePath);
        Status (error.Length > 0
            ? $"HidHide: {error}. You can also do it by hand in the HidHide Configuration Client: add WheelBridge.exe under Applications, tick your Moza under Devices, and enable hiding."
            : hide ? "The wheel is now hidden from games. Wheel Bridge can still see it." : "The wheel is visible to games again.");
    }

    void ToggleBridge()
    {
        if (bridge.IsBridging)
        {
            bridge.StopBridging();
            Status ("Bridge stopped. The virtual Xbox controller was unplugged.");
        }
        else
        {
            if (!bridge.Wheel.IsOpen)
            {
                Status ("Connect the wheel first.");
                return;
            }
            var error = bridge.StartBridging();
            Status (error.Length > 0
                ? $"Could not create the Xbox controller: {error}. Install ViGEmBus (click its line in Setup), then restart Wheel Bridge."
                : "Running. Games now see an Xbox 360 controller driven by your wheel. Start Need for Speed.");
        }
        settings.Save();
        UpdateStartButton();
    }

    void UpdateStartButton()
    {
        var on = bridge.IsBridging;
        startButton.Text = on ? "RUNNING\nclick to stop" : "START\nBRIDGE";
        startButton.BackColor = on ? Theme.Green : Theme.Gold;
        startButton.ForeColor = Color.FromArgb (20, 16, 8);
    }

    void UpdateSetupStatus()
    {
        var open = bridge.Wheel.IsOpen;
        wheelStatus.Text = open ? (bridge.Wheel.HasForceFeedback ? "✓ Connected, FFB ready" : "✓ Connected, no FFB") : "Not connected";
        wheelStatus.ForeColor = open ? Theme.Green : Theme.Red;

        var vigem = VirtualPad.DriverInstalled();
        vigemStatus.Text = vigem ? "✓ ViGEmBus driver installed" : "✗ ViGEmBus driver missing - click here to download";
        vigemStatus.ForeColor = vigem ? Theme.Green : Theme.Red;

        var hh = HidHide.Installed();
        hidHideStatus.Text = hh ? "✓ HidHide installed" : "✗ HidHide missing - click here to download";
        hidHideStatus.ForeColor = hh ? Theme.Green : Theme.Red;
        hideButton.Enabled = showButton.Enabled = hh && open;
    }

    void RefreshButtonLabels()
    {
        foreach (var (name, label) in buttonLabels)
            label.Text = learnTarget == name ? "press..." : settings.Buttons.TryGetValue (name, out var idx) ? $"Button {idx + 1}" : "-";
    }

    void Tick()
    {
        live.Invalidate();

        // Learning a button mapping: the first newly pressed wheel button wins
        if (learnTarget != null)
        {
            var now = bridge.ButtonsDown;
            for (var i = 0; i < now.Length; ++i)
            {
                if (now[i] && (i >= learnStart.Length || !learnStart[i]))
                {
                    settings.Buttons[learnTarget] = i;
                    Status ($"Wheel button {i + 1} now acts as Xbox {learnTarget}.");
                    learnTarget = null;
                    RefreshButtonLabels();
                    settings.Save();
                    break;
                }
            }
        }

        // Detecting a pedal: the axis that moved furthest
        if (detecting is { } d)
        {
            var best = -1;
            var bestMove = 12000;
            for (var i = 1; i < bridge.AxisRaw.Length; ++i)
            {
                var move = Math.Abs (bridge.AxisRaw[i] - d.Baseline[i]);
                if (Wheel.AxisNames[i] != "X" && move > bestMove) { best = i; bestMove = move; }
            }
            if (best > 0)
            {
                d.Box.SelectedItem = Wheel.AxisNames[best];
                Status ($"Found it: {Wheel.AxisNames[best]} axis. Let go of the pedal.");
                detecting = null;
                settings.Save();
            }
            else if (DateTime.Now > d.Until)
            {
                Status ("No pedal movement seen. Try Detect again and press the pedal all the way.");
                detecting = null;
            }
        }

        if (bridge.ForceError.Length > 0 && bridge.Wheel.IsOpen)
            Status ("Force feedback problem: " + bridge.ForceError + ". Close Pit House or any game using the wheel, then pick the wheel again.");
    }
}
