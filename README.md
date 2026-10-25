# Atomic Kinection

Turn a **Kinect v2** (Xbox One Kinect / Kinect for Windows v2) into a normal Windows webcam **and** microphone, with no Microsoft Kinect SDK, runtime or driver.

Atomic Kinection is freeware for Windows 10/11 (64-bit). The Kinect is driven by the open-source [libfreenect2](https://github.com/OpenKinect/libfreenect2) library over the free libusbK USB driver. Apps such as WhatsApp, Teams and Zoom see a webcam called **Atomic Kinection** and a microphone called **CABLE Output**.

> Screenshots: none yet.

---

## Features

- **"Atomic Kinection" virtual webcam**: the Kinect colour camera at 1920×1080, 30 fps.
- **Kinect microphone**: the Kinect's mic array is read directly over USB and played into VB-CABLE, so apps can pick it as **CABLE Output (VB-Audio Virtual Cable)**. There's a mic volume (gain) slider.
- **View modes**: Normal (colour), Night vision (infrared) and Depth (colour-coded distance). The chosen mode goes to both the preview and the webcam, and is remembered.
- **Auto Framing**: follows your face (YuNet face detection via OpenCV) and keeps you framed.
- **Zoom / Wide** button: the label shows what it will switch to. Zoomed also tightens the Auto Framing crop.
- **Record / Stop**: records exactly what the webcam shows (1920×1080, 30 fps) with the Kinect mic, as MP4 (H.264 + AAC), with a red REC timer.
- **Snapshots** to `Documents\AtomicKinection`.
- **Auto-reconnect**: if the Kinect drops out, the webcam shows the last picture with a red bar and restarts by itself when the Kinect comes back. Corrupt colour frames are skipped quietly, and the stream restarts itself if they pile up.
- The Kinect feed (`akfeed.exe`) runs while the Atomic Kinection window is open. Closing the window stops the streams and lets go of the Kinect, so other apps can use it.

## Requirements

- Windows 10 or 11, 64-bit.
- .NET Framework 4.8 (built into Windows 10 1903+ and Windows 11).
- A **USB 3.0** port, ideally plugged straight into the PC (no hub).
- A Kinect v2 sensor **plus the Kinect Adapter for Windows** (the box with the power brick).
- Admin rights for the install.

## Kinect v2 hardware and power

- The Kinect v2 needs the **Kinect Adapter for Windows**: it joins the Kinect's proprietary plug to a USB 3.0 cable and a separate mains power brick. The Kinect cannot run on USB power alone.
- The adapter's light should be on and the Kinect's white light comes on while it's streaming.
- **Power-cycling**: if the Kinect gets stuck (see Troubleshooting), unplug the **power brick** (not just the USB) for about 10 seconds, then plug it back in.
- The Kinect v2 has **no tilt motor** (unlike the original Kinect). Tilt it by hand. Auto Framing gives a digital pan/zoom instead.

## Driver setup (Zadig / libusbK)

Atomic Kinection needs the Kinect to use the free **libusbK** driver instead of Microsoft's Kinect driver. The installer does this with **Zadig** (bundled):

1. In Zadig, open **Options** and tick **List All Devices**.
2. Pick **Xbox NUI Sensor (Composite Parent)** (USB ID `045E 02D8`).
3. On the right choose **libusbK**, click **Replace Driver**, wait for "successful", then close Zadig.

While libusbK is in place, Microsoft Kinect apps (Kinect SDK 2.0, Kinect Studio and so on) can't use the Kinect.

### Undoing it (restoring the Microsoft drivers)

- **Easiest**: run `Uninstall-AtomicKinection.cmd`. It removes the libusbK driver packages Zadig made for the Kinect (`pnputil /delete-driver ... /uninstall`) and rescans devices, so Windows goes back to its default driver.
- **By hand**: open Device Manager, find the Kinect under *libusbK USB Devices*, right-click **Uninstall device**, tick **Attempt to remove the driver for this device**, then unplug and replug the Kinect. If Microsoft Kinect apps still can't see it, reinstall the Kinect for Windows Runtime/SDK 2.0 from Microsoft.

## Install

1. Plug in the Kinect (USB 3 + power).
2. Run `Install-AtomicKinection.cmd` and click **Yes** at the admin prompt.
3. Follow the Zadig steps above when Zadig opens.
4. VB-CABLE is installed automatically if it's missing. A Windows restart is recommended afterwards.

The installer puts the app in `C:\Program Files\AtomicKinection`, registers the virtual webcam, and adds `akfeed.exe` to start at sign-in.

> Note: the installer, Zadig and the VB-CABLE installer are **not stored in this repository** (see *What's not in this repo*). They're included in the release zip.

After installing VB-CABLE, check in Windows **Sound settings** that your default speakers and microphone haven't switched over to "CABLE".

## Uninstall

Run `Uninstall-AtomicKinection.cmd`. It removes the app and the webcam, puts the Kinect's original Windows driver back, and asks whether to remove VB-CABLE too (other apps may use it). Your files in `Documents\AtomicKinection` are kept.

## Using it in WhatsApp, Teams, Zoom and others

- **Camera**: choose **Atomic Kinection**.
- **Microphone**: choose **CABLE Output (VB-Audio Virtual Cable)**.
- Do a test call and adjust the mic slider in the Atomic Kinection window if your voice is too quiet or too loud.

## Modes and buttons

| Button | What it does |
|---|---|
| **Normal** | Colour camera, 1920×1080 at 30 fps. |
| **Night vision** | Infrared picture, works in the dark (512×424 sensor, scaled up). |
| **Depth** | Colour-coded distance view (near = warm colours). |
| **Auto Framing** | On/off. Follows your face and keeps your head and shoulders framed. |
| **Zoom / Wide** | Shows "Zoom" while wide and "Wide" while zoomed. |
| **Record / Stop** | Records the webcam picture plus Kinect mic to MP4. |
| **Snapshot (PNG)** | Saves a still to `Documents\AtomicKinection`. |
| **Open recordings folder** | Opens `Videos\Atomic Kinection`. |
| Mic slider | Kinect mic gain (0–40 dB, default 18). |

Whatever mode you pick is what the webcam sends to other apps and what gets recorded.

## settings.ini

`Documents\AtomicKinection\settings.ini` is read continually, so changes apply straight away. The buttons write most of these keys for you.

| Key | Values | Meaning |
|---|---|---|
| `Mode=` | `Normal` / `Night` / `Depth` | View mode. |
| `AutoFraming=` | `1` / `0` | Auto Framing on/off. |
| `Zoom=` | `Zoomed` / `Wide` | Zoom state. |
| `MicGainDb=` | `0`–`40` (default `18`) | Kinect mic gain in dB. |
| `Brightness=` | `0.5`–`4` (default `1.0`) | Digital brightness lift (gamma-style). `1.3`–`2.0` suits dim rooms. |
| `ExposureMode=` | `semi` / `auto` / `manual` (default `auto`) | Colour camera exposure mode. |
| `ExposureMs=` | `1`–`66` (default `30`) | Max exposure time (semi) or fixed time (manual), in ms. |
| `AnalogGain=` | `1`–`4` (default `2`) | Sensor gain in manual mode. |
| `ExposureComp=` | `-2`–`2` (default `0`) | Exposure compensation in auto mode. |

Note: exposure times above about 33 ms can drop the camera below 30 fps.

## Where files go

- **Recordings**: `Videos\Atomic Kinection\Atomic Kinection YYYY-MM-DD HH-MM-SS.mp4`
- **Snapshots and settings**: `Documents\AtomicKinection\`
- **Log**: `%LOCALAPPDATA%\AtomicKinection\akfeed.log`

## Troubleshooting

**Camera shows 0 frames / black / stuck after a crash or driver swap**
Unplug the Kinect's **power brick** for about 10 seconds and plug it back in. The Kinect v2 can lock up and only a power cycle clears it.

**"Failed to decompress rgb image" / "Not a JPEG file" errors**
Usually seen after the PC wakes from sleep. 1.2.1 and later skip these frames and restart the stream by itself. If it keeps happening, restart `akfeed.exe` (sign out and in), power-cycle the Kinect, or try a different USB 3 port with no hub.

**USB controller (ASMedia note)**
The Kinect v2 is fussy about USB 3 controllers. Some ASMedia (and some older Renesas/Etron) controllers give dropped or corrupt frames or don't work at all. Use a port on the motherboard's own Intel or AMD USB 3 controller, plugged in directly. Device Manager > View > Devices by connection shows which controller a port belongs to.

**Picture too dark or too bright**
Since 1.3.5 the colour camera uses full auto exposure (`ExposureMode=auto`) by default, with brightness 1.0. If the picture is still too dark or too bright, drag the **Brightness** slider (0.5x–2.0x, beside the Record button) or set `Brightness=` in `settings.ini`; the change shows up straight away, no restart needed. `ExposureComp=` (`-2`–`2`) also nudges auto exposure. To go back to the old capped mode, set `ExposureMode=semi` and keep `ExposureMs=` at 33 or below to hold 30 fps.

**Picture looks mirrored**
The webcam sends a non-mirrored picture. Many apps (the Windows Camera app, Teams, Zoom, WhatsApp) mirror **your own self-view** on your screen, but the people on the call see it the right way round. Check with text on a shirt or a sign, or look at a recording.

**No mic sound in the call**
Pick **CABLE Output** as the microphone in the app, raise the mic slider, and check Windows hasn't made "CABLE Input" your default speakers.

**Night vision / depth slower than colour**
Since 1.3.2 night vision and depth are decoded on the graphics card (OpenCL) at 30 fps, with an automatic CPU fallback (about 15 fps) on PCs without OpenCL.

## Building from source

Tested with **Visual Studio 2026 Community** (x64 native tools), CMake and Ninja. All third-party sources are downloaded separately; nothing pre-built is stored here.

1. **Get the dependencies** into a folder of your choice (called `%AKDEPS%` below). See `deps/README.md`:
   - libfreenect2 `master` (https://github.com/OpenKinect/libfreenect2)
   - libusb 1.0.27 Windows binaries (VS2022 MS64 static lib)
   - libjpeg-turbo / TurboJPEG (import lib + `turbojpeg.dll`)
   - OpenCV 4.12.0 source
2. **Patch libfreenect2**: copy `libfreenect2-patches/src/*.cpp` over the matching files in libfreenect2's `src` folder (see `libfreenect2-patches/README.md`).
3. **Build libfreenect2 (static)**: `deps\build-libfreenect2.cmd`
4. **Build OpenCV 4.12 (static, minimal)**: `deps\build-opencv.cmd`
5. **Build akfeed.exe and AtomicKinection.exe**: `build\build.cmd`
6. **Build the virtual camera**: open `src/camera/AtomicKinectionCamSource/AtomicKinectionCamSource.vcxproj` in Visual Studio (Release x64). CamSetup is a single file: `cl /EHsc /O2 CamSetup.cpp` plus its `.rc`.
7. **Package**: put the outputs together with the files in `installer/`, `models/`, `licenses/` and `THIRD-PARTY-NOTICES.txt` in the release layout described in `installer/README.md`, then add Zadig 2.9 and the VB-CABLE driver pack (downloaded from their official sites).

## Project layout

```
src/app/                  AtomicKinection.exe - C# WinForms preview/control window (.NET 4.8)
src/feed/                 akfeed.exe - C++ background feed: Kinect via libfreenect2, modes,
                          Auto Framing (OpenCV/YuNet), Kinect mic -> VB-CABLE, MP4 recording
src/camera/               Media Foundation virtual camera (CamSource DLL, based on VCamSample)
                          and CamSetup.exe (registers/unregisters the camera)
src/kaudio/               Kinect USB audio prototypes and test tools used during development
libfreenect2-patches/     Our modified libfreenect2 source files
deps/                     Scripts and notes for building OpenCV and libfreenect2
build/                    Build script for akfeed + the app
installer/                Install/uninstall scripts and the release-zip layout
models/                   YuNet face detection model (OpenCV Zoo, MIT)
licenses/                 Third-party licence texts
THIRD-PARTY-NOTICES.txt   Third-party credits and licence notes
```

## What's not in this repo

Built programs and libraries, the OpenCV/libfreenect2/libusb source trees, Zadig, the VB-CABLE installer, driver backups, test captures and logs. They're either built from the scripts here or downloaded from their official sites.

## Licence

Atomic Kinection's own code is released under the [MIT License](LICENSE), Copyright (c) 2026 Trevor Jennings. Third-party parts keep their own licences, listed in `THIRD-PARTY-NOTICES.txt` and `licenses/`.

## Credits

- [libfreenect2](https://github.com/OpenKinect/libfreenect2): OpenKinect contributors (Apache 2.0)
- [libusb](https://libusb.info) (LGPL 2.1) and the [libusbK](https://github.com/mcuee/libusbk) driver
- [libjpeg-turbo](https://libjpeg-turbo.org) / TurboJPEG (BSD-3-Clause, IJG)
- [OpenCV](https://opencv.org) 4.12 (Apache 2.0), with Protocol Buffers (BSD-3) and zlib
- [YuNet](https://github.com/opencv/opencv_zoo) face detection model: Shiqi Yu (MIT)
- [Zadig / libwdi](https://zadig.akeo.ie): Pete Batard / Akeo Consulting (GPL v3 / LGPL v3)
- [VB-CABLE](https://www.vb-cable.com): VB-Audio Software. *VB-CABLE is a donationware, all participations are welcome.*
- VCamSample: basis of the virtual webcam (see `licenses/LICENSE-VCamSample.txt`)

## Changelog

**1.3.5**
- Auto exposure is now the default (`ExposureMode=auto`, `Brightness=1.0`, `ExposureComp=0`), so new installs start with a bright picture. Existing `settings.ini` values are kept.
- New **Brightness** slider beside the Record button (0.5x–2.0x in 0.1 steps, default 1.0x). It writes `Brightness=` to `settings.ini` and the feed picks it up live.
- Installer no longer hangs when upgrading from 1.3.3: it force-stops any running Atomic Kinection / `akfeed.exe` by name first, and only asks the installed feed to `--stop` if it is 1.3.4 or later, with a 5 s timeout.

**1.3.4**
- Closing the window now releases the Kinect: the app tells `akfeed.exe` to stop its streams, close the Kinect and its USB handles, join its threads and exit (it is killed only if it hasn't exited after 8 s).
- `akfeed.exe` also shuts down cleanly on Ctrl+C / console close, sign-out and shutdown (WM_ENDSESSION), `taskkill` without `/f`, and `akfeed.exe --stop`. After a crash it stops the Kinect before exiting.
- The feed no longer starts at sign-in (the installer removes the old sign-in entry), and it is not restarted while the window is closing.
- Fixed a small leak of the mic's USB transfers on every reconnect.

**1.3.2**
- Night vision and Depth decoded on the graphics card (OpenCL, via libfreenect2's OpenCL depth decoder): 30 fps instead of about 15. Falls back to the CPU automatically when OpenCL isn't available.
- With OpenCL, colour, IR and depth stream together, so switching views is instant with no Kinect restart. This also fixes a crash in the 1.3.2 test build when leaving Night/Depth.
- Status file reports `GpuDepth`.

**1.3.1**
- Auto Framing aims a little higher (more headroom); adjustable with `FramingOffsetY=` in `settings.ini` (default 0.12).

**1.3.0**
- View mode buttons: Normal, Night vision (IR), Depth.
- Auto Framing with YuNet face tracking.
- Zoom / Wide toggle, labelled with what it will switch to.
- Record / Stop to MP4 (H.264 + AAC) in `Videos\Atomic Kinection`, with a REC timer.
- Webcam and recordings at 1920×1080, not mirrored, in every mode.
- Removed the old webcam button that only showed instructions.

**1.2.1**
- Hid the console window.
- Corrupt colour (JPEG) frames are skipped quietly, and the stream restarts by itself if they pile up.
- Brightness and exposure settings in `settings.ini`.

**1.2.0**
- Rebuilt on libfreenect2 + libusbK: no Microsoft Kinect SDK, runtime or driver needed.
- Kinect microphone captured directly over USB and passed to apps through VB-CABLE ("CABLE Output").
- Mic gain slider. Auto-reconnect when the Kinect drops out.

**1.1.0 – 1.1.1**
- First versions: "Atomic Kinection" virtual webcam built on the Microsoft Kinect SDK 2.0. These are retired.
