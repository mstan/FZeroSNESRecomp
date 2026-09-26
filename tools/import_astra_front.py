"""Import the reviewed owner-supplied Astra Front ROM as a course-only IPS.

The full ROM is never exported. Source hashes, original names/music/order and
normalized resources are checked before publishing any pack files.
"""
import argparse
import hashlib
import json
from pathlib import Path

from audit_track_metadata import audit_metadata, span
from audit_intro_font import audit_fzedit_intro_font
from inspect_bs_deluxe import STOCK_SHA256, apply_ips
from make_ips import make_ips
from pack_course_resources import pack_resources
from parse_track_pack import fields, parse_fields, qualify, write_new

ROOT = Path(__file__).resolve().parents[1]
SOURCE_SHA256 = '76e03d7ef6d1c5e63da554f00e48843f589ec1407a4aaf80ed46af5df4f988c0'
TRACKS = [
    ('u-zero-1', 'U Zero I', 'astra', 55, 'silence'),
    ('candany', 'Candany', 'astra', 58, 'white-land-1'),
    ('u-zero-2', 'U Zero II', 'astra', 57, 'silence'),
    ('u-zero-3', 'U Zero III', 'astra', 59, 'silence'),
    ('vulcanom', 'Vulcanom', 'astra', 60, 'fire-field'),
    ('u-zero-4', 'U Zero IV', 'front', 61, 'silence'),
    ('brutal-wind-1', 'Brutal Wind I', 'front', 56, 'death-wind'),
    ('moon', 'Moon', 'front', 63, 'silence'),
    ('u-zero-5', 'U Zero V', 'front', 62, 'silence'),
    ('death-city', 'Death City', 'front', 64, 'mute-city'),
]


def digest(data):
    return hashlib.sha256(data).hexdigest()


def manifest(target):
    return ('# Astra Front: ten authored courses; unused CGP resources omitted.\n'
            'format=1\nid=astra-front\nname=F-Zero Astra Front\n'
            'author=MF9_05 / Worthy\nadapter=fzero-course-v1\n'
            f'source_sha256={STOCK_SHA256}\ntarget_sha256={digest(target)}\n'
            'cup=astra|Astra|0\ncup=front|Front|1\n' +
            ''.join(f'track={ident}|{name}|{cup}|{slot}\n' for ident, name, cup, slot, _ in TRACKS)).encode()


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--stock', type=Path, required=True)
    p.add_argument('--donor', type=Path, required=True)
    p.add_argument('--out', type=Path, default=ROOT/'assets/track-packs')
    p.add_argument('--inspector', type=Path, default=ROOT/'build/FZeroInspectCourses.exe')
    a = p.parse_args()
    stock, source = a.stock.read_bytes(), a.donor.read_bytes()
    if len(stock) == 0x80200:
        stock = stock[512:]
    if len(source) % 1024 == 512:
        source = source[512:]
    if digest(stock) != STOCK_SHA256 or digest(source) != SOURCE_SHA256:
        raise ValueError('Unreviewed stock/source revision; do not reuse guessed offsets')
    layout_path = ROOT/'assets/track-packs/astra-front.layout'
    layout = fields(layout_path)
    original = audit_metadata(source, layout, parse_fields(manifest(source).decode()))
    if [t['snes_music'] for t in original['tracks']] != [t[4] for t in TRACKS]:
        raise ValueError('Authored music mapping changed')
    # Native small-font menu labels, independently decoded from the source.
    alphabet = {0xaf:'A', 0xd4:'S', 0xd5:'T', 0xd3:'R', 0xc7:'F', 0xd0:'O', 0xce:'N', 0xff:' '}
    labels = []
    for i in range(2):
        pointer = int.from_bytes(span(source, 0x10878d + i*2, 2), 'little')
        labels.append(''.join(alphabet[b] for b in span(source, 0x100000 | pointer, 26)[::2]).strip())
    if labels != ['ASTRA', 'FRONT']:
        raise ValueError(f'Authored cup labels changed: {labels}')
    slots = [t[3] for t in TRACKS]
    # The exact source uses the standard 8-bit FZEdit selector: command 6
    # chooses 10 + league*5 + race, independently of resource-table indices.
    if span(source, 0x02c26a, 26) != bytes.fromhex(
            'a5 46 29 07 c9 06 d0 11 a5 53 a6 58 d0 08 a5 90 '
            '0a 0a 65 90 65 53 18 69 0a 60'):
        raise ValueError('Unreviewed Astra MSU selector')
    msu = {int(slot): int(track) for slot, track in
           (entry.split('|') for entry in layout.get('msu', []))}
    if layout.get('msu_source') != ['astra-front'] or msu != {
            slot: 10 + order for order, slot in enumerate(slots)}:
        raise ValueError('Astra MSU mapping must follow authored GP order')
    intro_glyphs = audit_fzedit_intro_font(stock, source, layout, slots)
    before = qualify(source, layout_path, a.inspector, slots)
    target, ranges = pack_resources(stock, source, layout, slots)
    if before != qualify(target, layout_path, a.inspector, slots):
        raise ValueError('Selected normalized resources changed while packing')
    for glyph in intro_glyphs:
        for part in ('top', 'bottom'):
            address = int(glyph[part], 16)
            if span(source, address, 16) != span(target, address, 16):
                raise ValueError('Intro letter artwork changed while packing')
    after = audit_metadata(target, layout, parse_fields(manifest(target).decode()))
    if original['tracks'] != after['tracks']:
        raise ValueError('Course metadata changed while packing')
    patch = make_ips(stock, target)
    if apply_ips(stock, patch) != target:
        raise ValueError('IPS round trip mismatch')
    report = dict(source_sha256=SOURCE_SHA256, packed_sha256=digest(target), patch_sha256=digest(patch),
                  cup_names=labels, metadata=original, intro_glyphs=intro_glyphs,
                  msu_source=layout['msu_source'][0], msu_tracks=msu, retained_spans=ranges,
                  retained_bytes=sum(end-start for start,end in ranges),
                  resource_validation=before.splitlines())
    outputs = {'astra-front.ini': manifest(target), 'astra-front.ips': patch,
               'astra-front-import.txt': (json.dumps(report, indent=2)+'\n').encode()}
    for name, data in outputs.items():
        if (a.out/name).exists() and (a.out/name).read_bytes() != data:
            raise ValueError(f'Refusing to overwrite changed output: {a.out/name}')
    a.out.mkdir(parents=True, exist_ok=True)
    for name, data in outputs.items():
        write_new(a.out/name, data)
    print(f'Astra Front: 2 cups, 10 courses, {len(patch):,} patch bytes; source names/music/resources verified')


if __name__ == '__main__':
    try:
        main()
    except (ValueError, OSError) as error:
        raise SystemExit(str(error))
