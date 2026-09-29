using System;
using System.IO;
using System.IO.Pipes;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Linq;
namespace DeskHalo_Host;
public static class VirtualDisplays
{
    private static readonly SemaphoreSlim gate = new(1);
    [System.Runtime.InteropServices.DllImport("user32.dll")]
    private static extern int SetDisplayConfig(uint paths, IntPtr pathArray, uint modes, IntPtr modeArray, uint flags);
    private static async Task<string> Command(string command)
    {
        await gate.WaitAsync();
        try {
        using var timeout = new CancellationTokenSource(10000);
        using var pipe = new NamedPipeClientStream(".", "MTTVirtualDisplayPipe", PipeDirection.InOut, PipeOptions.Asynchronous);
        await pipe.ConnectAsync(timeout.Token);
        byte[] bytes = Encoding.Unicode.GetBytes(command + "\0");
        await pipe.WriteAsync(bytes, timeout.Token); await pipe.FlushAsync(timeout.Token);
        byte[] buffer = new byte[4096]; using var output = new MemoryStream();
        try { int n; while ((n = await pipe.ReadAsync(buffer, timeout.Token)) > 0) { output.Write(buffer, 0, n); if (output.Length > 65536) throw new IOException("Driver response exceeds limit"); } } catch (IOException) when (command.StartsWith("SETDISPLAYCOUNT", StringComparison.Ordinal)) { /* Driver reload closes the pipe. Verify enumeration below. */ }
        return Encoding.UTF8.GetString(output.ToArray()).TrimEnd('\0', '\r', '\n');
        } finally { gate.Release(); }
    }
    public static async Task<string> CheckAsync()
    {
        try { return "Virtual display driver: " + await Command("PING"); }
        catch { return Capture.Displays().Any(d=>d.IsVirtual) ? "Virtual display driver active." : "Driver unavailable or stopped. Use Set up extra displays to diagnose it."; }
    }
    public static async Task<string> SetCountAsync(int count)
    {
        if (count < 0 || count > 3) throw new ArgumentOutOfRangeException(nameof(count));
        if(count==0){DisplayModes.DetachAll();return "Virtual desktops detached.";}
        try {
            string response = await Command("SETDISPLAYCOUNT " + count);
            if (response.Contains("Failed to update display count", StringComparison.OrdinalIgnoreCase) || response.Contains("Unknown command", StringComparison.OrdinalIgnoreCase)) throw new IOException(response);
            await Task.Delay(1200);
            int result = SetDisplayConfig(0, IntPtr.Zero, 0, IntPtr.Zero, 0x84);
            if (result != 0) throw new IOException("Driver updated, but Windows could not extend the desktop (" + result + "). Choose Extend in Windows Display Settings.");
            for (int attempt = 0; attempt < 20 && Capture.Displays().Count(d => d.IsVirtual) != count; attempt++) await Task.Delay(300);
            if (Capture.Displays().Count(d => d.IsVirtual) != count) throw new IOException("Driver responded, but Windows has not exposed the requested desktops. Check Windows Display Settings and the driver status.");
            return "Virtual desktops requested: " + count + ". Windows is in Extend mode.";
        }
        catch (Exception ex) { throw new IOException(ex is OperationCanceledException ? "Virtual display driver did not respond. Use Set up extra displays on the PC first." : ex.Message, ex); }
    }
}
