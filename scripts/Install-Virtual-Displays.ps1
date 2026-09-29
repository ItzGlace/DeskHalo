# Review before running. This script installs the separately downloaded MttVDD driver.
# It must run in an Administrator PowerShell. No signature enforcement is disabled.
[CmdletBinding()]
param([switch]$Install)
if ($Install) { Start-Transcript -Path (Join-Path $PSScriptRoot 'installation.log') -Append | Out-Null }
$ErrorActionPreference = 'Stop'
if (-not $Install) {
    Write-Host 'Planned changes: install the verified SignPath-signed MttVDD package; trust its signer in LocalMachine TrustedPublisher; create one root display device; copy its configuration to C:\VirtualDisplayDriver if no configuration exists.'
    Write-Host 'Run this file with -Install from an Administrator PowerShell to proceed.'
    exit
}
$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
if (-not ([Security.Principal.WindowsPrincipal]$identity).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) { throw 'Open PowerShell as administrator first.' }
$package = Join-Path $PSScriptRoot 'VirtualDisplayDriver'
$expected = @{
    'mttvdd.cat'='08A0093FC9B2E32B287A6F8A77CA4DE0A31830D29FC33D2B13A918DC859468F6'
    'MttVDD.dll'='C9CA837F57A98FBD43BC416A7F535A95843626E7759EAF85CF0CD7CE334DBB05'
    'MttVDD.inf'='550D211FE481E74DFE3F9D724ED78BE48B3A9113405965D683D9373E8D672F5D'
    'vdd_settings.xml'='EDB2501D6D5DA17F66D15D4B97A6F4A3F0D8963165AC4A6A6259D95118288020'
}
foreach ($name in $expected.Keys) { if ((Get-FileHash (Join-Path $package $name) -Algorithm SHA256).Hash -ne $expected[$name]) { throw "Hash mismatch: $name" } }
$signature = Get-AuthenticodeSignature (Join-Path $package 'mttvdd.cat')
if ($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Subject -notlike '*SignPath Foundation*') { throw 'Driver catalog signature is not the expected valid signature.' }
$certStore = [Security.Cryptography.X509Certificates.X509Store]::new('TrustedPublisher','LocalMachine')
try { $certStore.Open('ReadWrite'); $certStore.Add($signature.SignerCertificate) } finally { $certStore.Close() }
New-Item -ItemType Directory -Path 'C:\VirtualDisplayDriver' -Force | Out-Null
if (-not (Test-Path -LiteralPath 'C:\VirtualDisplayDriver\vdd_settings.xml')) { Copy-Item -LiteralPath (Join-Path $package 'vdd_settings.xml') -Destination 'C:\VirtualDisplayDriver\vdd_settings.xml' }
[xml]$configuration=Get-Content -LiteralPath 'C:\VirtualDisplayDriver\vdd_settings.xml'
Copy-Item -LiteralPath 'C:\VirtualDisplayDriver\vdd_settings.xml' -Destination ('C:\VirtualDisplayDriver\vdd_settings.xml.backup-' + (Get-Date -Format yyyyMMddHHmmss))
$configuration.vdd_settings.options.HardwareCursor='false'
$configuration.vdd_settings.options.debuglogging='false'
if (-not ($configuration.vdd_settings.resolutions.resolution | Where-Object { $_.width -eq '7680' })) {
 $mode=$configuration.CreateElement('resolution')
 foreach($pair in @(@('width','7680'),@('height','4320'),@('refresh_rate','30'))) { $node=$configuration.CreateElement($pair[0]);$node.InnerText=$pair[1];[void]$mode.AppendChild($node) }
 [void]$configuration.vdd_settings.resolutions.AppendChild($mode)
}
$configuration.Save('C:\VirtualDisplayDriver\vdd_settings.xml')
Add-Type -TypeDefinition @'
using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Text;
public static class DeskHaloDriverSetup {
 [StructLayout(LayoutKind.Sequential)] struct DeviceInfo { public uint Size; public Guid ClassGuid; public uint DevInst; public IntPtr Reserved; }
 [DllImport("setupapi.dll",SetLastError=true)] static extern IntPtr SetupDiCreateDeviceInfoList(ref Guid cls,IntPtr parent);
 [DllImport("setupapi.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool SetupDiCreateDeviceInfo(IntPtr set,string name,ref Guid cls,string description,IntPtr parent,uint flags,ref DeviceInfo info);
 [DllImport("setupapi.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool SetupDiSetDeviceRegistryProperty(IntPtr set,ref DeviceInfo info,uint property,byte[] bytes,uint length);
 [DllImport("setupapi.dll",SetLastError=true)] static extern bool SetupDiCallClassInstaller(uint installFunction,IntPtr set,ref DeviceInfo info);
 [DllImport("setupapi.dll")] static extern bool SetupDiDestroyDeviceInfoList(IntPtr set);
 [DllImport("newdev.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool UpdateDriverForPlugAndPlayDevices(IntPtr parent,string hardwareId,string inf,uint flags,out bool reboot);
 static void Check(bool ok) { if(!ok) throw new Win32Exception(Marshal.GetLastWin32Error()); }
 public static bool Install(string inf,bool create) {
  Guid cls=new Guid("4d36e968-e325-11ce-bfc1-08002be10318"); IntPtr set=IntPtr.Zero; var info=new DeviceInfo(); bool registered=false;
  try {
   if(create) { set=SetupDiCreateDeviceInfoList(ref cls,IntPtr.Zero); if(set==new IntPtr(-1))throw new Win32Exception(Marshal.GetLastWin32Error());
    info.Size=(uint)Marshal.SizeOf(typeof(DeviceInfo)); Check(SetupDiCreateDeviceInfo(set,"MttVDD",ref cls,"Virtual Display Driver",IntPtr.Zero,1,ref info));
    byte[] ids=Encoding.Unicode.GetBytes("Root\\MttVDD\0\0"); Check(SetupDiSetDeviceRegistryProperty(set,ref info,1,ids,(uint)ids.Length));
    Check(SetupDiCallClassInstaller(0x19,set,ref info)); registered=true;
   }
   bool reboot; Check(UpdateDriverForPlugAndPlayDevices(IntPtr.Zero,"Root\\MttVDD",inf,1,out reboot)); return reboot;
  } catch { if(registered)SetupDiCallClassInstaller(5,set,ref info);throw; }
  finally { if(set!=IntPtr.Zero&&set!=new IntPtr(-1))SetupDiDestroyDeviceInfoList(set); }
 }
}
'@
$existing = Get-PnpDevice -Class Display -ErrorAction SilentlyContinue | Where-Object { $_.FriendlyName -eq 'Virtual Display Driver' }
$reboot = [DeskHaloDriverSetup]::Install((Join-Path $package 'MttVDD.inf'), -not [bool]$existing)
Write-Host "Driver installed. Restart required: $reboot. Restart DeskHalo Host, then add a desktop."
Stop-Transcript | Out-Null
