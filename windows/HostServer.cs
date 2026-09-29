using Microsoft.AspNetCore.Builder;
using Microsoft.AspNetCore.Http;
using Microsoft.AspNetCore.Hosting;
using Microsoft.Extensions.Logging;
using System;
using System.Linq;
using System.Net.NetworkInformation;
using System.Net.Sockets;
using System.Security.Cryptography;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
namespace DeskHalo_Host;
public sealed class HostServer : IAsyncDisposable
{
    private WebApplication? app;
    private readonly SemaphoreSlim captureLock = new(1);
    public string PairingCode { get; } = RandomNumberGenerator.GetInt32(100000, 1000000).ToString();
    public bool Sharing => app != null;
    private long framesSent;
    public long FramesSent => Interlocked.Read(ref framesSent);
    public string LastStatus { get; private set; } = "Waiting for Quest";
    public static string[] LocalAddresses() => NetworkInterface.GetAllNetworkInterfaces().Where(n => n.OperationalStatus == OperationalStatus.Up)
        .SelectMany(n => n.GetIPProperties().UnicastAddresses).Where(a => a.Address.AddressFamily == AddressFamily.InterNetwork && !System.Net.IPAddress.IsLoopback(a.Address))
        .Select(a => a.Address.ToString()).Distinct().ToArray();
    public async Task StartAsync()
    {
        if (app != null) return;
        var builder = WebApplication.CreateSlimBuilder(); builder.Logging.ClearProviders(); builder.WebHost.UseUrls("http://0.0.0.0:47654");
        var pending = builder.Build();
        pending.Use(async (context, next) => {
            context.Response.Headers.CacheControl = "no-store";
            string supplied = context.Request.Headers["X-DeskHalo-Code"].ToString();
            if (!CryptographicOperations.FixedTimeEquals(Encoding.UTF8.GetBytes(supplied), Encoding.UTF8.GetBytes(PairingCode))) {
                context.Response.StatusCode = 401; await context.Response.WriteAsync("Pairing code required"); return;
            }
            await next(context);
        });
        FileTransfer.Map(pending);AudioStream.Map(pending);
        pending.MapGet("/hello", () => Results.Json(new { app = "DeskHalo", version = "0.6.0", codec = "h264", displays = Capture.Displays() }));
        pending.MapGet("/connection-tests", () => Results.Json(ConnectionTests.Request()));
        pending.MapGet("/probe", () => Results.Bytes(new byte[131072], "application/octet-stream"));
        pending.MapPost("/connection-tests", async (HttpContext context) => { if(context.Request.ContentLength is null or > 8192) return Results.BadRequest(); var report=await System.Text.Json.JsonDocument.ParseAsync(context.Request.Body,cancellationToken:context.RequestAborted);using(report){ConnectionTests.Report(report.RootElement.GetProperty("generation").GetInt64(),report.RootElement.GetProperty("text").GetString() ?? "");}return Results.Ok(); });
        pending.MapPost("/pointer",(HttpContext c)=>{if(!int.TryParse(c.Request.Query["id"],out int id)||!float.TryParse(c.Request.Query["u"],System.Globalization.NumberStyles.Float,System.Globalization.CultureInfo.InvariantCulture,out float u)||!float.TryParse(c.Request.Query["v"],System.Globalization.NumberStyles.Float,System.Globalization.CultureInfo.InvariantCulture,out float v)||!float.IsFinite(u)||!float.IsFinite(v)||u<0||u>1||v<0||v>1)return Results.BadRequest();try{PointerInput.Apply(id,u,v,c.Request.Query["down"]=="1");return Results.Ok();}catch(Exception e){return Results.Json(new{error=e.Message},statusCode:409);}});
        pending.MapGet("/input", (HttpContext context) => InputStream.Serve(context));
        pending.MapGet("/video", (HttpContext context) => VideoStream.Serve(context, text => LastStatus = text));
        pending.MapGet("/keys", () => Results.Json(new { keys = Capture.Keys() }));
        pending.MapGet("/stats", () => Results.Json(new { frames = FramesSent, status = LastStatus, encoderDiagnostics=VideoStream.Diagnostics }));
        pending.MapGet("/frame", async (HttpContext context) => {
            int id = int.TryParse(context.Request.Query["id"], out int value) ? value : 0;
            int width = int.TryParse(context.Request.Query["width"], out value) ? value : 1280;
            int height = int.TryParse(context.Request.Query["height"], out value) ? value : 720;
            if (id < 0 || width < 320 || width > 2560 || height < 180 || height > 1440) { context.Response.StatusCode = 400; return; }
            await captureLock.WaitAsync(context.RequestAborted);
            try {
                byte[] bytes = Capture.Frame(id, width, height); context.Response.ContentType = "image/jpeg";
                await context.Response.Body.WriteAsync(bytes, context.RequestAborted);
                Interlocked.Increment(ref framesSent); LastStatus = $"Display {id + 1} • {width} × {height}";
            } catch (ArgumentOutOfRangeException) { context.Response.StatusCode = 404; }
              catch (System.ComponentModel.Win32Exception ex) { LastStatus = "Capture unavailable: " + ex.Message; context.Response.StatusCode = 503; await context.Response.WriteAsync(LastStatus); }
            finally { captureLock.Release(); }
        });
        pending.MapPost("/display-mode", (HttpContext c) => { if(!int.TryParse(c.Request.Query["id"],out int id)||!int.TryParse(c.Request.Query["width"],out int w)||!int.TryParse(c.Request.Query["height"],out int h)||!int.TryParse(c.Request.Query["fps"],out int f))return Results.BadRequest();try{return Results.Json(new {message=DisplayModes.Apply(id,w,h,f)});}catch(Exception e){return Results.Json(new {error=e.Message},statusCode:409);}});
        pending.MapPost("/virtual", async (HttpContext context) => {
            if (!int.TryParse(context.Request.Query["count"], out int count) || count < 0 || count > 3) return Results.BadRequest();
            try { return Results.Json(new { message = await VirtualDisplays.SetCountAsync(count) }); }
            catch (Exception ex) { return Results.Json(new { error = ex.Message }, statusCode: 503); }
        });
        try { await pending.StartAsync(); app = pending; _ = ConnectionTests.ConfigureUsb(); } catch { await pending.DisposeAsync(); throw; }
    }
    public async Task StopAsync() { PointerInput.Release();var old = app; app = null; if (old != null) { await old.StopAsync(); await old.DisposeAsync(); } }
    public async ValueTask DisposeAsync() { await StopAsync(); captureLock.Dispose(); }
}
