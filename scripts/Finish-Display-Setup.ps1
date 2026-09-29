$ErrorActionPreference = 'Stop'
$path = 'C:\VirtualDisplayDriver\vdd_settings.xml'
Copy-Item -LiteralPath $path -Destination ($path + '.before-8k-' + (Get-Date -Format yyyyMMddHHmmss))
[xml]$config=Get-Content -LiteralPath $path
$config.vdd_settings.options.HardwareCursor='false'
$config.vdd_settings.options.debuglogging='false'
$config.vdd_settings.options.logging='false'
if (-not ($config.vdd_settings.resolutions.resolution | Where-Object { $_.width -eq '7680' })) {
 $mode=$config.CreateElement('resolution')
 foreach($pair in @(@('width','7680'),@('height','4320'),@('refresh_rate','30'))) { $node=$config.CreateElement($pair[0]);$node.InnerText=$pair[1];[void]$mode.AppendChild($node) }
 [void]$config.vdd_settings.resolutions.AppendChild($mode)
}
$config.Save($path)
$devices=@(Get-PnpDevice -Class Display | Where-Object {$_.InstanceId -like 'ROOT\MTTVDD\*'})
if($devices.Count -ne 1){throw 'Expected one MttVDD device.'}
pnputil.exe /restart-device $devices[0].InstanceId
Start-Sleep -Seconds 5
pnputil.exe /enum-devices /instanceid $devices[0].InstanceId
Write-Host '8K modes added; diagnostic logging disabled; hardware cursor remains disabled.'
