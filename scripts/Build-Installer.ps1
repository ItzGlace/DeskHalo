param(
    [Parameter(Mandatory=$true)][string]$PublishDir,
    [Parameter(Mandatory=$true)][string]$Output,
    [string]$Wix='wix'
)
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot
$PublishDir=(Resolve-Path -LiteralPath $PublishDir).Path
$Output=[IO.Path]::GetFullPath($Output)
$intermediate=Join-Path $repo 'artifacts\installer'
New-Item -ItemType Directory -Force $intermediate | Out-Null
function Xml([string]$value){[Security.SecurityElement]::Escape($value)}
function Id([string]$value){
    $hash=[Security.Cryptography.SHA256]::Create()
    try { 'F'+([BitConverter]::ToString($hash.ComputeHash([Text.Encoding]::UTF8.GetBytes($value))).Replace('-','').Substring(0,30)) }
    finally {$hash.Dispose()}
}
$directories=@{}
$components=[Collections.Generic.List[string]]::new()
$groups=[Collections.Generic.List[string]]::new()
$directories['']='INSTALLFOLDER'
$files=Get-ChildItem -LiteralPath $PublishDir -File -Recurse | Sort-Object FullName
foreach($file in $files){
    $relative=$file.FullName.Substring($PublishDir.Length+1)
    if($relative -match '(^DriverSetup\\|\.pdb$|\.log$|session.*\.json$|^Start-Test|^Verify-Test)'){continue}
    $folder=Split-Path $relative
    $parts=$folder.Split('\',[StringSplitOptions]::RemoveEmptyEntries)
    $current=''
    foreach($part in $parts){
        $parent=$directories[$current]
        $current=if($current){$current+'\'+$part}else{$part}
        if(-not $directories.ContainsKey($current)){
            $dirId='D'+(Id $current)
            $directories[$current]=$dirId
            $groups.Add('<DirectoryRef Id="'+$parent+'"><Directory Id="'+$dirId+'" Name="'+(Xml $part)+'" /></DirectoryRef>')
        }
    }
    $id=Id $relative
    $componentGuid=([guid]::new([Security.Cryptography.MD5]::HashData([Text.Encoding]::UTF8.GetBytes("DeskHalo:"+$relative)))).ToString()
    $dir=$directories[$folder]
    # HKCU key paths allow a clean per-user installation without administrator rights.
    $components.Add('<Component Id="C'+$id+'" Directory="'+$dir+'" Guid="'+$componentGuid+'"><File Id="'+$id+'" Source="$(var.PublishDir)\'+(Xml $relative)+'" /><RegistryValue Root="HKCU" Key="Software\DeskHalo\Installer\Files" Name="'+$id+'" Type="integer" Value="1" KeyPath="yes" /></Component>')
}
foreach($entry in $directories.GetEnumerator()){
    $id=Id ('remove-'+$entry.Key)
    $components.Add('<Component Id="C'+$id+'" Directory="'+$entry.Value+'" Guid="*"><RemoveFolder Id="'+$id+'" On="uninstall" /><RegistryValue Root="HKCU" Key="Software\DeskHalo\Installer\Folders" Name="'+$id+'" Type="integer" Value="1" KeyPath="yes" /></Component>')
}
$payload='<Wix xmlns="http://wixtoolset.org/schemas/v4/wxs"><Fragment>'+($groups -join "`n")+'<ComponentGroup Id="Payload">'+($components -join "`n")+'</ComponentGroup></Fragment></Wix>'
$payloadPath=Join-Path $intermediate 'Payload.wxs'
[IO.File]::WriteAllText($payloadPath,$payload)
& $Wix build (Join-Path $repo 'installer\Package.wxs') $payloadPath -arch x64 -d "PublishDir=$PublishDir" -o $Output
if($LASTEXITCODE -ne 0){throw 'MSI build failed'}
