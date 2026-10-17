# SPDX-License-Identifier: GPL-3.0-or-later
param([string]$BindAddress, [int]$Port=18749, [switch]$TestMode)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$temporary=Join-Path ([IO.Path]::GetTempPath()) ('smp-deck-'+[Guid]::NewGuid().ToString('N'))
$listener=$null
function Hash-Bytes([byte[]]$Bytes) {
    $algorithm=[Security.Cryptography.SHA256]::Create()
    try { return ([BitConverter]::ToString($algorithm.ComputeHash($Bytes))).Replace('-','').ToLowerInvariant() }
    finally { $algorithm.Dispose() }
}
try {
    Write-Host 'Super Metroid Plus - send to Steam Deck'
    Write-Host 'Connect the PC and Deck to the same home network. No SSH or Deck password needed.'
    $rom=Join-Path $root 'games/Super Metroid.sfc'
    if(-not (Test-Path -LiteralPath $rom)) {
        & powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'Import-ROM.ps1')
        if($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $rom)){throw 'ROM import was cancelled or failed.'}
    }
    if((Hash-Bytes ([IO.File]::ReadAllBytes($rom))) -ne '12b77c4bc9c1832cee8881244659065ee1d84c70c3d29e6eaf92e6798cc2ca72'){throw 'Run Import ROM.cmd with the matching unmodified ROM first.'}
    if(-not $BindAddress) {
        $addresses=@(Get-NetIPAddress -AddressFamily IPv4 | Where-Object {
            $_.AddressState -eq 'Preferred' -and $_.IPAddress -ne '127.0.0.1' -and $_.IPAddress -notlike '169.254.*'
        })
        if($addresses.Count -eq 0){throw 'No connected IPv4 network was found.'}
        if($addresses.Count -eq 1){$BindAddress=$addresses[0].IPAddress}
        else {
            Write-Host 'Choose the Wi-Fi or Ethernet connection shared with your Deck:'
            for($index=0;$index -lt $addresses.Count;$index++){Write-Host ("{0}: {1} ({2})" -f ($index+1),$addresses[$index].IPAddress,$addresses[$index].InterfaceAlias)}
            $choice=0
            if(-not [int]::TryParse((Read-Host 'Connection number'),[ref]$choice) -or $choice -lt 1 -or $choice -gt $addresses.Count){throw 'No valid connection selected.'}
            $BindAddress=$addresses[$choice-1].IPAddress
        }
    }
    $address=[Net.IPAddress]::Parse($BindAddress)
    if($address.AddressFamily -ne [Net.Sockets.AddressFamily]::InterNetwork){throw 'Select an IPv4 address.'}
    if($TestMode -and $BindAddress -ne '127.0.0.1'){throw 'Test mode is restricted to loopback.'}
    New-Item -ItemType Directory -Path $temporary | Out-Null
    Add-Type -AssemblyName System.IO.Compression
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $zipPath=Join-Path $temporary 'deck.zip'
    $zip=[IO.Compression.ZipFile]::Open($zipPath,[IO.Compression.ZipArchiveMode]::Create)
    try {
        $manifest=Get-Content -LiteralPath (Join-Path $root 'PACKAGE-MANIFEST.json') -Raw | ConvertFrom-Json
        foreach($property in $manifest.files.PSObject.Properties) {
            $rel=$property.Name
            if($rel -match '(^/|\\|:|(^|/)\.\.(/|$)|^(saves|states|screenshots|logs|reports|cheats|patches)/)'){throw "Unsafe package path: $rel"}
            $file=Join-Path $root $rel
            if((Hash-Bytes ([IO.File]::ReadAllBytes($file))) -ne $property.Value){throw "Package file changed: $rel. Extract a fresh friends ZIP and retry."}
            [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip,$file,('Super Metroid Plus/'+$rel),[IO.Compression.CompressionLevel]::Optimal) | Out-Null
        }
        foreach($rel in @('PACKAGE-MANIFEST.json','games/Super Metroid.sfc')) {
            [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip,(Join-Path $root $rel),('Super Metroid Plus/'+$rel),[IO.Compression.CompressionLevel]::Optimal) | Out-Null
        }
    } finally {$zip.Dispose()}
    $payload=[IO.File]::ReadAllBytes($zipPath)
    $sha=Hash-Bytes $payload
    $token=[Guid]::NewGuid().ToString('N').Substring(0,12)
    $base="http://${BindAddress}:${Port}/$token"
    $bootstrap=[IO.File]::ReadAllText((Join-Path $PSScriptRoot 'deck_bootstrap.py')).Replace('__PAYLOAD_URL__',"$base/deck.zip").Replace('__PAYLOAD_SHA__',$sha)
    $encoded=[Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes($bootstrap))
    $desktop="[Desktop Entry]`nType=Application`nName=Install Super Metroid Plus`nComment=Download this PC's verified game package and set up Steam`nExec=python3 -c `"import base64;exec(base64.b64decode('$encoded'))`"`nIcon=applications-games`nTerminal=false`n"
    $html=@"
<!doctype html><meta charset="utf-8"><meta name="viewport" content="width=device-width"><title>Install Super Metroid Plus</title>
<style>body{font:20px system-ui;max-width:760px;margin:40px auto;padding:20px;background:#101824;color:#fff}a{display:inline-block;background:#6ee7b7;color:#101824;padding:16px;border-radius:12px}li{margin:18px 0}</style>
<h1>Install Super Metroid Plus</h1><p>Leave the PC transfer window open until setup finishes.</p>
<a href="$base/Install-Super-Metroid-Plus.desktop" download>Download Deck installer</a>
<ol><li>Open Downloads in the Deck's Dolphin file manager.</li><li>Right-click <b>Install-Super-Metroid-Plus.desktop</b>, choose Properties &rarr; Permissions, enable <b>Is executable</b>, then OK.</li><li>Open that file. Choose Execute or Continue if SteamOS asks whether to run it.</li><li>Follow the setup dialogs. Exit Steam when asked, then return to Gaming Mode after installation.</li></ol>
<p>Setup adds Super Metroid Plus and its Coordinates entry using Proton 10.0. Steam may download Proton on first launch. Your Deck's saves and settings are kept.</p>
<p>This transfer includes the ROM you selected on your PC, but no PC saves or account settings. The transfer ends when you close the PC helper. No SSH, administrator password or terminal typing is required on the Deck.</p>
"@
    $listener=[Net.Sockets.TcpListener]::new($address,$Port)
    $listener.Start()
    Write-Host "`nOn the Deck: switch to Desktop Mode, open a browser and type:`n`n$base`n" -ForegroundColor Green
    Write-Host 'If Windows asks, allow access on your private/home network.'
    Write-Host 'Keep this window open until setup finishes. Press Ctrl+C or close it to end sharing.'
    Write-Host 'Sharing ends automatically after 30 minutes. Only the setup page, installer and sealed game ZIP are served.'
    if($TestMode){[IO.File]::WriteAllText((Join-Path $root 'deck-transfer-test-url.txt'),$base)}
    $deadline=[DateTime]::UtcNow.AddMinutes(30)
    while([DateTime]::UtcNow -lt $deadline) {
        if(-not $listener.Pending()){Start-Sleep -Milliseconds 150;continue}
        $client=$listener.AcceptTcpClient()
        try {
            $client.ReceiveTimeout=5000;$client.SendTimeout=60000
            $stream=$client.GetStream()
            $request=New-Object Collections.Generic.List[byte]
            while($request.Count -lt 8192) {
                $next=$stream.ReadByte();if($next -lt 0){break};$request.Add([byte]$next)
                if($request.Count -ge 4 -and [Text.Encoding]::ASCII.GetString($request.ToArray(),$request.Count-4,4) -eq "`r`n`r`n"){break}
            }
            $line=([Text.Encoding]::ASCII.GetString($request.ToArray()) -split "`r`n")[0]
            $status='200 OK';$type='text/html; charset=utf-8';$extra=''
            switch -Exact ($line) {
                "GET /$token HTTP/1.1" {$body=[Text.Encoding]::UTF8.GetBytes($html)}
                "GET /$token/ HTTP/1.1" {$body=[Text.Encoding]::UTF8.GetBytes($html)}
                "GET /$token/Install-Super-Metroid-Plus.desktop HTTP/1.1" {$body=[Text.Encoding]::UTF8.GetBytes($desktop);$type='application/octet-stream';$extra="Content-Disposition: attachment; filename=Install-Super-Metroid-Plus.desktop`r`n"}
                "GET /$token/deck.zip HTTP/1.1" {$body=$payload;$type='application/zip'}
                default {$status='404 Not Found';$body=[Text.Encoding]::UTF8.GetBytes('Not found')}
            }
            $header=[Text.Encoding]::ASCII.GetBytes("HTTP/1.1 $status`r`nContent-Type: $type`r`nContent-Length: $($body.Length)`r`n${extra}Cache-Control: no-store`r`nConnection: close`r`n`r`n")
            $stream.Write($header,0,$header.Length);$stream.Write($body,0,$body.Length)
            if($TestMode -and $line -eq "GET /$token/test-stop HTTP/1.1"){$deadline=[DateTime]::MinValue}
        } catch {Write-Host 'A browser connection ended. You can retry the download.'}
        finally {$client.Dispose()}
    }
} catch {Write-Host $_ -ForegroundColor Red;exit 1}
finally {
    if($listener){$listener.Stop()}
    # Delete only the exact temporary folder created by this invocation.
    if(Test-Path -LiteralPath $temporary){
        $resolved=(Resolve-Path -LiteralPath $temporary).Path
        $tempRoot=[IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\')+'\'
        if($resolved.StartsWith($tempRoot,[StringComparison]::OrdinalIgnoreCase) -and (Split-Path -Leaf $resolved) -like 'smp-deck-*'){Remove-Item -LiteralPath $resolved -Recurse -Force}
    }
}
