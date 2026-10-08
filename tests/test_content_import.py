"""Native importer integration: no ROM needed; optional private FZEdit sample.

python tests/test_content_import.py --tool build/FZeroImportContent.exe --sample HM.zip
"""
import argparse
import json
import os
from pathlib import Path
import shutil
import stat
import subprocess
import tempfile
import unittest
import zipfile

TOOL = SAMPLE = None


class ImportTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='fzero-import-test-')
        self.root = Path(self.temp.name)
        self.mods = self.root / 'mods'

    def tearDown(self):
        self.temp.cleanup()

    def run_import(self, source, name='Imported course', succeeds=False):
        result = subprocess.run([str(TOOL), str(source), str(self.mods), name],
                                cwd=self.root, capture_output=True, text=True,
                                encoding='utf-8', errors='replace', timeout=45)
        self.assertIn(result.returncode, (0, 1), result.stderr)
        self.assertEqual(result.returncode, 0 if succeeds else 1, result.stderr)
        self.assertFalse(any((self.mods / '.imports').glob('*')))
        return result

    def archive(self, entries, name='input.zip'):
        path = self.root / name
        with zipfile.ZipFile(path, 'w', zipfile.ZIP_DEFLATED) as z:
            for key, value in entries:
                z.writestr(key, value)
        return path

    def raw(self, extra=''):
        root = self.root / 'project'
        root.mkdir()
        (root / 'course.fzm').write_text(
            'Version=FZEdit Version 0.9\nInGameMapName=Test course\n' + extra)
        return root

    def test_bad_paths(self):
        for index, name in enumerate(('../outside.txt', '/outside.txt', 'C:/bad.txt',
                                      'CON.foo.txt', 'x/../bad.txt', 'x\\bad.txt')):
            with self.subTest(name=name):
                self.run_import(self.archive([(name, b'bad')], f'bad{index}.zip'))
                self.assertFalse((self.root / 'outside.txt').exists())

    def test_case_collisions(self):
        self.run_import(self.archive([('a.txt', 'a'), ('A.txt', 'b')]))

    def test_symlink(self):
        entry = zipfile.ZipInfo('linked.txt')
        entry.create_system = 3
        entry.external_attr = (stat.S_IFLNK | 0o777) << 16
        self.run_import(self.archive([(entry, '../secret.txt')]))

    def test_directory_limit(self):
        self.run_import(self.archive([(f'd{i}/', '') for i in range(20001)]))

    def test_path_depth(self):
        self.run_import(self.archive([('x/' * 40 + 'a.txt', 'bad')]))

    def test_missing_project_files(self):
        self.run_import(self.raw())
        self.assertFalse((self.mods / 'packs').exists())

    def test_deep_reconstruction_is_rejected_without_crashing(self):
        source = self.raw('ReconstructionFile=deep.json\n')
        (source / 'deep.json').write_text(
            '{"format":"fzero.reconstruction","version":1,"groups":{},"unused":'
            + '[' * 100000 + '0' + ']' * 100000 + '}')
        result = self.run_import(source)
        self.assertTrue(any(word in result.stderr.lower() for word in ('nest', 'depth', 'deep')), result.stderr)
        self.assertFalse((self.mods / 'packs').exists())

    def test_patch_pair_is_routed_to_converter(self):
        result = self.run_import(self.archive([('hack.ips', 'PATCHEOF'), ('hack.bps', 'BPS1')]))
        self.assertIn('conversion tools', result.stderr)

    def test_rom_zip_is_routed_to_converter(self):
        result = self.run_import(self.archive([('download/hack.SMC', bytes(512)),
                                               ('download/tool.exe', 'not executed'),
                                               ('download/notes.pdf', 'documentation')]))
        self.assertIn('conversion tools', result.stderr)

    def test_music_only_zip_explains_mapping(self):
        result = self.run_import(self.archive([('hack-1.pcm', b'MSU1')]))
        self.assertIn('music but no courses', result.stderr)

    def test_user_name_is_not_a_path(self):
        self.run_import(self.raw(), name='x\nunsafe')

    def installed_sample(self):
        if SAMPLE is None:
            self.skipTest('Supply --sample for the private FZEdit source acceptance tests')
        self.run_import(SAMPLE, 'My Huckmine', succeeds=True)
        return next((self.mods / 'packs').iterdir())

    def test_accept_name_preserve_course_and_duplicate_reject(self):
        pack = self.installed_sample()
        d = json.loads((pack / 'courses.json').read_text())
        self.assertEqual(d['name'], 'My Huckmine')
        self.assertNotEqual(d['courses'][0]['name'], 'My Huckmine')
        before = (pack / 'courses.json').read_bytes()
        self.run_import(SAMPLE, 'Different display name')
        self.assertEqual(before, (pack / 'courses.json').read_bytes())

    def test_unrelated_broken_manifest_does_not_block_import(self):
        broken = self.mods / 'packs' / 'broken'
        broken.mkdir(parents=True)
        (broken / 'pack.json').write_text('{bad JSON')
        self.installed_sample()

    def test_included_id_cannot_be_replaced_even_if_folder_is_missing(self):
        pack = self.installed_sample()
        ident = json.loads((pack / 'pack.json').read_text())['id']
        (self.mods / '.bundled-packs.json').write_text(json.dumps(
            {'format': 1, 'packs': [{'id': ident, 'title': 'Included course'}]}))
        shutil.rmtree(pack)  # Simulate a missing bundled folder in our fixture.
        result = self.run_import(SAMPLE)
        self.assertIn('included with F-Zero Forever', result.stderr)
        self.assertFalse(pack.exists())

    def pack_zip(self, pack, name):
        return self.archive([(p.relative_to(pack).as_posix(), p.read_bytes())
                             for p in pack.rglob('*') if p.is_file()], name)

    def test_ready_zip_and_title(self):
        pack = self.installed_sample()
        archive = self.pack_zip(pack, 'ready.zip')
        original = json.loads((pack / 'courses.json').read_text())
        shutil.rmtree(pack)  # Own temporary test fixture only.
        self.run_import(archive, 'Chosen title', succeeds=True)
        final = json.loads(next((self.mods / 'packs').glob('*/courses.json')).read_text())
        self.assertEqual(final['courses'], original['courses'])
        self.assertEqual(final['cups'], original['cups'])
        self.assertEqual(final['id'], original['id'])
        self.assertEqual(final['name'], 'Chosen title')

    def test_two_existing_duplicate_zips_do_not_allow_a_third(self):
        pack = self.installed_sample()
        archive = self.pack_zip(pack, 'ready.zip')
        shutil.copyfile(archive, self.mods / 'packs' / 'a.zip')
        shutil.copyfile(archive, self.mods / 'packs' / 'b.zip')
        shutil.rmtree(pack)
        result = self.run_import(archive)
        self.assertIn('already installed', result.stderr)

    def test_conversion_warnings_are_not_hidden_after_success(self):
        pack = self.installed_sample()
        (pack / 'conversion-report.json').write_text(json.dumps(
            {'warnings': ['One recording could not be mapped. Original ZIP is unchanged.']}))
        archive = self.pack_zip(pack, 'with-warning.zip')
        shutil.rmtree(pack)
        result = self.run_import(archive, succeeds=True)
        self.assertIn('One recording could not be mapped', result.stdout)

    def test_tampered_payload_is_rejected(self):
        pack = self.installed_sample()
        (pack / 'courses.json').write_text('{}')
        archive = self.pack_zip(pack, 'tampered.zip')
        shutil.rmtree(pack)
        self.run_import(archive)

    def test_plain_zip_with_directory_entries(self):
        if SAMPLE is None:
            self.skipTest('Supply --sample')
        with zipfile.ZipFile(SAMPLE) as z:
            entries = [('project/', '')] + [('project/' + n, z.read(n))
                                            for n in z.namelist() if not n.endswith('/')]
        self.run_import(self.archive(entries), succeeds=True)

    def test_nested_archive_expansion_is_bounded_before_decode(self):
        pack = self.installed_sample()
        nested = self.root / 'oversized.zip'
        with zipfile.ZipFile(nested, 'w', zipfile.ZIP_DEFLATED) as z:
            with z.open('large.bin', 'w') as stream:
                chunk = bytes(1024 * 1024)
                for _ in range(257):
                    stream.write(chunk)
        shutil.copyfile(nested, pack / 'oversized.zip')
        archive = self.pack_zip(pack, 'bounded.zip')
        shutil.rmtree(pack)
        result = self.run_import(archive)
        self.assertIn('size limit', result.stderr)
        self.assertFalse((self.root / 'mods/packs/.cache').exists())

    def test_uppercase_fzm_extension(self):
        if SAMPLE is None:
            self.skipTest('Supply --sample')
        with zipfile.ZipFile(SAMPLE) as z:
            entries = [(n[:-4] + '.FZM' if n.lower().endswith('.fzm') else n, z.read(n))
                       for n in z.namelist() if not n.endswith('/')]
        self.run_import(self.archive(entries), succeeds=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--tool', type=Path, required=True)
    parser.add_argument('--sample', type=Path)
    args, remaining = parser.parse_known_args()
    TOOL = args.tool.resolve()
    SAMPLE = args.sample.resolve() if args.sample else None
    unittest.main(argv=[__file__, *remaining])
