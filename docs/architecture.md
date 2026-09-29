# Architecture, 0.2

Windows captures monitors with FFmpeg gdigrab, including the cursor, and encodes H.264 using a hardware encoder where available. Quest parses access units, decodes them with MediaCodec into SurfaceTexture, and renders GPU textures into OpenXR quad swapchains. See [streaming.md](streaming.md) for bitrate, latency and dependency details.

An optional XR_FB_passthrough reconstruction layer sits beneath an alpha projection layer and the screen/keyboard quads. If the extension cannot initialize, a dark background remains and the runtime error is reported. Quest 2 passthrough is grayscale. Camera frames are handled by the runtime, not sent to Windows.

The menu starts hidden. The left Touch ≡ button toggles it and anchors it in front of the current head pose. The right Touch aim pose intersects that panel and the trigger activates settings. The right Meta/Oculus system button belongs to the operating system and cannot be repurposed. Screens start 1.25 m from the recentered head position and can move between 0.6 and 3.5 m. Screen width has the same range. The smaller tilted keyboard has independent height and distance adjustments.

USB uses ADB reverse from Quest localhost:47654 to PC localhost:47654. Hotspot and shared-router connections use the PC's local IP. They use the same authenticated local HTTP protocol; no cloud service is involved.

All endpoints require X-DeskHalo-Code and disable caching.

| Method / path | Purpose |
|---|---|
| GET /hello | Host version, codec and Windows display inventory, including isVirtual |
| GET /video?id=0&width=1280&height=720&fps=30 | Continuous Annex-B H.264, maximum three simultaneous streams |
| GET /frame?id=0&width=1280&height=720 | Legacy JPEG diagnostic endpoint |
| GET /keys | Currently held Windows virtual-key codes |
| GET /stats | Legacy frame count and current streaming status |
| POST /virtual?count=1 | Set the installed MttVDD driver's total virtual-monitor count, extend Windows and verify enumeration |

VR panels and Windows monitors are separate resources. Adding a VR panel alone does not create a Windows monitor. Successful desktop creation refreshes inventory and assigns the newest enumerated display to a visible panel. A failed request reports the driver problem. Source selections currently use enumeration indices; monitor hot-plug changes can require reassignment.
