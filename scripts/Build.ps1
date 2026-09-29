param([string]$JavaHome = "$env:ProgramFiles\Android\Android Studio\jbr")
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
dotnet build (Join-Path $repo 'windows\DeskHalo Host.csproj') -p:Platform=x64 -p:PublishTrimmed=false
if ($LASTEXITCODE -ne 0) { throw 'Windows build failed.' }
$env:JAVA_HOME = $JavaHome
Push-Location (Join-Path $repo 'android')
try { & .\gradlew.bat :app:assembleDebug --console=plain; if ($LASTEXITCODE -ne 0) { throw 'Quest build failed.' } }
finally { Pop-Location }
