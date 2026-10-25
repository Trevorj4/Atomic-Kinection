ATOMIC KINECTION v1.3.6 - Kinect v2 (Xbox One / Kinect for Windows v2) webcam + microphone
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
- akfeed.exe runs while the Atomic Kinection window is open and releases the Kinect when the window closes.

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
5. Start menu and desktop shortcuts named "Atomic Kinection" open the app.

UNINSTALL
Run Uninstall-AtomicKinection.cmd: removes the app, webcam, and Start menu and desktop
shortcuts, puts the Kinect's original Windows driver back, and offers to remove VB-CABLE.

NOT IN THIS BUILD: 3D scanner and body tracking.
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


NEW IN 1.3.6
- Closing the window releases the Kinect and akfeed exits cleanly (no 0xC0000409 shutdown crash).
- Colour, night vision and depth are the same true orientation (text reads correctly in the app and in recordings).


NEW IN 1.3.1 / 1.3.2
- Auto Framing sits a little higher (more headroom). Adjust with FramingOffsetY= in settings.ini (default 0.12).
- Night vision and Depth are decoded on the graphics card (OpenCL) at 30 fps; falls back to the CPU (about 15 fps)
  on PCs without OpenCL. With OpenCL, switching views is instant (no Kinect restart).
