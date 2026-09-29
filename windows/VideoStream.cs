using Microsoft.AspNetCore.Http;
using System;
using System.Diagnostics;
using System.IO;
using System.Threading;
using System.Threading.Tasks;

namespace DeskHalo_Host;

// One encoder per visible panel. The pipe and HTTP backpressure bound memory use.
public static class VideoStream
{
    private static readonly SemaphoreSlim slots = new(3);
    private static string? preferredEncoder;
    public static string Diagnostics {get;private set;}="Hardware encoding pending";
    public static async Task Serve(HttpContext context, Action<string> report)
    {
        int Read(string name, int fallback) => int.TryParse(context.Request.Query[name], out int value) ? value : fallback;
        int id = Read("id", 0), width = Read("width", 1280), height = Read("height", 720), fps = Read("fps", 30);
        var displays = Capture.Displays();
        if (id < 0 || id >= displays.Length || !((width == 1280 && height == 720) || (width == 1920 && height == 1080) || (width == 2560 && height == 1440) || (width == 3840 && height == 2160) || (width == 7680 && height == 4320)) || (fps != 15 && fps != 30 && fps != 60 && fps != 90 && fps != 120)) {
            context.Response.StatusCode = 400; return;
        }
        if (!await slots.WaitAsync(0, context.RequestAborted)) { context.Response.StatusCode = 429; return; }
        try {
            string executable = Path.Combine(AppContext.BaseDirectory, "Streaming", "ffmpeg.exe");
            if (!File.Exists(executable)) throw new IOException("Streaming/ffmpeg.exe is missing. Run scripts/Prepare-Streaming.ps1.");
            string[] encoders = preferredEncoder == null ? ["h264_qsv", "h264_nvenc", "h264_amf", "libopenh264"] : System.Linq.Enumerable.ToArray(System.Linq.Enumerable.Distinct(new[]{preferredEncoder,"h264_qsv","h264_nvenc","h264_amf","libopenh264"}));
            bool gaming = Read("gaming", 0) == 1;
            bool separateCursor = Read("cursor", 0) == 1;
            // Desktop Duplication's forced BGRA output crushes SDR shadows on advanced-colour
            // desktops. GDI supplies the Windows SDR representation without an assumed gamma.
            // Keep hardware H.264 encoding; do not compensate with a guessed contrast filter.
            bool canDuplicate = false;
            var attempts = new System.Collections.Generic.List<(string encoder, bool dda)>();
            if(canDuplicate) foreach(var e in encoders) attempts.Add((e,true));
            foreach(var e in encoders) attempts.Add((e,false));
            string failure = "Encoder unavailable";
            foreach (var attempt in attempts) {
                string encoder = attempt.encoder;
                var d = displays[id];
                int bitrate = Read("bitrate", height <= 720 ? 3000 : height <= 1080 ? 6000 : height <= 1440 ? 12000 : height <= 2160 ? 24000 : 50000);
                bitrate = Math.Clamp(bitrate * Math.Max(1, fps / 60), 1000, 80000);
                var start = new ProcessStartInfo(executable) { UseShellExecute = false, CreateNoWindow = true, RedirectStandardOutput = true, RedirectStandardError = true };
                void Add(params string[] args) { foreach (var arg in args) start.ArgumentList.Add(arg); }
                Add("-hide_banner", "-loglevel", "warning", "-nostdin", "-fflags", "nobuffer", "-analyzeduration", "0", "-probesize", "32");
                if(attempt.dda) Add("-f","lavfi","-i",$"ddagrab=output_idx=0:framerate={fps}:draw_mouse={(separateCursor?0:1)}:output_fmt=bgra","-an");
                else Add("-f", "gdigrab", "-framerate", fps.ToString(), "-draw_mouse", separateCursor ? "0" : "1", "-offset_x", d.X.ToString(), "-offset_y", d.Y.ToString(), "-video_size", $"{d.Width}x{d.Height}", "-i", "desktop", "-an");
                // GDI timestamps use microseconds. With probing disabled, FFmpeg can infer
                // a million-FPS output and duplicate a captured frame indefinitely.
                // Explicit encoder timing preserves the requested rate without a catch-up queue.
                Add("-r",fps.ToString(),"-fps_mode","cfr","-enc_time_base",$"1:{fps}");
                Add("-vf", (attempt.dda ? "hwdownload,format=bgra," : "") + $"scale={width}:{height}:force_original_aspect_ratio=decrease:flags=fast_bilinear:in_range=pc:out_color_matrix=bt709:out_range=tv,pad={width}:{height}:(ow-iw)/2:(oh-ih)/2,format=yuv420p", "-c:v", encoder, "-b:v", bitrate + "k", "-maxrate", bitrate + "k", "-bufsize", (gaming ? Math.Max(100,bitrate/10) : bitrate/4) + "k", "-g", fps.ToString(), "-bf", "0");
                if (encoder == "h264_qsv") Add("-async_depth", "1", "-look_ahead", "0", "-preset", "veryfast");
                if (encoder == "h264_nvenc") Add("-preset", "p1", "-tune", "ull", "-zerolatency", "1");
                if (encoder == "h264_amf") Add("-usage", "ultralowlatency");
                if (encoder == "libopenh264") Add("-max_nal_size", "1400");
                Add("-color_range","tv","-colorspace","bt709","-color_primaries","bt709","-color_trc","iec61966-2-1");
                Add("-bsf:v", "h264_metadata=aud=insert", "-flush_packets", "1", "-f", "h264", "pipe:1");
                using var process = Process.Start(start) ?? throw new IOException("Cannot start encoder");
                string error = "";
                var stderr = Task.Run(async () => { string? line; while ((line = await process.StandardError.ReadLineAsync()) != null) error = (error + "\n" + line)[^Math.Min(4096, error.Length + line.Length + 1)..]; });
                try {
                    byte[] buffer = new byte[8192];
                    using var startup = CancellationTokenSource.CreateLinkedTokenSource(context.RequestAborted);
                    startup.CancelAfter(8000);
                    int n;
                    try { n = await process.StandardOutput.BaseStream.ReadAsync(buffer, startup.Token); }
                    catch (OperationCanceledException) when (!context.RequestAborted.IsCancellationRequested) { failure = encoder + " timed out"; Diagnostics=failure; continue; }
                    if (n == 0) { await stderr; failure = encoder + ": " + error; Diagnostics=failure; continue; }
                    preferredEncoder = encoder=="libopenh264" ? null : encoder;
                    if(encoder!="libopenh264")Diagnostics="Hardware encoder active: "+encoder;
                    context.Response.ContentType = "video/h264";
                    context.Response.Headers["X-DeskHalo-Encoder"] = encoder;
                    report($"H.264 • {(attempt.dda ? "Desktop Duplication" : "GDI")} • {encoder} • {width}×{height} at {fps} FPS • {bitrate / 1000f:0.#} Mbps");
                    do {
                        await context.Response.Body.WriteAsync(buffer.AsMemory(0, n), context.RequestAborted);
                        await context.Response.Body.FlushAsync(context.RequestAborted);
                    } while ((n = await process.StandardOutput.BaseStream.ReadAsync(buffer, context.RequestAborted)) > 0);
                    return;
                } finally {
                    if (!process.HasExited) process.Kill(entireProcessTree: true);
                    await process.WaitForExitAsync(); await stderr;
                }
            }
            throw new IOException(failure);
        } catch (OperationCanceledException) when (context.RequestAborted.IsCancellationRequested) { }
        catch (Exception ex) { report(ex.Message); if (!context.Response.HasStarted) { context.Response.StatusCode = 503; await context.Response.WriteAsync(ex.Message); } else context.Abort(); }
        finally { slots.Release(); }
    }
}
