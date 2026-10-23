# Atomic Kinection 1.3.2 - full uninstall (run via Uninstall-AtomicKinection.cmd; needs admin)
& (Join-Path $PSScriptRoot 'camera\Uninstall-Camera.ps1')
foreach ($dir in @(
    [Environment]::GetFolderPath('CommonPrograms'),
    [Environment]::GetFolderPath('CommonDesktopDirectory'))) {
  if ([string]::IsNullOrWhiteSpace($dir)) { continue }
  Remove-Item -LiteralPath (Join-Path $dir 'Atomic Kinection.lnk') -Force -ErrorAction SilentlyContinue
}
Write-Host 'Start menu and desktop shortcuts removed.'
Remove-Item "$env:ProgramData\AtomicKinection" -Recurse -Force -ErrorAction SilentlyContinue
Get-ChildItem 'C:\Users\*\AppData\Local\AtomicKinection' -Directory -ErrorAction SilentlyContinue | Remove-Item -Recurse -Force -ErrorAction SilentlyContinue
# Put the Kinect's original Windows driver back: remove the libusbK packages Zadig made for the Kinect
$blocks = (pnputil /enum-drivers | Out-String) -split "`r?`n`r?`n"
foreach ($b in $blocks) {
  if ($b -match 'Original Name:\s+(xbox_nui_sensor\S*\.inf)' -and $b -match 'Provider Name:\s+libusbK' -and $b -match 'Published Name:\s+(oem\d+\.inf)') {
    $oem = ([regex]::Match($b, 'Published Name:\s+(oem\d+\.inf)')).Groups[1].Value
    Write-Host "Removing Kinect libusbK driver $oem"; pnputil /delete-driver $oem /uninstall /force | Out-Null }
}
pnputil /scan-devices | Out-Null
Write-Host 'Kinect driver restored to Windows default (Microsoft Kinect apps need the Kinect runtime installed).'
$a = Read-Host 'Also remove VB-CABLE (virtual microphone)? Other apps may use it. [y/N]'
if ($a -match '^[yY]') { $s = Join-Path $PSScriptRoot 'vbcable\VBCABLE_Setup_x64.exe'; if (Test-Path $s) { Start-Process $s -ArgumentList '-u','-h' -Wait; Write-Host 'VB-CABLE removed (restart Windows to finish).' } }
Write-Host 'Atomic Kinection removed. Your saved files in Documents\AtomicKinection were kept.' -ForegroundColor Green
Read-Host 'Press Enter to close'
