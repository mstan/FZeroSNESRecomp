"""Focused import/staging exclusion checks using synthetic PCM files."""
import contextlib
import hashlib
import io
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import import_cgp_music as music


class MusicExclusionTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.keep = b'MSU1' + bytes(4) + b'keep audio'
        self.excluded = b'MSU1' + bytes(4) + b'excluded audio'
        entry = lambda b: dict(bytes=len(b), sha256=hashlib.sha256(b).hexdigest())
        self.manifest = dict(tracks={'4': entry(self.keep)},
                             excluded_tracks={'25': entry(self.excluded)})
        self.patch = patch.object(music, 'load_manifest', return_value=self.manifest)
        self.patch.start()
        self.addCleanup(self.patch.stop)

    def folder(self, name):
        path = self.root / name
        path.mkdir()
        (path / 'cgp-4.pcm').write_bytes(self.keep)
        (path / 'cgp.msu').write_bytes(b'')
        return path

    def test_source_archive_exclusions_are_skipped(self):
        archive = self.root / 'source.zip'
        with zipfile.ZipFile(archive, 'w') as z:
            z.writestr('P1/P1-4.pcm', self.keep)
            z.writestr('P1/P1-25.pcm', self.excluded)
            z.writestr('P1/unrelated.txt', b'not music')
        out = self.root / 'import'
        with patch.object(sys, 'argv', ['import_cgp_music', str(archive), '--out', str(out)]):
            with contextlib.redirect_stdout(io.StringIO()):
                music.main()
        self.assertEqual({p.name for p in out.iterdir()}, {'cgp.msu', 'cgp-4.pcm'})
        self.assertEqual((out / 'cgp-4.pcm').read_bytes(), self.keep)

    def test_package_verification_rejects_reintroduced_file(self):
        path = self.folder('bundle')
        (path / 'cgp-25.pcm').write_bytes(self.excluded)
        with self.assertRaises(ValueError):
            music.verify_music(path)

    def test_staging_prunes_excluded_files_from_old_build(self):
        src, dst = self.folder('source'), self.folder('build')
        (dst / 'cgp-25.pcm').write_bytes(self.excluded)
        self.assertEqual(music.stage_music(src, dst), 1)
        self.assertFalse((dst / 'cgp-25.pcm').exists())
        self.assertEqual((dst / 'cgp-4.pcm').read_bytes(), self.keep)

    def test_prune_preserves_replacements_with_the_same_number(self):
        dst = self.folder('custom')
        replacement = b'a different recording'
        (dst / 'cgp-25.pcm').write_bytes(replacement)
        with self.assertRaises(ValueError):
            music.prune_excluded(dst)
        self.assertEqual((dst / 'cgp-25.pcm').read_bytes(), replacement)


if __name__ == '__main__':
    unittest.main()
