@echo off
echo Installing Atomic Kinection 1.3.6 (freeware) - click Yes on the admin prompt.
powershell -NoProfile -ExecutionPolicy Bypass -Command "Start-Process powershell -Verb RunAs -Wait -ArgumentList '-NoProfile -ExecutionPolicy Bypass -File \"%~dp0Install-AtomicKinection.ps1\"'"
if exist "%ProgramFiles%\AtomicKinection\AtomicKinection.exe" start "" "%ProgramFiles%\AtomicKinection\AtomicKinection.exe"
echo Done. Open Atomic Kinection from the Start menu or the desktop shortcut.
echo Camera: "Atomic Kinection". Microphone: "CABLE Output (VB-Audio Virtual Cable)".
pause
