using System;
using System.Linq;
using System.Runtime.InteropServices;
namespace DeskHalo_Host;
public static class DisplayModes {
 [StructLayout(LayoutKind.Sequential,CharSet=CharSet.Unicode)] struct Mode {
  [MarshalAs(UnmanagedType.ByValTStr,SizeConst=32)] public string Name;
  public ushort Spec,Driver,Size,Extra; public uint Fields; public int X,Y; public uint Orientation,FixedOutput;
  public short Color,Duplex,YResolution,TTOption,Collate;
  [MarshalAs(UnmanagedType.ByValTStr,SizeConst=32)] public string Form;
  public ushort LogPixels; public uint Bits,Width,Height,Flags,Frequency,IcmMethod,IcmIntent,Media,Dither,Reserved1,Reserved2,PanningWidth,PanningHeight;
 }
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern bool EnumDisplaySettings(string name,int index,ref Mode mode);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern int ChangeDisplaySettingsEx(string? name,ref Mode mode,IntPtr hwnd,uint flags,IntPtr param);
 public static void DetachAll(){foreach(var d in Capture.Displays().Where(d=>d.IsVirtual)){var mode=new Mode{Size=(ushort)Marshal.SizeOf<Mode>()};if(!EnumDisplaySettings(d.Name,-1,ref mode))continue;mode.Width=mode.Height=0;mode.Fields=0x180020;int result=ChangeDisplaySettingsEx(d.Name,ref mode,IntPtr.Zero,1,IntPtr.Zero);if(result!=0)throw new InvalidOperationException("Could not detach virtual desktop: "+result);}}
 public static string Apply(int id,int width,int height,int rate) {
  var screens=Capture.Displays();if(id<0||id>=screens.Length||!screens[id].IsVirtual)throw new ArgumentException("Select an extended virtual desktop first. Physical monitor modes are not changed.");
  if(!new[]{(2560,1440),(3840,2160),(7680,4320),(1920,1080),(1280,720)}.Contains((width,height))||!new[]{30,60,90,120}.Contains(rate))throw new ArgumentException("Unsupported desktop mode");
  var d=screens[id];var selected=new Mode();bool found=false;
  for(int i=0;i<2048;i++){var mode=new Mode{Size=(ushort)Marshal.SizeOf<Mode>()};if(!EnumDisplaySettings(d.Name,i,ref mode))break;if(mode.Width==width&&mode.Height==height&&mode.Frequency==rate){selected=mode;found=true;break;}}
  if(!found)throw new InvalidOperationException("Driver does not advertise this desktop mode. Run Finish Display Setup for 8K, or choose another rate.");
  selected.X=d.X;selected.Y=d.Y;selected.Fields=0x5c0020;
  int test=ChangeDisplaySettingsEx(d.Name,ref selected,IntPtr.Zero,2,IntPtr.Zero);if(test!=0)throw new InvalidOperationException("Windows rejected mode test: "+test);
  int result=ChangeDisplaySettingsEx(d.Name,ref selected,IntPtr.Zero,1,IntPtr.Zero);if(result!=0)throw new InvalidOperationException("Windows could not apply mode: "+result);
  return $"Desktop {id+1}: {width}×{height} at {rate} Hz";
 }
}
