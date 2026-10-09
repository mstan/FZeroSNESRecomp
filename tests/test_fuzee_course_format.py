"""ROM-free Fuzee format fixtures and malformed-resource boundaries."""
from pathlib import Path
import struct
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import fuzee_course_format as fuzee


def put(rom, address, value):
    position = (address >> 16) * 0x8000 + (address & 0x7fff)
    rom[position:position+len(value)] = value


def fixture():
    rom = bytearray(0x100000)
    for address, signature in fuzee.SIGNATURES.items():
        put(rom, address, signature)
    put(rom, 0x009f08, fuzee.CLASSIC_SELECTOR)
    # Index grid plus two block definitions straddle a LoROM bank boundary.
    put(rom, 0x11fff0, bytes(512) + struct.pack('<16H', *([0x7000]*16))
        + struct.pack('<16H', *([0x7020]*16)))
    put(rom, 0x159000, struct.pack('<32H', *range(0x1000, 0x1010), *range(0x2000, 0x2010)))
    put(rom, 0x039f00, struct.pack('<4H', 0x1fff, 36, 0x5900, 4) * 9)
    # Replace a 2x2 square at block (1,1), selecting two opposite corners.
    put(rom, 0x118100, struct.pack('<H4BH', 33, 1, 0, 0, 1, 0xffff))
    put(rom, 0x02e129, bytes([0xe0, 0xc7, 0xd7] + [0xe0]*19))
    put(rom, 0x108000, struct.pack('<22H', *([0x9000]*22)))
    put(rom, 0x109000, struct.pack('<BHBBHHB', 1, 0x9100, 0, 1, 1000, 2000, 0))
    put(rom, 0x109100, struct.pack('<6H', 0x9200, 0x9300, 0x9400, 0x9500, 0x9600, 0x9700))
    for address, data in [(0x109200, b'\x01\xfe'), (0x109300, b'\xff\x03'),
                          (0x109400, b'\xa1\xb2'), (0x109500, b'\x11\x12'),
                          (0x109600, b'\x21\x22'), (0x109700, b'\x31\x32')]:
        put(rom, address, data)
    return rom


def cell(data, x, y):
    return struct.unpack_from('<H', data, (y * 512 + x) * 2)[0]


class FuzeeFormat(unittest.TestCase):
    def test_bank_crossing_and_map_orientation(self):
        data = fuzee.road_map(fixture(), 0)
        self.assertEqual(len(data), 512*256*2)
        for x, y in [(0, 0), (511, 255), (27, 19), (55, 75)]:
            self.assertEqual(cell(data, x, y), 0x1000 + x % 16)

    def test_variant_replaces_exact_square_without_touching_base(self):
        rom = fixture()
        base = fuzee.road_map(rom, 7)
        alternate = fuzee.road_map(rom, 7, 1)
        self.assertEqual(cell(base, 17, 17), 0x1001)
        self.assertEqual(cell(alternate, 17, 17), 0x2001)
        self.assertEqual(cell(alternate, 33, 17), 0x1001)
        self.assertEqual(cell(alternate, 17, 33), 0x1001)
        self.assertEqual(cell(alternate, 33, 33), 0x2001)
        self.assertEqual(cell(alternate, 49, 49), 0x1001)

    def test_selector_order_keeps_practice_aliases(self):
        report, maps = fuzee.decode(fixture())
        self.assertEqual(len(report['courses']), 22)
        self.assertEqual((report['courses'][1]['venue'], report['courses'][1]['variant']), (7, 1))
        self.assertEqual(report['courses'][2]['variant'], 2)
        self.assertEqual(report['courses'][14]['cup_index'], 2)
        self.assertEqual(report['courses'][14]['race_index'], 4)
        self.assertEqual(report['courses'][15]['mode'], 'practice')
        self.assertEqual(report['courses'][15]['map'], report['courses'][0]['map'])
        self.assertEqual(set(maps), {'0-0', '7-1', '7-2'})
        self.assertFalse(report['playable_pack_created'])

    def test_signed_checkpoint_deltas_and_ai_preservation(self):
        sections = fuzee.checkpoints(fixture(), 0x9000)
        self.assertEqual(sections[0]['origin'], [1000, 2000])
        self.assertEqual(sections[0]['points'], [
            dict(x=1008, y=1992, flags=0xa1, ai=[0x11, 0x21, 0x31]),
            dict(x=992, y=2016, flags=0xb2, ai=[0x12, 0x22, 0x32])])

    def test_branch_keeps_its_own_origin(self):
        rom = fixture()
        put(rom, 0x109009, struct.pack('<BHBBHHB', 0xff, 0x9100, 0, 0, 3000, 4000, 0))
        sections = fuzee.checkpoints(rom, 0x9000)
        self.assertEqual(len(sections), 2)
        self.assertEqual(sections[1]['origin'], [3000, 4000])
        self.assertEqual(sections[1]['points'][0]['x'], 3008)
        self.assertEqual(sections[1]['points'][0]['y'], 3992)

    def test_signature_and_truncation_do_not_identify_editor(self):
        rom = fixture()
        put(rom, 0x00a060, b'\xea')
        self.assertFalse(fuzee.recognized(rom))
        self.assertFalse(fuzee.recognized(bytes(0x8000)))
        with self.assertRaisesRegex(ValueError, 'Not a recognized'):
            fuzee.decode(rom)

    def test_dictionary_indices_and_row_pointers_are_bounded(self):
        for address, value in [(0x11fff0, b'\xff'),
                               (0x1281f0, b'\x01\x70'),
                               (0x1281f0, b'\xe0\x6f')]:
            with self.subTest(address=address, value=value):
                rom = fixture()
                put(rom, address, value)
                with self.assertRaises(ValueError):
                    fuzee.road_map(rom, 0)

    def test_replacement_cannot_wrap_to_next_row(self):
        rom = fixture()
        put(rom, 0x118100, struct.pack('<H', 31))
        with self.assertRaisesRegex(ValueError, '2x2'):
            fuzee.road_map(rom, 7, 1)

    def test_replacement_requires_terminator(self):
        rom = fixture()
        put(rom, 0x118100, bytes(256))
        with self.assertRaises(ValueError):
            fuzee.road_map(rom, 7, 1)

    def test_checkpoint_arrays_cannot_wrap_into_rom(self):
        rom = fixture()
        put(rom, 0x109100, b'\xff\xff')
        with self.assertRaisesRegex(ValueError, 'checkpoint array'):
            fuzee.checkpoints(rom, 0x9000)

    def test_checkpoint_headers_and_tables_cannot_cross_bank(self):
        rom = fixture()
        with self.assertRaisesRegex(ValueError, 'crosses its bank'):
            fuzee.checkpoints(rom, 0xfff8)
        put(rom, 0x109001, b'\xf8\xff')
        with self.assertRaisesRegex(ValueError, 'crosses its bank'):
            fuzee.checkpoints(rom, 0x9000)

    def test_unsupported_variant_is_rejected(self):
        with self.assertRaisesRegex(ValueError, 'venue/variant'):
            fuzee.road_map(fixture(), 3, 2)


if __name__ == '__main__':
    unittest.main()
