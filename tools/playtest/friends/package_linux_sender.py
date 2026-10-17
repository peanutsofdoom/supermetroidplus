"""Bundle a single runnable Linux sender with the qualified ROM-free Deck release."""
from pathlib import Path
import hashlib
import json
import zipfile

ROOT = Path(__file__).resolve().parents[3]
SCRIPTS = ROOT / 'tools/playtest/friends'
OUTPUT = ROOT / 'build/releases/linux-to-steam-deck-20261006'
FRIENDS = ROOT / 'build/playtest/friends-package-deck-final-20261006/Super-Metroid-Plus-Friends-249-250.zip'


def main():
    OUTPUT.mkdir(parents=True, exist_ok=True)
    target = OUTPUT / 'Send-SuperMetroidPlus-to-Deck.pyz'
    if target.exists():
        raise RuntimeError('Keep the earlier sender release.')
    package = FRIENDS.read_bytes()
    sha = hashlib.sha256(package).hexdigest()
    if sha != '2ab15f300b3e98e064d150398d74ad38bd875b1ddbd5ac50659d2a13426528f8':
        raise RuntimeError('Review the changed friends release before embedding it.')
    with zipfile.ZipFile(target, 'w', zipfile.ZIP_DEFLATED) as archive:
        archive.writestr('__main__.py', (SCRIPTS / 'send_to_deck_linux.py').read_bytes())
        archive.writestr('sender_bootstrap.py', (SCRIPTS / 'sender_bootstrap.py').read_bytes())
        archive.writestr('friends.zip', package, compress_type=zipfile.ZIP_STORED)
        archive.writestr('SENDER.json', json.dumps({'normal': 249, 'coordinates': 250, 'runtime': 'Windows build through Proton on Steam Deck', 'friends_sha256': sha, 'rom_included': False}))
    instructions = '''SEND SUPER METROID PLUS FROM UBUNTU TO STEAM DECK

On the Ubuntu PC, save the .pyz into Downloads and run:
  python3 ~/Downloads/Send-SuperMetroidPlus-to-Deck.pyz

The script includes the Deck game package. It automatically finds the ROM you
already imported into the native Ubuntu game, or opens a ROM picker. It does
not run the native Ubuntu game and does not copy PC saves/settings.

On the Deck, use Desktop Mode and put the short displayed address into the
browser ADDRESS BAR. Both devices must be on the same home network.

1. Click Download installer on the Deck.
2. Click Allow transfer on the Ubuntu PC.
3. On the Deck, open the downloaded archive and choose Extract. Open the
   extracted Super Metroid Plus Setup folder, then Install Super Metroid Plus.
   Choose Run/Execute/Trust if SteamOS asks.
4. Follow the dialogs, exiting Steam when asked. Return to Gaming Mode.

The archive carries executable permission, so a normal extraction avoids the
Properties checkbox. If an archive app removes permissions, enable Is executable
in the installer's Properties once. No terminal commands, sudo password or SSH
setup are needed on the Deck. The one browser address is the only Deck typing.

Steam entries use the Windows/Proton build already deployed on the owner's Deck.
Steam may download Proton10.0 on first launch; if it asks for compatibility,
select Proton10.0. Use a Gamepad Steam Input layout. Existing Deck saves/settings
are preserved and replacements backed up. No automatic save synchronization.

Keep the PC sender open. It stops sharing after the Deck reports completion,
when you close its instruction window, or after30minutes. Download a fresh
installer after restarting the sender. Only the approved device gets the package.
The public .pyz contains no ROM, personal save or private access key.

If the Deck browser cannot connect, check the chosen network and whether guest
Wi-Fi, a VPN or a host firewall is blocking local connections. This script does
not change firewall rules or router settings. No port forwarding is needed.

Optional PC arguments: --rom /path/to/rom.sfc, --bind YOUR_PC_IP, --terminal.
Python3 is required; zenity provides Ubuntu dialogs when available. The terminal
fallback needs no Python packages. The .pyz is a ZIP containing its Python source
and the ROM-free game/source package, with GPL license notices inside friends.zip.

See VALIDATION.json alongside this file for exactly what was tested.
'''
    (OUTPUT / 'START HERE.txt').write_text(instructions, encoding='utf-8')
    result = {'file': target.name, 'bytes': target.stat().st_size, 'sha256': hashlib.sha256(target.read_bytes()).hexdigest()}
    (OUTPUT / 'package-summary.json').write_text(json.dumps(result, indent=2))
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
