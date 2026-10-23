# libfreenect2 patches

Modified copies of four libfreenect2 (`master`) source files. Copy them over the originals in `libfreenect2-master/src/` before building.

- `src/libfreenect2.cpp`: removes the UsbDk option call (we use libusbK) and adds an optional colour-only mode (`AK_RGB_ONLY` environment variable, off by default).
- `src/usb_control.cpp`: modified alongside the above for Atomic Kinection.
- `src/opencl_depth_packet_processor.cpp`, `src/opencl_kde_depth_packet_processor.cpp`: one-line build fix (`#undef CL_ICDL_VERSION`) because current Khronos OpenCL headers define that name as a macro.

Also raise `cmake_minimum_required` in libfreenect2's top-level `CMakeLists.txt` to 3.5 for current CMake.

libfreenect2 is used under the Apache License 2.0 (it's dual-licensed Apache-2.0 / GPL-2.0). See `../licenses/`.
