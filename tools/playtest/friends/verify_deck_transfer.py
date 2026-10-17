"""Local end-to-end check; never connects to or modifies an actual Steam Deck."""
import base64
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time
import urllib.error
import urllib.request
from unittest.mock import patch
import zipfile

ROOT = Path(__file__).resolve().parents[3]
PACKAGE = ROOT / 'build/playtest/friends-package-deck-final-20261006/Super-Metroid-Plus-Friends-249-250.zip'
WORK = ROOT / 'build/playtest/deck-installer-20261006' / str(time.time_ns())


def main():
    WORK.mkdir(parents=True)
    with zipfile.ZipFile(PACKAGE) as z:
        assert not any(Path(n).suffix.lower() in {'.sfc', '.smc', '.srm', '.bst'} for n in z.namelist())
        z.extractall(WORK / 'pc')
    pc = next((WORK / 'pc').iterdir())
    shutil.copyfile(ROOT / 'preview/Owner Playable 2026-09-24/games/Super Metroid.sfc', pc / 'games/Super Metroid.sfc')
    # This canary must never be included in the transfer, even from a used PC folder.
    (pc / 'saves/private.srm').write_bytes(b'private-progress')
    stdout = (WORK / 'server.log').open('w')
    process = subprocess.Popen(['powershell.exe', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', str(pc / 'tools/Send-To-Deck.ps1'), '-BindAddress', '127.0.0.1', '-Port', '18759', '-TestMode'], stdout=stdout, stderr=subprocess.STDOUT, creationflags=subprocess.CREATE_NO_WINDOW)
    try:
        marker = pc / 'deck-transfer-test-url.txt'
        for _ in range(120):
            if marker.exists():
                break
            if process.poll() is not None:
                raise RuntimeError((WORK / 'server.log').read_text())
            time.sleep(.25)
        url = marker.read_text()
        get = lambda path: urllib.request.urlopen(url + path, timeout=10).read()
        assert b'Download Deck installer' in get('')
        desktop = get('/Install-Super-Metroid-Plus.desktop').decode()
        encoded = re.search(r"b64decode\('([A-Za-z0-9+/=]+)'\)", desktop)[1]
        bootstrap = base64.b64decode(encoded).decode()
        compile(bootstrap, 'downloaded-bootstrap', 'exec')
        assert '__PAYLOAD_' not in bootstrap
        data = get('/deck.zip')
        digest = hashlib.sha256(data).hexdigest()
        assert digest in bootstrap
        for path in ['/../games/Super%20Metroid.sfc', '/other', '/../../private']:
            try:
                get(path)
                raise AssertionError('Unexpected endpoint was served')
            except urllib.error.HTTPError as e:
                assert e.code == 404
        archive = WORK / 'transfer.zip'
        archive.write_bytes(data)
    finally:
        if process.poll() is None:
            try:
                urllib.request.urlopen(url + '/test-stop', timeout=5).read()
            except urllib.error.HTTPError:
                pass
            except Exception:
                process.terminate()
            process.wait(timeout=10)
        stdout.close()
    with zipfile.ZipFile(archive) as z:
        assert not any('/saves/' in n or 'private.srm' in n for n in z.namelist())
    spec = importlib.util.spec_from_file_location('setup_test', pc / 'tools/deck_setup.py')
    setup = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(setup)
    source = setup.unpack(archive, WORK / 'unpacked')
    setup.__file__ = str(source / 'tools/deck_setup.py')
    sys.path.insert(0, str(source / 'tools'))
    import steam_setup
    home = WORK / 'fake-deck'
    steam = home / '.local/share/Steam'
    (steam / 'userdata/123/config').mkdir(parents=True)
    (steam / 'config').mkdir()
    config = steam / 'config/config.vdf'
    original_config = b'"InstallConfigStore" { "Software" { "Valve" { "Steam" { "Keep" "exact" } } } }'
    config.write_bytes(original_config)
    shortcut = steam / 'userdata/123/config/shortcuts.vdf'
    other = {'appid': 42, 'AppName': 'My other game', 'Exe': 'keep', 'tags': {'0': 'Favorite'}}
    shortcut.write_bytes(steam_setup.write_vdf({'shortcuts': {'0': other}}))
    real_exists = Path.exists
    def exists(path):
        return True if str(path).replace('\\', '/') == '/etc/os-release' else real_exists(path)
    with patch.object(sys, 'platform', 'linux'), patch.object(Path, 'exists', exists), patch.object(Path, 'home', return_value=home), patch.object(setup.shutil, 'which', return_value='zenity'), patch.object(setup, 'dialog', return_value=''), patch.object(steam_setup, 'running_steam', return_value=False):
        setup.main()
        root = home / 'Games/SuperMetroidPlus'
        report = json.loads((root / 'DECK-STEAM.json').read_text())
        assert (Path(report['backup']) / 'config.vdf').read_bytes() == original_config
        (root / 'saves').mkdir()
        (root / 'saves/Super Metroid.srm').write_bytes(b'Deck progress')
        first = shortcut.read_bytes(), config.read_bytes()
        setup.main()
        assert first == (shortcut.read_bytes(), config.read_bytes())
        assert (root / 'saves/Super Metroid.srm').read_bytes() == b'Deck progress'
    entries = steam_setup.read_vdf(shortcut.read_bytes())['shortcuts']
    assert len(entries) == 3 and entries['0'] == other
    assert config.read_text().count('"proton_10"') == 2
    result = {'status': 'PASS', 'archive': str(PACKAGE), 'archive_sha256': hashlib.sha256(PACKAGE.read_bytes()).hexdigest(), 'http_download': True, 'bootstrap_compiles_and_pins_hash': True, 'unexpected_endpoints_refused': True, 'pc_save_excluded': True, 'rom_free_distribution': True, 'fresh_install_and_repeat': True, 'missing_compatibility_mapping_created': True, 'exact_original_config_backup': True, 'unrelated_steam_entry_preserved': True, 'deck_save_preserved': True, 'actual_steam_deck_ui_tested': False}
    (WORK / 'validation.json').write_text(json.dumps(result, indent=2))
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
