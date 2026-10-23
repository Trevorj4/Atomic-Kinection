# Dependencies

Nothing here is pre-built. Download each into one folder and set `AKDEPS` to it, e.g. `set AKDEPS=C:\AKDeps`.

| Dependency | Version | Where | Put it in |
|---|---|---|---|
| libfreenect2 | `master` | https://github.com/OpenKinect/libfreenect2 | `%AKDEPS%\libfreenect2\libfreenect2-master` |
| libusb | 1.0.27 (Windows binaries .7z) | https://github.com/libusb/libusb/releases | `%AKDEPS%\libfreenect2\libusb127` |
| libjpeg-turbo / TurboJPEG | 64-bit VC build | https://libjpeg-turbo.org | `%AKDEPS%\libfreenect2\deps` (`turbojpeg.lib`, `turbojpeg.dll`, headers) |
| OpenCV | 4.12.0 source | https://github.com/opencv/opencv/releases/tag/4.12.0 | `%AKDEPS%\opencv\opencv-4.12.0` |
| Visual Studio | 2026 Community, C++ desktop workload, CMake, Ninja | https://visualstudio.microsoft.com | |

Then:

1. Copy `..\libfreenect2-patches\src\*.cpp` into `libfreenect2-master\src\` (and raise `cmake_minimum_required` to 3.5 in its `CMakeLists.txt` if your CMake refuses older versions).
2. Run `build-libfreenect2.cmd`.
3. Run `build-opencv.cmd`.
4. Run `..\build\build.cmd`.

If the CMake variable names for libusb/TurboJPEG differ in your libfreenect2 checkout, see its `cmake_modules` folder.
