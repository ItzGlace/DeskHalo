
$ErrorActionPreference='Stop'
$source=Split-Path $PSScriptRoot
$outputs=Split-Path $source
$log=Join-Path $outputs 'DeskHalo-v0.5-update.log'
Start-Transcript -Path $log -Force
try {
    Write-Host 'DeskHalo v0.5 - keyboard frame and custom model update'
    $android=Join-Path $source 'android'
    $env:JAVA_HOME='C:\Program Files\Android\Android Studio\jbr'
    $env:GRADLE_USER_HOME=Join-Path $env:USERPROFILE '.gradle'
    $env:ANDROID_HOME=Join-Path $env:LOCALAPPDATA 'Android\Sdk'
    $adb=Join-Path $env:LOCALAPPDATA 'Android\Sdk\platform-tools\adb.exe'
    Write-Host 'Building Quest. The first build downloads Assimp 5.4.3 and compiles its OBJ/GLB/FBX importers.'
    & (Join-Path $android 'gradlew.bat') -p $android :app:assembleDebug --console=plain --max-workers=4
    if($LASTEXITCODE -ne 0){throw 'Quest build failed. See DeskHalo-v0.5-update.log.'}
    $apk=Join-Path $android 'app\build\outputs\apk\debug\app-debug.apk'
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $archive=[IO.Compression.ZipFile]::OpenRead($apk)
    try {
        if(-not $archive.GetEntry('lib/arm64-v8a/libdeskhalo.so')){
            throw 'APK is missing libdeskhalo.so; installation stopped to prevent a startup crash.'
        }
    } finally { $archive.Dispose() }
    Copy-Item -LiteralPath $apk -Destination (Join-Path $outputs 'DeskHalo-Quest-v0.5.apk') -Force
    $hostFolder=Join-Path $outputs 'DeskHalo-Windows-v0.4'
    $report=Join-Path $hostFolder 'session.local.json'
    if(-not (Test-Path -LiteralPath $report)){throw 'Start the DeskHalo host first, then rerun this update.'}
    $session=Get-Content -LiteralPath $report -Raw | ConvertFrom-Json
    & $adb install -r $apk
    if($LASTEXITCODE -ne 0){throw 'Quest installation failed. Check the USB cable and headset USB authorization.'}
    & $adb reverse tcp:47654 tcp:47654
    if($LASTEXITCODE -ne 0){throw 'USB reverse connection failed.'}
    & $adb shell am force-stop itz.glace.deskhalo
    & $adb shell am start -n itz.glace.deskhalo/.MainActivity --ez testConnect true --es host 127.0.0.1 --es code $session.code
    if($LASTEXITCODE -ne 0){throw 'Quest launch failed.'}
    Start-Sleep -Seconds 6
    $appPid=(& $adb shell pidof itz.glace.deskhalo).Trim()
    if(-not $appPid){throw 'Quest app stopped. Keep the headset awake; send the log.'}
    & $adb shell logcat --pid=$appPid -d -s DeskHalo:V AndroidRuntime:E | Out-File (Join-Path $outputs 'DeskHalo-v0.5-quest.log') -Encoding utf8
    Write-Host 'DeskHalo v0.5 installed. Keyboard > Measure with fingers. Appearance > Import model.'
    Write-Host 'Virtual displays were not added. The existing Windows host is still running.'
} catch {
    Write-Host ('UPDATE FAILED: '+$_.Exception.Message) -ForegroundColor Red
    throw
} finally {
    Stop-Transcript
}
