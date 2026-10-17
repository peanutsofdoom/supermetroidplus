#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Self-contained Linux-to-Steam-Deck sender, using Python's standard library."""
import argparse
import base64
import hashlib
import html
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import io
import ipaddress
import json
import os
from pathlib import Path, PurePosixPath
import secrets
import shutil
import subprocess
import sys
import tarfile
import threading
import time
import zipfile

ROM_HASH = '12b77c4bc9c1832cee8881244659065ee1d84c70c3d29e6eaf92e6798cc2ca72'
LIMIT = 250_000_000


def sha(data):
    return hashlib.sha256(data).hexdigest()


def asset(name):
    # __file__ lives inside the executable .pyz; no extraction/dependencies.
    with zipfile.ZipFile(sys.argv[0]) as archive:
        return archive.read(name)


def rom_bytes(path):
    if path.stat().st_size not in (3145728, 3146240):
        raise ValueError('Expected an original Super Metroid (Japan, USA) ROM, 3 MiB.')
    data = path.read_bytes()
    if len(data) == 3146240:
        data = data[512:]
    if sha(data) != ROM_HASH:
        raise ValueError('That ROM does not match this release. Your original file was not changed.')
    return data


class UI:
    def __init__(self, terminal=False):
        self.gui = not terminal and bool(shutil.which('zenity')) and bool(os.environ.get('DISPLAY') or os.environ.get('WAYLAND_DISPLAY'))
        self.prompts = set()
        self.prompt_lock = threading.Lock()

    def close_prompts(self):
        with self.prompt_lock:
            for process in self.prompts:
                if process.poll() is None:
                    process.terminate()

    def choose_rom(self):
        data_home = Path(os.environ.get('XDG_DATA_HOME', str(Path.home() / '.local/share')))
        choices = [data_home / 'SuperMetroidPlus/games/Super Metroid.sfc', Path.cwd() / 'games/Super Metroid.sfc']
        for path in choices:
            if path.is_file():
                try:
                    data = rom_bytes(path)
                    print('Using your already imported Super Metroid ROM. The source stays unchanged.', flush=True)
                    return data
                except ValueError:
                    pass
        if self.gui:
            result = subprocess.run(['zenity', '--file-selection', '--title=Choose your original Super Metroid ROM', '--file-filter=SNES ROM | *.sfc *.smc', '--file-filter=All files | *'], capture_output=True, text=True)
            if result.returncode:
                raise InterruptedError('ROM selection cancelled.')
            path = Path(result.stdout.strip())
        else:
            path = Path(input('Path to your Super Metroid ROM: ').strip()).expanduser()
        return rom_bytes(path)

    def choose_address(self):
        rows = json.loads(subprocess.check_output(['ip', '-j', '-4', 'addr', 'show', 'up'], text=True))
        addresses = []
        for interface in rows:
            if interface['ifname'].startswith(('lo', 'docker', 'veth', 'br-', 'virbr', 'tailscale', 'tun', 'wg')):
                continue
            for entry in interface.get('addr_info', []):
                value = ipaddress.IPv4Address(entry['local'])
                if not value.is_loopback and not value.is_link_local:
                    addresses.append((str(value), interface['ifname']))
        if not addresses:
            raise RuntimeError('No home-network IPv4 address found. Connect to Wi-Fi/Ethernet, or run with --bind YOUR_PC_IP.')
        if len(addresses) == 1:
            return addresses[0][0]
        if self.gui:
            args = ['zenity', '--list', '--title=Choose the network shared with your Deck', '--text=Select the PC Wi-Fi/Ethernet connection on the same network as the Deck.', '--column=Address', '--column=Connection']
            for address, interface in addresses:
                args += [address, interface]
            result = subprocess.run(args, capture_output=True, text=True)
            selected = result.stdout.strip()
            if result.returncode or selected not in [p[0] for p in addresses]:
                raise InterruptedError('Network selection cancelled.')
            return selected
        for number, (address, interface) in enumerate(addresses, 1):
            print(str(number) + ': ' + interface + ' — ' + address)
        return addresses[int(input('Connection number: ')) - 1][0]

    def approve(self, address):
        message = ('A device at ' + address + ' requested the Deck installer.\n\n'
                   'Did you just click Download on the Steam Deck? Allow this device to receive the game and your ROM?')
        if self.gui:
            process = subprocess.Popen(['zenity', '--question', '--no-markup', '--title=Allow Steam Deck transfer?', '--width=520', '--text=' + message, '--ok-label=Allow transfer', '--cancel-label=Reject'])
            with self.prompt_lock:
                self.prompts.add(process)
            try:
                return process.wait() == 0
            finally:
                with self.prompt_lock:
                    self.prompts.discard(process)
        return input(message + '\nType yes to allow: ').strip().lower() == 'yes'

    def message(self, text, error=False):
        print(text, flush=True)
        if self.gui:
            subprocess.run(['zenity', '--error' if error else '--info', '--no-markup', '--title=Super Metroid Plus to Steam Deck', '--width=560', '--text=' + text])


def make_payload(friends_zip, rom):
    """Only manifest-listed release files and the validated ROM are transferred."""
    output = io.BytesIO()
    with zipfile.ZipFile(io.BytesIO(friends_zip)) as source, zipfile.ZipFile(output, 'w', zipfile.ZIP_DEFLATED) as dest:
        manifests = [n for n in source.namelist() if n.count('/') == 1 and n.endswith('/PACKAGE-MANIFEST.json')]
        if len(manifests) != 1:
            raise ValueError('Invalid embedded release manifest.')
        manifest_name = manifests[0]
        prefix = manifest_name.split('/')[0] + '/'
        manifest_data = source.read(manifest_name)
        manifest = json.loads(manifest_data)['files']
        total = 0
        for name, expected in manifest.items():
            path = PurePosixPath(name)
            if path.is_absolute() or '..' in path.parts or '\\' in name or ':' in name or path.parts[0] in {'saves', 'states', 'logs', 'reports', 'screenshots', 'patches', 'cheats'}:
                raise ValueError('Unsafe embedded payload path: ' + name)
            total += source.getinfo(prefix + name).file_size
            if total > LIMIT:
                raise ValueError('Embedded release exceeds size limit.')
            data = source.read(prefix + name)
            if sha(data) != expected:
                raise ValueError('Embedded release file changed: ' + name)
            dest.writestr('Super Metroid Plus/' + name, data)
        if sha(rom) != ROM_HASH:
            raise ValueError('ROM failed validation.')
        dest.writestr('Super Metroid Plus/PACKAGE-MANIFEST.json', manifest_data)
        dest.writestr('Super Metroid Plus/games/Super Metroid.sfc', rom)
    return output.getvalue()


def installer_archive(base, payload_hash, bootstrap):
    script = bootstrap.replace('__PAYLOAD_URL__', base + '/deck.zip').replace('__PAYLOAD_SHA__', payload_hash).replace('__STATUS_URL__', base + '/status')
    encoded = base64.b64encode(script.encode()).decode()
    desktop = ('[Desktop Entry]\nType=Application\nName=Install Super Metroid Plus\n'
               'Comment=Install the verified game package from your computer\n'
               'Exec=python3 -c "import base64;exec(base64.b64decode(\'' + encoded + '\'))"\n'
               'Icon=applications-games\nTerminal=false\n')
    output = io.BytesIO()
    with tarfile.open(fileobj=output, mode='w:gz') as archive:
        for name, data, mode in [('Install Super Metroid Plus.desktop', desktop, 0o755),
                                 ('READ ME.txt', 'Open Install Super Metroid Plus. Choose Run/Execute/Trust if SteamOS asks. Keep the PC sender open until finished.\n', 0o644)]:
            content = data.encode()
            entry = tarfile.TarInfo('Super Metroid Plus Setup/' + name)
            entry.size, entry.mode = len(content), mode
            archive.addfile(entry, io.BytesIO(content))
    return output.getvalue()


class TransferServer(ThreadingHTTPServer):
    daemon_threads = True
    allow_reuse_address = True

    def __init__(self, address, payload, bootstrap, approve):
        super().__init__(address, TransferHandler)
        self.payload = payload
        self.token = secrets.token_hex(24)
        self.approve = approve
        self.approved = set()
        self.approval_lock = threading.Lock()
        self.status = None
        self.status_lock = threading.Lock()
        self.completed = threading.Event()
        self.expires = time.monotonic() + 1800
        self.url = 'http://' + self.server_address[0] + ':' + str(self.server_address[1])
        self.archive = installer_archive(self.url + '/' + self.token, sha(payload), bootstrap)


class TransferHandler(BaseHTTPRequestHandler):
    def log_message(self, *_):
        pass

    def setup(self):
        super().setup()
        self.connection.settimeout(30)

    def reply(self, code, data, kind='text/html; charset=utf-8', download=None):
        self.send_response(code)
        self.send_header('Content-Type', kind)
        self.send_header('Content-Length', str(len(data)))
        self.send_header('Cache-Control', 'no-store')
        self.send_header('X-Content-Type-Options', 'nosniff')
        if download:
            self.send_header('Content-Disposition', 'attachment; filename="' + download + '"')
        self.end_headers()
        try:
            self.wfile.write(data)
        except (BrokenPipeError, ConnectionResetError):
            pass

    def authorized(self):
        return self.client_address[0] in self.server.approved and time.monotonic() < self.server.expires

    def do_GET(self):
        server = self.server
        if time.monotonic() >= server.expires:
            return self.reply(410, b'Transfer expired. Restart the PC sender.')
        if self.path == '/':
            page = '''<!doctype html><meta charset="utf-8"><meta name="viewport" content="width=device-width"><title>Super Metroid Plus for Steam Deck</title>
<style>body{font:21px system-ui;max-width:750px;margin:30px auto;padding:20px;background:#111827;color:white}a{display:inline-block;padding:18px;border-radius:12px;background:#6ee7b7;color:#102030;font-weight:bold}li{margin:20px 0}</style>
<h1>Install on your Steam Deck</h1><p>Keep the sender running on your computer.</p>
<a href="/installer.tar.gz">Download installer</a>
<ol><li>After clicking Download, click <b>Allow transfer</b> on your computer.</li>
<li>On the Deck, open the downloaded archive and choose <b>Extract</b> (or right-click it in Downloads and choose <b>Extract here</b>).</li>
<li>Open the extracted <b>Super Metroid Plus Setup</b> folder and open <b>Install Super Metroid Plus</b>. Choose Run, Execute or Trust if asked.</li>
<li>Follow the dialogs. Exit Steam when asked. Return to Gaming Mode after setup.</li></ol>
<p>The archive carries the executable permission, so a normal extraction does not require the Properties checkbox. If your archive app removes permissions, use Properties → Permissions → Is executable once.</p>
<p>The game uses the tested Windows/Proton build on the Deck. Steam may need to download Proton 10.0. Existing Deck saves/settings are kept.</p>'''
            return self.reply(200, page.encode())
        if self.path == '/installer.tar.gz':
            with server.approval_lock:
                if self.client_address[0] not in server.approved:
                    if not server.approve(self.client_address[0]):
                        return self.reply(403, b'Transfer was not approved on the computer. Click Download again to retry.')
                    server.approved.add(self.client_address[0])
            return self.reply(200, server.archive, 'application/gzip', 'Super-Metroid-Plus-Deck-Setup.tar.gz')
        if self.path == '/' + server.token + '/deck.zip' and self.authorized():
            return self.reply(200, server.payload, 'application/zip', 'deck.zip')
        return self.reply(404, b'Not found')

    def do_POST(self):
        if self.path != '/' + self.server.token + '/status' or not self.authorized():
            return self.reply(404, b'Not found')
        try:
            size = int(self.headers.get('Content-Length', '0'))
            if not 0 < size <= 2048:
                raise ValueError()
            value = json.loads(self.rfile.read(size))
            if not isinstance(value, dict) or value.get('state') not in ['installed', 'cancelled', 'failed']:
                raise ValueError()
        except (ValueError, TypeError):
            return self.reply(400, b'Invalid status')
        with self.server.status_lock:
            self.server.status = value['state']
        self.reply(200, b'OK', 'text/plain')
        if value['state'] == 'installed':
            self.server.completed.set()


def main():
    parser = argparse.ArgumentParser(description='Send Super Metroid Plus from Linux to Steam Deck. No SSH or administrator password needed.')
    parser.add_argument('--rom', type=Path)
    parser.add_argument('--bind', help='IPv4 address of the PC connection shared with the Deck')
    parser.add_argument('--port', type=int, default=18749)
    parser.add_argument('--terminal', action='store_true', help='Use terminal prompts instead of desktop dialogs')
    args = parser.parse_args()
    ui = UI(args.terminal)
    server = None
    instructions = None
    try:
        rom = rom_bytes(args.rom.expanduser()) if args.rom else ui.choose_rom()
        address = str(ipaddress.IPv4Address(args.bind or ui.choose_address()))
        print('Preparing the verified Deck package…', flush=True)
        package = asset('friends.zip')
        metadata = json.loads(asset('SENDER.json'))
        if sha(package) != metadata['friends_sha256']:
            raise ValueError('The embedded game package is damaged. Download the sender again.')
        payload = make_payload(package, rom)
        server = TransferServer((address, args.port), payload, asset('sender_bootstrap.py').decode(), ui.approve)
        worker = threading.Thread(target=server.serve_forever, daemon=True)
        worker.start()
        message = ('On the Deck, switch to Desktop Mode and enter this in a browser address bar:\n\n' + server.url + '\n\n'
                   'Both devices must be on the same home network.\n'
                   'Click Download on the Deck, then Allow transfer on this computer.\n'
                   'Extract the download and open its installer. No terminal commands are needed on the Deck.\n\n'
                   'Keep this window open until setup finishes. Sharing stops automatically after 30 minutes. Close this window to stop early.')
        print(message, flush=True)
        if ui.gui:
            instructions = subprocess.Popen(['zenity', '--info', '--no-markup', '--title=Send to Steam Deck — keep open', '--width=620', '--text=' + message, '--ok-label=Stop sharing'])
        while not server.completed.wait(.25) and time.monotonic() < server.expires:
            if instructions and instructions.poll() is not None:
                break
        completed = server.completed.is_set()
        ui.close_prompts()
        server.shutdown()
        server.server_close()
        server = None
        if completed:
            if instructions and instructions.poll() is None:
                instructions.terminate()
                instructions.wait()
            ui.message('The Deck installer reported success. Sharing has stopped. Return to Gaming Mode on the Deck and open Super Metroid Plus.')
        else:
            print('Sharing stopped. Run this sender again if you need another transfer.', flush=True)
    except (KeyboardInterrupt, InterruptedError):
        print('Transfer cancelled.', flush=True)
    except Exception as error:
        ui.message(str(error), error=True)
        return 1
    finally:
        ui.close_prompts()
        if server:
            server.shutdown()
            server.server_close()
        if instructions and instructions.poll() is None:
            instructions.terminate()
            instructions.wait()
    return 0


if __name__ == '__main__':
    sys.exit(main())
