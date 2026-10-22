@echo off
rem Builds akfeed.exe and AtomicKinection.exe into build\out. Set AKDEPS first (see deps\README.md).
if "%AKDEPS%"=="" (echo Set AKDEPS first & exit /b 1)
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
set ROOT=%~dp0..
set L=%AKDEPS%\libfreenect2
set OCV=%AKDEPS%\opencv\install
set OCL=%AKDEPS%\libfreenect2\oclsdk\OpenCL-SDK-v2026.05.29-Win-x64
if not exist "%~dp0out" mkdir "%~dp0out"
cd /d "%~dp0out"
set OLIBS=
for %%f in ("%OCV%\staticlib\opencv_*.lib" "%OCV%\staticlib\libprotobuf.lib" "%OCV%\staticlib\zlib.lib") do call set OLIBS=%%OLIBS%% "%%f"
rc /nologo /fo akfeed.res "%ROOT%\src\feed\akfeed.rc" || exit /b 1
cl /nologo /EHsc /O2 /MT /std:c++17 /DLIBFREENECT2_STATIC /I"%L%\libfreenect2-master\include" /I"%L%\libfreenect2-master\include\internal" /I"%OCL%\include" /I"%L%\build-ocl" /I"%L%\libusb127\include" /I"%L%\deps" /I"%OCV%\include" "%ROOT%\src\feed\akfeed.cpp" akfeed.res /Fe:akfeed.exe /link /LIBPATH:"%L%\build-ocl\lib\Release" freenect2static.lib "%L%\libusb127\VS2022\MS64\static\libusb-1.0.lib" "%L%\deps\turbojpeg.lib" %OLIBS% ole32.lib shell32.lib advapi32.lib user32.lib mfplat.lib mfreadwrite.lib mfuuid.lib "%OCL%\lib\OpenCL.lib" delayimp.lib /DELAYLOAD:OpenCL.dll /SUBSYSTEM:WINDOWS /ENTRY:wmainCRTStartup || exit /b 1
"C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\Roslyn\csc.exe" /nologo /target:winexe /optimize /unsafe /out:AtomicKinection.exe /r:System.dll,System.Core.dll,System.Drawing.dll,System.Windows.Forms.dll "%ROOT%\src\app\*.cs" || exit /b 1
copy /y "%L%\deps\turbojpeg.dll" . >nul
echo BUILD OK - outputs in %CD%
