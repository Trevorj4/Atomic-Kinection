#include "frame_orient.h"

#include <cstdint>
#include <cstdio>

static int failures = 0;

static void Expect(bool ok, const char* message) {
  if (ok) return;
  std::fprintf(stderr, "FAIL %s\n", message);
  ++failures;
}

int main() {
  static_assert(!kMirrorFeed, "1.3.6 keeps sensor-left on the left");

  const std::uint8_t src[8] = {1, 2, 3, 9, 5, 6, 7, 8};
  std::uint8_t dst[8] = {};
  CopyBgraRow(src, dst, 2, false, nullptr);
  Expect(dst[0] == 1 && dst[1] == 2 && dst[2] == 3 && dst[3] == 255, "unmirrored row keeps column 0");
  Expect(dst[4] == 5 && dst[5] == 6 && dst[6] == 7 && dst[7] == 255, "unmirrored row keeps column 1");

  std::uint8_t lut[256];
  for (int i = 0; i < 256; ++i) lut[i] = static_cast<std::uint8_t>(255 - i);
  CopyBgraRow(src, dst, 2, true, lut);
  Expect(dst[0] == lut[5] && dst[1] == lut[6] && dst[2] == lut[7] && dst[3] == 255, "mirrored row takes the last column");
  Expect(dst[4] == lut[1] && dst[5] == lut[2] && dst[6] == lut[3] && dst[7] == 255, "mirrored row takes column 0 second");

  Expect(OrientedColumn(0, 4, false) == 0, "column 0 stays 0");
  Expect(OrientedColumn(1, 4, false) == 1, "column 1 stays 1");
  Expect(OrientedColumn(0, 4, true) == 3, "mirrored column 0 is the last");
  Expect(OrientedColumn(3, 4, true) == 0, "mirrored last column is 0");

  if (failures) {
    std::fprintf(stderr, "%d orientation check(s) failed\n", failures);
    return 1;
  }
  std::puts("frame orientation ok");
  return 0;
}
