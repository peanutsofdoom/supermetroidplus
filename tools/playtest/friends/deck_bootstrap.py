# SPDX-License-Identifier: GPL-3.0-or-later
# Filled in by the PC helper. Only this session's sealed ZIP is downloaded.
import hashlib
from pathlib import Path
import runpy
import subprocess
import sys
import tempfile
import urllib.request
import zipfile

try:
    with tempfile.TemporaryDirectory(prefix='smp-deck-') as temporary:
        stage = Path(temporary)
        with urllib.request.urlopen('__PAYLOAD_URL__', timeout=60) as response:
            payload = response.read(250_000_001)
        if len(payload) > 250_000_000 or hashlib.sha256(payload).hexdigest() != '__PAYLOAD_SHA__':
            raise RuntimeError('Download incomplete or changed. Keep the PC helper open and download the installer again.')
        archive = stage / 'payload.zip'
        archive.write_bytes(payload)
        # The entire ZIP is pinned above, including this extraction validator.
        with zipfile.ZipFile(archive) as bundle:
            helper = bundle.read('Super Metroid Plus/tools/deck_setup.py')
        namespace = {'__name__': 'smp_bootstrap_validator'}
        exec(compile(helper, 'deck_setup.py', 'exec'), namespace)
        source = namespace['unpack'](archive, stage / 'unpacked')
        runpy.run_path(str(source / 'tools/deck_setup.py'), run_name='__main__')
except Exception as error:
    subprocess.run(['zenity', '--error', '--no-markup', '--width=560', '--title=Super Metroid Plus download', '--text=' + str(error)])
    sys.exit(1)
