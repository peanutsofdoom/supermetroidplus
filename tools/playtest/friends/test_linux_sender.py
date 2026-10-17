import base64
import importlib.util
import io
import json
from pathlib import Path
import re
import tarfile
import threading
import time
import unittest
from unittest.mock import patch
import urllib.error
import urllib.request
import zipfile

spec = importlib.util.spec_from_file_location('sender', Path(__file__).with_name('send_to_deck_linux.py'))
sender = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sender)


class SenderTests(unittest.TestCase):
    def fixture(self, manifest=None):
        content = io.BytesIO()
        files = {'Play.exe': b'qualified runtime', 'tools/deck_setup.py': b'# installer'}
        manifest = manifest or {name: sender.sha(data) for name, data in files.items()}
        with zipfile.ZipFile(content, 'w') as z:
            for name, data in files.items():
                z.writestr('Friends/' + name, data)
            z.writestr('Friends/PACKAGE-MANIFEST.json', json.dumps({'files': manifest}))
            z.writestr('Friends/saves/private.srm', b'private progress')
        return content.getvalue()

    def test_sealed_payload_excludes_unlisted_personal_files(self):
        with patch.object(sender, 'ROM_HASH', sender.sha(b'rom')):
            payload = sender.make_payload(self.fixture(), b'rom')
        with zipfile.ZipFile(io.BytesIO(payload)) as z:
            self.assertNotIn('Super Metroid Plus/saves/private.srm', z.namelist())
            self.assertEqual(z.read('Super Metroid Plus/games/Super Metroid.sfc'), b'rom')

    def test_manifest_hash_and_unsafe_paths_rejected(self):
        with self.assertRaisesRegex(ValueError, 'changed'):
            sender.make_payload(self.fixture({'Play.exe': 'wrong'}), b'rom')
        for path in ['../outside', '/absolute', 'saves/progress.srm', 'foo\\bar', 'C:/file']:
            with self.assertRaisesRegex(ValueError, 'Unsafe'):
                sender.make_payload(self.fixture({path: 'hash'}), b'rom')

    def test_browser_transfer_requires_approval_and_installer_is_executable(self):
        decisions, requests = [False, True], []
        def approve(address):
            requests.append(address)
            return decisions.pop(0)
        server = sender.TransferServer(('127.0.0.1', 0), b'payload', 'url="__PAYLOAD_URL__"\nhash="__PAYLOAD_SHA__"\nstatus="__STATUS_URL__"\n', approve)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        def get(path):
            return urllib.request.urlopen(server.url + path, timeout=3).read()
        try:
            self.assertIn(b'Extract', get('/'))
            with self.assertRaises(urllib.error.HTTPError) as error:
                get('/' + server.token + '/deck.zip')
            self.assertEqual(error.exception.code, 404)
            with self.assertRaises(urllib.error.HTTPError) as error:
                get('/installer.tar.gz')
            self.assertEqual(error.exception.code, 403)
            archive = get('/installer.tar.gz')
            self.assertEqual(requests, ['127.0.0.1', '127.0.0.1'])
            with tarfile.open(fileobj=io.BytesIO(archive), mode='r:gz') as tar:
                entry = tar.getmember('Super Metroid Plus Setup/Install Super Metroid Plus.desktop')
                self.assertEqual(entry.mode, 0o755)
                desktop = tar.extractfile(entry).read().decode()
            encoded = re.search(r"b64decode\('([^']+)'\)", desktop)[1]
            code = base64.b64decode(encoded).decode()
            compile(code, 'bootstrap', 'exec')
            self.assertIn(sender.sha(b'payload'), code)
            self.assertEqual(get('/' + server.token + '/deck.zip'), b'payload')
            with self.assertRaises(urllib.error.HTTPError):
                get('/../../private')
            for body, expected in [(b'[]', 400), (b'{"state":"bad"}', 400)]:
                with self.assertRaises(urllib.error.HTTPError) as error:
                    urllib.request.urlopen(urllib.request.Request(server.url + '/' + server.token + '/status', data=body), timeout=3)
                self.assertEqual(error.exception.code, expected)
            request = urllib.request.Request(server.url + '/' + server.token + '/status', data=b'{"state":"cancelled"}')
            urllib.request.urlopen(request, timeout=3).read()
            self.assertFalse(server.completed.is_set())
            request.data = b'{"state":"installed"}'
            urllib.request.urlopen(request, timeout=3).read()
            self.assertTrue(server.completed.wait(1))
            server.expires = time.monotonic() - 1
            with self.assertRaises(urllib.error.HTTPError) as error:
                get('/')
            self.assertEqual(error.exception.code, 410)
        finally:
            server.shutdown()
            server.server_close()
            thread.join()


if __name__ == '__main__':
    unittest.main()
