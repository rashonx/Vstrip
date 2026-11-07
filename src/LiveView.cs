using System.Drawing.Drawing2D;

namespace WheelBridge;

// Picture of what the bridge is doing right now: wheel angle, stick output, pedals, rumble.
public sealed class LiveView : Panel
{
    public Bridge? Bridge;

    public LiveView()
    {
        DoubleBuffered = true;
        BackColor = Theme.Card;
    }

    protected override void OnPaint (PaintEventArgs e)
    {
        var g = e.Graphics;
        g.SmoothingMode = SmoothingMode.AntiAlias;
        g.TextRenderingHint = System.Drawing.Text.TextRenderingHint.ClearTypeGridFit;
        var b = Bridge;
        float deg = b?.SteeringDeg ?? 0, stick = b?.StickOut ?? 0;

        using var titleFont = new Font ("Segoe UI", 10f, FontStyle.Bold);
        using var smallFont = new Font ("Segoe UI", 8.5f);
        using var bigFont = new Font ("Segoe UI", 18f, FontStyle.Bold);
        TextRenderer.DrawText (g, "LIVE", titleFont, new Point (14, 10), Theme.Text);

        // Steering wheel graphic
        var centre = new PointF (Width / 2f, 130);
        const float r = 78;
        using (var rim = new Pen (Theme.Track, 14))
            g.DrawEllipse (rim, centre.X - r, centre.Y - r, r * 2, r * 2);
        var state = g.Save();
        g.TranslateTransform (centre.X, centre.Y);
        g.RotateTransform (deg);
        using (var spoke = new Pen (Color.FromArgb (90, 96, 108), 10))
        {
            g.DrawLine (spoke, -r + 6, 0, -18, 0);
            g.DrawLine (spoke, 18, 0, r - 6, 0);
            g.DrawLine (spoke, 0, 18, 0, r - 6);
        }
        using (var hub = new SolidBrush (Color.FromArgb (40, 44, 52)))
            g.FillEllipse (hub, -22, -22, 44, 44);
        using (var mark = new Pen (Theme.Gold, 14))
            g.DrawArc (mark, -r, -r, r * 2, r * 2, -100, 20);
        g.Restore (state);
        var degText = $"{deg:0}°";
        var size = TextRenderer.MeasureText (degText, bigFont);
        TextRenderer.DrawText (g, degText, bigFont, new Point ((int) (centre.X - size.Width / 2f), (int) (centre.Y + r + 10)), Theme.Text);

        // Stick output bar (what the game receives)
        int y = 270;
        TextRenderer.DrawText (g, "Game steering (Xbox stick)", smallFont, new Point (14, y), Theme.Dim);
        var bar = new RectangleF (14, y + 18, Width - 28, 12);
        using (var track = new SolidBrush (Theme.Track)) g.FillRectangle (track, bar);
        using (var fill = new SolidBrush (Theme.Gold))
        {
            var half = bar.Width / 2f * Math.Clamp (stick, -1f, 1f);
            g.FillRectangle (fill, half >= 0 ? bar.X + bar.Width / 2f : bar.X + bar.Width / 2f + half, bar.Y, Math.Abs (half), bar.Height);
        }
        using (var mid = new Pen (Color.White)) g.DrawLine (mid, bar.X + bar.Width / 2f, bar.Y - 3, bar.X + bar.Width / 2f, bar.Bottom + 3);

        // Pedals
        y += 48;
        void Pedal (string name, float v, Color c)
        {
            TextRenderer.DrawText (g, $"{name}  {v * 100:0}%", smallFont, new Point (14, y), Theme.Dim);
            var r2 = new RectangleF (14, y + 18, Width - 28, 10);
            using (var track = new SolidBrush (Theme.Track)) g.FillRectangle (track, r2);
            using (var fill = new SolidBrush (c)) g.FillRectangle (fill, r2.X, r2.Y, r2.Width * Math.Clamp (v, 0, 1), r2.Height);
            y += 38;
        }
        Pedal ("Throttle", b?.Throttle ?? 0, Theme.Green);
        Pedal ("Brake", b?.Brake ?? 0, Theme.Red);
        Pedal ("Clutch", b?.Clutch ?? 0, Theme.Blue);
        Pedal ("Game rumble to wheel", b?.RumbleOut ?? 0, Theme.Gold);

        // Buttons currently held
        var held = b == null ? "" : string.Join (", ", Enumerable.Range (0, b.ButtonsDown.Length).Where (i => b.ButtonsDown[i]).Select (i => $"{i + 1}"));
        TextRenderer.DrawText (g, "Wheel buttons held: " + (held.Length == 0 ? "none" : held), smallFont, new Point (14, y + 4), Theme.Dim);
    }
}
