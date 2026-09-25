"""Private-ROM check of imported course intro glyphs on both native engines.

Checks actual captured OBJ tiles against independently decoded donor art, with
ordinary native glyphs as controls. Captures remain local and gitignored.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'tools'))
from audit_intro_font import atlas, audit_fzedit_intro_font
from audit_track_metadata import span
from parse_track_pack import fields


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for key in ('build', 'stock', 'source', 'out'):
        p.add_argument('--'+key, type=Path, required=True)
    p.add_argument('--pack', default='astra-front')
    a = p.parse_args()
    build, out = a.build.resolve(), a.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    registry = ROOT/'assets/track-packs'
    shutil.copytree(registry, out/'assets/track-packs')
    shutil.copytree(ROOT/'assets/vehicle-packs', out/'assets/vehicle-packs')
    (out/'packs').mkdir()
    for file in registry.glob('*.ini'):
        (out/f'packs/{file.stem}.disabled').write_text('0\n' if file.stem == a.pack else '1\n')
    manifest, layout = fields(registry/f'{a.pack}.ini'), fields(registry/f'{a.pack}.layout')
    tracks = [t.split('|') for t in manifest['track']]
    stock, source = a.stock.read_bytes(), a.source.read_bytes()
    audit_fzedit_intro_font(stock, source, layout, [int(t[3]) for t in tracks])
    native, donor = atlas(stock), atlas(source)
    overrides = {int(row.split('|')[0], 16) for row in layout.get('intro_glyph', [])}
    clean = {k:v for k,v in os.environ.items() if not k.startswith(('FZERO_', 'SNESRECOMP_', 'SDL_', 'LNG_'))}

    def check(case):
        expanded, track = case
        ident, _, cup, slot = track
        ordinal = [t for t in tracks if t[2] == cup].index(track)
        label = f"{'expanded' if expanded else 'retail'}-{ident}"
        env = dict(clean, FZERO_BS_CARS='0', FZERO_BS_TRACKS='0', FZERO_RULES='',
                   FZERO_DELUXE_DATA='embedded', FZERO_CGP_CARS='7' if expanded else '0',
                   FZERO_CGP_REBALANCE='15' if expanded else '0',
                   FZERO_TRACK_PACKS='packs', FZERO_CUP=f'{a.pack}/{cup}',
                   FZERO_TEST_COURSE=str(ordinal), SNESRECOMP_SAVE_ROOT=f's{int(expanded)}{tracks.index(track)}',
                   SNESRECOMP_INPUT_SCRIPT='320-326:8,560-566:8,640-646:8,730-736:8,790-796:8',
                   FZERO_CAPTURE_FRAMES='950', FZERO_CAPTURE_PREFIX=str(out/label))
        proc = subprocess.run([str(build/'FZeroSNESRecompHeadless.exe'), str(a.stock.resolve()), '1000'],
                              cwd=out, env=env, capture_output=True, text=True, timeout=90)
        log = proc.stdout + proc.stderr
        (out/f'{label}.log').write_text(log)
        assert proc.returncode == 0 and 'fzero_native: PASS' in log, (label, log[-1000:])
        capture = out/f'{label}-000950.bin'
        raw = capture.read_bytes()
        scene = json.loads(capture.with_suffix('.json').read_text())
        assert scene['state'][:2] == [2, 0], (label, 'not the course intro')
        pointer = int.from_bytes(span(source, int(layout['names'][0], 16) + int(slot)*3, 3), 'little')
        name = span(source, pointer, 128).split(b'\0', 1)[0]
        codes = {0x8e if c == 0xfe else c for c in name[6:] if c != 0xff}
        assert all(any((o[3] & 255) == code and o[2] == 83 for o in scene['oam']) for code in codes), label
        # FzeroSourceFrame: 224 raster lines, 1120 bytes each, then VRAM.
        assert len(raw) == 676872, 'Review changed capture ABI'
        for code in codes | overrides:
            for half in (0, 1):
                tile = code + half*16
                offset = 224*1120 + 0xa000 + tile*32
                if code in codes:
                    mapped = span(source, (0x108095 if not half else 0x108150) + code, 1)[0]
                    expected = donor[mapped][2]
                else:
                    expected = native[tile][2]
                assert raw[offset:offset+32] == expected + bytes(16), (label, hex(code), half)
        subprocess.run([str(build/'FZeroRenderCapture.exe'), str(capture), '4:3', str(out/f'{label}.ppm')],
                       check=True, capture_output=True)
        print(label+': intro text/art/scoping PASS', flush=True)
        return label

    with ThreadPoolExecutor(max_workers=3) as pool:
        passed = list(pool.map(check, [(e,t) for e in (False, True) for t in tracks]))
    (out/'validation.json').write_text(json.dumps(dict(pack=a.pack, passed=passed), indent=2)+'\n')


if __name__ == '__main__':
    main()
