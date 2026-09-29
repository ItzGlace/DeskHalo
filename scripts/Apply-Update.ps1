param([Parameter(Mandatory=$true)][string]$ReleaseRoot)
$ErrorActionPreference='Stop'
$outputs=(Resolve-Path -LiteralPath $ReleaseRoot).Path
$folder=Join-Path $outputs 'DeskHalo-Windows-v0.6.1-release'
$exe=Join-Path $folder 'DeskHalo Host.exe'
$apk=Join-Path $outputs 'DeskHalo-Quest-v0.6.1-preview.apk'
$adb=Join-Path $env:LOCALAPPDATA 'Android\Sdk\platform-tools\adb.exe'
$report=Join-Path $folder 'session.local.json'
function Invoke-Adb([string[]]$Arguments,[int]$Timeout=15){
    $info=New-Object System.Diagnostics.ProcessStartInfo
    $info.FileName=$adb
    $info.Arguments='-P 5037 '+(($Arguments | ForEach-Object {'"'+$_+'"'}) -join ' ')
    $info.UseShellExecute=$false; $info.CreateNoWindow=$true
    $info.RedirectStandardOutput=$true; $info.RedirectStandardError=$true
    $info.EnvironmentVariables['ANDROID_USER_HOME']=Join-Path $env:USERPROFILE '.android'
    $process=New-Object System.Diagnostics.Process
    $process.StartInfo=$info
    try {
        $null=$process.Start()
        $stdout=$process.StandardOutput.ReadToEndAsync();$stderr=$process.StandardError.ReadToEndAsync()
        if(-not $process.WaitForExit($Timeout*1000)){$process.Kill();throw "ADB command timed out after $Timeout seconds."}
        $text=$stdout.GetAwaiter().GetResult()+$stderr.GetAwaiter().GetResult()
        if($process.ExitCode -ne 0){throw "ADB failed (exit $($process.ExitCode)): $text"}
        return $text
    } finally {$process.Dispose()}
}
function Test-AdbServer {
    $client=New-Object System.Net.Sockets.TcpClient
    try {
        $pending=$client.BeginConnect('127.0.0.1',5037,$null,$null)
        if(-not $pending.AsyncWaitHandle.WaitOne(1500)){return $false}
        $client.EndConnect($pending);$stream=$client.GetStream();$stream.ReadTimeout=2000;$stream.WriteTimeout=2000
        $packet=[Text.Encoding]::ASCII.GetBytes('000chost:version');$stream.Write($packet,0,$packet.Length)
        $reply=New-Object byte[] 4;$offset=0
        while($offset -lt 4){$count=$stream.Read($reply,$offset,4-$offset);if($count -eq 0){return $false};$offset+=$count}
        return [Text.Encoding]::ASCII.GetString($reply) -eq 'OKAY'
    } catch {return $false} finally {$client.Dispose()}
}
Start-Transcript -Path (Join-Path $outputs 'DeskHalo-v0.6-update.log') -Force
try {
    if(-not (Test-Path -LiteralPath $exe)){throw 'The Windows release build is missing.'}
    if(-not (Test-Path -LiteralPath $apk)){throw 'The Quest APK is missing.'}
    if(-not (Test-Path -LiteralPath $adb)){throw 'Android SDK platform-tools/adb.exe was not found.'}
    # Reuse Android Studio's server. Killing it causes competing restarts and USB disconnects.
    for($attempt=0;$attempt -lt 3 -and -not (Test-AdbServer);$attempt++){
        try {Invoke-Adb -Arguments @('start-server') | Write-Host} catch {Write-Host $_.Exception.Message}
        Start-Sleep -Seconds 1
    }
    if(-not (Test-AdbServer)){throw 'ADB server on port 5037 is not responding. See the ADB error above; installation has not started.'}
    $serial=$null;$lastDevices=''
    for($attempt=0;$attempt -lt 15;$attempt++){
        try {$lastDevices=Invoke-Adb -Arguments @('devices','-l')} catch {$lastDevices=$_.Exception.Message;Start-Sleep -Seconds 2;continue}
        $devices=@($lastDevices -split "`r?`n" | Where-Object {$_ -match '^\S+\s+device\s' -and $_ -match 'model:Quest'})
        if($devices.Count -gt 1){throw 'Multiple Quest headsets are connected. Disconnect the extra headset and retry.'}
        if($devices.Count -eq 1){$serial=($devices[0] -split '\s+')[0];break}
        if($lastDevices -match '\sunauthorized\b'){throw 'Quest is connected but USB debugging is unauthorized. Accept the USB debugging prompt inside the headset.'}
        Start-Sleep -Seconds 2
    }
    if(-not $serial){throw "No ready Quest was found. ADB reported: $lastDevices"}
    Write-Host "Quest connected: $serial. Installing 0.6.1 (automatic framing + 45-degree controllers)."
    $installed=$false
    for($attempt=0;$attempt -lt 3;$attempt++){
        try {
            $result=Invoke-Adb -Arguments @('-s',$serial,'install','--no-streaming','-r',$apk) -Timeout 120
            if($result -notmatch '\bSuccess\b'){throw $result}
            $installed=$true;break
        } catch {if($attempt -eq 2){throw};Write-Host "ADB connection interrupted; retrying installation. $($_.Exception.Message)";Start-Sleep -Seconds 2}
    }
    $package=Invoke-Adb -Arguments @('-s',$serial,'shell','dumpsys','package','itz.glace.deskhalo')
    if($package -notmatch 'versionCode=7\b' -or $package -notmatch 'versionName=0\.6\.1\b'){throw 'Installation did not produce the expected Quest build 0.6.1. The old app has not been launched.'}
    Write-Host 'Verified installed Quest version: 0.6.1 (build 7)' -ForegroundColor Green
    Invoke-Adb -Arguments @('-s',$serial,'reverse','tcp:47654','tcp:47654') | Out-Null
    foreach($app in Get-Process -Name 'DeskHalo Host' -ErrorAction SilentlyContinue){
        if($app.Path -and $app.Path.StartsWith($outputs,[StringComparison]::OrdinalIgnoreCase)){
            $null=$app.CloseMainWindow()
            if(-not $app.WaitForExit(4000)){Stop-Process -Id $app.Id}
        }
    }
    $started=Get-Date
    Start-Process -FilePath $exe -ArgumentList ('--test-session "'+$report+'"')
    $session=$null
    for($i=0;$i -lt 40;$i++){
        Start-Sleep -Milliseconds 250
        if((Test-Path -LiteralPath $report) -and (Get-Item -LiteralPath $report).LastWriteTime -ge $started){
            $session=Get-Content -LiteralPath $report -Raw | ConvertFrom-Json
            if($session.code -match '^\d{6}$'){break}
        }
    }
    if(-not $session -or $session.code -notmatch '^\d{6}$'){throw 'Host did not start sharing. Check its window.'}
    Invoke-Adb -Arguments @('-s',$serial,'shell','am','force-stop','itz.glace.deskhalo') | Out-Null
    Invoke-Adb -Arguments @('-s',$serial,'shell','am','start','-n','itz.glace.deskhalo/.MainActivity','--ez','testConnect','true','--es','host','127.0.0.1','--es','code',$session.code) | Out-Null
    Write-Host 'DeskHalo Quest 0.6.1 installed, verified and paired. Hold the two-hand framing gesture steady for 1.2 seconds to align the visible keyboard.' -ForegroundColor Green
} finally {Stop-Transcript}
