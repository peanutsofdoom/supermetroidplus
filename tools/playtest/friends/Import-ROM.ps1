# SPDX-License-Identifier: GPL-3.0-or-later
param([string]$Source)
$ErrorActionPreference='Stop'
try {
 if(-not $Source){
  Add-Type -AssemblyName System.Windows.Forms
  $dialog=New-Object Windows.Forms.OpenFileDialog
  $dialog.Title='Choose your unmodified Super Metroid ROM'
  $dialog.Filter='SNES ROM (*.sfc;*.smc)|*.sfc;*.smc|All files (*.*)|*.*'
  if($dialog.ShowDialog() -ne 'OK'){exit 0}
  $Source=$dialog.FileName
 }
 $data=[IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $Source).Path)
 if($data.Length -eq 3146240){$data=$data[512..($data.Length-1)]}
 $sha=[Security.Cryptography.SHA256]::Create()
 try {$hash=([BitConverter]::ToString($sha.ComputeHash($data))).Replace('-','').ToLowerInvariant()}finally{$sha.Dispose()}
 if($hash -ne '12b77c4bc9c1832cee8881244659065ee1d84c70c3d29e6eaf92e6798cc2ca72'){throw 'This ROM does not match this build. Use unmodified Super Metroid (Japan, USA), 3 MiB. Your file was not changed.'}
 $games=Join-Path (Split-Path -Parent $PSScriptRoot) 'games'
 New-Item -ItemType Directory -Path $games -Force | Out-Null
 $target=Join-Path $games 'Super Metroid.sfc'
 if(Test-Path -LiteralPath $target){
  $checker=[Security.Cryptography.SHA256]::Create()
  try {$existing=([BitConverter]::ToString($checker.ComputeHash([IO.File]::ReadAllBytes($target)))).Replace('-','').ToLowerInvariant()}finally{$checker.Dispose()}
  if($existing -eq $hash){Write-Output 'Matching ROM is already installed.';exit 0}
  throw 'A different ROM is already in games/Super Metroid.sfc. Move it aside before importing.'
 }
 [IO.File]::WriteAllBytes($target,$data)
 Write-Output 'ROM ready. Launch Play.exe or Play With Coordinates.exe.'
} catch {Write-Error $_;exit 1}
