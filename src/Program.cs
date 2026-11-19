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
            using var bmp = new Bitmap (form.Width, form.Height);
            form.DrawToBitmap (bmp, new Rectangle (Point.Empty, bmp.Size));
            bmp.Save (args[1], System.Drawing.Imaging.ImageFormat.Png);
            return;
        }
        Application.Run (new MainForm());
    }
}
