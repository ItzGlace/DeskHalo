using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Imaging;
using System.Drawing.Drawing2D;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
namespace DeskHalo_Host;
public static class Capture
{
    public record Display(string Name, int X, int Y, int Width, int Height, bool IsVirtual);
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)] private struct DisplayDevice {
        public int Size;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)] public string Name;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 128)] public string Description;
        public uint Flags;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 128)] public string Id;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 128)] public string Key;
    }
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] private static extern bool EnumDisplayDevices(string? device, uint index, ref DisplayDevice output, uint flags);
    private static bool IsVirtual(string name) {
        for (uint i = 0; ; i++) {
            var device = new DisplayDevice { Size = Marshal.SizeOf<DisplayDevice>() };
            if (!EnumDisplayDevices(null, i, ref device, 0)) return false;
            if (device.Name == name) return device.Description.Contains("Virtual Display", StringComparison.OrdinalIgnoreCase) || device.Description.Contains("MttVDD", StringComparison.OrdinalIgnoreCase);
        }
    }
    [StructLayout(LayoutKind.Sequential)] private struct Rect { public int Left, Top, Right, Bottom; }
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)] private struct MonitorInfo
    { public int Size; public Rect Monitor, Work; public uint Flags; [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)] public string Device; }
    private delegate bool MonitorCallback(IntPtr monitor, IntPtr dc, ref Rect rect, IntPtr data);
    [DllImport("user32.dll")] private static extern bool EnumDisplayMonitors(IntPtr dc, IntPtr clip, MonitorCallback callback, IntPtr data);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] private static extern bool GetMonitorInfo(IntPtr monitor, ref MonitorInfo info);
    [DllImport("user32.dll")] private static extern short GetAsyncKeyState(int key);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] private static extern uint MapVirtualKey(uint code, uint mapType);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] private static extern int GetKeyNameText(int param, System.Text.StringBuilder buffer, int size);
    public static Display[] Displays()
    {
        var result = new List<Display>();
        EnumDisplayMonitors(IntPtr.Zero, IntPtr.Zero, (IntPtr handle, IntPtr dc, ref Rect rect, IntPtr data) => {
            var info = new MonitorInfo { Size = Marshal.SizeOf<MonitorInfo>(), Device = "" };
            if (GetMonitorInfo(handle, ref info)) result.Add(new Display(info.Device, rect.Left, rect.Top, rect.Right - rect.Left, rect.Bottom - rect.Top, IsVirtual(info.Device)));
            return true;
        }, IntPtr.Zero);
        return result.OrderBy(d => d.Name).ToArray();
    }
    public static int[] Keys() => Enumerable.Range(8, 247).Where(k => (GetAsyncKeyState(k) & 0x8000) != 0).ToArray();
    public static string KeyName(int key)
    {
        var text = new System.Text.StringBuilder(64); int code = (int)MapVirtualKey((uint)key, 0) << 16;
        if (key is >= 33 and <= 46 || key is 163 or 165) code |= 1 << 24;
        return GetKeyNameText(code, text, 64) > 0 ? text.ToString() : $"VK {key}";
    }
    public static byte[] Frame(int id, int width, int height)
    {
        var displays = Displays();
        if (id < 0 || id >= displays.Length) throw new ArgumentOutOfRangeException(nameof(id), "Display is no longer available");
        var d = displays[id];
        using var source = new Bitmap(d.Width, d.Height, PixelFormat.Format24bppRgb);
        using (var g = Graphics.FromImage(source)) g.CopyFromScreen(d.X, d.Y, 0, 0, source.Size, CopyPixelOperation.SourceCopy);
        using var frame = new Bitmap(width, height, PixelFormat.Format24bppRgb);
        using (var g = Graphics.FromImage(frame)) {
            g.Clear(Color.FromArgb(16, 17, 20)); g.InterpolationMode = InterpolationMode.HighQualityBilinear;
            float scale = Math.Min((float)width / d.Width, (float)height / d.Height);
            int w = (int)(d.Width * scale), h = (int)(d.Height * scale);
            g.DrawImage(source, (width - w) / 2, (height - h) / 2, w, h);
        }
        using var stream = new MemoryStream(); var jpeg = ImageCodecInfo.GetImageEncoders().First(x => x.FormatID == ImageFormat.Jpeg.Guid);
        using var parameters = new EncoderParameters(1); parameters.Param[0] = new EncoderParameter(System.Drawing.Imaging.Encoder.Quality, 75L);
        frame.Save(stream, jpeg, parameters); return stream.ToArray();
    }
}
