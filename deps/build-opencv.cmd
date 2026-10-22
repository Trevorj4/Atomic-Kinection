@echo off
rem Builds a minimal static OpenCV 4.12.0 (/MT) for akfeed.exe.
rem Set AKDEPS to your dependency folder first, with opencv-4.12.0 source extracted in %AKDEPS%\opencv\opencv-4.12.0
if "%AKDEPS%"=="" (echo Set AKDEPS first & exit /b 1)
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
cd /d "%AKDEPS%\opencv"
cmake -S opencv-4.12.0 -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF -DBUILD_WITH_STATIC_CRT=ON -DBUILD_LIST=core,imgproc,dnn,objdetect -DWITH_OPENCL=OFF -DWITH_IPP=OFF -DWITH_ITT=OFF -DWITH_FFMPEG=OFF -DWITH_MSMF=OFF -DWITH_DSHOW=OFF -DWITH_ADE=OFF -DBUILD_TESTS=OFF -DBUILD_PERF_TESTS=OFF -DBUILD_EXAMPLES=OFF -DBUILD_opencv_apps=OFF -DBUILD_JAVA=OFF -DBUILD_opencv_python3=OFF -DWITH_EIGEN=OFF -DWITH_OPENEXR=OFF -DWITH_PROTOBUF=ON -DBUILD_PROTOBUF=ON -DCMAKE_INSTALL_PREFIX="%AKDEPS%\opencv\install" || exit /b 1
cmake --build build --target install || exit /b 1
