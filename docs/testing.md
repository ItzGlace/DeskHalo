# Test record — 2026-09-29

This is a working test log, not a claim that all features are validated.

- Windows host: compiled using the installed .NET 10 SDK and WinUI dependencies, zero errors and warnings.
- Quest client: Gradle debug APK with native arm64 OpenXR library compiled successfully.
- Connected test headset: Meta Quest 2, Android API 34.
- APK was installed successfully after cancelling an unfinished package-install session that had temporarily frozen the package.
- OpenXR device logs reached READY / SYNCHRONIZED / VISIBLE / FOCUSED states. No native startup failure was reported in that run.
- ADB reverse forwarding for TCP 47654 was configured.
- Host `/hello`, `/keys` and authentication responded. An unauthenticated request returned HTTP 401.
- A host started inside the agent's restricted process environment could not capture the desktop: GDI reported an invalid handle. The user was asked to start the normal desktop executable for the real capture test.
- The test PC initially had one physical display and no virtual display driver. Actual extra desktop creation therefore remains dependent on driver installation.

Still to verify: authenticated desktop frames from the normal Windows session, headset-visible rendering and controller interaction, physical key feedback, Wi-Fi/hotspot paths and measured FPS at each quality setting. See subsequent updates to this file for results.

## Version 0.2 update

- Both apps compiled successfully; Windows build had zero warnings/errors.
- Mixed 3/4-byte Annex-B start codes, fragmented reads from 1 to 8192 bytes, final-frame/EOF handling and oversized-frame rejection passed the JVM parser test.
- Intel QSV encoded a synthetic 720p30 H.264 sample successfully on the PC.
- Quest 2 logs confirmed OMX.qcom.video.decoder.avc, successful passthrough feature/layer creation and a focused OpenXR session. The synthetic run held 72 Hz VR rendering; its Node timer-fed stream decoded at approximately 25 FPS.
- Updated APK installed successfully. A later real desktop host run passed all 10 protocol checks, including actual JPEG diagnostics at 720p, 1080p and 1440p. These JPEG timings are diagnostic only; live VR uses H.264.
- Real desktop H.264: Intel QSV produced 1,256,550 bytes over a 5.04-second receive sample at 720p30 (approximately 2 Mbps for that desktop content). FFprobe decoded the captured stream. Encoder startup was approximately 1.5 seconds; this is not an end-to-end latency measurement.
- Headset logs held approximately 30 decoded FPS at 720p and 59–62 decoded FPS at 1440p60 during the observed interval. This is a short single-screen test, not a sustained multi-screen benchmark.
- The user confirmed live desktop, room passthrough and the left-menu-button settings panel were all visible. A headset capture also showed the closer tilted keyboard and the Windows pointer. Settings changes to 1440p60 reached the host.
- The final APK includes an sRGB conversion correction for the external video texture. It compiled, installed and restarted successfully; the final screenshot faced down away from the desktop, so the corrected desktop colours need a further visual check.
- Pending: driver installation and real extended-desktop creation, multi-screen stress and wireless-path tests. The driver was not installed at the last check; the prepared installer requires the user's Windows administrator prompt. It now writes an installation transcript for diagnosis.
