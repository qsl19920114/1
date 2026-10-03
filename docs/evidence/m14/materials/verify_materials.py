#!/usr/bin/env python3
"""Focused hostile-archive regression checks; run from any directory."""
import importlib.util
import io
from pathlib import Path
import tarfile
import tempfile
import unittest

REPO = Path(__file__).resolve().parents[4]
SCRIPT = REPO / 'scripts/showcase/prepare_character_materials.py'


class MaterialExtractionTest(unittest.TestCase):
    def setUp(self):
        self.assertTrue(SCRIPT.is_file(), 'material preparation command must exist')
        spec = importlib.util.spec_from_file_location('materials', SCRIPT)
        self.module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(self.module)

    def test_archive_boundaries_and_exact_bytes(self):
        m = self.module
        with tempfile.TemporaryDirectory() as folder:
            folder = Path(folder)
            payload = b'bounded image bytes'
            def archive(entries):
                path = folder / 'fixture.tar.gz'
                with tarfile.open(path, 'w:gz') as tar:
                    for name, kind, size in entries:
                        info = tarfile.TarInfo(name)
                        info.type = kind
                        info.size = size
                        info.linkname = '/tmp/escape'
                        tar.addfile(info, io.BytesIO(payload[:size]) if kind == tarfile.REGTYPE else None)
                return path
            valid = ('./portrait.png', tarfile.REGTYPE, len(payload))
            self.assertEqual(m.read_selected(archive([valid]), {'portrait.png'}), {'portrait.png': payload})
            for bad in [('../escape.png', tarfile.REGTYPE, 1),
                        ('/portrait.png', tarfile.REGTYPE, 1),
                        ('portrait.png', tarfile.SYMTYPE, 0),
                        ('portrait.png', tarfile.LNKTYPE, 0),
                        ('portrait.png', tarfile.DIRTYPE, 0)]:
                with self.subTest(bad=bad), self.assertRaises(ValueError):
                    m.read_selected(archive([bad]), {'portrait.png'})
            with self.assertRaises(ValueError):
                m.read_selected(archive([valid, valid]), {'portrait.png'})
            with self.assertRaises(ValueError):
                m.read_selected(archive([]), {'portrait.png'})
            previous = m.MAX_IMAGE
            m.MAX_IMAGE = 1
            with self.assertRaises(ValueError):
                m.read_selected(archive([valid]), {'portrait.png'})
            m.MAX_IMAGE = previous

    def test_output_symlinks_rejected(self):
        with tempfile.TemporaryDirectory() as folder:
            folder = Path(folder)
            (folder / 'real').mkdir()
            (folder / 'link').symlink_to(folder / 'real', target_is_directory=True)
            with self.assertRaises(ValueError):
                self.module.safe_output(folder / 'link' / 'new.png', REPO.parent / 'hypit')
            with self.assertRaises(ValueError):
                self.module.safe_output(REPO.parent / 'hypit' / 'forbidden.png', REPO.parent / 'hypit')


if __name__ == '__main__':
    unittest.main(verbosity=2)
