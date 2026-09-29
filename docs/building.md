# Building DeskHalo

## Windows

Install Visual Studio with WinUI tooling, .NET 10 SDK and Windows SDK. Open `DeskHalo.slnx` or run:

```powershell
dotnet publish "windows/DeskHalo Host.csproj" -c Release -r win-x64 --self-contained true -p:Platform=x64 -p:PublishTrimmed=false -p:PublishReadyToRun=false -o artifacts/windows
```

Prepare the pinned FFmpeg runtime first using `scripts/Prepare-Streaming.ps1`; see [streaming.md](streaming.md). Driver binaries are not part of the public installer; install the signed driver from its upstream project if additional Windows monitors are needed.

The publish target explicitly copies compiled XAML (`*.xbf`) and the app resource index (`DeskHalo Host.pri`). Do not distribute only the executable or omit these files.

To build the per-user x64 MSI, install WiX 5.0.2 and run with PowerShell 7:

```powershell
dotnet tool install wix --version 5.0.2 --tool-path artifacts/wix
./scripts/Build-Installer.ps1 -PublishDir artifacts/windows -Output artifacts/DeskHalo-Host-v0.6.0-x64.msi -Wix ./artifacts/wix/wix.exe
```

Copy LICENSE, THIRD_PARTY_NOTICES.md and licenses/ into the publish directory before packaging. The MSI does not install a kernel driver or change firewall policy.

## Quest

Open `android/` in Android Studio. Requirements: Android Studio's bundled JBR, Android SDK 37, NDK 28.2.13676358 and CMake 3.22.1. Set the SDK location using Android Studio or `ANDROID_HOME`.

```powershell
cd android
./gradlew.bat :app:assembleDebug
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

The first native build downloads Assimp 5.4.3. Only OBJ, glTF/GLB and FBX importers are enabled. The APK must contain `lib/arm64-v8a/libdeskhalo.so`; a CMake target override prevents Assimp's debug postfix from changing its name.

The published preview APK uses the existing development certificate so it can update the test installation without deleting profiles. A build on another computer may use a different debug certificate and cannot replace it directly. Signing keys are never committed. Configure your own release signing before store distribution.

## Tests

`tests/KeyboardFrameTest.java` tests gesture dimensions, pose and stability. `tests/AccessUnitsTest.java` tests H.264 parsing. `tests/protocol.mjs` runs authenticated host checks; provide the host and pairing code locally, never commit session reports. Real headset visual alignment and motion quality require a person wearing the Quest.
