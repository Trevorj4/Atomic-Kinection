# Installs Atomic Kinection 1.3.4 files + the "Atomic Kinection" webcam (needs admin)
$ErrorActionPreference = 'Stop'
$src = Split-Path -Parent $PSScriptRoot
$dst = "$env:ProgramFiles\AtomicKinection"
New-Item -ItemType Directory -Force $dst | Out-Null
New-Item -ItemType Directory -Force "$env:ProgramData\AtomicKinection" | Out-Null
& icacls "$env:ProgramData\AtomicKinection" /grant "*S-1-1-0:(OI)(CI)M" | Out-Null
if (Test-Path "$dst\akfeed.exe") { & "$dst\akfeed.exe" --stop | Out-Null } # ask a running feed to release the Kinect first
Get-Process AtomicKinection,akfeed -ErrorAction SilentlyContinue | Where-Object { $_.Path -like "$dst*" } | Stop-Process -Force
Start-Sleep 1
Stop-Service FrameServer -Force -ErrorAction SilentlyContinue
# 1.1.x left Microsoft Kinect SDK DLLs here - 1.3.2 does not use them
Remove-Item "$dst\Microsoft.Kinect.dll","$dst\Microsoft.Kinect.Fusion.dll","$dst\Kinect20.Fusion.dll" -Force -ErrorAction SilentlyContinue
Copy-Item "$src\AtomicKinection.exe","$src\AtomicKinection.exe.config","$src\akfeed.exe","$src\turbojpeg.dll","$src\camera\AtomicKinectionCamSource.dll","$src\camera\AtomicKinectionCamSetup.exe" $dst -Force
New-Item -ItemType Directory -Force "$dst\models" | Out-Null
Copy-Item "$src\models\face_detection_yunet_2023mar.onnx" "$dst\models" -Force
& regsvr32.exe /s "$dst\AtomicKinectionCamSource.dll"
& "$dst\AtomicKinectionCamSetup.exe" install
# 1.3.4: the feed no longer starts at sign-in. It runs only while the Atomic Kinection window is open,
# and lets go of the Kinect when the window closes. Remove the sign-in entry older versions added.
Remove-ItemProperty 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Run' -Name 'AtomicKinectionCamFeed' -ErrorAction SilentlyContinue
Write-Host 'Atomic Kinection files and camera installed.'
