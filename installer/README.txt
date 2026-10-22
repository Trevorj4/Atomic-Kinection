ATOMIC KINECTION v1.3.0 - Kinect v2 (Xbox One / Kinect for Windows v2) webcam + microphone
Freeware. No Microsoft Kinect software, runtime or driver is used.
==========================================================================================

WHAT IT DOES
- "Atomic Kinection" webcam: the Kinect colour camera, 1920x1080 at 30 fps, not mirrored.
- Kinect microphone array -> VB-CABLE. In WhatsApp/Teams/Zoom pick the microphone
  "CABLE Output (VB-Audio Virtual Cable)".
- Auto-reconnect: if the Kinect drops out, the camera shows the last picture with a red bar
  and everything restarts by itself when the Kinect is back.
- AtomicKinection.exe: preview, snapshots (Documents\AtomicKinection) and the mic gain slider
  (saved in Documents\AtomicKinection\settings.ini as MicGainDb=).
- akfeed.exe runs in the background from sign-in, so camera + mic work with the window closed.

NEEDS
- Windows 10/11 64-bit, USB 3.0 port, Kinect adapter + power brick.
- .NET Framework 4.8 (built into Windows 10 1903+ and 11).

INSTALL
1. Plug in the Kinect.
2. Run Install-AtomicKinection.cmd and click Yes.
3. When Zadig opens: Options -> List All Devices, pick "Xbox NUI Sensor (Composite Parent)"
   (USB ID 045E 02D8), choose libusbK, click Replace Driver, close Zadig.
   Note: while this driver is in place, Microsoft Kinect apps (Kinect SDK) cannot use the Kinect.
4. VB-CABLE is installed automatically if missing (www.vb-cable.com - donationware).

UNINSTALL
Run Uninstall-AtomicKinection.cmd: removes the app and webcam, puts the Kinect's original
Windows driver back, and offers to remove VB-CABLE.

NOT YET IN 1.3.0 (planned): depth, infrared, 3D scanner, body/face tracking, Auto Framing.
The 1.1.x versions (Microsoft Kinect SDK based) are in the Archive folder.

Licences: see THIRD-PARTY-NOTICES.txt and the licenses folder. Source code is in src\.

NEW IN 1.3.0
- View buttons: Normal colour, Night vision (IR), Depth view (colour-coded distance). The chosen view goes to the
  "Atomic Kinection" webcam and the preview, and is remembered.
- Auto Framing: follows your face (YuNet face detection) and keeps you centred.
- Zoom / Wide button (labelled with what it will do); Zoomed also tightens the Auto Framing crop.
- Record / Stop: saves exactly what the webcam shows, 1920x1080 at 30 fps, with the Kinect microphone, as MP4
  (H.264 + AAC) in Videos\Atomic Kinection, named by date and time. A red REC timer shows while recording.
- Webcam and recordings are 1920x1080 and not mirrored in every view.

