"""Exercise the shipped .pyz in Linux with a simulated Deck Steam home."""
import argparse
import base64
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tarfile
import tempfile
import time
import urllib.error
import urllib.request
import zipfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('sender', type=Path)
    parser.add_argument('rom', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    home = args.output.resolve() / 'simulated-deck-home'
    steam = home / '.local/share/Steam'
    (steam / 'userdata/123/config').mkdir(parents=True)
    (steam / 'config').mkdir()
    config = steam / 'config/config.vdf'
    original_config = b'"InstallConfigStore" { "Software" { "Valve" { "Steam" { "Keep" "unchanged" } } } }'
    config.write_bytes(original_config)
    root = home / 'Games/SuperMetroidPlus'
    (root / 'saves').mkdir(parents=True)
    (root / 'saves/Super Metroid.srm').write_bytes(b'personal Deck save')
    (root / 'settings-template.bml').write_bytes(b'existing Deck settings')
    (root / 'Play.exe').write_bytes(b'previous launcher')
    fake_bin = args.output.resolve() / 'fake-bin'
    fake_bin.mkdir()
    zenity = fake_bin / 'zenity'
    zenity.write_text('#!/bin/sh\nexit 0\n')
    zenity.chmod(0o755)
    environment = dict(os.environ, HOME=str(home), PATH=str(fake_bin) + ':' + os.environ['PATH'])
    results = []
    for attempt in [1, 2]:
        log_path = args.output / ('sender-' + str(attempt) + '.log')
        log = log_path.open('w')
        process = subprocess.Popen(['python3', str(args.sender.resolve()), '--terminal', '--bind', '127.0.0.1', '--port', '18763', '--rom', str(args.rom.resolve())], stdin=subprocess.PIPE, stdout=log, stderr=subprocess.STDOUT, text=True)
        try:
            base = 'http://127.0.0.1:18763'
            for _ in range(150):
                if process.poll() is not None:
                    raise RuntimeError(log_path.read_text())
                try:
                    with urllib.request.urlopen(base, timeout=.3) as response:
                        page = response.read()
                    break
                except (OSError, urllib.error.URLError):
                    time.sleep(.1)
            else:
                raise TimeoutError('Sender did not become ready.')
            assert b'Extract' in page
            process.stdin.write('yes\n')
            process.stdin.flush()
            with urllib.request.urlopen(base + '/installer.tar.gz', timeout=20) as response:
                archive_path = args.output / ('installer-' + str(attempt) + '.tar.gz')
                archive_path.write_bytes(response.read())
            extracted = args.output / ('extracted-' + str(attempt))
            subprocess.run(['tar', '-xzf', str(archive_path), '-C', str(args.output)], check=True)
            installer = args.output / 'Super Metroid Plus Setup/Install Super Metroid Plus.desktop'
            assert installer.stat().st_mode & 0o111, 'Extraction lost executable permission'
            code = base64.b64decode(re.search(r"b64decode\('([^']+)'\)", installer.read_text())[1]).decode()
            compile(code, 'downloaded-bootstrap', 'exec')
            # This executes the complete downloaded bootstrap: real HTTP ZIP
            # download, integrity check, extraction, payload installation and
            # shortcut update. Only GUI answers and Steam's home are simulated.
            result = subprocess.run(['python3', '-c', code], env=environment, capture_output=True, text=True, timeout=45)
            (args.output / ('installer-' + str(attempt) + '.log')).write_text(result.stdout + result.stderr)
            assert result.returncode == 0, result.stdout + result.stderr
            assert process.wait(timeout=10) == 0
            assert 'reported success' in log_path.read_text()
            assert (root / 'saves/Super Metroid.srm').read_bytes() == b'personal Deck save'
            assert (root / 'settings-template.bml').read_bytes() == b'existing Deck settings'
            assert (root / 'Play.exe').read_bytes()[:2] == b'MZ'
            assert (root / 'games/Super Metroid.sfc').stat().st_size == 3145728
            assert config.read_text().count('"proton_10"') == 2
            assert '"Keep" "unchanged"' in config.read_text()
            shortcuts = steam / 'userdata/123/config/shortcuts.vdf'
            if attempt == 1:
                initial_shortcuts, initial_config = shortcuts.read_bytes(), config.read_bytes()
                assert any(p.read_bytes() == b'previous launcher' for p in (root / 'deployment-backups').glob('*/Play.exe'))
                assert any(p.read_bytes() == original_config for p in (root / 'deployment-backups').glob('*/config.vdf'))
            else:
                assert shortcuts.read_bytes() == initial_shortcuts
                assert config.read_bytes() == initial_config
            results.append({'attempt': attempt, 'success': True})
        finally:
            if process.poll() is None:
                process.terminate()
                process.wait(timeout=10)
            log.close()
    with zipfile.ZipFile(args.sender) as z:
        assert set(z.namelist()) == {'__main__.py', 'sender_bootstrap.py', 'friends.zip', 'SENDER.json'}
        assert json.loads(z.read('SENDER.json'))['rom_included'] is False
    result = {'status': 'PASS', 'sender_sha256': hashlib.sha256(args.sender.read_bytes()).hexdigest(),
              'native_ubuntu_sender_process': True, 'complete_downloaded_bootstrap_executed': True,
              'unix_tar_executable_permission_preserved': True, 'automatic_success_shutdown': True,
              'save_settings_and_steam_entries_preserved': True, 'exact_replacement_backups': True,
              'repeat_install_idempotent': True, 'attempts': results,
              'physical_deck_or_kde_trust_dialog_tested': False}
    (args.output / 'validation.json').write_text(json.dumps(result, indent=2))
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
