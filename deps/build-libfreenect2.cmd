@echo off
rem Builds libfreenect2 master as a static library (/MT) with our patches applied.
rem Layout expected under %AKDEPS%\libfreenect2:
rem   libfreenect2-master\   upstream source, with ..\..\libfreenect2-patches\src\*.cpp copied into its src\
rem   libusb127\             libusb 1.0.27 Windows release (include\, VS2022\MS64\static\libusb-1.0.lib)
rem   deps\                  turbojpeg.lib, turbojpeg.dll and TurboJPEG headers
rem   oclsdk\OpenCL-SDK-v2026.05.29-Win-x64\  Khronos OpenCL SDK (github.com/KhronosGroup/OpenCL-SDK releases)
if "%AKDEPS%"=="" (echo Set AKDEPS first & exit /b 1)
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
set L=%AKDEPS%\libfreenect2
cmake -S "%L%\libfreenect2-master" -B "%L%\build-ocl" -DBUILD_SHARED_LIBS=OFF -DCMAKE_POLICY_DEFAULT_CMP0091=NEW -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded -DENABLE_OPENGL=OFF -DENABLE_OPENCL=ON -DOpenCL_INCLUDE_DIR="%L%\oclsdk\OpenCL-SDK-v2026.05.29-Win-x64\include" -DOpenCL_LIBRARY="%L%\oclsdk\OpenCL-SDK-v2026.05.29-Win-x64\lib\OpenCL.lib" -DENABLE_CUDA=OFF -DBUILD_EXAMPLES=OFF -DLibUSB_INCLUDE_DIRS="%L%\libusb127\include" -DLibUSB_LIBRARIES="%L%\libusb127\VS2022\MS64\static\libusb-1.0.lib" -DTurboJPEG_INCLUDE_DIRS="%L%\deps" -DTurboJPEG_LIBRARIES="%L%\deps\turbojpeg.lib" || exit /b 1
cmake --build "%L%\build-ocl" --config Release || exit /b 1
