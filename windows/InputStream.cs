using Microsoft.AspNetCore.Http;
using System;
using System.Runtime.InteropServices;
using System.Text.Json;
using System.Threading.Tasks;
namespace DeskHalo_Host;
public static class InputStream
{
    [StructLayout(LayoutKind.Sequential)] struct Point { public int X, Y; }
    [StructLayout(LayoutKind.Sequential)] struct CursorInfo { public int Size; public uint Flags; public IntPtr Handle; public Point Position; }
    [DllImport("user32.dll")] static extern bool GetCursorInfo(ref CursorInfo cursor);
    public static async Task Serve(HttpContext context)
    {
        context.Response.ContentType = "application/x-ndjson";
        var displays = Capture.Displays(); long refresh = Environment.TickCount64;
        try {
            while (!context.RequestAborted.IsCancellationRequested) {
                if (Environment.TickCount64 - refresh > 1000) { displays = Capture.Displays(); refresh = Environment.TickCount64; }
                var cursor = new CursorInfo { Size = Marshal.SizeOf<CursorInfo>() };
                bool visible = GetCursorInfo(ref cursor) && (cursor.Flags & 1) != 0;
                int id = -1; double u = 0, v = 0;
                for (int i = 0; i < displays.Length; i++) {
                    var d = displays[i];
                    if (cursor.Position.X < d.X || cursor.Position.Y < d.Y || cursor.Position.X >= d.X + d.Width || cursor.Position.Y >= d.Y + d.Height) continue;
                    id = i; double aspect = (double)d.Width / d.Height;
                    u = .5 + ((double)(cursor.Position.X-d.X)/d.Width-.5)*Math.Min(1, aspect/(16.0/9));
                    v = .5 + ((double)(cursor.Position.Y-d.Y)/d.Height-.5)*Math.Min(1, (16.0/9)/aspect); break;
                }
                await context.Response.WriteAsync(JsonSerializer.Serialize(new { keys = Capture.Keys(), id, u, v, visible }) + "\n", context.RequestAborted);
                await context.Response.Body.FlushAsync(context.RequestAborted);
                await Task.Delay(8, context.RequestAborted);
            }
        } catch (OperationCanceledException) when (context.RequestAborted.IsCancellationRequested) { }
    }
}
