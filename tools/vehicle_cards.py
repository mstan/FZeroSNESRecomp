"""Adapt native F-Zero information artwork to the Deluxe card tile layout.

Reads the original $03:9892 tilemap stream and $03:98EE three-plane atlas.
No guest code, whole menu replacement or emulator is needed for extraction.
The caller verifies the reviewed donor before using this layout adapter.
"""
CARD_MAGIC = b"FZCARD1\0"
CARD_SIZE = 0x580
CARD_ENTRY_SIZE = CARD_SIZE + 6
CARD_POSITIONS = ([(x, 7) for x in range(22, 29)] +
                  [(x, 9) for x in range(22, 25)] +
                  [(x, 11) for x in range(22, 24)] +
                  [(x, 13) for x in range(22, 25)] +
                  [(x, 17) for x in range(17, 28)] +
                  [(x, 18) for x in range(16, 24)] +
                  [(x, 19) for x in range(15, 21)] +
                  [(x, 20) for x in range(15, 18)] + [(15, 21)])


def decode_info_maps(rom):
    cursor, end = 0x7b8b0, 0x7c100
    words, high, low = [], 0, 0
    while cursor < end:
        command = rom[cursor]
        cursor += 1
        if not command & 0x20:
            if cursor >= end:
                raise ValueError("Truncated information tilemap word")
            high, low = command, rom[cursor]
            cursor += 1
            words.append(high << 8 | low)
        elif command & 0x80:
            length = (command & 31) + 1
            if cursor + length > end:
                raise ValueError("Truncated information tilemap run")
            words.extend(high << 8 | value for value in rom[cursor:cursor + length])
            cursor += length
        elif not command & 0x40:
            words.extend([high << 8 | low] * ((command & 31) + 1))
        else:
            if len(words) != 4096:
                raise ValueError("Expected four complete information tilemaps")
            return words
        if len(words) > 4096:
            raise ValueError("Oversized information tilemap")
    raise ValueError("Unterminated information tilemap")


def card_tile(rom, word):
    tile, palette = word & 1023, (word >> 10) & 7
    if tile >= 256 or palette > 1:
        raise ValueError("Unsupported information tile/attribute")
    source = rom[0x76800 + tile * 24:][:24]
    output = bytearray(32)
    # Native black/background -> transparent; white -> Deluxe white;
    # the native acceleration curve -> Deluxe's shared curve color.
    colors = {1: 0, 2: 13, 6: 0, 7: 15}
    for y in range(8):
        sy = 7 - y if word & 0x8000 else y
        for x in range(8):
            sx = 7 - x if word & 0x4000 else x
            pixel = ((source[sy * 2] >> (7 - sx)) & 1) | \
                    (((source[sy * 2 + 1] >> (7 - sx)) & 1) << 1) | \
                    (((source[16 + sy] >> (7 - sx)) & 1) << 2)
            if pixel not in colors or (pixel == 7 and palette):
                raise ValueError("Unsupported information palette color")
            for plane in range(4):
                output[plane // 2 * 16 + y * 2 + plane % 2] |= \
                    ((colors[pixel] >> plane) & 1) << (7 - x)
    return bytes(output)


def information_cards(rom):
    maps = decode_info_maps(rom)
    output = bytearray(CARD_MAGIC)
    for slot, row in enumerate((0, 2, 1, 3)):
        card = b"".join(card_tile(rom, maps[row * 1024 + y * 32 + x])
                        for x, y in CARD_POSITIONS)
        assert len(card) == CARD_SIZE
        output += card
        output += rom[0x18901 + slot * 6:][:6]
        # A donor graph outside the reviewed Deluxe slots needs a new adapter.
        for y in range(17, 22):
            for x in range(15, 28):
                if (x, y) not in CARD_POSITIONS and any(card_tile(rom, maps[row * 1024 + y * 32 + x])):
                    raise ValueError("Acceleration graph exceeds the native card layout")
    return bytes(output)
