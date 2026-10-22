@echo off
powershell -NoProfile -ExecutionPolicy Bypass -Command "Start-Process powershell -Verb RunAs -Wait -ArgumentList '-NoProfile -ExecutionPolicy Bypass -File \"%~dp0Install-Camera.ps1\"'"
start "" "%ProgramFiles%\AtomicKinection\AtomicKinection.exe" --camfeed
echo Done. Choose "Atomic Kinection" as the camera in any app.
pause
