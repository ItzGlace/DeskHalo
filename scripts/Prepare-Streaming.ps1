[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$ArchivePath)
$ErrorActionPreference = 'Stop'
# FFmpeg LGPL build N-126947-g45f3fecca9-20260928, BtbN asset 595476193.
# Obtain the pinned archive described in docs/streaming.md. Never run an unverified replacement.
$expected = 'b12b2da1bf1d9495d6650f2621f54a7111aa82cb210544b5dba53746a88b8502'
if ((Get-FileHash -LiteralPath $ArchivePath -Algorithm SHA256).Hash.ToLowerInvariant() -ne $expected) { throw 'FFmpeg archive hash mismatch.' }
$repo = Split-Path $PSScriptRoot -Parent
$unpack = Join-Path $repo 'dependencies\ffmpeg'
Expand-Archive -LiteralPath $ArchivePath -DestinationPath $unpack -Force
$package = Join-Path $unpack 'ffmpeg-master-latest-win64-lgpl'
$target = Join-Path $repo 'windows\Streaming'
New-Item -ItemType Directory -Path $target -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $package 'bin\ffmpeg.exe') -Destination $target -Force
Copy-Item -LiteralPath (Join-Path $package 'LICENSE.txt') -Destination (Join-Path $target 'LICENSE-FFmpeg.txt') -Force
Write-Host 'Verified streaming runtime prepared. Rebuild the Windows app.'
