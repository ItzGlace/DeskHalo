# DeskHalo 0.6 preview

## Changes

- Compact keyboard canvas: removed status/header labels and unused margins; added a small minute-updated clock.
- Keyboard rests at 5° toward the user, including calibrated finger-frame placement.
- Controller visual orientation rotated 90° forward; named model buttons retain pressed feedback.
- Bundled MIT-licensed WebXR generic hands and Touch v2 GLB controllers with translucent amber shading; custom imports remain available.
- Explicit encoded SDR copy into the sRGB compositor image, with automatic framebuffer conversion disabled during that copy where supported. Full-range desktop RGB is explicitly converted into limited-range BT.709 video on the host. No gaming contrast filter is applied.
- Fixed Windows publish output omitting XAML/resource files and Android library naming. Added a per-user MSI with bundled app runtimes.
- GDI SDR capture is used with hardware H.264 encoding. On the test PC, Desktop Duplication crushed dark tones before encoding (a grey sample near RGB 24 became RGB 4); the GDI encode/decode round trip preserved it near RGB 23. This prioritizes correct SDR colours and may cost more capture CPU time.

## Validation

- Android debug APK built successfully and installed on Quest 2.
- OBJ/GLB/FBX import smoke tests passed; invalid GLB was rejected.
- All four bundled models loaded on Quest: two articulated hands and two controllers.
- OpenXR and passthrough initialized; Quest reported sRGB write control support.
- KeyboardFrame geometry and stability assertions passed.
- Windows self-contained Release publish succeeded; compiled XAML and resource index are included.
- MSI built with WiX 5.0.2.
- Ten live protocol checks passed, including pairing rejection, keyboard state, invalid display requests and actual desktop JPEG capture at 720p, 1080p and 1440p.
- User confirmed the keyboard/layout/controller changes looked good; the initial colour change was insufficient and prompted the capture-path correction above.

Headset appearance, controller alignment, colour perception and sustained high-FPS performance still need visual verification. Stream targets are not measured performance claims. This is a preview release, not a store-certified build.
