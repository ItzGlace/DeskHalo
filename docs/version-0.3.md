# DeskHalo 0.3 — controls and test scope

## Controls

- The Quest connection form defaults to USB. Switch USB off to enter a Wi-Fi/hotspot address. The host attempts ADB USB reverse when sharing starts; Android Studio's ADB server and an authorized USB device are required.
- Left ≡ → Keyboard → Hide keyboard / Show keyboard.
- Left ≡ → Profiles: three save/load slots, automatic restoration of the last configuration, Gaming and Desktop presets. Profiles retain FPS, resolution, panel count/sources/size/distance, keyboard layout/visibility/placement, passthrough and gaming mode. Pairing codes are not saved in profiles.
- Stream FPS choices: 15, 30, 60, 90 and 120. The native renderer enumerates supported headset refresh rates and requests a supported rate. Actual headset Hz is shown separately. Requesting 120 video FPS does not guarantee 120 unique game frames, decoder FPS or headset Hz.
- Gaming preset: one 720p120 panel, keyboard hidden, gaming capture enabled. Desktop preset: 1080p60, keyboard shown. Size and distance remain independently adjustable.

## Latency and colour

Cursor/key states use a separate authenticated streaming endpoint. Cursor coordinates are rendered as a local VR overlay, independent of video encode/decode delay; the encoded Windows cursor is disabled for this client. The overlay currently uses a standard arrow, not each application's custom cursor shape. It hides when Windows hides its cursor, including many games, and after stale input. This does not implement remote mouse injection, gamepad forwarding, audio, or SteamVR/PCVR streaming.

Gaming mode tries DXGI Desktop Duplication for the single-display case, then falls back to GDI if unavailable. Multiple-display capture uses GDI to preserve correct monitor selection. Hardware H.264 remains enabled, with no B-frames/look-ahead and a smaller rate-control buffer. The current Desktop Duplication path still downloads pixels for scaling before encoding; it is not a fully GPU-only pipeline. Reliable TCP/ADB transport can still incur delay under congestion.

Colour handling specifies limited-range BT.709 YUV with sRGB transfer metadata, corrects video texture transfer when writing sRGB swapchains, and sets OpenXR output to Rec.709. This release targets SDR desktops. HDR game/desktop tone-mapping is not implemented or validated.

## Connection tests

The Windows Test USB + network connections button requests tests from the connected Quest. Quest probes localhost (USB reverse) and the host's advertised IPv4 interfaces and reports reachability, response-header delay and a short 128 KiB transfer probe. These numbers are a quick diagnostic, not sustained throughput or motion-to-photon latency. VPN/virtual adapters can appear and be unreachable. Only currently connected Wi-Fi routes can pass; testing 2.4 GHz, 5 GHz and a PC hotspot requires connecting Quest to each in turn.

## Extra Windows desktops

The previous installer was blocked by Windows PowerShell's script policy before execution. The 0.3 host starts the reviewed local installer with process-scoped RemoteSigned policy and an administrator prompt. Machine-wide execution policy and driver signature enforcement remain unchanged. The installer writes DriverSetup/installation.log. Actual virtual monitor creation still depends on successful driver installation and Windows enumeration. The Quest now waits up to 25 seconds for a display-change request instead of timing out after four seconds.

## Verified so far

- Windows and native Android builds pass.
- Annex-B parser fragmentation/size-limit tests pass.
- Live PC encoding tests succeeded at 720p90 and 720p120 using Desktop Duplication + Intel QSV.
- The 120 FPS test stream decoded and its BT.709 primaries/matrix, limited range and sRGB transfer metadata were verified.
- App logo generated with the built-in image tool and integrated into Android and Windows. Prompt is in branding/PROMPT.md.
- Direct authenticated HTTP checks executed from the Quest shell reached the host through USB localhost and its current 2.4 GHz Wi-Fi address. Two other advertised PC adapter addresses were unreachable. These shell preflight checks are separate from the app's interactive measurement report and do not establish 5 GHz/PC-hotspot connectivity.

Pending physical tests: Quest-side 90/120 Hz confirmation, keyboard/profile/cursor behaviour, connection-test results on each network, and successful driver installation/extended desktops. See testing.md for the evolving record.
