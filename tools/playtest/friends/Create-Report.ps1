# SPDX-License-Identifier: GPL-3.0-or-later
param([switch]$NoOpen)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$reports=Join-Path $root 'reports'
New-Item -ItemType Directory -Path $reports -Force | Out-Null
$name='SMP-254-'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[Guid]::NewGuid().ToString('N').Substring(0,6)
$dest=Join-Path $reports $name
New-Item -ItemType Directory -Path $dest | Out-Null
$included=@();$skipped=@()
foreach($rel in @('BUILD.json','REPORT TEMPLATE.txt','logs/last-session.json','logs/launch-plan.json','logs/launcher-session.json')) {
 $source=Join-Path $root $rel
 if(-not (Test-Path -LiteralPath $source -PathType Leaf)){continue}
 $item=Get-Item -LiteralPath $source
 if(($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -or $item.Length -gt 2MB){$skipped+=$rel;continue}
 try {
  $text=[IO.File]::ReadAllText($source)
  foreach($local in @($root,$root.Replace('\','/'),$root.Replace('\','\\'),$env:USERPROFILE,$env:USERPROFILE.Replace('\','/'),$env:USERPROFILE.Replace('\','\\'))){if($local){$text=$text.Replace($local,'<local-folder>')}}
  $target=Join-Path $dest $rel
  New-Item -ItemType Directory -Path (Split-Path $target) -Force | Out-Null
  [IO.File]::WriteAllText($target,$text,(New-Object Text.UTF8Encoding($false)))
  $included+=$rel
 } catch {$skipped+=$rel}
}
@{build='253 normal / 254 Coordinates';os=[Environment]::OSVersion.VersionString;process_bits=([IntPtr]::Size*8);included=$included;skipped=$skipped;automatic_upload=$false;saves_included=$false;rom_included=$false} |
 ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $dest 'report-details.json') -Encoding UTF8
$zip=$dest+'.zip'
Add-Type -AssemblyName System.IO.Compression.FileSystem
[IO.Compression.ZipFile]::CreateFromDirectory($dest,$zip,[IO.Compression.CompressionLevel]::Optimal,$true)
Write-Output "Created: $zip"
Write-Output 'Attach this ZIP and a short clip or screenshot to your Discord issue thread. Paste the report template into the message.'
if(-not $NoOpen){Start-Process explorer.exe -ArgumentList @('/select,',('"'+$zip+'"'))}
