#pragma once
// Order of one akfeed connection teardown. See akfeed.cpp.
//
// Windows libusb 1.0.27's backend exit is process-global: winusb_exit deletes
// the autoclaim CRITICAL_SECTION and unloads libusbK even if another context
// is still alive (libusb core.c libusb_exit calls the backend exit for every
// context). A second libusb_exit then DeleteCriticalSection's that lock again.
// That is a fail-fast, exception 0xC0000409, FAST_FAIL_FATAL_APP_EXIT (parameter
// 7) — the same code abort() uses. It runs from ~Freenect2, which is after
// "Kinect released" and before "joining threads". A session that never opened
// the Kinect never created the second context, so it exited cleanly.
//
// OpenCL IR/depth frames are the other half. ~OpenCLFrame returns its buffer to
// the processor allocator. delete dev destroys that allocator. Frames still
// held by SyncMultiFrameListener must be released first, while the allocator
// (and its command queue) is alive. A throw from that destructor is also
// std::terminate, which is the same fail-fast.

#include <cstddef>

enum class ShutdownStep {
  CancelMic,
  CloseMic,
  StopDevice,
  CloseDevice,
  ReleaseFrames,
  DeleteDevice,
  StopEventLoop,
  LibusbExit
};

// nullptr when the sequence is safe, otherwise a short reason.
inline const char* CheckShutdownOrder(const ShutdownStep* steps, std::size_t n) {
  int exits = 0, releaseAt = -1, deleteAt = -1, closeDevAt = -1, stopLoopAt = -1, exitAt = -1, closeMicAt = -1;
  for (std::size_t i = 0; i < n; ++i) {
    switch (steps[i]) {
    case ShutdownStep::CloseMic: closeMicAt = (int)i; break;
    case ShutdownStep::CloseDevice: closeDevAt = (int)i; break;
    case ShutdownStep::ReleaseFrames: releaseAt = (int)i; break;
    case ShutdownStep::DeleteDevice: deleteAt = (int)i; break;
    case ShutdownStep::StopEventLoop: stopLoopAt = (int)i; break;
    case ShutdownStep::LibusbExit: exitAt = (int)i; ++exits; break;
    default: break;
    }
  }
  if (exits != 1) return "libusb_exit must run exactly once";
  if (releaseAt < 0 || deleteAt < 0 || releaseAt > deleteAt) return "release listener frames before delete dev";
  if (closeDevAt < 0 || closeDevAt > releaseAt) return "close the device before releasing frames";
  if (stopLoopAt < 0 || exitAt < stopLoopAt) return "stop the USB event loop before libusb_exit";
  if (closeMicAt < 0 || closeMicAt > exitAt) return "close the mic before libusb_exit";
  return nullptr;
}
