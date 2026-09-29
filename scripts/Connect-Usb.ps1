param([string]$Adb = "$env:LOCALAPPDATA\Android\Sdk\platform-tools\adb.exe")
$ErrorActionPreference = 'Stop'
if (-not (Test-Path -LiteralPath $Adb)) { throw 'Pass -Adb with the full path to adb.exe.' }
& $Adb devices -l
& $Adb reverse tcp:47654 tcp:47654
if ($LASTEXITCODE -ne 0) { throw 'USB tunnel failed. Unlock Quest and approve USB debugging, then retry.' }
Write-Host 'USB ready. Use 127.0.0.1 in DeskHalo on Quest. Start sharing in DeskHalo Host first.'
