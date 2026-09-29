using System;
using System.Runtime.InteropServices;

namespace DeskHalo_Host;

// A native notification icon uses the existing WinUI message loop.
internal sealed class TrayIcon : IDisposable
{
    private const uint Callback = 0x8001;
    private readonly IntPtr window;
    private readonly SubclassProc callback;
    private readonly Action show, quit;
    private readonly uint taskbarCreated = RegisterWindowMessage("TaskbarCreated");
    private NotifyData data;
    private bool disposed;
    public bool Available { get; private set; }
    public TrayIcon(IntPtr window, Action show, Action quit)
    {
        this.window=window; this.show=show; this.quit=quit; callback=WindowProc;
        data=new NotifyData { Size=(uint)Marshal.SizeOf<NotifyData>(), Window=window, Id=1,
            Flags=7, Message=Callback, Tip="DeskHalo — click to open; right-click to quit",
            Icon=LoadImage(IntPtr.Zero, System.IO.Path.Combine(AppContext.BaseDirectory,"Assets","deskhalo.ico"),1,32,32,0x10), Info="", InfoTitle="" };
        if(SetWindowSubclass(window,callback,1,0)) Available=Shell_NotifyIcon(0,ref data);
    }
    private IntPtr WindowProc(IntPtr h,uint msg,UIntPtr wp,IntPtr lp,UIntPtr id,UIntPtr reference)
    {
        if(msg==taskbarCreated) Available=Shell_NotifyIcon(0,ref data);
        if(msg==Callback)
        {
            uint action=(uint)(lp.ToInt64()&0xffff);
            if(action==0x202 || action==0x203) show();
            if(action==0x205 || action==0x7b)
            {
                GetCursorPos(out var point); SetForegroundWindow(window);
                IntPtr menu=CreatePopupMenu();
                try {
                    AppendMenu(menu,0,1,"Open DeskHalo"); AppendMenu(menu,0,2,"Quit DeskHalo");
                    uint command=TrackPopupMenu(menu,0x100|2,point.X,point.Y,0,window,IntPtr.Zero);
                    if(command==1)show(); else if(command==2)quit();
                } finally {DestroyMenu(menu);}
                PostMessage(window,0,UIntPtr.Zero,IntPtr.Zero);
            }
            return IntPtr.Zero;
        }
        return DefSubclassProc(h,msg,wp,lp);
    }
    public void Dispose(){if(disposed)return;disposed=true;Shell_NotifyIcon(2,ref data);RemoveWindowSubclass(window,callback,1);if(data.Icon!=IntPtr.Zero)DestroyIcon(data.Icon);Available=false;}
    [StructLayout(LayoutKind.Sequential,CharSet=CharSet.Unicode)]
    private struct NotifyData {
        public uint Size; public IntPtr Window; public uint Id,Flags,Message; public IntPtr Icon;
        [MarshalAs(UnmanagedType.ByValTStr,SizeConst=128)] public string Tip;
        public uint State,StateMask;
        [MarshalAs(UnmanagedType.ByValTStr,SizeConst=256)] public string Info;
        public uint Version;
        [MarshalAs(UnmanagedType.ByValTStr,SizeConst=64)] public string InfoTitle;
        public uint InfoFlags; public Guid Guid; public IntPtr BalloonIcon;
    }
    [StructLayout(LayoutKind.Sequential)] private struct Point {public int X,Y;}
    private delegate IntPtr SubclassProc(IntPtr h,uint msg,UIntPtr wp,IntPtr lp,UIntPtr id,UIntPtr reference);
    [DllImport("shell32.dll",CharSet=CharSet.Unicode)] private static extern bool Shell_NotifyIcon(uint message,ref NotifyData data);
    [DllImport("comctl32.dll")] private static extern bool SetWindowSubclass(IntPtr h,SubclassProc proc,UIntPtr id,UIntPtr reference);
    [DllImport("comctl32.dll")] private static extern bool RemoveWindowSubclass(IntPtr h,SubclassProc proc,UIntPtr id);
    [DllImport("comctl32.dll")] private static extern IntPtr DefSubclassProc(IntPtr h,uint msg,UIntPtr wp,IntPtr lp);
    [DllImport("user32.dll",CharSet=CharSet.Unicode)] private static extern uint RegisterWindowMessage(string text);
    [DllImport("user32.dll",CharSet=CharSet.Unicode)] private static extern IntPtr LoadImage(IntPtr instance,string name,uint type,int cx,int cy,uint flags);
    [DllImport("user32.dll")] private static extern bool DestroyIcon(IntPtr icon);
    [DllImport("user32.dll")] private static extern bool GetCursorPos(out Point point);
    [DllImport("user32.dll")] private static extern bool SetForegroundWindow(IntPtr window);
    [DllImport("user32.dll")] private static extern IntPtr CreatePopupMenu();
    [DllImport("user32.dll",CharSet=CharSet.Unicode)] private static extern bool AppendMenu(IntPtr menu,uint flags,UIntPtr id,string text);
    [DllImport("user32.dll")] private static extern uint TrackPopupMenu(IntPtr menu,uint flags,int x,int y,int reserved,IntPtr window,IntPtr rect);
    [DllImport("user32.dll")] private static extern bool DestroyMenu(IntPtr menu);
    [DllImport("user32.dll")] private static extern bool PostMessage(IntPtr window,uint message,UIntPtr wp,IntPtr lp);
}
