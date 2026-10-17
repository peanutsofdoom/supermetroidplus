"""On-Deck private deployment. Stage payload first; configure Steam while closed."""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import re
import shutil
import struct
import tarfile
import time
import zlib


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def running_steam():
    return any(p.read_text().strip() == 'steam' for p in Path('/proc').glob('[0-9]*/comm')
               if p.exists())


def read_vdf(data):
    pos = 0
    def string():
        nonlocal pos
        end = data.index(0, pos)
        result = data[pos:end].decode()
        pos = end + 1
        return result
    def obj():
        nonlocal pos
        result = {}
        while True:
            kind = data[pos]
            pos += 1
            if kind == 8:
                return result
            key = string()
            if key in result:
                raise ValueError('Duplicate VDF key')
            if kind == 0:
                value = obj()
            elif kind == 1:
                value = string()
            elif kind == 2:
                value = struct.unpack_from('<I', data, pos)[0]
                pos += 4
            else:
                raise ValueError('Unsupported VDF type; original preserved')
            result[key] = value
    result = obj()
    if pos != len(data) or write_vdf(result) != data:
        raise ValueError('VDF failed lossless round-trip')
    return result


def write_vdf(tree):
    result = b''
    for key, value in tree.items():
        key = key.encode() + b'\0'
        if isinstance(value, dict):
            result += b'\0' + key + write_vdf(value)
        elif isinstance(value, str):
            result += b'\1' + key + value.encode() + b'\0'
        else:
            result += b'\2' + key + struct.pack('<I', value)
    return result + b'\x08'


def identity(root, coordinates=False):
    name = 'Super Metroid Plus' + (' - Coordinates' if coordinates else '')
    exe = '"' + str(root / ('Play With Coordinates.exe' if coordinates else 'Play.exe')) + '"'
    appid = zlib.crc32((exe + name).encode()) | 0x80000000
    return name, exe, appid


def stage(args, root):
    for process in Path('/proc').glob('[0-9]*/cmdline'):
        try:
            command = process.read_bytes().split(b'\0')[0].decode(errors='replace').replace('\\', '/')
        except (OSError, ProcessLookupError):
            continue
        if str(root).lower() + '/' in command.lower() and command.lower().endswith('.exe'):
            raise RuntimeError('Game or launcher is active; preserve ongoing play')
    archive = Path(args.archive)
    if sha(archive) != args.sha256:
        raise ValueError('Archive hash mismatch')
    backup = root.parent / ('SuperMetroidPlus-backup-' + str(time.time_ns()))
    backup.mkdir(parents=True)
    with tarfile.open(archive) as bundle:
        manifest = json.load(bundle.extractfile('DECK-MANIFEST.json'))
        members = bundle.getmembers()
        if len({m.name for m in members}) != len(members):
            raise ValueError('Duplicate archive members')
        if {m.name for m in members} != set(manifest['files']) | {'DECK-MANIFEST.json'}:
            raise ValueError('Archive inventory mismatch')
        for member in members:
            relative = Path(member.name)
            if not member.isfile() or relative.is_absolute() or '..' in relative.parts:
                raise ValueError('Unsafe archive path')
            if relative.parts[0] not in ('app', 'games', 'Firmware') and len(relative.parts) != 1:
                raise ValueError('Unexpected payload directory')
            if relative.name in ('settings.bml', 'authorized_keys') or relative.suffix in ('.srm', '.bsz'):
                raise ValueError('User data in payload')
            destination = root / relative
            if destination.resolve() != root.resolve() / relative:
                raise ValueError('Symlinked destination')
            data = bundle.extractfile(member).read()
            if member.name != 'DECK-MANIFEST.json' and hashlib.sha256(data).hexdigest() != manifest['files'][member.name]:
                raise ValueError('Payload hash mismatch')
        for member in members:
            destination = root / member.name
            # An established template contains user preferences; retain it.
            if member.name == 'settings-template.bml' and destination.exists():
                continue
            if destination.exists():
                saved = backup / member.name
                saved.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(destination, saved)
            destination.parent.mkdir(parents=True, exist_ok=True)
            temporary = destination.with_name(destination.name + '.deck-new')
            temporary.write_bytes(bundle.extractfile(member).read())
            temporary.replace(destination)
        for name, expected in manifest['files'].items():
            if name != 'settings-template.bml' and sha(root / name) != expected:
                raise ValueError('Installed hash mismatch')
    print(json.dumps(dict(installed=str(root), backup=str(backup), files=len(manifest['files']),
                          entries=[identity(root, flag) for flag in (False, True)])))


def shortcuts(args, root):
    if running_steam():
        raise RuntimeError('Close Steam before shortcut maintenance')
    steam = Path.home() / '.local/share/Steam'
    path = steam / 'userdata' / args.profile / 'config/shortcuts.vdf'
    config = steam / 'config/config.vdf'
    original = path.read_bytes() if path.exists() else write_vdf({'shortcuts': {}})
    tree = read_vdf(original)
    entries = tree['shortcuts']
    before = copy.deepcopy(entries)
    text = config.read_text()
    result = []
    owned_indices = set()
    common_prefix = steam / 'steamapps/compatdata' / str(identity(root)[2])
    launch_options = 'STEAM_COMPAT_DATA_PATH="' + str(common_prefix) + '" %command%'
    for coordinate in (False, True):
        name, exe, appid = identity(root, coordinate)
        found = [v for v in entries.values() if v.get('AppName', v.get('appname')) == name]
        if found:
            if len(found) != 1 or found[0].get('Exe', found[0].get('exe')) != exe or found[0].get('appid') != appid:
                raise ValueError('Existing shortcut identity mismatch')
        else:
            if any(v.get('appid') == appid for v in entries.values()):
                raise ValueError('Shortcut appid collision')
            entries[str(max(map(int, entries), default=-1) + 1)] = dict(
                appid=appid, AppName=name, Exe=exe, StartDir='"' + str(root) + '"',
                icon='', ShortcutPath='', LaunchOptions='', IsHidden=0, AllowDesktopConfig=1,
                AllowOverlay=1, OpenVR=0, Devkit=0, DevkitGameID='', DevkitOverrideAppID=0,
                LastPlayTime=0, FlatpakAppID='', tags={})
        for index, item in entries.items():
            if item.get('appid') == appid:
                owned_indices.add(index)
                key = next((key for key in item if key.lower() == 'launchoptions'), 'LaunchOptions')
                if item.get(key, '') not in ('', launch_options):
                    raise ValueError('Existing custom launch options require inspection')
                item[key] = launch_options
        pattern = r'("CompatToolMapping"\s*\{)'
        matches = list(re.finditer(pattern, text))
        if len(matches) != 1:
            raise ValueError('Expected exactly one compatibility mapping')
        # Insert only our absent identifiers; never rewrite unrelated configuration.
        entry_pattern = r'"' + str(appid) + r'"\s*\{[^{}]*\}'
        depth = 1
        end = None
        for token in re.finditer(r'"(?:\\.|[^"\\])*"|[{}]', text[matches[0].end():]):
            if token[0] == '{':
                depth += 1
            elif token[0] == '}':
                depth -= 1
                if depth == 0:
                    end = matches[0].end() + token.start()
                    break
        if end is None:
            raise ValueError('Unterminated compatibility mapping')
        present = list(re.finditer(entry_pattern, text[matches[0].end():end]))
        if present:
            if len(present) != 1 or '"proton_10"' not in present[0][0]:
                raise ValueError('Existing compatibility setting requires inspection')
        else:
            insertion = '\n\t\t\t\t\t"%s"\n\t\t\t\t\t{\n\t\t\t\t\t\t"name" "proton_10"\n\t\t\t\t\t\t"config" ""\n\t\t\t\t\t\t"priority" "250"\n\t\t\t\t\t}\n' % appid
            at = matches[0].end()
            text = text[:at] + insertion + text[at:]
        result.append(dict(name=name, appid=appid, gameid=(appid << 32) | 0x02000000))
    if any(entries[key] != value for key, value in before.items() if key not in owned_indices):
        raise ValueError('Unrelated shortcut changed')
    output = write_vdf(tree)
    read_vdf(output)
    if running_steam():
        raise RuntimeError('Steam restarted before write; originals preserved')
    # Proton locks <compatdata>/pfx.lock before it creates pfx/. In particular,
    # a first launch of Coordinates must not depend on Normal having run first.
    common_prefix.mkdir(parents=True, exist_ok=True)
    backup = root / 'deployment-backups' / str(time.time_ns())
    backup.mkdir(parents=True)
    (backup / 'shortcuts.vdf').write_bytes(original)
    shutil.copy2(config, backup / 'config.vdf')
    path.parent.mkdir(parents=True, exist_ok=True)
    try:
        temporary = path.with_suffix('.deck-new')
        temporary.write_bytes(output)
        temporary.replace(path)
        temporary = config.with_suffix('.deck-new')
        temporary.write_text(text)
        temporary.replace(config)
    except Exception:
        path.write_bytes(original)
        shutil.copy2(backup / 'config.vdf', config)
        raise
    report = dict(entries=result, backup=str(backup), original_shortcuts_preserved=len(before))
    (root / 'DECK-STEAM.json').write_text(json.dumps(report, indent=2))
    print(json.dumps(report))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('mode', choices=['stage', 'shortcuts'])
    parser.add_argument('--archive')
    parser.add_argument('--sha256')
    parser.add_argument('--profile')
    args = parser.parse_args()
    root = Path.home() / 'Games/SuperMetroidPlus'
    (stage if args.mode == 'stage' else shortcuts)(args, root)
