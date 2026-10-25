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
      // 1.3.4: whatever way the app ends, the feed lets go of the Kinect
      Application.SetUnhandledExceptionMode(UnhandledExceptionMode.CatchException);
      Application.ThreadException += (s, e) => { MainForm.StopFeed(); MessageBox.Show("Atomic Kinection hit an error and will close:\n" + e.Exception.Message, "Atomic Kinection"); Environment.Exit(1); };
      AppDomain.CurrentDomain.UnhandledException += (s, e) => MainForm.StopFeed();
      AppDomain.CurrentDomain.ProcessExit += (s, e) => MainForm.StopFeed();
      Microsoft.Win32.SystemEvents.SessionEnding += (s, e) => MainForm.StopFeed();
      Application.EnableVisualStyles(); Application.SetCompatibleTextRenderingDefault(false);
      Application.Run(new MainForm());
    }
  }
}
