"""Verify the reviewed FZEdit intro remapper against native letter graphics.

Course-name decoding alone is insufficient: a donor can remap the same character
codes to a different OBJ atlas. Import only changed, used letters as resources.
"""
from audit_track_metadata import GLYPHS, span


def atlas(rom):
    address, tiles = 0x0f8000, {}
    # The bounded native $039717 stream; output addresses are OBJ tile indices.
    for _ in range(256):
        header, first = span(rom, address, 2)
        if not header:
            return tiles
        count, mode = header & 63, header >> 6
        if not count or first + count > 256:
            raise ValueError('Invalid native intro atlas')
        address += 2
        length = (32, 8, 16, 24)[mode]
        for tile in range(first, first + count):
            tiles[tile] = (address, mode, span(rom, address, length))
            address += length
    raise ValueError('Unterminated native intro atlas')


def audit_fzedit_intro_font(stock, source, layout, slots):
    if span(source, 0x00d146, 30) != bytes.fromhex(
            '08 e2 20 c9 bb b0 31 eb 29 7f eb aa bf 50 81 10 48 '
            'bf 95 80 10 85 00 f0 03 20 93 d1 fa f0'):
        raise ValueError('Unreviewed intro font remapper')
    native, donor = atlas(stock), atlas(source)
    used = set()
    for slot in slots:
        pointer = int.from_bytes(span(source, int(layout['names'][0], 16) + slot*3, 3), 'little')
        name = span(source, pointer, 128).split(b'\0', 1)[0]
        used.update(0x8e if code == 0xfe else code for code in name[6:] if code != 0xff)
    changed = {}
    for code in sorted(used):
        top = span(source, 0x108095 + code, 1)[0]
        bottom = span(source, 0x108150 + code, 1)[0]
        if any(tile not in donor or donor[tile][1] != 2 for tile in (top, bottom)):
            raise ValueError('Review intro glyph compression/blank/flip semantics')
        if (donor[top][2], donor[bottom][2]) != (native[code][2], native[code+16][2]):
            changed[code] = (donor[top][0], donor[bottom][0])
    declared = {}
    for row in layout.get('intro_glyph', []):
        code, top, bottom = (int(part, 16) for part in row.split('|'))
        if code in declared:
            raise ValueError('Duplicate intro glyph override')
        declared[code] = (top, bottom)
    if changed != declared:
        raise ValueError(f'Intro glyph resources do not match donor differences: {changed}')
    return [dict(letter=GLYPHS[code], code=f'{code:02x}',
                 top=f'{top:06x}', bottom=f'{bottom:06x}')
            for code, (top, bottom) in changed.items()]
