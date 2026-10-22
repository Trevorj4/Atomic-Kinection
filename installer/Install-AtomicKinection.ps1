# Atomic Kinection 1.3.0 - full installer (run via Install-AtomicKinection.cmd; needs admin). Freeware.
# No Microsoft Kinect software is used. The Kinect is driven by libfreenect2 over the libusbK driver.
$ErrorActionPreference = 'Stop'
$here = $PSScriptRoot
Write-Host 'Atomic Kinection 1.3.0 installer' -ForegroundColor Cyan

# 1) App, feed helper and virtual webcam
& (Join-Path $here 'camera\Install-Camera.ps1')

# 2) VB-CABLE (virtual microphone) - VB-Audio donationware, bundled with permission terms (see THIRD-PARTY-NOTICES.txt)
$cable = Get-PnpDevice -PresentOnly -ErrorAction SilentlyContinue | Where-Object { $_.FriendlyName -eq 'VB-Audio Virtual Cable' }
if ($cable) { Write-Host 'VB-CABLE already installed - skipping.' }
else {
  Write-Host ''
  Write-Host 'Installing VB-CABLE (virtual microphone) by VB-Audio - www.vb-cable.com' -ForegroundColor Yellow
  Write-Host 'VB-CABLE is a donationware, all participations are welcome.' -ForegroundColor Yellow
  $p = Start-Process (Join-Path $here 'vbcable\VBCABLE_Setup_x64.exe') -ArgumentList '-i','-h' -Wait -PassThru
  Write-Host "VB-CABLE setup finished (exit $($p.ExitCode)). Windows may ask for a restart later."
}

# 3) Kinect USB driver -> libusbK (whole device). Done with Zadig (libwdi), because it creates a driver signed for this PC.
function Get-KinectService { $d = Get-PnpDevice -PresentOnly -ErrorAction SilentlyContinue | Where-Object { $_.InstanceId -match '^USB\\VID_045E&PID_02D8\\[^&]+$' } | Select-Object -First 1
  if (-not $d) { return $null }; (Get-PnpDeviceProperty -InstanceId $d.InstanceId -KeyName DEVPKEY_Device_Service).Data }
$svc = Get-KinectService
if ($svc -eq 'libusbK') { Write-Host 'Kinect already uses the libusbK driver - skipping.' }
elseif ($null -eq $svc) { Write-Host 'Kinect not found. Plug in the Kinect (USB 3 + power) and run Install-AtomicKinection.cmd again to set its driver.' -ForegroundColor Yellow }
else {
  Write-Host ''
  Write-Host 'Now the Kinect needs the free libusbK driver. Zadig will open:' -ForegroundColor Yellow
  Write-Host '  1. Options menu -> tick "List All Devices"'
  Write-Host '  2. In the list pick "Xbox NUI Sensor (Composite Parent)"   (USB ID 045E 02D8)'
  Write-Host '  3. On the right choose "libusbK", click "Replace Driver", wait for "successful", close Zadig.'
  Write-Host '  (Uninstall-AtomicKinection.cmd puts the original driver back.)'
  Start-Process (Join-Path $here 'drivers\zadig-2.9.exe') -Wait
  Start-Sleep 3; $svc = Get-KinectService
  if ($svc -eq 'libusbK') { Write-Host 'Kinect driver set to libusbK.' -ForegroundColor Green } else { Write-Host "Kinect driver is '$svc', not libusbK - run the installer again and follow the Zadig steps." -ForegroundColor Red }
}
Write-Host ''
Write-Host 'Atomic Kinection 1.3.0 installed.' -ForegroundColor Green
Write-Host 'Camera: pick "Atomic Kinection".  Microphone: pick "CABLE Output (VB-Audio Virtual Cable)".'
