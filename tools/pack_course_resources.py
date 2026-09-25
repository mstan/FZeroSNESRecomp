"""Retain only resources reachable from selected, reviewed FZEdit courses.

This creates a private data donor for the existing IPS importer. Unselected
expanded-ROM assets and gameplay patches are discarded. Original addresses are
preserved, including same-bank pointers; no instruction relocation is needed.
"""
from audit_track_metadata import span


def pack_resources(stock, donor, layout, slots):
    target = bytearray(stock) + bytearray(max(0, len(donor) - len(stock)))
    copied = []

    def keep(address, length):
        data = span(donor, address, length)
        offset = ((address & 0x7f0000) >> 1) | (address & 0x7fff)
        target[offset:offset + length] = data
        copied.append((offset, offset + length))
        return data

    def entry(key, slot, stride=3):
        return keep(int(layout[key][0], 16) + slot * stride, stride)

    def resource(key, slot, length):
        address = int.from_bytes(entry(key, slot), 'little')
        return keep(address, length)

    for glyph in layout.get('intro_glyph', []):
        _, top, bottom = (int(part, 16) for part in glyph.split('|'))
        keep(top, 16)
        keep(bottom, 16)

    for slot in slots:
        if not 0 <= slot < int(layout['count'][0]):
            raise ValueError('Selected resource slot is out of range')
        for key, length in (('pools', 0x2400), ('palettes', 0xe0), ('graphics', 256*33),
                            ('sky_graphics', 0x2000), ('sky_back', 0x700),
                            ('sky_front', 0x540), ('terrain', 0x400)):
            resource(key, slot, length)
        for key, stride in (('settings', 1), ('gradients', 1), ('map_positions', 4),
                            ('opponents', 3), ('music', 1)):
            entry(key, slot, stride)
        keep(int.from_bytes(entry('minimaps', slot), 'little') + 0x8000, 0x200)
        maps = entry('maps', slot, 10)
        for start in (0, 5):
            keep(int.from_bytes(maps[start:start+3], 'little'),
                 int.from_bytes(maps[start+3:start+5], 'little') * 16)
        name = int.from_bytes(entry('names', slot), 'little')
        text = span(donor, name, 128)
        keep(name, text.index(0) + 1)
        if 'palette_cycles' in layout:
            table = int(layout['palette_cycles'][0], 16)
            address = (table & 0xff0000) | int.from_bytes(entry('palette_cycles', slot, 2), 'little')
            for i in range(15):
                if int.from_bytes(keep(address + 2*i, 2), 'little') & 0x8000:
                    break
            else:
                raise ValueError('Unterminated palette cycle list')
        address = int.from_bytes(entry('shortcuts', slot), 'little')
        for i in range(17):
            if int.from_bytes(keep(address + 17*i, 2), 'little') & 0x8000:
                break
            if i == 16:
                raise ValueError('Unterminated shortcut list')
            keep(address + 17*i, 17)
        address = int.from_bytes(entry('paths', slot), 'little')
        bank = address & 0xff0000
        for i in range(256):
            marker = keep(address + 9*i, 1)[0]
            if not marker:
                break
            record = keep(address + 9*i, 9)
            pointers = keep(bank | int.from_bytes(record[1:3], 'little'), 12)
            for j in range(6):
                keep(bank | int.from_bytes(pointers[2*j:2*j+2], 'little'), record[4] + 1)
        else:
            raise ValueError('Unterminated checkpoint list')
    # These two recognized instruction fragments locate metadata for the
    # read-only import audit. They are evidence, not executable runtime hooks.
    selector = keep(0x00f7e0, 17)
    order_loader = keep(0x10824d, 21)
    if selector[8] != 0xbf or order_loader[10] != 0xbf:
        raise ValueError('Unreviewed metadata locators')
    keep(int.from_bytes(order_loader[11:14], 'little'), len(slots))
    merged = []
    for start, end in sorted(copied):
        if merged and start <= merged[-1][1]:
            merged[-1][1] = max(end, merged[-1][1])
        else:
            merged.append([start, end])
    return bytes(target), merged
