"""Inspect Fuzee 0.04 course data without running the donor's executable code.

This decodes road definitions and checkpoint/AI arrays, not a playable pack.
See docs/FUZEE_FORMAT.md for provenance and remaining conversion work.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct


# These consumers are documented by the author's _task_main.cpp ROM writer.
# Match several independent instructions, not an editor name or ROM digest.
SIGNATURES = {
    0x009f63: bytes.fromhex('bf 00 80 11'),       # Variant offset table
    0x009f76: bytes.fromhex('ea'),                # Separate venue 2/3 maps
    0x009f7e: bytes.fromhex('20 da 9f'),          # Direct block decoder
    0x009f88: bytes.fromhex('20 da 9f'),          # Direct row decoder too
    0x009f97: bytes.fromhex('f4 11 00'),          # Variant data bank
    0x009fa1: bytes.fromhex('be 00 81'),
    0x009fa6: bytes.fromhex('b9 02 81'),
    0x009fad: bytes.fromhex('b9 04 81'),
    0x00a006: bytes(range(0x10, 0x20)),           # Packed pointer banks
    0x00a060: bytes.fromhex('b9 06 a0'),
    0x00d60b: bytes.fromhex('f4 10 00'),          # Checkpoint bank
    0x00d625: bytes.fromhex('bd 1e 80'),          # Practice checkpoint table
    0x00d647: bytes.fromhex('bd 00 80'),          # GP checkpoint table
    0x00a040: bytes.fromhex('bf 00 9f 03 85 22 bf 02 9f 03 85 26'),
}
CLASSIC_SELECTOR = bytes.fromhex(
    '08 e2 30 a9 0f a6 58 d0 06 a5 90 0a 0a 65 90 '
    '18 65 53 aa bf 29 e1 02 8d de 0a 29 0f 8d f5 0c a8')
MAP_WIDTH = 512                 # 16-pixel chips
MAP_HEIGHT = 256
MAX_CHECKPOINTS = 254


def read(rom, address, length):
    """Strict LoROM read; never wrap a bad pointer into unrelated ROM data."""
    bank, offset = address >> 16, address & 0xffff
    if not 0 <= bank < 0x40 or offset < 0x8000 or length < 0:
        raise ValueError(f'Invalid Fuzee ROM address {address:06x}')
    physical = bank * 0x8000 + offset - 0x8000
    if physical + length > len(rom):
        raise ValueError(f'Truncated Fuzee resource at {address:06x}')
    return rom[physical:physical + length]


def word(data, offset=0):
    return int.from_bytes(data[offset:offset+2], 'little')


def recognized(rom):
    try:
        return (read(rom, 0x009f08, len(CLASSIC_SELECTOR)) == CLASSIC_SELECTOR
                and all(read(rom, address, len(signature)) == signature
                        for address, signature in SIGNATURES.items()))
    except ValueError:
        return False


def packed_resource(rom, entry):
    pointer, paragraphs = struct.unpack('<HH', entry)
    if pointer & 0x800 == 0 or not paragraphs:
        raise ValueError('Invalid Fuzee packed resource pointer/size')
    bank = 0x10 + (pointer >> 12)
    address = (bank << 16) | ((pointer & 0xfff) << 4)
    # Read physical bytes across LoROM bank boundaries, as the game does.
    return read(rom, address, paragraphs * 16)


def region_resources(rom, venue):
    entry = read(rom, 0x039f00 + venue * 8, 8)
    blocks = packed_resource(rom, entry[:4])
    rows = packed_resource(rom, entry[4:])
    if not 0x220 <= len(blocks) < 0x2200 or (len(blocks) - 0x200) % 32:
        raise ValueError('Invalid Fuzee block dictionary size')
    if not 32 <= len(rows) < 0x9000 or len(rows) % 32:
        raise ValueError('Invalid Fuzee row dictionary size')
    return blocks[:0x200], blocks[0x200:], rows


def variant_blocks(rom, venue, variant, base):
    if variant == 0:
        return base
    if variant not in (1, 2) or variant == 2 and venue != 7:
        raise ValueError('Invalid Fuzee venue/variant combination')
    offset = read(rom, 0x118000 + venue + (2 if variant == 2 else 0), 1)[0]
    patches = read(rom, 0x118100, 0x100)
    indices = bytearray(base)
    while offset + 2 <= len(patches):
        position = word(patches, offset)
        if position == 0xffff:
            return bytes(indices)
        if offset + 6 > len(patches) or position >= 512 or position % 32 == 31 or position // 32 == 15:
            raise ValueError('Invalid Fuzee 2x2 map replacement')
        indices[position:position+2] = patches[offset+2:offset+4]
        indices[position+32:position+34] = patches[offset+4:offset+6]
        offset += 6
    raise ValueError('Unterminated Fuzee map replacements')


def road_map(rom, venue, variant=0):
    """Expand to 512x256 little-endian chip-definition offsets (not pixels)."""
    base, blocks, rows = region_resources(rom, venue)
    indices = variant_blocks(rom, venue, variant, base)
    output = bytearray(MAP_WIDTH * MAP_HEIGHT * 2)
    for position, index in enumerate(indices):
        if (index + 1) * 32 > len(blocks):
            raise ValueError('Fuzee block index exceeds dictionary')
        for row in range(16):
            pointer = word(blocks, index * 32 + row * 2) - 0x7000
            if pointer < 0 or pointer % 32 or pointer + 32 > len(rows):
                raise ValueError('Fuzee row pointer exceeds dictionary')
            destination = ((position // 32 * 16 + row) * MAP_WIDTH + position % 32 * 16) * 2
            output[destination:destination+32] = rows[pointer:pointer+32]
    return bytes(output)


def checkpoints(rom, pointer):
    if pointer < 0x8000:
        raise ValueError('Invalid Fuzee checkpoint pointer')
    address = 0x100000 | pointer
    def route_read(address, length):
        if address < 0x108000 or address + length > 0x110000:
            raise ValueError('Fuzee checkpoint resource crosses its bank')
        return read(rom, address, length)

    sections = []
    total = 0
    for section in range(2):
        header = route_read(address, 9)
        kind = header[0]
        if kind != (1 if section == 0 else 0xff):
            raise ValueError('Unrecognized Fuzee checkpoint section')
        start, end = header[3:5]
        count = end + 1 - start
        total += count + 1  # Initial point / branch marker
        if count <= 0 or total > MAX_CHECKPOINTS:
            raise ValueError('Invalid Fuzee checkpoint count')
        arrays_pointer = word(header, 1)
        if arrays_pointer < 0x8000:
            raise ValueError('Invalid Fuzee checkpoint array table')
        pointers = struct.unpack('<6H', route_read(0x100000 | arrays_pointer, 12))
        arrays = []
        for item in pointers:
            if item < 0x8000 or item + start + count > 0x10000:
                raise ValueError('Invalid Fuzee checkpoint array')
            arrays.append(route_read(0x100000 | (item + start), count))
        x, y = word(header, 5), word(header, 7)
        origin = [x, y]
        points = []
        for index in range(count):
            x = (x + int.from_bytes(arrays[0][index:index+1], 'little', signed=True) * 8) & 0xffff
            y = (y + int.from_bytes(arrays[1][index:index+1], 'little', signed=True) * 8) & 0xffff
            points.append(dict(x=x, y=y, flags=arrays[2][index],
                               ai=[stream[index] for stream in arrays[3:]]))
        sections.append(dict(kind=kind, origin=origin, points=points))
        address += 9
        if route_read(address, 1)[0] == 0:
            return sections
    raise ValueError('Unterminated Fuzee checkpoint headers')


def decode(rom):
    if not recognized(rom):
        raise ValueError('Not a recognized Fuzee 0.04 resource loader')
    maps = {}
    courses = []
    for slot, code in enumerate(read(rom, 0x02e129, 22)):
        venue = code & 15
        variant = {0xe: 0, 0xc: 1, 0xd: 2}.get(code >> 4)
        if venue >= 9 or variant is None:
            raise ValueError('Unrecognized Fuzee course selector')
        key = f'{venue}-{variant}'
        if key not in maps:
            maps[key] = road_map(rom, venue, variant)
        route = checkpoints(rom, word(read(rom, 0x108000 + slot * 2, 2)))
        row = dict(slot=slot, mode='gp' if slot < 15 else 'practice',
                   venue=venue, variant=variant, map=key, checkpoints=route)
        if slot < 15:
            row.update(cup_index=slot // 5, race_index=slot % 5)
        courses.append(row)
    report = dict(format='fzero.fuzee-resource-audit', version=1,
                  loader_family='fuzee-0.04', target_sha256=hashlib.sha256(rom).hexdigest(),
                  donor_code_executed=False, playable_pack_created=False,
                  decoded_fields=['GP/practice order', 'road definition maps',
                                  'variant replacements', 'checkpoints and AI bytes'],
                  remaining_fields=['course/cup names', 'SPC/MSU assignments', 'tile art and palettes',
                                    'sky resources', 'minimaps', 'terrain', 'opponents',
                                    'shortcuts', 'custom mechanics and executable changes'],
                  maps={key: dict(width=MAP_WIDTH, height=MAP_HEIGHT,
                                  encoding='little-endian uint16 chip-definition offsets',
                                  sha256=hashlib.sha256(value).hexdigest()) for key, value in maps.items()},
                  courses=courses)
    return report, maps


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('rom', type=Path, help='Private, already patched/headerless donor ROM')
    parser.add_argument('--out', type=Path, required=True, help='New audit directory (not an installable pack)')
    args = parser.parse_args()
    if args.out.exists():
        parser.error('Output already exists; choose a new directory')
    with args.rom.open('rb') as stream:
        rom = stream.read(0x100000 + 1)
    if len(rom) != 0x100000:
        parser.error('Expected a headerless 1 MiB Fuzee ROM')
    try:
        report, maps = decode(rom)
    except ValueError as error:
        parser.error(str(error))
    args.out.mkdir(parents=True)
    for key, data in maps.items():
        (args.out / f'map-{key}.bin').write_bytes(data)
    (args.out / 'fuzee-audit.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    print(f'Decoded 15 GP and 7 practice entries, {len(maps)} road variants; no pack installed.')


if __name__ == '__main__':
    main()
