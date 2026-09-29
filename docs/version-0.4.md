# DeskHalo 0.4 test build

Implemented:
- Windows virtual-desktop mode selection: 1440p (2K/QHD), 4K and 8K. Physical monitor modes are protected. Stream resolution is negotiated against the hardware decoder and a shared pixel budget; an 8K desktop can therefore be streamed at a lower resolution. This is not native 8K H.264 decoding on Quest 2.
- Decoder frame/operating rate, hardware decoder selection, stale decoded frame dropping, 2-second input-buffer watchdog and 4-second network timeout. Stream FPS and headset refresh are independent; headset refresh requests at least 90 Hz where supported.
- SurfaceTexture callbacks on a dedicated display-priority thread. Transparent compositor background clears to premultiplied black.
- Native OpenXR hands (skeletal models), controller models, left pinch menu, right pinch/trigger selection and desktop click/drag. PC pointer injection releases a held button after 600 ms without updates. Elevated Windows applications may reject injected input.
- Keyboard snaps to stable palms-down typing formation held for 1.2 seconds. Separate hands beyond 45 cm to rearm. Recenter or keyboard height/distance controls clear the snapped pose. Snap pose is session-local; existing profiles retain the adjustable keyboard offsets.
- Resting controller models fade from full opacity to 10% after ten seconds without significant motion. Tracking loss retains the last known model at 10%.
- Authenticated file transfer, 2 GiB upload limit, safe names, duplicate preservation, partial upload cleanup. Windows Send File stages an outbox file; Quest Transfer/Files downloads into Download/DeskHalo or sends through the system document picker. Received PC files go to Documents/DeskHalo Transfers/From Quest.
- 48 kHz stereo PCM PC sound via WASAPI; optional permission-gated Quest microphone relay plays through PC speakers. These directions are mutually exclusive to avoid a feedback loop. This does not install a Windows virtual microphone for games/voice-chat applications.
- Virtual display repair disables the driver's hardware cursor. Reload messages about abandoned old swapchains no longer falsely report a failed add. Adding panels requires distinct desktop sources. Zero desktop count detaches virtual monitors rather than asking the driver for its unsupported zero-monitor configuration.

Validation performed:
- Windows build passed with zero warnings/errors.
- Android v0.4 built successfully in the normal desktop launcher (11 seconds). The final hardware-decoder selection, focus pause/resume and source-validation edits are included in the verification rebuild.
- Annex-B parser tests pass (fragmented input, mixed prefixes, final frame, EOF and size bound).
- Driver initially failed with Code 43 and repeated UMDF crashes. After the backed-up repair it remained Started and accepted SETDISPLAYCOUNT 2. User completed final 8K setup and requested the temporary desktops removed. The first detach attempt did not persist; the normal host inventory still showed DISPLAY17 at (2880,0), separate from the physical display at (0,0). The verification launcher calls the corrected host detach operation.

Pending device validation:
- Final APK build/install, sustained 90/120 FPS and freeze recovery, perceived flicker, hand pinch and keyboard alignment, controller fade, audible sound and transfer round trips.
- Full end-to-end separate virtual desktops at 2K/4K/8K, including desktop topology verification in a normal interactive Windows host session. No claim that native 8K streaming or 120 unique source frames is supported on every headset/display.

Run DeskHalo-Windows-v0.4/Start-Test.cmd from the desktop. It starts the host, builds/installs the matching Quest APK and writes test logs. Tests/protocol-v04.mjs exercises pairing, filenames, upload, pointer validation, physical display protection and PCM streaming. Test-Quest-FPS.ps1 in the test folder measures 90/120 FPS while the headset is awake.

Sources used for implementation: [Android MediaFormat](https://developer.android.com/reference/android/media/MediaFormat), [Meta native hand tracking](https://developers.meta.com/horizon/documentation/native/android/mobile-hand-tracking/), [NAudio 2.2.1 WASAPI capture](https://github.com/naudio/NAudio/blob/v2.2.1/NAudio.Wasapi/WasapiCapture.cs).

No GitHub upload has been performed.
