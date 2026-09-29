[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
if (-not ([Security.Principal.WindowsPrincipal]$identity).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) { throw 'Run this repair as administrator.' }
Start-Transcript -Path (Join-Path $PSScriptRoot 'repair.log') -Append
try {
    $path = 'C:\VirtualDisplayDriver\vdd_settings.xml'
    Copy-Item -LiteralPath $path -Destination ($path + '.backup-' + (Get-Date -Format yyyyMMddHHmmss))
    [xml]$config = Get-Content -LiteralPath $path
    $config.vdd_settings.options.HardwareCursor = 'false'
    $config.vdd_settings.options.logging = 'true'
    $config.vdd_settings.options.debuglogging = 'true'
    $config.vdd_settings.options.SDR10bit = 'false'
    $config.vdd_settings.options.HDRPlus = 'false'
    $config.vdd_settings.monitors.count = '1'
    $config.Save($path)
    $devices = @(Get-PnpDevice -Class Display | Where-Object { $_.InstanceId -like 'ROOT\MTTVDD\*' })
    if ($devices.Count -ne 1) { throw 'Expected exactly one DeskHalo MttVDD device; no device was restarted.' }
    & pnputil.exe /restart-device $devices[0].InstanceId
    if ($LASTEXITCODE -ne 0) { throw 'Device restart failed.' }
    Start-Sleep -Seconds 5
    & pnputil.exe /enum-devices /instanceid $devices[0].InstanceId /properties
    Write-Host 'Repair attempted. DeskHalo will verify device status and separate desktop enumeration.'
} finally { Stop-Transcript }
