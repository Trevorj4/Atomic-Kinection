$dst = "$env:ProgramFiles\AtomicKinection"
Remove-ItemProperty 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Run' -Name 'AtomicKinectionCamFeed' -ErrorAction SilentlyContinue
if (Test-Path "$dst\akfeed.exe") { & "$dst\akfeed.exe" --stop | Out-Null }
Get-Process AtomicKinection,akfeed -ErrorAction SilentlyContinue | Where-Object { $_.Path -like "$dst*" } | Stop-Process -Force
if (Test-Path "$dst\AtomicKinectionCamSetup.exe") { & "$dst\AtomicKinectionCamSetup.exe" uninstall }
if (Test-Path "$dst\AtomicKinectionCamSource.dll") { & regsvr32.exe /s /u "$dst\AtomicKinectionCamSource.dll" }
Stop-Service FrameServer -Force -ErrorAction SilentlyContinue
Start-Sleep 2
Remove-Item $dst -Recurse -Force -ErrorAction SilentlyContinue
Write-Host 'Atomic Kinection camera removed.'
