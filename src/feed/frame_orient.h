#pragma once
// Left-right orientation shared by colour, IR and depth.
//
// 1.3.0–1.3.5 flipped every stream horizontally on the way out: ComposeColour
// read column W-1-x, and IR and depth used cv::flip(..., 1). That is the only
// left-right mirror in the app, the virtual camera, or the libfreenect2
// patches. Commit 22dec2e removed the Features line "not mirrored" and left
// the flip in place, so 1.3.5 (the installed build) still publishes the
// flipped image. 1.3.6 does not flip. Colour uses CopyBgraRow. IR and depth
// use the same kMirrorFeed switch (FeedFloat in akfeed.cpp), so all three
// stay in one orientation.
//
// libfreenect2 issue 172 says the raw frames already look like a mirror and
// that one cv::flip un-mirrors them — which is what 1.3.0 was trying to do.
// kMirrorFeed is the single switch for a Kinect text-card check: false keeps
// sensor-left on the left; true restores the 1.3.5 flip.

#include <cstddef>
#include <cstdint>

constexpr bool kMirrorFeed = false;

inline std::size_t OrientedColumn(int x, int width, bool mirror) {
  return mirror ? static_cast<std::size_t>(width - 1 - x) : static_cast<std::size_t>(x);
}

// src and dst are tightly packed BGRA/BGRX rows. lut may be null (copy bytes as-is).
inline void CopyBgraRow(const std::uint8_t* src, std::uint8_t* dst, int width, bool mirror, const std::uint8_t* lut) {
  for (int x = 0; x < width; ++x) {
    const std::uint8_t* p = src + OrientedColumn(x, width, mirror) * 4;
    std::uint8_t* q = dst + static_cast<std::size_t>(x) * 4;
    q[0] = lut ? lut[p[0]] : p[0];
    q[1] = lut ? lut[p[1]] : p[1];
    q[2] = lut ? lut[p[2]] : p[2];
    q[3] = 255;
  }
}
