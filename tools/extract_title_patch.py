"""Clip an IPS to F-Zero's original title tile/palette layout, without a ROM.

This is a presentation adapter, not a general hack converter: the donor must
retain the original title sprite layout and DMA descriptors. Community Grand Prix does.
Writes outside these two resource ranges (including MSU/code) are discarded.
"""
import argparse
from pathlib import Path

RANGES = ((0x66c00, 0x68000), (0x7c2e0, 0x7c360))


def extract(patch):
    if patch[:5] != b'PATCH':
        raise ValueError('Expected IPS input')
    cursor, output = 5, bytearray(b'PATCH')
    while patch[cursor:cursor+3] != b'EOF':
        if cursor+5 > len(patch):
            raise ValueError('Truncated IPS record')
        address = int.from_bytes(patch[cursor:cursor+3], 'big')
        length = int.from_bytes(patch[cursor+3:cursor+5], 'big')
        cursor += 5
        if length:
            data = patch[cursor:cursor+length]
            cursor += length
            if len(data) != length:
                raise ValueError('Truncated IPS data')
        else:
            if cursor+3 > len(patch):
                raise ValueError('Truncated IPS RLE')
            length = int.from_bytes(patch[cursor:cursor+2], 'big')
            data = patch[cursor+2:cursor+3]*length
            cursor += 3
        for start, end in RANGES:
            lo, hi = max(address, start), min(address+length, end)
            if hi > lo:
                output += lo.to_bytes(3, 'big') + (hi-lo).to_bytes(2, 'big')
                output += data[lo-address:hi-address]
    if len(patch)-cursor not in (3, 6):
        raise ValueError('Unexpected IPS trailer')
    return bytes(output+b'EOF')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    args.output.write_bytes(extract(args.input.read_bytes()))
