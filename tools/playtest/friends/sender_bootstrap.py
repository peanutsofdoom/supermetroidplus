# SPDX-License-Identifier: GPL-3.0-or-later
import hashlib
import json
from pathlib import Path
import runpy
import subprocess
import sys
import tempfile
import urllib.request
import zipfile


def report(state):
    try:
        request = urllib.request.Request('__STATUS_URL__', data=json.dumps({'state': state}).encode(), headers={'Content-Type': 'application/json'})
        with urllib.request.urlopen(request, timeout=5) as response:
            response.read()
    except Exception:
        pass  # Reporting is optional; it cannot undo a successful local install.


try:
    with tempfile.TemporaryDirectory(prefix='smp-deck-setup-') as temporary:
        stage = Path(temporary)
        with urllib.request.urlopen('__PAYLOAD_URL__', timeout=60) as response:
            payload = response.read(250_000_001)
        if len(payload) > 250_000_000 or hashlib.sha256(payload).hexdigest() != '__PAYLOAD_SHA__':
            raise RuntimeError('Download incomplete or changed. Keep the PC sender open and download the installer again.')
        archive = stage / 'payload.zip'
        archive.write_bytes(payload)
        with zipfile.ZipFile(archive) as bundle:
            helper = bundle.read('Super Metroid Plus/tools/deck_setup.py')
        namespace = {'__name__': 'smp_bootstrap_validator'}
        exec(compile(helper, 'deck_setup.py', 'exec'), namespace)
        source = namespace['unpack'](archive, stage / 'unpacked')
        # Call main explicitly so cancellation is distinguishable from success.
        installer = runpy.run_path(str(source / 'tools/deck_setup.py'), run_name='smp_deck_setup')
        installer['main']()
    report('installed')
except InterruptedError:
    report('cancelled')
except Exception as error:
    report('failed')
    subprocess.run(['zenity', '--error', '--no-markup', '--width=560', '--title=Super Metroid Plus setup', '--text=' + str(error)])
    sys.exit(1)
