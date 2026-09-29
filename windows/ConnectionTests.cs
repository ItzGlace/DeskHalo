using System;
using System.IO;
using System.Linq;
using System.Net.Sockets;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
namespace DeskHalo_Host;
public static class ConnectionTests
{
    public static long Generation { get; private set; }
    public static string Result { get; private set; } = "USB is preferred. Connect Quest, then test available paths.";
    public static object Request() => new { generation = Generation, addresses = new[] { "127.0.0.1" }.Concat(HostServer.LocalAddresses()).Distinct().ToArray() };
    public static void Start() { Generation = DateTimeOffset.UtcNow.ToUnixTimeMilliseconds(); Result = "Waiting for connected Quest to probe USB and PC network addresses…"; }
    public static void Report(long generation, string text) { if(generation == Generation) Result = text[..Math.Min(text.Length,4000)]; }
    public static async Task<string> ConfigureUsb()
    {
        try {
            using var timeout = new CancellationTokenSource(5000);
            using var client = new TcpClient(); await client.ConnectAsync("127.0.0.1",5037,timeout.Token);
            var stream = client.GetStream();
            async Task Service(string command) {
                byte[] body=Encoding.UTF8.GetBytes(command), prefix=Encoding.ASCII.GetBytes(body.Length.ToString("x4"));
                await stream.WriteAsync(prefix,timeout.Token);await stream.WriteAsync(body,timeout.Token);
                byte[] status=new byte[4];await stream.ReadExactlyAsync(status,timeout.Token);
                if(Encoding.ASCII.GetString(status)!="OKAY") {await stream.ReadExactlyAsync(status,timeout.Token);int n=Convert.ToInt32(Encoding.ASCII.GetString(status),16);if(n>4096)throw new IOException("Invalid ADB response");byte[] error=new byte[n];await stream.ReadExactlyAsync(error,timeout.Token);throw new IOException(Encoding.UTF8.GetString(error));}
            }
            await Service("host:transport-usb");await Service("reverse:forward:tcp:47654;tcp:47654");
            return "USB tunnel ready • 127.0.0.1:47654";
        } catch(Exception ex) {return "USB setup: "+ex.Message+". Open Android Studio and authorize USB debugging on Quest.";}
    }
}
