import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import zipfile

spec = importlib.util.spec_from_file_location('deck_setup', Path(__file__).with_name('deck_setup.py'))
setup = importlib.util.module_from_spec(spec)
spec.loader.exec_module(setup)


class InstallerTests(unittest.TestCase):
    def fixture(self, directory):
        source = Path(directory) / 'source'
        source.mkdir()
        data = {'Play.exe': b'new-release', 'settings-template.bml': b'defaults', 'tools/helper.py': b'helper'}
        for rel, value in data.items():
            p = source / rel
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_bytes(value)
        (source / 'games').mkdir()
        (source / 'games/Super Metroid.sfc').write_bytes(b'rom')
        (source / 'PACKAGE-MANIFEST.json').write_text(json.dumps({'files': {k: setup.digest(v) for k, v in data.items()}}))
        return source

    def test_preserves_progress_settings_and_exact_backups(self):
        with tempfile.TemporaryDirectory() as d, patch.object(setup, 'ROM_HASH', setup.digest(b'rom')):
            source = self.fixture(d)
            root = Path(d) / 'installed'
            for rel, value in {'saves/Super Metroid.srm': b'progress', 'app/settings.bml': b'controls', 'settings-template.bml': b'custom', 'Play.exe': b'old', 'CAPTURE-SAFE-FULLSCREEN': b'marker'}.items():
                target = root / rel
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(value)
            backup = setup.install_files(source, root)
            self.assertEqual((root / 'saves/Super Metroid.srm').read_bytes(), b'progress')
            self.assertEqual((root / 'app/settings.bml').read_bytes(), b'controls')
            self.assertEqual((root / 'settings-template.bml').read_bytes(), b'custom')
            self.assertEqual((backup / 'Play.exe').read_bytes(), b'old')
            self.assertEqual((backup / 'CAPTURE-SAFE-FULLSCREEN').read_bytes(), b'marker')
            self.assertFalse((root / 'CAPTURE-SAFE-FULLSCREEN').exists())
            setup.install_files(source, root)
            self.assertEqual((root / 'Play.exe').read_bytes(), b'new-release')

    def test_rollback_after_copy_failure(self):
        with tempfile.TemporaryDirectory() as d:
            source = self.fixture(d)
            root = Path(d) / 'installed'
            root.mkdir()
            (root / 'Play.exe').write_bytes(b'old')
            real_copy = setup.shutil.copyfile
            def failing(src, dst, **kwargs):
                if str(dst).endswith('helper.py.smp-new'):
                    raise OSError('simulated disk failure')
                return real_copy(src, dst, **kwargs)
            with patch.object(setup.shutil, 'copyfile', side_effect=failing):
                with self.assertRaisesRegex(OSError, 'disk failure'):
                    setup.install_files(source, root)
            self.assertEqual((root / 'Play.exe').read_bytes(), b'old')
            self.assertFalse((root / 'settings-template.bml').exists())

    def test_reject_personal_data(self):
        with tempfile.TemporaryDirectory() as d:
            source = self.fixture(d)
            (source / 'PACKAGE-MANIFEST.json').write_text(json.dumps({'files': {'saves/player.srm': 'bad'}}))
            with self.assertRaisesRegex(ValueError, 'Personal data'):
                setup.install_files(source, Path(d) / 'installed')

    def test_archive_integrity_and_traversal(self):
        with tempfile.TemporaryDirectory() as d, patch.object(setup, 'ROM_HASH', setup.digest(b'rom')):
            source = self.fixture(d)
            archive = Path(d) / 'good.zip'
            with zipfile.ZipFile(archive, 'w') as z:
                for f in source.rglob('*'):
                    if f.is_file():
                        z.write(f, 'Super Metroid Plus/' + f.relative_to(source).as_posix())
            unpacked = setup.unpack(archive, Path(d) / 'unpacked')
            self.assertEqual((unpacked / 'Play.exe').read_bytes(), b'new-release')
            with zipfile.ZipFile(archive, 'a') as z:
                z.writestr('../outside', b'bad')
            with self.assertRaisesRegex(ValueError, 'Unsafe'):
                setup.unpack(archive, Path(d) / 'bad')
            self.assertFalse((Path(d) / 'outside').exists())

    def test_wrong_existing_rom_refused(self):
        with tempfile.TemporaryDirectory() as d:
            source = self.fixture(d)
            root = Path(d) / 'installed'
            (root / 'games').mkdir(parents=True)
            (root / 'games/Super Metroid.sfc').write_bytes(b'other')
            with self.assertRaisesRegex(ValueError, 'different ROM'):
                setup.install_files(source, root)
            self.assertEqual((root / 'games/Super Metroid.sfc').read_bytes(), b'other')


if __name__ == '__main__':
    unittest.main()
