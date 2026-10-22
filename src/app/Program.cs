using System;
using System.Diagnostics;
using System.IO;
using System.Windows.Forms;
namespace AtomicKinection {
  static class Program {
    [STAThread] static void Main(string[] args) {
      if (Array.IndexOf(args, "--camfeed") >= 0) { // 1.1.x compatibility: the feed is now akfeed.exe
        var exe = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "akfeed.exe");
        if (File.Exists(exe) && Process.GetProcessesByName("akfeed").Length == 0) Process.Start(new ProcessStartInfo(exe) { UseShellExecute = false, CreateNoWindow = true });
        return;
      }
      Application.EnableVisualStyles(); Application.SetCompatibleTextRenderingDefault(false);
      Application.Run(new MainForm());
    }
  }
}
