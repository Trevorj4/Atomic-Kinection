#include "shutdown_order.h"

#include <cstdio>
#include <cstring>

static int failures = 0;

static void Expect(const char* got, const char* want, const char* message) {
  const bool same = (got == nullptr && want == nullptr) || (got && want && std::strcmp(got, want) == 0);
  if (same) return;
  std::fprintf(stderr, "FAIL %s (got %s)\n", message, got ? got : "ok");
  ++failures;
}

int main() {
  // The order akfeed.cpp uses on a normal shutdown.
  const ShutdownStep good[] = {
    ShutdownStep::CancelMic,
    ShutdownStep::CloseMic,
    ShutdownStep::StopDevice,
    ShutdownStep::CloseDevice,
    ShutdownStep::ReleaseFrames,
    ShutdownStep::DeleteDevice,
    ShutdownStep::StopEventLoop,
    ShutdownStep::LibusbExit
  };
  Expect(CheckShutdownOrder(good, sizeof good / sizeof good[0]), nullptr, "akfeed shutdown order");

  const ShutdownStep twice[] = { ShutdownStep::LibusbExit, ShutdownStep::CloseMic, ShutdownStep::CloseDevice,
    ShutdownStep::ReleaseFrames, ShutdownStep::DeleteDevice, ShutdownStep::StopEventLoop, ShutdownStep::LibusbExit };
  Expect(CheckShutdownOrder(twice, sizeof twice / sizeof twice[0]), "libusb_exit must run exactly once", "second libusb_exit");

  const ShutdownStep lateFrames[] = { ShutdownStep::CloseMic, ShutdownStep::CloseDevice, ShutdownStep::DeleteDevice,
    ShutdownStep::ReleaseFrames, ShutdownStep::StopEventLoop, ShutdownStep::LibusbExit };
  Expect(CheckShutdownOrder(lateFrames, sizeof lateFrames / sizeof lateFrames[0]), "release listener frames before delete dev", "frames after delete dev");

  const ShutdownStep earlyExit[] = { ShutdownStep::CloseMic, ShutdownStep::LibusbExit, ShutdownStep::CloseDevice,
    ShutdownStep::ReleaseFrames, ShutdownStep::DeleteDevice, ShutdownStep::StopEventLoop };
  Expect(CheckShutdownOrder(earlyExit, sizeof earlyExit / sizeof earlyExit[0]), "stop the USB event loop before libusb_exit", "exit before the event thread stops");

  const ShutdownStep noMic[] = { ShutdownStep::CloseDevice, ShutdownStep::ReleaseFrames, ShutdownStep::DeleteDevice,
    ShutdownStep::StopEventLoop, ShutdownStep::LibusbExit };
  Expect(CheckShutdownOrder(noMic, sizeof noMic / sizeof noMic[0]), "close the mic before libusb_exit", "mic left open");

  if (failures) {
    std::fprintf(stderr, "%d shutdown-order check(s) failed\n", failures);
    return 1;
  }
  std::puts("shutdown order ok");
  return 0;
}
