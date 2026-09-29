# Third-party components

- Khronos OpenXR Android loader 1.1.49: https://github.com/KhronosGroup/OpenXR-SDK — Apache-2.0 / MIT as distributed by Khronos. Resolved from Maven; not copied into source.
- Microsoft Windows App SDK / WinUI and .NET / System.Drawing.Common: supplied via Microsoft SDK and NuGet packages; retain their upstream notices.
- Gradle wrapper and Android Gradle Plugin: upstream distribution and Android build tool licenses apply.
- Optional Virtual Display Driver 25.7.23: https://github.com/VirtualDrivers/Virtual-Display-Driver — a separate installation prerequisite, not DeskHalo-authored code. Review the upstream license and distribution terms before bundling it in a release. The downloaded test driver is kept outside this source repository.

- IBM Plex Sans ExtraLight and Light: IBM, SIL Open Font License 1.1. The original license is included alongside the fonts in both applications. Upstream: https://github.com/IBM/plex.
- FFmpeg: a separate LGPLv3 executable in the local test delivery, not linked into DeskHalo. Its license accompanies that executable. Build identity, checksum and source references are recorded in docs/streaming.md. Third-party runtime binaries are excluded from Git.

- NAudio.Core, NAudio.Wasapi and NAudio.WinMM 2.2.1: Mark Heath, MIT. See licenses/NAudio-MIT.txt. Packages are restored from NuGet.

## Assimp 5.4.3

Custom model import uses the BSD-3-Clause licensed Open Asset Import Library, pinned to the [v5.4.3 release](https://github.com/assimp/assimp/releases/tag/v5.4.3). CMake downloads its official source archive on the first native build. The native build copies the upstream LICENSE and bundled dependency notices into APK assets/licenses/assimp. The importers and bundled assets were built and loaded on Quest 2 in v0.6.

## WebXR Input Profiles assets

Bundled generic-hand and oculus-touch-v2 left/right GLB models come from https://github.com/immersive-web/webxr-input-profiles/tree/main/packages/assets, MIT, copyright 2019 Amazon. See licenses/WebXR-Input-Profiles-MIT.txt. WebXR bone names are mapped to OpenXR joints; controller button meshes animate with live inputs.
