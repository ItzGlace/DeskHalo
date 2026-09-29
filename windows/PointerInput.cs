using System;
using System.Linq;
using System.Runtime.InteropServices;
namespace DeskHalo_Host;
public static class PointerInput {
 [StructLayout(LayoutKind.Sequential)] struct Mouse {public int X,Y;public uint Data,Flags,Time;public UIntPtr Extra;}
 [StructLayout(LayoutKind.Sequential)] struct Input {public uint Type;public Mouse Mouse;}
 [DllImport("user32.dll",SetLastError=true)] static extern uint SendInput(uint count,Input[] inputs,int size);
 private static readonly object gate=new();private static bool held;private static System.Threading.Timer? release;
 public static void Apply(int id,float u,float v,bool down) {lock(gate){
  var displays=Capture.Displays();if(id<0||id>=displays.Length)throw new ArgumentException("Display unavailable");var d=displays[id];
  float scaledAspect=(float)d.Width/d.Height;float viewAspect=16f/9;float imageWidth=Math.Min(1,scaledAspect/viewAspect),imageHeight=Math.Min(1,viewAspect/scaledAspect);
  float x=Math.Clamp((u-(1-imageWidth)/2)/imageWidth,0,1),y=Math.Clamp((v-(1-imageHeight)/2)/imageHeight,0,1);
  int left=displays.Min(a=>a.X),top=displays.Min(a=>a.Y),right=displays.Max(a=>a.X+a.Width),bottom=displays.Max(a=>a.Y+a.Height);
  uint flags=0x8001|0x4000;if(down!=held)flags|=down?2u:4u;
  var input=new Input{Mouse=new Mouse{X=(int)((d.X+x*(d.Width-1)-left)*65535/Math.Max(1,right-left-1)),Y=(int)((d.Y+y*(d.Height-1)-top)*65535/Math.Max(1,bottom-top-1)),Flags=flags}};
  if(SendInput(1,new[]{input},Marshal.SizeOf<Input>())!=1)throw new System.ComponentModel.Win32Exception(Marshal.GetLastWin32Error());held=down;
  release??=new System.Threading.Timer(_=>Release());release.Change(600,System.Threading.Timeout.Infinite);
 }}
 public static void Release(){lock(gate){if(held){SendInput(1,new[]{new Input{Mouse=new Mouse{Flags=4}}},Marshal.SizeOf<Input>());held=false;}}}
}
