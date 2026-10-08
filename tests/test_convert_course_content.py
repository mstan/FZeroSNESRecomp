"""ROM-free conversion boundaries; native corpus validation is run separately."""
from copy import deepcopy
import io
import json
from pathlib import Path
import stat
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
import zipfile
import zlib

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'tools'))
import convert_course_content as converter
from make_ips import make_ips
from parse_track_pack import fields


def archive_bytes(members):
    stream = io.BytesIO()
    with zipfile.ZipFile(stream, 'w', zipfile.ZIP_DEFLATED) as archive:
        for name, content in members:
            if isinstance(name, str):
                info = zipfile.ZipInfo(name)
                info.filename = name  # Write the unsafe spelling literally on Windows.
                info.compress_type = zipfile.ZIP_DEFLATED
                name = info
            archive.writestr(name, content)
    return stream.getvalue()


def bps_literal(source, target):
    def number(value):
        output = bytearray()
        while True:
            byte = value & 127; value >>= 7
            if not value:
                return bytes(output + bytes([byte | 128]))
            output.append(byte); value -= 1
    stream = b'BPS1'+number(len(source))+number(len(target))+number(0)
    stream += number(((len(target)-1) << 2) | 1)+target
    stream += struct.pack('<II', zlib.crc32(source), zlib.crc32(target))
    return stream+struct.pack('<I', zlib.crc32(stream))


def pcm(frames=8, loop=0):
    return b'MSU1'+struct.pack('<I', loop)+bytes(frames*4)


def metadata_donor():
    """Two reordered resources exercise generic consumers without a profile."""
    rom = bytearray(0x100000)
    def put(address, value):
        offset = ((address & 0x7f0000) >> 1) | (address & 0x7fff)
        rom[offset:offset+len(value)] = value
    def pointer(address):
        return address.to_bytes(3, 'little')
    put(0x00f7e0, bytes.fromhex('08 e2 30 c2 10 ae 59 10 bf')+pointer(0x109100)+bytes.fromhex('e2 10 ea ea ea'))
    put(0x10824d, bytes.fromhex('8a c2 30 29 ff 00 8d 5b 10 bb bf')+pointer(0x109110)+bytes.fromhex('29 ff 00 8d 59 10 20'))
    put(0x10845b, bytes.fromhex('ad 59 10 0a 0a aa bf')+pointer(0x109126)+bytes.fromhex('8d d9 0a bf')+pointer(0x109128)+bytes.fromhex('8d db 0a'))
    put(0x10846f, bytes.fromhex('ad 59 10 0a 6d 59 10 aa 8b bf')+pointer(0x109120)+bytes.fromhex('a8 e2 20 bf')+pointer(0x109122)+b'\x48')
    put(0x10839b, bytes.fromhex('da c2 30 ad 59 10 0a 6d 59 10 aa bf')+pointer(0x109140)+bytes.fromhex('85 00 bf')+pointer(0x109141)+bytes.fromhex('85 01 e2 30 fa'))
    put(0x109100, bytes([9, 72])); put(0x109110, bytes([1, 0]))
    put(0x109140, pointer(0x109200)+pointer(0x109300))
    put(0x109200, bytes.fromhex('10 53 01 82 1b ff')+bytes([converter.ALPHABET[9]])+b'\0')
    put(0x109300, bytes.fromhex('10 53 01 82 1b ff')+bytes([converter.ALPHABET[16]])+b'\0')
    put(0x02c26a, bytes.fromhex('a5 46 29 07 c9 06 d0 11 a5 53 a6 58 d0 08 a5 90 0a 0a 65 90 65 53 18 69 0c 60'))
    return bytes(rom)


class ConversionBoundaries(unittest.TestCase):
    def test_ips_bps_equivalence_and_zip_pair_selection(self):
        source, target = bytes(0x8000), b'A'+bytes(0x7fff)
        ips, bps = make_ips(source, target), bps_literal(source, target)
        with tempfile.TemporaryDirectory() as temp, patch('parse_track_pack.STOCK_SHA256', converter.digest(source)):
            path = Path(temp)/'pair.zip'
            path.write_bytes(archive_bytes([('course.ips', ips), ('course.bps', bps)]))
            result, report = converter.read_submission(path, source)
            self.assertEqual(result, target)
            self.assertEqual(report['patch_member'], 'course.bps')
            self.assertTrue(report['bps_crcs_verified'])
            self.assertEqual(len(report['archive_patch_targets']), 2)
            self.assertEqual(path.read_bytes(), archive_bytes([('course.ips', ips), ('course.bps', bps)]))

    def test_bps_crc_failure_and_missing_stock_are_rejected(self):
        source = bytes(0x8000)
        corrupt = bytearray(bps_literal(source, b'A'+source[1:])); corrupt[-1] ^= 1
        with tempfile.TemporaryDirectory() as temp, patch('parse_track_pack.STOCK_SHA256', converter.digest(source)):
            path = Path(temp)/'course.bps'; path.write_bytes(corrupt)
            with self.assertRaisesRegex(ValueError, 'CRC'):
                converter.read_submission(path, source)
            with self.assertRaisesRegex(ValueError, 'requires --stock'):
                converter.read_submission(path)

    def test_distinct_zip_targets_need_explicit_selection(self):
        source = bytes(0x8000)
        a, b = make_ips(source, b'A'+source[1:]), make_ips(source, b'B'+source[1:])
        data = archive_bytes([('a.ips', a), ('b.ips', b)])
        with patch('parse_track_pack.STOCK_SHA256', converter.digest(source)):
            with self.assertRaisesRegex(ValueError, 'distinct'):
                converter.select_patch(converter.zip_patches(data), source)
            selected, _ = converter.select_patch(converter.zip_patches(data, 'b.ips'), source)
            self.assertEqual(selected[1], 'b.ips')

    def test_archive_paths_links_and_case_aliases(self):
        for name in ('../track.ips', '/track.ips', 'C:/track.ips', 'folder\\track.ips', './track.ips', 'bad./track.ips'):
            with self.subTest(name=name), self.assertRaisesRegex(ValueError, 'unsafe'):
                converter.zip_patches(archive_bytes([(name, b'PATCHEOF')]))
        with self.assertRaisesRegex(ValueError, 'duplicate'):
            converter.zip_patches(archive_bytes([('track.ips', b'PATCHEOF'), ('TRACK.IPS', b'PATCHEOF')]))
        link = zipfile.ZipInfo('track.ips'); link.create_system = 3
        link.external_attr = (stat.S_IFLNK | 0o777) << 16
        with self.assertRaisesRegex(ValueError, 'link'):
            converter.zip_patches(archive_bytes([(link, b'PATCHEOF')]))

    def test_archive_member_and_expansion_bounds(self):
        data = archive_bytes([('course.ips', b'PATCHEOF'), ('readme.txt', bytes(100))])
        with patch.object(converter, 'ZIP_TOTAL_LIMIT', 100), self.assertRaisesRegex(ValueError, 'expanded'):
            converter.zip_patches(data)
        with patch.object(converter, 'ZIP_MEMBER_LIMIT', 1), self.assertRaisesRegex(ValueError, 'too many'):
            converter.zip_patches(data)
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp)/'large'; path.write_bytes(bytes(9))
            with self.assertRaisesRegex(ValueError, 'exceeds'):
                converter.read_bounded(path, 8)

    def test_disk_zip_streams_rom_and_only_audio_headers(self):
        raw = bytes(512)+bytes(0x8000)
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp)/'submission.zip'
            path.write_bytes(archive_bytes([('hack.smc', raw), ('hack-10.pcm', pcm(10000)),
                                           ('tools/optional.asm', b'not executed'), ('INSTRUCTIONS.txt', b'author notes')]))
            opened = []
            original = zipfile.ZipFile.open
            def observe(archive, entry, *args, **kwargs):
                opened.append(entry.filename if isinstance(entry, zipfile.ZipInfo) else entry)
                return original(archive, entry, *args, **kwargs)
            with patch.object(converter, 'read_bounded', side_effect=AssertionError('ZIP buffered whole')), \
                 patch.object(zipfile.ZipFile, 'open', observe):
                target, report = converter.read_submission(path)
            self.assertEqual(target, raw[512:])
            self.assertEqual(report['removed_copier_header_bytes'], 512)
            self.assertEqual(report['donor_member'], 'hack.smc')
            self.assertEqual(report['audio_inventory']['pcm_count'], 1)
            self.assertTrue(report['audio_inventory']['members'][0]['header_valid'])
            self.assertEqual(report['ignored_members'], ['tools/optional.asm', 'INSTRUCTIONS.txt'])
            self.assertNotIn('tools/optional.asm', opened)
            self.assertNotIn('INSTRUCTIONS.txt', opened)

    def test_ambiguous_zip_publishes_report_and_member_resolves(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp); source = root/'different.zip'
            source.write_bytes(archive_bytes([('a.sfc', bytes(0x8000)), ('b.sfc', b'B'+bytes(0x7fff))]))
            with patch('subprocess.run', side_effect=AssertionError('Ambiguous donor invoked code')):
                report = converter.convert(source, root/'review')
            self.assertEqual(report['status'], 'review-required')
            self.assertIn('different donor revisions', report['reason'])
            self.assertEqual(len(report['archive_donor_candidates']), 2)
            self.assertNotIn('target_sha256', report)
            target, selected = converter.read_submission(source, patch_member='b.sfc')
            self.assertEqual(target, b'B'+bytes(0x7fff))
            self.assertEqual(selected['donor_member'], 'b.sfc')

    def test_reviewed_donor_does_not_hide_another_unknown_revision(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp)/'mixed.zip'
            path.write_bytes(archive_bytes([('known.sfc', bytes(0x8000)), ('new.sfc', b'B'+bytes(0x7fff))]))
            profile = ROOT/'assets/track-packs/bower-league.ini'
            with patch.object(converter, 'reviewed_profile', side_effect=lambda data: profile if data[0] == 0 else None):
                target, report = converter.read_submission(path)
            self.assertIsNone(target)
            self.assertIn('different donor revisions', report['reason'])

    def test_audio_only_archive_has_clear_review_reason(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp); source = root/'music.zip'
            source.write_bytes(archive_bytes([('music-1.pcm', pcm()), ('README.md', b'music notes')]))
            report = converter.convert(source, root/'review')
            self.assertIn('audio-only', report['reason'])
            self.assertEqual(report['status'], 'review-required')
            self.assertEqual(report['audio_inventory']['mapped_pcm_count'], 0)
            self.assertFalse((root/'review/music').exists())

    def test_zip_compressed_file_and_path_collision_bounds(self):
        with tempfile.TemporaryDirectory() as temp:
            source = Path(temp)/'bound.zip'; source.write_bytes(archive_bytes([('hack.sfc', bytes(0x8000))]))
            with patch.object(converter, 'ZIP_ARCHIVE_LIMIT', 1), self.assertRaisesRegex(ValueError, 'Compressed ZIP'):
                converter.read_submission(source)
            with patch.object(converter, 'ZIP_FILE_LIMIT', 1), self.assertRaisesRegex(ValueError, 'expanded'):
                converter.read_submission(source)
            with patch.object(converter, 'ZIP_DIRECTORY_LIMIT', 1), self.assertRaisesRegex(ValueError, 'central directory'):
                converter.read_submission(source)
            with patch.object(converter, 'IMAGE_LIMIT', 0x7fff), self.assertRaisesRegex(ValueError, '32 KiB'):
                converter.decode_donor(bytes(0x8000), '.sfc', None)
        with self.assertRaisesRegex(ValueError, 'collision'):
            converter.zip_patches(archive_bytes([('folder', b'file'), ('folder/hack.ips', b'PATCHEOF')]))

    def audio_pack(self, root, members):
        source = root/'music.zip'; source.write_bytes(archive_bytes(members))
        pack = root/'pack'; pack.mkdir()
        index = dict(courses=[dict(id='a', source='courses/a.zip', music=dict(track=10)),
                              dict(id='b', source='courses/b.zip', music=dict(track=11))],
                     soundtracks=[dict(prefix='reviewed')], menu_music=dict(title='music/reviewed-4.pcm'))
        (pack/'courses.json').write_text(json.dumps(index))
        with zipfile.ZipFile(source) as archive:
            _, _, audio = converter.zip_inventory(archive)
        report = dict(audio_inventory=audio, donor_member='hack.ips', warnings=[])
        return source, pack, report

    def test_reviewed_audio_maps_course_and_menu_without_guessing(self):
        with tempfile.TemporaryDirectory() as temp:
            source, pack, report = self.audio_pack(Path(temp), [('reviewed-10.pcm', pcm()),
                ('nested/b.pcm', pcm(loop=2)), ('reviewed-4.pcm', pcm(16)),
                ('reviewed-99.pcm', pcm()), ('other-11.pcm', pcm()), ('notes/song.ogg', b'unsupported')])
            before = source.read_bytes()
            converter.copy_archive_audio(source, report, pack)
            self.assertEqual((pack/'music/a.pcm').read_bytes(), pcm())
            self.assertEqual((pack/'music/b.pcm').read_bytes(), pcm(loop=2))
            self.assertEqual((pack/'music/reviewed-4.pcm').read_bytes(), pcm(16))
            self.assertEqual(report['audio_inventory']['mapped_pcm_count'], 3)
            self.assertEqual(report['audio_inventory']['unmapped_pcm_count'], 2)
            self.assertEqual({p.name for p in (pack/'music').iterdir()}, {'a.pcm', 'b.pcm', 'reviewed-4.pcm'})
            self.assertTrue(any('unchanged ZIP' in warning for warning in report['warnings']))
            self.assertTrue(any('other audio' in warning for warning in report['warnings']))
            self.assertEqual(source.read_bytes(), before)

    def test_ambiguous_audio_skips_disputed_file_and_keeps_other_music(self):
        with tempfile.TemporaryDirectory() as temp:
            source, pack, report = self.audio_pack(Path(temp), [('reviewed-10.pcm', pcm()),
                ('a.pcm', pcm(16)), ('reviewed-11.pcm', pcm())])
            converter.copy_archive_audio(source, report, pack)
            self.assertFalse((pack/'music/a.pcm').exists())
            self.assertEqual((pack/'music/b.pcm').read_bytes(), pcm())
            self.assertEqual(report['audio_inventory']['mapped_pcm_count'], 1)
            self.assertTrue(any('multiple matching' in warning for warning in report['warnings']))
            self.assertEqual(len([item for item in report['audio_inventory']['members'] if 'mapping_reason' in item]), 2)

    def test_corrupt_audio_warns_without_partial_file(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            info = zipfile.ZipInfo('reviewed-10.pcm'); info.compress_type = zipfile.ZIP_STORED
            source, pack, report = self.audio_pack(root, [(info, pcm(20000)), ('reviewed-11.pcm', pcm())])
            with zipfile.ZipFile(source) as archive:
                entry = archive.getinfo('reviewed-10.pcm')
                offset = entry.header_offset+30+len(entry.filename.encode())+len(entry.extra)+entry.file_size-1
            corrupt = bytearray(source.read_bytes()); corrupt[offset] ^= 1; source.write_bytes(corrupt)
            converter.copy_archive_audio(source, report, pack)
            self.assertFalse((pack/'music/a.pcm').exists())
            self.assertEqual((pack/'music/b.pcm').read_bytes(), pcm())
            self.assertFalse(list(pack.rglob('*.part')))
            self.assertEqual(report['audio_inventory']['mapped_pcm_count'], 1)
            self.assertIn('CRC', report['audio_inventory']['members'][0]['copy_error'])
            self.assertTrue(any('Courses are available' in warning for warning in report['warnings']))

    def test_invalid_pcm_header_is_inventory_only(self):
        with tempfile.TemporaryDirectory() as temp:
            source, pack, report = self.audio_pack(Path(temp), [('reviewed-10.pcm', pcm(loop=9)),
                ('reviewed-11.pcm', b'not PCM'), ('reviewed-4.pcm', pcm(16))])
            converter.copy_archive_audio(source, report, pack)
            self.assertEqual(report['audio_inventory']['mapped_pcm_count'], 1)
            self.assertEqual(report['audio_inventory']['unmapped_pcm_count'], 2)
            self.assertFalse((pack/'music/a.pcm').exists())

    def test_small_corrupt_pcm_header_does_not_reject_archive_donor(self):
        with tempfile.TemporaryDirectory() as temp:
            source = Path(temp)/'small-corrupt.zip'
            info = zipfile.ZipInfo('hack-10.pcm'); info.compress_type = zipfile.ZIP_STORED
            source.write_bytes(archive_bytes([('hack.sfc', bytes(0x8000)), (info, pcm())]))
            with zipfile.ZipFile(source) as archive:
                entry = archive.getinfo('hack-10.pcm')
                offset = entry.header_offset+30+len(entry.filename.encode())+len(entry.extra)+8
            raw = bytearray(source.read_bytes()); raw[offset] ^= 1; source.write_bytes(raw)
            target, report = converter.read_submission(source)
            self.assertEqual(target, bytes(0x8000))
            recording = report['audio_inventory']['members'][0]
            self.assertFalse(recording['header_valid'])
            self.assertIn('CRC', recording['header_error'])

    def test_duplicate_audio_publication_budget_keeps_courses(self):
        with tempfile.TemporaryDirectory() as temp:
            source, pack, report = self.audio_pack(Path(temp), [('reviewed-10.pcm', pcm())])
            index = json.loads((pack/'courses.json').read_text())
            index['courses'][1]['music']['track'] = 10
            (pack/'courses.json').write_text(json.dumps(index))
            with patch.object(converter, 'ZIP_TOTAL_LIMIT', len(pcm())+1):
                converter.copy_archive_audio(source, report, pack)
            self.assertFalse((pack/'music').exists())
            self.assertTrue((pack/'courses.json').is_file())
            self.assertEqual(report['audio_inventory']['mapped_pcm_count'], 0)
            self.assertTrue(any('publication limit' in warning for warning in report['warnings']))

    def test_generic_metadata_probe_is_evidence_not_qualification(self):
        donor = metadata_donor(); report = converter.probe_fzedit_metadata(donor)
        self.assertEqual(report['status'], 'recognized-resource-metadata')
        self.assertEqual(report['internal_resource_count'], 2)
        self.assertEqual(report['source_order_prefix'], [1, 0])
        self.assertEqual([row['name'] for row in report['tracks']], ['Q', 'J'])
        self.assertEqual([row['spc_index'] for row in report['tracks']], [8, 1])
        self.assertEqual([row['msu_track'] for row in report['tracks']], [12, 13])
        self.assertFalse(report['executable_compatibility_verified'])
        self.assertFalse(report['cup_labels_verified'])
        changed = bytearray(donor); changed[0x77e0] = 0
        self.assertEqual(converter.probe_fzedit_metadata(changed)['status'], 'unrecognized-loader')
        changed = bytearray(donor); changed[0x1426a] = 0
        report = converter.probe_fzedit_metadata(changed)
        self.assertFalse(report['msu_selector']['recognized'])
        self.assertFalse(any('msu_track' in row for row in report['tracks']))

    def test_unknown_rom_is_report_only_and_exit_two(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp); source = root/'new-hack.sfc'
            raw = bytes(512)+bytes(0x8000); source.write_bytes(raw)
            output = root/'review'
            with patch('subprocess.run', side_effect=AssertionError('Unknown donor invoked code')):
                self.assertEqual(converter.main([str(source), '--out', str(output)]), 2)
            self.assertEqual(source.read_bytes(), raw)
            self.assertEqual({p.name for p in output.iterdir()}, {'conversion-report.json', 'REVIEW.txt'})
            report = json.loads((output/'conversion-report.json').read_text())
            self.assertEqual(report['status'], 'review-required')
            self.assertEqual(report['removed_copier_header_bytes'], 512)
            self.assertFalse(report['source_rom_exported'])
            self.assertFalse(report['donor_code_executed'])

    def test_existing_output_is_never_replaced(self):
        with tempfile.TemporaryDirectory() as temp:
            out = Path(temp)/'existing'; out.mkdir(); (out/'sentinel').write_bytes(b'owner content')
            with self.assertRaisesRegex(ValueError, 'Refusing'):
                converter.convert(Path(temp)/'missing.ips', out)
            self.assertEqual((out/'sentinel').read_bytes(), b'owner content')

    def test_failed_known_export_publishes_no_partial_pack(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp); source = root/'known.sfc'; source.write_bytes(bytes(0x8000))
            tool = root/'native.exe'; tool.write_bytes(b'not executed')
            out = root/'not-published'
            profile = ROOT/'assets/track-packs/bower-league.ini'
            workspaces = []
            def fail_export(*args, **kwargs):
                workspaces.append(Path(args[3]).parents[1])
                raise ValueError('Invalid course resource')
            with patch.object(converter, 'read_stock', return_value=bytes(0x8000)), \
                 patch.object(converter, 'reviewed_profile', return_value=profile), \
                 patch.object(converter, 'audit_metadata', return_value={}), \
                 patch.object(converter, 'export_pack', side_effect=fail_export):
                with self.assertRaisesRegex(ValueError, 'Invalid course'):
                    converter.convert(source, out, stock=source, exporter=tool, inspector=tool)
            self.assertFalse(out.exists())
            self.assertEqual(len(workspaces), 1)
            self.assertFalse(workspaces[0].exists())
            self.assertEqual(source.read_bytes(), bytes(0x8000))

    def check_publication(self, root, *, fail_copy=False):
        source = root/'known.sfc'; source.write_bytes(bytes(0x8000))
        tool = root/'native.exe'; tool.write_bytes(b'not executed')
        out = root/('nested-install-stage-'+'x'*80)/'mods/.imports/123456789012345-0/converted'
        profile = ROOT/'assets/track-packs/bower-league.ini'
        workspaces = []
        def export(*args, **kwargs):
            pack = Path(args[3]); pack.mkdir(parents=True)
            (pack/'pack.json').write_bytes(b'complete validated pack')
            workspaces.append(pack.parents[1])
            return dict(record_hashes={})
        def inspect(inspector, temp, pack, *args):
            # Model the native loader's deepest cache member path. A workspace
            # under this destination reproduces the Windows failure (>260).
            suffix = Path('mods/packs/.cache/sources')/('a'*64)/'brutal-wind-1_Horizon_Tilemap.gif'
            if len(str(temp/suffix)) >= 260:
                raise ValueError('Cannot create cached asset: Windows path limit')
            return dict(native_loader_passed=True)
        copy = converter.shutil.copytree
        def copy_pack(source, dest):
            if fail_copy:
                Path(dest).mkdir(); (Path(dest)/'partial').write_bytes(b'incomplete copy')
                raise OSError('Publication disk is full')
            return copy(source, dest)
        with patch.object(converter, 'read_stock', return_value=bytes(0x8000)), \
             patch.object(converter, 'reviewed_profile', return_value=profile), \
             patch.object(converter, 'audit_metadata', return_value={}), \
             patch.object(converter, 'export_pack', side_effect=export), \
             patch.object(converter, 'inspect_roundtrip', side_effect=inspect), \
             patch.object(converter.shutil, 'copytree', side_effect=copy_pack):
            if fail_copy:
                with self.assertRaisesRegex(OSError, 'disk is full'):
                    converter.convert(source, out, stock=source, exporter=tool, inspector=tool)
                self.assertFalse(out.exists())
            else:
                report = converter.convert(source, out, stock=source, exporter=tool, inspector=tool)
                self.assertEqual(report['status'], 'converted')
                self.assertEqual((out/'pack.json').read_bytes(), b'complete validated pack')
        self.assertFalse(workspaces[0].exists())
        self.assertFalse(list(out.parent.glob('.fzc-publish-*')))
        self.assertEqual(source.read_bytes(), bytes(0x8000))

    def test_deep_destination_keeps_native_cache_below_windows_path_limit(self):
        with tempfile.TemporaryDirectory() as temp:
            self.check_publication(Path(temp))

    def test_publication_copy_failure_leaves_no_partial_pack(self):
        with tempfile.TemporaryDirectory() as temp:
            self.check_publication(Path(temp), fail_copy=True)

    def test_duplicate_and_unsafe_registry_identities(self):
        data = fields(ROOT/'assets/track-packs/bower-league.ini')
        converter.validate_identities(data)
        variants = []
        duplicate = deepcopy(data); duplicate['cup'].append(duplicate['cup'][0]); variants.append(duplicate)
        duplicate = deepcopy(data); duplicate['track'].append(duplicate['track'][0]); variants.append(duplicate)
        unsafe = deepcopy(data); unsafe['id'] = ['../pack']; variants.append(unsafe)
        missing = deepcopy(data); missing['track'][0] = missing['track'][0].replace('|bower|', '|absent|'); variants.append(missing)
        slot = deepcopy(data); slot['track'][0] = slot['track'][0][:-1]+slot['track'][1][-1]; variants.append(slot)
        for data in variants:
            with self.subTest(data=data), self.assertRaises(ValueError):
                converter.validate_identities(data)

    def test_unknown_diff_report_is_bounded(self):
        summary = converter.changed_summary(bytes(1000), bytes([1, 0])*500)
        self.assertEqual(summary['changed_stock_bytes'], 500)
        self.assertEqual(summary['changed_range_count'], 500)
        self.assertEqual(len(summary['changed_ranges']), 128)
        self.assertTrue(summary['changed_ranges_truncated'])

    def test_native_presentation_changes_fail_even_with_same_record_hash(self):
        with tempfile.TemporaryDirectory() as temporary:
            temp = Path(temporary); pack = temp/'packs/example'; pack.mkdir(parents=True)
            baseline = temp/'native'; baseline.mkdir(); (baseline/'course.fzc').write_bytes(b'course')
            (pack/'courses.json').write_text(json.dumps(dict(id='example', courses=[dict(id='course')])))
            record_hash = '0'*64
            def inspect(*args, **kwargs):
                dump = temp/'roundtrip'; (dump/'example--course.fzc').write_bytes(b'changed title glyph')
                return subprocess.CompletedProcess(args[0], 0, f'example/course {record_hash}\n', '')
            with patch('subprocess.run', side_effect=inspect), self.assertRaisesRegex(ValueError, 'native course fields'):
                converter.inspect_roundtrip('inspector.exe', temp, pack, baseline, dict(record_hashes={'course': record_hash}))


if __name__ == '__main__':
    unittest.main()
