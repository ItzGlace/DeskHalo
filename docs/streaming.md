# Streaming in 0.6.1

The host runs a separate FFmpeg process for each visible screen, captures the chosen Windows monitor using gdigrab, scales and letterboxes the image, and sends Annex-B H.264 over one persistent authenticated HTTP response. Quest requests a separate cursor overlay. Intel QSV, NVIDIA NVENC and AMD AMF are attempted in that order; OpenH264 is the CPU fallback. The selected encoder appears in the host status and response header. The Intel encoder was exercised on the test PC; other hardware encoders remain unvalidated.

Output frame rate and encoder time base are explicit (`-r`, `-fps_mode cfr`, `-enc_time_base`). GDI timestamps use microseconds; without sufficient probing, inferring an output rate from them can create excessive duplicate frames. The requested rate is a target, not a guarantee of sustained capture/encode/decode throughput.

At 720p the default bitrate is 3 Mbps, 1080p uses 6 Mbps, 1440p uses 12 Mbps, 4K uses 24 Mbps and 8K uses 50 Mbps per screen. At 120 FPS the target doubles, capped at 80 Mbps. Actual bitrate varies. The stream disables B-frames, disables QSV look-ahead, uses a one-second GOP target and inserts access-unit delimiters. HTTP writes use 8 KiB buffers; these are application buffers, not a promise of network packet size. Inter-frame compression provides the main bandwidth reduction.

Quest uses MediaCodec AVC decoding into SurfaceTexture. The render thread samples the external GPU texture into an OpenXR swapchain, avoiding JPEG decoding and full-frame CPU bitmap uploads. Input access units have a 4 MiB bound; there is no application frame queue. Settings changes disconnect and recreate streams. Three simultaneous high-resolution decoders can exceed a headset's capacity; lower resolution or FPS if necessary. Wi-Fi congestion can still cause delay with this reliable TCP transport.

## Reproducible local runtime

The test runtime is BtbN FFmpeg LGPL build `N-126947-g45f3fecca9-20260928`, asset ID `595476193`, archive `ffmpeg-master-latest-win64-lgpl.zip`.

- Asset metadata/download: https://api.github.com/repos/BtbN/FFmpeg-Builds/releases/assets/595476193 (download with `Accept: application/octet-stream`).
- SHA-256: `b12b2da1bf1d9495d6650f2621f54a7111aa82cb210544b5dba53746a88b8502`.
- Build recipes and upstream source information: https://github.com/BtbN/FFmpeg-Builds and https://github.com/FFmpeg/FFmpeg/commit/45f3fecca9.

Run `scripts/Prepare-Streaming.ps1 -ArchivePath <downloaded-zip>`, then rebuild. The moving `latest` download may contain a different build in future; the script intentionally rejects it. If the pinned asset is no longer retained upstream, select and review a new build and update its hash explicitly. Runtime binaries are ignored by Git. The local test delivery includes the executable and its LGPLv3 license; preserve upstream notices and corresponding source/build information when preparing a public release.

The optional `/frame` JPEG endpoint remains a diagnostic compatibility endpoint; the Quest no longer uses it for streaming.
