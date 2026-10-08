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
            with patch.object(converter, 'read_stock', return_value=bytes(0x8000)), \
                 patch.object(converter, 'reviewed_profile', return_value=profile), \
                 patch.object(converter, 'audit_metadata', return_value={}), \
                 patch.object(converter, 'export_pack', side_effect=ValueError('Invalid course resource')):
                with self.assertRaisesRegex(ValueError, 'Invalid course'):
                    converter.convert(source, out, stock=source, exporter=tool, inspector=tool)
            self.assertFalse(out.exists())
            self.assertFalse(list(root.glob('.fzero-conversion-*')))
            self.assertEqual(source.read_bytes(), bytes(0x8000))

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
