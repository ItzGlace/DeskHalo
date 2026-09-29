using Microsoft.UI.Xaml;
using System;
using System.Linq;
using System.Threading.Tasks;
namespace DeskHalo_Host;
public sealed partial class MainWindow : Window
{
    private readonly HostServer server = new();
    private readonly DispatcherTimer timer = new() { Interval = TimeSpan.FromMilliseconds(100) };
    private int virtualCount;
    private readonly TrayIcon tray;
    private bool quitting;
    private void ShowHost(){AppWindow.Show();(AppWindow.Presenter as Microsoft.UI.Windowing.OverlappedPresenter)?.Maximize();Activate();}
    private async void QuitHost(){if(quitting)return;quitting=true;timer.Stop();tray.Dispose();await server.DisposeAsync();Close();}
    [System.Runtime.InteropServices.DllImport("user32.dll")]
    private static extern uint GetDpiForWindow(IntPtr window);
    public MainWindow()
    {
        InitializeComponent(); Title = "DeskHalo Host";AppWindow.SetIcon(System.IO.Path.Combine(AppContext.BaseDirectory,"Assets","deskhalo.ico"));
        // AppWindow sizes are physical pixels; respect scaling and the desktop work area.
        var area=Microsoft.UI.Windowing.DisplayArea.GetFromWindowId(AppWindow.Id,Microsoft.UI.Windowing.DisplayAreaFallback.Primary).WorkArea;
        uint dpi=GetDpiForWindow(WinRT.Interop.WindowNative.GetWindowHandle(this));
        double scale=Math.Max(1,dpi/96.0);
        AppWindow.Resize(new Windows.Graphics.SizeInt32(Math.Min(area.Width,(int)(1180*scale)),Math.Min(area.Height,(int)(980*scale))));
        (AppWindow.Presenter as Microsoft.UI.Windowing.OverlappedPresenter)?.Maximize();
        tray=new TrayIcon(WinRT.Interop.WindowNative.GetWindowHandle(this),ShowHost,QuitHost);
        AppWindow.Closing+=(_,e)=>{if(!quitting&&tray.Available){e.Cancel=true;AppWindow.Hide();}else if(!quitting){e.Cancel=true;QuitHost();}};
        CodeText.Text = server.PairingCode;
        AddressText.Text = "USB: 127.0.0.1:47654\n" + string.Join("\n", HostServer.LocalAddresses().Select(x => "Wi-Fi: " + x + ":47654"));
        timer.Tick += (_, _) => Refresh(); timer.Start();
        Closed += (_, _) => { timer.Stop(); tray.Dispose(); };
        _ = CheckDriver();
        string[] args = Environment.GetCommandLineArgs();
        int testArg = Array.IndexOf(args, "--test-session");
        if (testArg >= 0 && testArg + 1 < args.Length) _ = StartTestSession(args[testArg + 1]);
    }
    private void Refresh()
    {
        ConnectionText.Text = ConnectionTests.Result;
        virtualCount = Capture.Displays().Count(d => d.IsVirtual); DisplayCountText.Text = virtualCount.ToString();
        DisplaysText.Text = string.Join("\n", Capture.Displays().Select((d, i) => $"{i + 1:00}   {d.Name}   {d.Width} × {d.Height}"));
        var keys = server.Sharing ? Capture.Keys() : Array.Empty<int>();
        KeyText.Text = keys.Length == 0 ? "No keys pressed" : string.Join("  ", keys.Select(Capture.KeyName));
        StatusText.Text = server.Sharing ? $"Sharing • {server.LastStatus}" : "Ready • Sharing is off";
    }
    private async void ToggleSharing(object sender, RoutedEventArgs e)
    {
        ShareButton.IsEnabled = false;
        try { if (server.Sharing) await server.StopAsync(); else await server.StartAsync(); }
        catch (Exception ex) { StatusText.Text = ex.Message; }
        ShareButton.Content = server.Sharing ? "Stop sharing" : "Start sharing"; ShareButton.IsEnabled = true;
    }
    private async void ChooseTransfer(object sender,RoutedEventArgs e){try{FileTransfer.Prepare();var picker=new Windows.Storage.Pickers.FileOpenPicker();picker.FileTypeFilter.Add("*");WinRT.Interop.InitializeWithWindow.Initialize(picker,WinRT.Interop.WindowNative.GetWindowHandle(this));var file=await picker.PickSingleFileAsync();if(file!=null){var folder=await Windows.Storage.StorageFolder.GetFolderFromPathAsync(FileTransfer.Outbox);await file.CopyAsync(folder,file.Name,Windows.Storage.NameCollisionOption.GenerateUniqueName);DriverText.Text="File ready. Open Transfer → Files in Quest to download it.";}}catch(Exception ex){DriverText.Text=ex.Message;}}
    private void OpenTransfers(object sender,RoutedEventArgs e){FileTransfer.Prepare();System.Diagnostics.Process.Start(new System.Diagnostics.ProcessStartInfo(FileTransfer.Root){UseShellExecute=true});}
    private async void TestConnections(object sender, RoutedEventArgs e) { if(!server.Sharing){ConnectionText.Text="Start sharing first";return;} ConnectionText.Text=await ConnectionTests.ConfigureUsb();ConnectionTests.Start();}
    private void InstallDriver(object sender, RoutedEventArgs e)
    {
        try {
            string script = System.IO.Path.Combine(AppContext.BaseDirectory, "DriverSetup", "Install-Virtual-Displays.ps1");
            if (!System.IO.File.Exists(script)) {
                System.Diagnostics.Process.Start(new System.Diagnostics.ProcessStartInfo("https://github.com/VirtualDrivers/Virtual-Display-Driver/releases") { UseShellExecute = true });
                DriverText.Text = "Install the signed Virtual Display Driver from its official release, then add a desktop. See the DeskHalo guide.";
                return;
            }
            System.Diagnostics.Process.Start(new System.Diagnostics.ProcessStartInfo("powershell.exe", "-NoExit -NoProfile -ExecutionPolicy RemoteSigned -File \"" + script + "\" -Install") { UseShellExecute = true, Verb = "runas" });
            DriverText.Text = "Complete Windows' administrator prompt, then use + to add a desktop.";
        } catch (Exception ex) { DriverText.Text = ex.Message; }
    }
    private async Task CheckDriver() { DriverText.Text = await VirtualDisplays.CheckAsync(); }
    private async Task StartTestSession(string reportPath)
    {
        try {
            await server.StartAsync(); ConnectionTests.Start(); ShareButton.Content = "Stop sharing";
            await System.IO.File.WriteAllTextAsync(reportPath, System.Text.Json.JsonSerializer.Serialize(new { code = server.PairingCode, port = 47654, displays = Capture.Displays() }));
        } catch (Exception ex) { await System.IO.File.WriteAllTextAsync(reportPath, ex.ToString()); }
    }
    private async Task SetDisplays(int count)
    {
        try { DriverText.Text = await VirtualDisplays.SetCountAsync(count); virtualCount = count; DisplayCountText.Text = count.ToString(); }
        catch (Exception ex) { DriverText.Text = ex.Message; }
    }
    private async void AddDisplay(object sender, RoutedEventArgs e) => await SetDisplays(Math.Min(3, virtualCount + 1));
    private async void RemoveDisplay(object sender, RoutedEventArgs e) => await SetDisplays(Math.Max(0, virtualCount - 1));
}
