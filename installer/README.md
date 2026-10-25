# Installer scripts and release layout

Installer version **1.3.4** (`VERSION.txt`).

The release zip (`AtomicKinection-vX.Y.Z`) is laid out like this. Items marked *(not in git)* are built or downloaded.

```
AtomicKinection.exe, AtomicKinection.exe.config, akfeed.exe, turbojpeg.dll   (not in git: build outputs)
Install-AtomicKinection.cmd / .ps1, Uninstall-AtomicKinection.cmd / .ps1
AtomicKinection.ico
README.txt, THIRD-PARTY-NOTICES.txt, VERSION.txt
camera\   AtomicKinectionCamSource.dll, AtomicKinectionCamSetup.exe (not in git), Install/Uninstall-Camera.cmd/.ps1
drivers\  zadig-2.9.exe (not in git: https://zadig.akeo.ie)
vbcable\  VB-CABLE driver pack, unmodified (not in git: https://www.vb-cable.com)
models\   face_detection_yunet_2023mar.onnx
licenses\ third-party licence texts
src\      source code
```

Install puts files in `C:\Program Files\AtomicKinection`, registers the webcam DLL with `regsvr32` and `AtomicKinectionCamSetup.exe install`, and (from 1.3.4) removes the old `AtomicKinectionCamFeed` sign-in entry: `akfeed.exe` is started by the app and stops when the app closes. It copies `AtomicKinection.ico` beside the app and creates an **Atomic Kinection** shortcut in the all-users Start menu and on the public desktop. The shortcut launches `AtomicKinection.exe` and uses that icon. Uninstall removes both shortcuts. `build\build.cmd` embeds the same icon in `AtomicKinection.exe`.
