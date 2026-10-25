using System;
using System.Diagnostics;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;
using System.IO.MemoryMappedFiles;
using System.Runtime.InteropServices;
using System.Windows.Forms;

namespace AtomicKinection {
  /// <summary>Atomic Kinection 1.3.4 window: live preview of the Kinect feed (from akfeed.exe), snapshots and the mic gain.</summary>
  public sealed class MainForm : Form {
    const int W = 1920, H = 1080, Header = 64; const uint Magic = 0x314B4B41;
    readonly PictureBox view = new PictureBox { Dock = DockStyle.Fill, SizeMode = PictureBoxSizeMode.Zoom, BackColor = Color.Black };
    readonly Button snapBtn = new Button { Text = "Snapshot (PNG)", AutoSize = true };
    readonly Button folderBtn = new Button { Text = "Open recordings folder", AutoSize = true };
    readonly Button normalBtn = new Button { Text = "Normal colour", AutoSize = true }, nightBtn = new Button { Text = "Night vision (IR)", AutoSize = true }, depthBtn = new Button { Text = "Depth view", AutoSize = true };
    readonly Button afBtn = new Button { AutoSize = true }, zoomBtn = new Button { AutoSize = true }, recBtn = new Button { Text = "\u25CF Record", AutoSize = true, ForeColor = Color.DarkRed };
    readonly Label recLbl = new Label { AutoSize = true, ForeColor = Color.Red, Font = new Font("Segoe UI", 11f, FontStyle.Bold), Padding = new Padding(6, 6, 0, 0) };
    static string RecFlag => Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "AtomicKinection", "record.flag");
    static string VideosDir => Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.MyVideos), "Atomic Kinection");
    readonly TrackBar gain = new TrackBar { Minimum = 0, Maximum = 40, TickFrequency = 5, Width = 220, Value = 18 };
    readonly Label gainLbl = new Label { AutoSize = true, Padding = new Padding(0, 8, 0, 0) };
    readonly ProgressBar meter = new ProgressBar { Width = 160, Height = 16, Maximum = 60, Margin = new Padding(3, 10, 3, 3) };
    readonly ToolStripStatusLabel status = new ToolStripStatusLabel { Spring = true, TextAlign = ContentAlignment.MiddleLeft };
    readonly Timer timer = new Timer { Interval = 15 };
    MemoryMappedFile mmf; MemoryMappedViewAccessor acc; readonly byte[] buf = new byte[W * H * 4]; Bitmap bmp; int lastSeq = -1; DateTime lastStatus;

    public static string OutDir { get { var d = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments), "AtomicKinection"); Directory.CreateDirectory(d); return d; } }
    static string SettingsFile => Path.Combine(OutDir, "settings.ini");
    static string StatusFile => Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "AtomicKinection", "status.txt");
    public MainForm() {
      Text = "Atomic Kinection 1.3.4"; Width = 1280; Height = 820; Font = new Font("Segoe UI", 9.5f);
      try {
        var ico = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "AtomicKinection.ico");
        if (File.Exists(ico)) Icon = new Icon(ico, 256, 256);
        else { var emb = Icon.ExtractAssociatedIcon(Application.ExecutablePath); if (emb != null) Icon = emb; }
      } catch { }
      var bar = new FlowLayoutPanel { Dock = DockStyle.Top, AutoSize = true, Padding = new Padding(4) };
      bar.Controls.AddRange(new Control[] { snapBtn, folderBtn, new Label { Text = "   Mic gain:", AutoSize = true, Padding = new Padding(0, 8, 0, 0) }, gain, gainLbl, new Label { Text = "Level:", AutoSize = true, Padding = new Padding(0, 8, 0, 0) }, meter });
      var modes = new FlowLayoutPanel { Dock = DockStyle.Top, AutoSize = true, Padding = new Padding(4) };
      modes.Controls.AddRange(new Control[] { new Label { Text = "View:", AutoSize = true, Padding = new Padding(0, 8, 0, 0) }, normalBtn, nightBtn, depthBtn, new Label { Text = "   ", AutoSize = true }, afBtn, zoomBtn, new Label { Text = "   ", AutoSize = true }, recBtn, recLbl });
      var ss = new StatusStrip(); ss.Items.Add(status);
      Controls.Add(view); Controls.Add(modes); Controls.Add(bar); Controls.Add(ss);
      gain.Value = Math.Max(0, Math.Min(40, (int)Math.Round(ReadGain()))); ShowGain();
      gain.ValueChanged += (s, e) => { ShowGain(); WriteGain(gain.Value); };
      snapBtn.Click += (s, e) => Snapshot();
      folderBtn.Click += (s, e) => { Directory.CreateDirectory(VideosDir); Process.Start("explorer.exe", VideosDir); };
      normalBtn.Click += (s, e) => SetMode("Normal"); nightBtn.Click += (s, e) => SetMode("Night"); depthBtn.Click += (s, e) => SetMode("Depth");
      afBtn.Click += (s, e) => { WriteSetting("AutoFraming=", ReadSetting("AutoFraming=") == "1" ? "0" : "1"); ShowModes(); };
      zoomBtn.Click += (s, e) => { WriteSetting("Zoom=", ReadSetting("Zoom=") == "Zoomed" ? "Wide" : "Zoomed"); ShowModes(); };
      recBtn.Click += (s, e) => { try { if (File.Exists(RecFlag)) File.Delete(RecFlag); else { Directory.CreateDirectory(Path.GetDirectoryName(RecFlag)); File.WriteAllText(RecFlag, "1"); } } catch { } };
      ShowModes();
      timer.Tick += (s, e) => Tick(); timer.Start();
      EnsureFeed();
    }
    void SetMode(string md) { WriteSetting("Mode=", md); ShowModes(); }
    void ShowModes() {
      string md = ReadSetting("Mode=") ?? "Normal"; bool af = ReadSetting("AutoFraming=") == "1", z = ReadSetting("Zoom=") == "Zoomed";
      var on = Color.FromArgb(190, 225, 255);
      normalBtn.BackColor = md != "Night" && md != "Depth" ? on : SystemColors.Control; nightBtn.BackColor = md == "Night" ? on : SystemColors.Control; depthBtn.BackColor = md == "Depth" ? on : SystemColors.Control;
      afBtn.Text = "Auto Framing: " + (af ? "On" : "Off"); afBtn.BackColor = af ? Color.FromArgb(200, 235, 200) : SystemColors.Control;
      zoomBtn.Text = z ? "Wide" : "Zoom"; zoomBtn.BackColor = SystemColors.Control; // label = the action it will do
      foreach (Control c in new Control[] { normalBtn, nightBtn, depthBtn, afBtn, zoomBtn }) if (c is Button b) b.UseVisualStyleBackColor = b.BackColor == SystemColors.Control;
    }
    static string ReadSetting(string key) { try { foreach (var l in File.ReadAllLines(SettingsFile)) if (l.StartsWith(key, StringComparison.OrdinalIgnoreCase)) return l.Substring(key.Length).Trim(); } catch { } return null; }
    static void WriteSetting(string key, string val) {
      try {
        var lines = File.Exists(SettingsFile) ? new System.Collections.Generic.List<string>(File.ReadAllLines(SettingsFile)) : new System.Collections.Generic.List<string>();
        lines.RemoveAll(l => l.StartsWith(key, StringComparison.OrdinalIgnoreCase)); lines.Add(key + val); File.WriteAllLines(SettingsFile, lines);
      } catch { }
    }
    void ShowGain() => gainLbl.Text = $"+{gain.Value} dB";
    static double ReadGain() { try { foreach (var l in File.ReadAllLines(SettingsFile)) if (l.StartsWith("MicGainDb=", StringComparison.OrdinalIgnoreCase) && double.TryParse(l.Substring(10), System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture, out var g)) return g; } catch { } return 18; }
    static void WriteGain(int db) {
      try {
        var lines = File.Exists(SettingsFile) ? new System.Collections.Generic.List<string>(File.ReadAllLines(SettingsFile)) : new System.Collections.Generic.List<string>();
        lines.RemoveAll(l => l.StartsWith("MicGainDb=", StringComparison.OrdinalIgnoreCase)); lines.Add("MicGainDb=" + db.ToString(System.Globalization.CultureInfo.InvariantCulture));
        File.WriteAllLines(SettingsFile, lines);
      } catch { }
    }
    static volatile bool closing; // 1.3.4: set once the window is closing so the feed is not restarted
    protected override void OnFormClosing(FormClosingEventArgs e) {
      base.OnFormClosing(e); if (e.Cancel) return;
      closing = true; timer.Stop();
      try { acc?.Dispose(); mmf?.Dispose(); } catch { } acc = null; mmf = null;
      StopFeed();
    }
    /// <summary>1.3.4: tells akfeed.exe to stop its streams and close the Kinect, waits for it to exit, and kills it only as a last resort.</summary>
    public static void StopFeed() {
      closing = true;
      try { if (System.Threading.EventWaitHandle.TryOpenExisting(@"Local\AtomicKinectionFeedQuit", out var ev)) using (ev) ev.Set(); } catch { }
      foreach (var p in Process.GetProcessesByName("akfeed")) {
        try { if (!p.WaitForExit(8000)) { p.Kill(); p.WaitForExit(2000); } } catch { } finally { p.Dispose(); }
      }
    }
    static void EnsureFeed() {
      if (closing) return;
      if (Process.GetProcessesByName("akfeed").Length > 0) return;
      var exe = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "akfeed.exe");
      if (File.Exists(exe)) try { Process.Start(new ProcessStartInfo(exe) { UseShellExecute = false, CreateNoWindow = true }); } catch { }
    }
    const int PW = W, PH = H; // full-resolution preview, one reused bitmap, 30 fps
    unsafe void Tick() {
      try {
        if (acc == null) { mmf = MemoryMappedFile.OpenExisting(@"Local\AtomicKinectionFrame", MemoryMappedFileRights.Read); acc = mmf.CreateViewAccessor(0, Header + W * H * 4, MemoryMappedFileAccess.Read); }
        if (acc.ReadUInt32(0) == Magic && WindowState != FormWindowState.Minimized) {
          int seq = acc.ReadInt32(16);
          if (seq != lastSeq && (seq & 1) == 0) {
            if (bmp == null) { bmp = new Bitmap(PW, PH, PixelFormat.Format32bppRgb); view.Image = bmp; }
            byte* p = null; acc.SafeMemoryMappedViewHandle.AcquirePointer(ref p);
            try {
              uint* src = (uint*)(p + acc.PointerOffset + Header);
              var bd = bmp.LockBits(new Rectangle(0, 0, PW, PH), ImageLockMode.WriteOnly, PixelFormat.Format32bppRgb);
              uint* dst = (uint*)bd.Scan0; int ds = bd.Stride / 4;
              for (int y = 0; y < PH; y++) { Buffer.MemoryCopy(src + y * W, dst + y * ds, W * 4, W * 4); }
              bmp.UnlockBits(bd);
            } finally { acc.SafeMemoryMappedViewHandle.ReleasePointer(); }
            lastSeq = seq; view.Invalidate();
          }
        }
      } catch { acc?.Dispose(); mmf?.Dispose(); acc = null; mmf = null; }
      if ((DateTime.Now - lastStatus).TotalMilliseconds > 500) { lastStatus = DateTime.Now; ReadStatus(); }
    }
    void ReadStatus() {
      try {
        var info = new System.Collections.Generic.Dictionary<string, string>();
        foreach (var l in File.ReadAllLines(StatusFile)) { int i = l.IndexOf('='); if (i > 0) info[l.Substring(0, i)] = l.Substring(i + 1); }
        double.TryParse(info["MicLevelDb"], System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture, out var lv);
        meter.Value = Math.Max(0, Math.Min(60, (int)(lv + 60)));
        bool fresh = (DateTime.Now - File.GetLastWriteTime(StatusFile)).TotalSeconds < 5;
        string g(string k) => info.TryGetValue(k, out var v) ? v : "?";
        status.Text = fresh ? $"Kinect: {g("Kinect")}   |   {g("Mode")} view {g("Resolution")} @ {g("Fps")} fps   |   Auto Framing {g("AutoFraming")} (faces {g("Faces")}), {g("Zoom")}   |   mic {lv:0} dBFS   |   drops {g("Drops")}, reconnects {g("Reconnects")}" : "Kinect feed not running - starting it...";
        bool rec = g("Recording") == "1"; double.TryParse(g("RecSeconds"), System.Globalization.NumberStyles.Float, System.Globalization.CultureInfo.InvariantCulture, out var rs);
        recBtn.Text = rec ? "\u25A0 Stop" : "\u25CF Record"; recLbl.Text = rec ? $"\u25CF REC {(int)rs / 60:00}:{(int)rs % 60:00}" : (File.Exists(RecFlag) ? "starting..." : "");
        if (!fresh) EnsureFeed();
      } catch { status.Text = "Waiting for the Kinect feed..."; EnsureFeed(); }
    }
    void Snapshot() {
      if (acc == null) return;
      acc.ReadArray(Header, buf, 0, buf.Length);
      using (var full = new Bitmap(W, H, PixelFormat.Format32bppRgb)) {
        var bd = full.LockBits(new Rectangle(0, 0, W, H), ImageLockMode.WriteOnly, PixelFormat.Format32bppRgb); Marshal.Copy(buf, 0, bd.Scan0, buf.Length); full.UnlockBits(bd);
        var f = Path.Combine(OutDir, $"Kinect-{DateTime.Now:yyyy-MM-dd_HH-mm-ss}.png"); full.Save(f, ImageFormat.Png); status.Text = "Saved " + f; }
    }
  }
}
