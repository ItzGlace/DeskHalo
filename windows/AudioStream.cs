using Microsoft.AspNetCore.Builder;
using Microsoft.AspNetCore.Http;
using NAudio.Wave;
using NAudio.CoreAudioApi;
using System;
using System.Threading;
using System.Threading.Channels;
using System.Threading.Tasks;

namespace DeskHalo_Host;
public static class AudioStream
{
    private sealed class Loopback : WasapiCapture { public Loopback(MMDevice device):base(device,false,20){} protected override AudioClientStreamFlags GetAudioClientStreamFlags()=>base.GetAudioClientStreamFlags()|AudioClientStreamFlags.Loopback; }
    private static readonly SemaphoreSlim playback=new(1), capture=new(1);
    public static void Map(WebApplication app)
    {
        app.MapGet("/audio", async (HttpContext c) => {
            if(!await capture.WaitAsync(0,c.RequestAborted)){c.Response.StatusCode=409;return;}
            try {
                using var enumerator=new MMDeviceEnumerator();using var device=enumerator.GetDefaultAudioEndpoint(DataFlow.Render,Role.Multimedia);using var recorder=new Loopback(device);
                recorder.WaveFormat=new WaveFormat(48000,16,2);
                var queue=Channel.CreateBounded<byte[]>(new BoundedChannelOptions(3){FullMode=BoundedChannelFullMode.DropOldest,SingleReader=true,SingleWriter=false});
                long lastPacket=Environment.TickCount64;
                recorder.DataAvailable+=(_,e)=>{Interlocked.Exchange(ref lastPacket,Environment.TickCount64);if(e.BytesRecorded>0)queue.Writer.TryWrite(e.Buffer.AsSpan(0,e.BytesRecorded).ToArray());};
                recorder.RecordingStopped+=(_,e)=>queue.Writer.TryComplete(e.Exception);
                c.Response.ContentType="application/octet-stream";c.Response.Headers["X-DeskHalo-Audio"]="s16le;rate=48000;channels=2";
                recorder.StartRecording();await c.Response.StartAsync(c.RequestAborted);
                using var heartbeat=new System.Threading.Timer(_=>{if(Environment.TickCount64-Interlocked.Read(ref lastPacket)>60)queue.Writer.TryWrite(new byte[3840]);},null,20,20);
                try {await foreach(var bytes in queue.Reader.ReadAllAsync(c.RequestAborted)){await c.Response.Body.WriteAsync(bytes,c.RequestAborted);await c.Response.Body.FlushAsync(c.RequestAborted);}}
                finally{recorder.StopRecording();}
            } catch(OperationCanceledException){} finally{capture.Release();}
        });
        // User explicitly enables Quest microphone relay. This plays on the PC;
        // it does not impersonate a Windows microphone device.
        app.MapPost("/audio/microphone", async (HttpContext c) => {
            if(!await playback.WaitAsync(0,c.RequestAborted)){c.Response.StatusCode=409;return;}
            try {
                var bodyLimit=c.Features.Get<Microsoft.AspNetCore.Http.Features.IHttpMaxRequestBodySizeFeature>();if(bodyLimit is {IsReadOnly:false})bodyLimit.MaxRequestBodySize=null;
                var buffer=new BufferedWaveProvider(new WaveFormat(48000,16,1)){BufferDuration=TimeSpan.FromMilliseconds(200),DiscardOnBufferOverflow=true};
                using var player=new WaveOutEvent{DesiredLatency=60};player.Init(buffer);player.Play();
                byte[] bytes=new byte[1920];int n;while((n=await c.Request.Body.ReadAsync(bytes,c.RequestAborted))>0){if(buffer.BufferedDuration.TotalMilliseconds>120)buffer.ClearBuffer();buffer.AddSamples(bytes,0,n);}player.Stop();
            } catch(OperationCanceledException){}finally{playback.Release();}
        });
    }
}
