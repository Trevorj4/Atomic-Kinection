#!/usr/bin/env python3
"""Host checks for the 1.3.6 source. No Kinect and no Visual Studio."""

import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
failures = []


def read(rel):
    raw = (ROOT / rel).read_bytes()
    if raw.startswith(b"\xff\xfe") or raw.startswith(b"\xfe\xff"):
        return raw.decode("utf-16")
    return raw.decode("utf-8")


def need(cond, message):
    if not cond:
        failures.append(message)


def main():
    feed = read("src/feed/akfeed.cpp")
    orient = read("src/feed/frame_orient.h")
    rc = read("src/feed/akfeed.rc")
    version = read("installer/VERSION.txt").strip()

    need(version == "1.3.6", "installer/VERSION.txt is %r" % version)
    need("constexpr bool kMirrorFeed = false;" in orient, "kMirrorFeed is not false")
    need("W - 1 - x" not in feed, "colour still indexes W - 1 - x")
    need(feed.count("libusb_exit(") == 1, "libusb_exit( calls: %d" % feed.count("libusb_exit("))
    need("libusb_init(&uc)" not in feed, "mic still opens its own libusb context")
    need("libusb_init(&feedCtx)" in feed, "missing the process libusb context")
    need("Freenect2 fn(feedCtx)" in feed, "Freenect2 is not given the shared context")
    need("libusb_handle_events(" not in feed, "a second libusb event loop is still present")
    need(feed.count("cv::flip(") == 1, "cv::flip( calls: %d" % feed.count("cv::flip("))
    flip_at = feed.find("cv::flip(")
    window = feed[max(0, flip_at - 180):flip_at]
    need("kMirrorFeed" in window, "cv::flip is not guarded by kMirrorFeed")
    need('Log("akfeed 1.3.6 started' in feed, "start log is not 1.3.6")

    cancel = feed.rfind("libusb_cancel_transfer")
    close_mic = feed.rfind("libusb_close(ah)", 0, feed.rfind("delete dev"))
    stop = feed.rfind("dev->stop()", 0, feed.rfind("delete dev"))
    close_dev = feed.rfind("dev->close()", 0, feed.rfind("delete dev"))
    release = feed.rfind("release listener frames", 0, feed.rfind("delete dev"))
    delete = feed.rfind("delete dev")
    stop_loop = feed.rfind("does not call libusb_exit", 0, feed.find("libusb_exit("))
    exit_at = feed.find("libusb_exit(")
    order = [cancel, close_mic, stop, close_dev, release, delete, stop_loop, exit_at]
    need(all(i >= 0 for i in order) and order == sorted(order), "shutdown calls are out of order: %s" % order)

    need("FILEVERSION 1,3,6,0" in rc and "PRODUCTVERSION 1,3,6,0" in rc, "akfeed.rc numeric version")
    need('VALUE "FileVersion", "1.3.6.0"' in rc, "akfeed.rc FileVersion string")
    need('VALUE "ProductVersion", "1.3.6"' in rc, "akfeed.rc ProductVersion string")
    need("1,3,4" not in rc and "1.3.5" not in rc and "1.3.4" not in rc, "akfeed.rc still mentions an older version")

    assembly = read("src/app/AssemblyInfo.cs")
    need(assembly.count("1.3.6.0") == 2, "AssemblyVersion/FileVersion")
    need('AssemblyInformationalVersion("1.3.6")' in assembly, "InformationalVersion")
    need("1.3.5" not in assembly and "1.3.4" not in assembly, "AssemblyInfo still has an older version")

    form = read("src/app/MainForm.cs")
    need('Text = "Atomic Kinection 1.3.6"' in form, "window title")

    for rel in (
        "src/camera/AtomicKinectionCamSetup/CamSetup.rc",
        "src/camera/AtomicKinectionCamSource/VCamSampleSource.rc",
    ):
        text = read(rel)
        need("FILEVERSION 1,3,6,0" in text and "PRODUCTVERSION 1,3,6,0" in text, rel + " numeric version")
        need('VALUE "FileVersion", "1.3.6.0"' in text, rel + " FileVersion")
        need('VALUE "ProductVersion", "1.3.6"' in text, rel + " ProductVersion")
        need("1,1,0,0" not in text and "1.1.0" not in text, rel + " still says 1.1.0")

    notices = read("THIRD-PARTY-NOTICES.txt").splitlines()[0]
    need(notices == "ATOMIC KINECTION 1.3.6 - THIRD-PARTY NOTICES", "third-party notices header")

    for rel, needle in (
        ("installer/Install-AtomicKinection.cmd", "Atomic Kinection 1.3.6"),
        ("installer/Install-AtomicKinection.ps1", "Atomic Kinection 1.3.6 installer"),
        ("installer/Install-AtomicKinection.ps1", "Atomic Kinection 1.3.6 installed."),
        ("installer/Uninstall-AtomicKinection.ps1", "Atomic Kinection 1.3.6"),
        ("installer/README.md", "Installer version **1.3.6**"),
        ("installer/README.txt", "ATOMIC KINECTION v1.3.6"),
        ("installer/camera/Install-Camera.ps1", "Atomic Kinection 1.3.6"),
    ):
        need(needle in read(rel), "%s missing %r" % (rel, needle))

    for rel in (
        "libfreenect2-patches/src/opencl_depth_packet_processor.cpp",
        "libfreenect2-patches/src/opencl_kde_depth_packet_processor.cpp",
    ):
        text = read(rel)
        need("catch (...)" in text, rel + " does not catch a destructor throw")

    # Current-version fields must not still say 1.3.4 or 1.3.5. Changelog text may.
    stale = []
    for rel in (
        "src/feed/akfeed.rc",
        "src/app/AssemblyInfo.cs",
        "installer/VERSION.txt",
        "src/camera/AtomicKinectionCamSetup/CamSetup.rc",
        "src/camera/AtomicKinectionCamSource/VCamSampleSource.rc",
    ):
        text = read(rel)
        if "1.3.4" in text or "1.3.5" in text or "1,3,4" in text or "1,3,5" in text:
            stale.append(rel)
    need(not stale, "stale version in " + ", ".join(stale))

    if failures:
        for item in failures:
            print("FAIL", item, file=sys.stderr)
        print("%d release check(s) failed" % len(failures), file=sys.stderr)
        return 1
    print("release source ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
