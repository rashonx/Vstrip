namespace WheelBridge;

static class Program
{
    [STAThread]
    static void Main (string[] args)
    {
        ApplicationConfiguration.Initialize();
        if (args.Length == 2 && args[0] == "--screenshot")
        {
            // Used by the build to take a picture of the window: WheelBridge.exe --screenshot out.png
            using var form = new MainForm { StartPosition = FormStartPosition.Manual, Location = new Point (0, 0), ShowInTaskbar = false };
            form.Show();
            for (var i = 0; i < 20; ++i) { Application.DoEvents(); Thread.Sleep (50); }
            // Draw each panel at its place (the build machine's screen is shorter than the window)
            form.AutoScrollPosition = Point.Empty;
            var size = new Size (form.Controls.Cast<Control>().Max (c => c.Right) + 16, form.Controls.Cast<Control>().Max (c => c.Bottom) + 8);
            using var bmp = new Bitmap (size.Width, size.Height);
            using (var g = Graphics.FromImage (bmp)) g.Clear (form.BackColor);
            foreach (Control c in form.Controls)
                if (c.Visible) c.DrawToBitmap (bmp, c.Bounds);
            bmp.Save (args[1], System.Drawing.Imaging.ImageFormat.Png);
            return;
        }
        Application.Run (new MainForm());
    }
}
