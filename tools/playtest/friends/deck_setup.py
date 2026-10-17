#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Click-through Deck setup. No sudo, SSH, shell commands, or account credentials."""
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import subprocess
import sys
import tempfile
import time
from types import SimpleNamespace
import zipfile

ROM_HASH = '12b77c4bc9c1832cee8881244659065ee1d84c70c3d29e6eaf92e6798cc2ca72'


def dialog(text, kind='info', **options):
    args = ['zenity', '--' + kind, '--title=Super Metroid Plus', '--width=560']
    if kind != 'file-selection':
        args += ['--text=' + text, '--no-markup']
    args += ['--' + k.replace('_', '-') + '=' + str(v) for k, v in options.items()]
    result = subprocess.run(args, capture_output=True, text=True)
    if result.returncode:
        raise InterruptedError('Setup cancelled. You can run it again later.')
    return result.stdout.strip()


def digest(data):
    return hashlib.sha256(data).hexdigest()


def unpack(archive, destination):
    """Reject traversal, symlinks, duplicates and oversized/unsealed payloads."""
    with zipfile.ZipFile(archive) as bundle:
        names = set()
        total = 0
        for info in bundle.infolist():
            path = PurePosixPath(info.filename)
            total += info.file_size
            if (path.is_absolute() or '..' in path.parts or '\\' in info.filename
                    or ':' in info.filename or info.filename in names
                    or (info.external_attr >> 16) & 0o170000 == 0o120000
                    or total > 250_000_000):
                raise ValueError('Unsafe or oversized package; nothing installed.')
            names.add(info.filename)
        bundle.extractall(destination)
    manifests = list(destination.glob('*/PACKAGE-MANIFEST.json'))
    if len(manifests) != 1:
        raise ValueError('Expected one friends package.')
    source = manifests[0].parent
    manifest = json.loads(manifests[0].read_text())['files']
    actual = {p.relative_to(source).as_posix() for p in source.rglob('*') if p.is_file()}
    if actual != set(manifest) | {'PACKAGE-MANIFEST.json', 'games/Super Metroid.sfc'}:
        raise ValueError('Package inventory differs from its manifest.')
    for rel, expected in manifest.items():
        if digest((source / rel).read_bytes()) != expected:
            raise ValueError('Package hash mismatch: ' + rel)
    if digest((source / 'games/Super Metroid.sfc').read_bytes()) != ROM_HASH:
        raise ValueError('ROM does not match this release.')
    return source


def install_files(source, root):
    # Every existing file replaced by this installation gets an exact backup.
    # Never copy personal data, and keep the Deck's template/settings on update.
    manifest = json.loads((source / 'PACKAGE-MANIFEST.json').read_text())['files']
    paths = list(manifest) + ['PACKAGE-MANIFEST.json', 'games/Super Metroid.sfc']
    protected = {'saves', 'states', 'screenshots', 'logs', 'reports', 'cheats', 'patches'}
    for rel in paths:
        p = PurePosixPath(rel)
        if p.parts[0] in protected or p.is_absolute() or '..' in p.parts:
            raise ValueError('Personal data in installer payload: ' + rel)
        target = root / rel
        if any(q.is_symlink() for q in [target, *target.parents]):
            raise ValueError('Install destination contains a symbolic link: ' + str(target))
    existing_rom = root / 'games/Super Metroid.sfc'
    if existing_rom.exists() and digest(existing_rom.read_bytes()) != ROM_HASH:
        raise ValueError('A different ROM is installed. Move it aside before retrying.')
    backup = root / 'deployment-backups' / ('friends-' + str(time.time_ns()))
    changed = []
    try:
        for rel in paths:
            target = root / rel
            if target.exists() and (rel == 'settings-template.bml' or target.read_bytes() == (source / rel).read_bytes()):
                continue
            saved = backup / rel
            if target.exists():
                saved.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(target, saved)
            target.parent.mkdir(parents=True, exist_ok=True)
            changed.append((target, saved))
            temp = target.with_name(target.name + '.smp-new')
            shutil.copyfile(source / rel, temp)
            temp.replace(target)
        marker = root / 'CAPTURE-SAFE-FULLSCREEN'
        if marker.exists():
            backup.mkdir(parents=True, exist_ok=True)
            shutil.copy2(marker, backup / marker.name)
            marker.unlink()
    except Exception:
        for target, saved in reversed(changed):
            if saved.exists():
                shutil.copy2(saved, target)
            else:
                target.unlink(missing_ok=True)
        raise
    return backup


def main():
    if sys.platform != 'linux' or not Path('/etc/os-release').exists():
        raise RuntimeError('Run this installer on the Steam Deck in Desktop Mode.')
    if not shutil.which('zenity'):
        raise RuntimeError('SteamOS dialog tool zenity is unavailable.')
    source = Path(__file__).resolve().parent.parent
    root = Path.home() / 'Games/SuperMetroidPlus'
    dialog('Install Super Metroid Plus into your Games folder and add its two Steam entries?\n\nYour existing Deck saves and settings will be kept. Finish any game first. Setup will ask you to exit Steam.', 'question')
    sys.path.insert(0, str(source / 'tools'))
    import steam_setup
    steam = Path.home() / '.local/share/Steam'
    profiles = sorted(p.name for p in (steam / 'userdata').glob('*') if p.name.isdecimal() and (p / 'config').is_dir())
    if not profiles:
        raise RuntimeError('Sign into Steam in Desktop Mode once, then rerun setup.')
    if len(profiles) == 1:
        profile = profiles[0]
    else:
        args = ['zenity', '--list', '--title=Choose Steam account', '--text=Choose the Steam user ID to receive the game entries.', '--column=Steam user ID', *profiles]
        result = subprocess.run(args, capture_output=True, text=True)
        profile = result.stdout.strip()
        if result.returncode or profile not in profiles:
            raise InterruptedError('No Steam account selected.')
    while steam_setup.running_steam():
        dialog('In the Steam desktop window, choose Steam > Exit. Then click OK here.\n\nSetup will not force-close Steam or a running game.')
    for process in Path('/proc').glob('[0-9]*/cmdline'):
        try:
            command = process.read_bytes().lower()
        except OSError:
            continue
        if b'smp-enemies-v' in command or b'play with coordinates.exe' in command or b'play.exe' in command:
            raise RuntimeError('Close Super Metroid Plus before installing. Then rerun setup.')
    # Reuse the qualified lossless shortcut editor. Refuse unexpected existing
    # identities/options instead of replacing someone else's Steam entries.
    config = steam / 'config/config.vdf'
    original_config = config.read_bytes()
    inserted = False
    if b'"CompatToolMapping"' not in original_config:
        import re
        text = original_config.decode('utf-8')
        match = re.search(r'"Steam"\s*\{', text, re.I)
        if not match:
            raise RuntimeError('Steam configuration is unfamiliar. Launch Steam once and retry.')
        # This empty mapping is staged before the existing editor; restore the
        # exact original on failure and retain it in the successful backup.
        text = text[:match.end()] + '\n"CompatToolMapping"\n{\n}\n' + text[match.end():]
        inserted = True
    install_files(source, root)
    if steam_setup.running_steam():
        raise RuntimeError('Steam reopened. Exit Steam and rerun setup to finish adding entries.')
    try:
        if inserted:
            config.write_text(text, encoding='utf-8')
        steam_setup.shortcuts(SimpleNamespace(profile=profile), root)
    except Exception:
        if inserted:
            config.write_bytes(original_config)
        raise
    if inserted:
        report = json.loads((root / 'DECK-STEAM.json').read_text())
        (Path(report['backup']) / 'config.vdf').write_bytes(original_config)
    dialog('Installed! Return to Gaming Mode and open Super Metroid Plus.\n\nSteam entries use Proton 10.0. Steam may download it on first launch. If Steam asks you to select compatibility, choose Proton 10.0 in Properties > Compatibility.\n\nUse the Gamepad controller layout. The Coordinates entry starts the reporting overlay on. You can assign F8 to a rear button to toggle it.\n\nYou can now close the PC transfer window.')


if __name__ == '__main__':
    try:
        main()
    except InterruptedError:
        pass
    except Exception as error:
        if shutil.which('zenity'):
            subprocess.run(['zenity', '--error', '--no-markup', '--width=560', '--title=Super Metroid Plus setup', '--text=' + str(error)])
        else:
            print(str(error), file=sys.stderr)
        sys.exit(1)
