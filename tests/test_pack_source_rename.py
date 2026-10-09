"""Exercise on-disk ZIP renames against a supplied, non-bundled course pack."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
import zipfile

TOOL = PACK = None


class SourceRenameTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.pack = self.root / 'packs' / 'test'
        shutil.copytree(PACK, self.pack)
        self.original = (self.pack / 'courses.json').read_bytes()
        self.index = json.loads(self.original)
        self.course = self.index['courses'][0]
        self.old = self.pack / self.course['source']
        self.new = self.old.with_name('Renamed course.zip')
        self.baseline = self.inspect()

    def tearDown(self):
        self.temp.cleanup()

    def inspect(self, success=True):
        result = subprocess.run([str(TOOL), str(self.pack.parent)], capture_output=True,
                                text=True, timeout=120)
        self.assertEqual(result.returncode, 0 if success else 1, result.stdout + result.stderr)
        return result.stdout

    def test_rename_updates_index_checksum_music_and_preserves_records(self):
        before = self.pack / 'music' / self.old.with_suffix('.pcm').name
        before.parent.mkdir(exist_ok=True)
        before.write_bytes(b'MSU1' + bytes(36))
        self.old.rename(self.new)
        self.assertEqual(self.inspect(), self.baseline)
        index = (self.pack / 'courses.json').read_bytes()
        course = json.loads(index)['courses'][0]
        self.assertEqual(course['source'], 'courses/Renamed course.zip')
        self.assertEqual(course['id'], self.course['id'])
        self.assertEqual(course['name'], self.course['name'])
        envelope = json.loads((self.pack / 'pack.json').read_text())
        self.assertEqual(envelope['payload']['sha256'], hashlib.sha256(index).hexdigest())
        self.assertFalse(before.exists())
        self.assertEqual((self.pack / 'music/Renamed course.pcm').read_bytes(), b'MSU1' + bytes(36))
        self.assertEqual(self.inspect(), self.baseline)  # Normal reload, no repair needed.

    def test_existing_new_music_is_not_overwritten(self):
        before = self.pack / 'music' / self.old.with_suffix('.pcm').name
        before.parent.mkdir(exist_ok=True)
        before.write_bytes(b'old')
        after = self.pack / 'music/Renamed course.pcm'
        after.write_bytes(b'user choice')
        self.old.rename(self.new)
        self.inspect()
        self.assertEqual(after.read_bytes(), b'user choice')
        self.assertEqual(before.read_bytes(), b'old')

    def test_explicit_source_identity_and_unicode_filename(self):
        # An author-supplied nested ID need not follow the reconstruction convention.
        with zipfile.ZipFile(self.old) as archive:
            self.course['source_id'] = json.loads(archive.read('pack.json'))['id']
        self.index['id'] += '-other'
        data = (json.dumps(self.index) + '\n').encode()
        (self.pack / 'courses.json').write_bytes(data)
        manifest = json.loads((self.pack / 'pack.json').read_text())
        manifest['id'] = self.index['id']
        manifest['payload']['sha256'] = hashlib.sha256(data).hexdigest()
        (self.pack / 'pack.json').write_text(json.dumps(manifest))
        before = self.inspect()
        destination = self.old.with_name('Canvas—水.zip')
        self.old.rename(destination)
        self.assertEqual(self.inspect(), before)
        index = json.loads((self.pack / 'courses.json').read_bytes())
        self.assertEqual(index['courses'][0]['source'], 'courses/Canvas—水.zip')

    def test_missing_or_duplicate_identity_does_not_guess_or_change_index(self):
        self.old.rename(self.root / 'removed.zip')
        self.inspect(False)
        self.assertEqual((self.pack / 'courses.json').read_bytes(), self.original)
        shutil.copyfile(self.root / 'removed.zip', self.new)
        shutil.copyfile(self.new, self.new.with_name('another.zip'))
        self.inspect(False)
        self.assertEqual((self.pack / 'courses.json').read_bytes(), self.original)

    def test_failed_publication_keeps_music_and_index(self):
        before = self.pack / 'music' / self.old.with_suffix('.pcm').name
        before.parent.mkdir(exist_ok=True)
        before.write_bytes(b'old')
        self.old.rename(self.new)
        (self.pack / '.renamed-pack.tmp').write_text('interrupted update')
        self.inspect(False)
        self.assertEqual((self.pack / 'courses.json').read_bytes(), self.original)
        self.assertEqual(before.read_bytes(), b'old')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--tool', type=Path, required=True)
    parser.add_argument('--pack', type=Path, required=True)
    args, remaining = parser.parse_known_args()
    TOOL, PACK = args.tool.resolve(), args.pack.resolve()
    unittest.main(argv=[__file__] + remaining)
