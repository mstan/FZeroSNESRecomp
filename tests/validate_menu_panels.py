"""Private-ROM checks for car palettes/columns across native panel animations.

Also compare the expanded league picker lettering with the native panel's
own lettering. Requires Pillow; captures and disposable saves stay in --out.
"""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
from PIL import Image, ImageDraw

from validate_native_menus import ROSTERS, player_route

ROOT = Path(__file__).resolve().parents[1]
PIXELS = 224 * 1120 + 65536
RAM = PIXELS + 256 * 224 * 4


def picture(data):
    return Image.frombytes('RGB', (256, 224), data[PIXELS:RAM], 'raw', 'BGRX')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ('build', 'stock', 'packs', 'out'):
        parser.add_argument('--' + key, type=Path, required=True)
    parser.add_argument('--filter', default='')
    args = parser.parse_args()
    build, stock, packs, out = (getattr(args, k).resolve() for k in ('build', 'stock', 'packs', 'out'))
    out.mkdir(parents=True, exist_ok=False)
    clean = {k: v for k, v in os.environ.items()
             if not k.startswith(('FZERO_', 'SNESRECOMP_', 'SDL_', 'LNG_'))}
    frames = [590, *range(600, 691, 3), 710, *range(730, 791, 3), 810]
    for identity, practice in ((2, False), (4, False), (11, False), (5, True), (0, False)):
        retail = identity == 0
        name = 'retail' if retail else f'{identity}-' + ('practice' if practice else 'gp')
        if args.filter and args.filter not in name:
            continue
        folder = out / name
        folder.mkdir()
        shutil.copytree(ROOT / 'assets', folder / 'assets')
        inputs = player_route(identity, 0 if retail else 7) + ',600-606:8'
        if practice:
            inputs = '300-306:32,' + inputs
        else:
            inputs += ',730-736:8'
        env = dict(clean, FZERO_DELUXE_DATA='embedded', FZERO_CGP_CARS='0' if retail else '7',
                   FZERO_CGP_REBALANCE='0' if retail else '15', FZERO_RULES='' if retail else 'all',
                   FZERO_BS_CARS='0', FZERO_BS_TRACKS='0', FZERO_PACK_LOADER='1',
                   FZERO_PACKS_DIR=str(packs), FZERO_TRACK_PACKS='settings',
                   FZERO_CUP='retail/knight' if retail else 'bs-deluxe/knight', SNESRECOMP_SAVE_ROOT='s',
                   SNESRECOMP_INPUT_SCRIPT=inputs, FZERO_CAPTURE_FRAMES=','.join(map(str, frames)),
                   FZERO_CAPTURE_PREFIX=str(folder / 'capture'),
                   SNESRECOMP_FRAME_DUMP=str(folder / 'final.ppm'))
        proc = subprocess.run([str(build / 'FZeroSNESRecompHeadless.exe'), str(stock), '811'],
                              cwd=folder, env=env, capture_output=True, text=True, timeout=180)
        (folder / 'run.log').write_text(proc.stdout + proc.stderr)
        assert proc.returncode == 0, proc.stderr[-2000:]
        reference = picture((folder / 'capture-000590.bin').read_bytes())
        selected_row = ROSTERS[7].index(identity) % 4
        regions = [(0, 48 + row * 40, 16, 72 + row * 40) for row in range(4)]
        regions += [(88, 48 + row * 40, 104, 72 + row * 40) for row in range(4)]
        regions += [(32, 48 + row * 40, 72, 72 + row * 40)
                    for row in range(4) if row != selected_row]
        if retail:
            regions = []
        sheet = Image.new('RGB', (256 * 4, 248 * 2))
        draw = ImageDraw.Draw(sheet)
        for frame in frames:
            data = (folder / f'capture-{frame:06}.bin').read_bytes()
            ram = data[RAM:RAM + 0x20000]
            assert ram[0x14dff] == identity, (name, frame, 'identity changed')
            assert bool(ram[0x58]) == practice, (name, frame, 'wrong mode')
            if not retail and data[100 * 1120 + 7] == 8:
                assert data[100 * 1120 + 576:100 * 1120 + 608] == ram[0x200:0x220], \
                    (name, frame, 'rotating car lost its native sprite layout')
            actual = picture(data)
            for area in regions:
                assert actual.crop(area).tobytes() == reference.crop(area).tobytes(), \
                    (name, frame, area, 'neighbor/static car changed during panel animation')
        for i, frame in enumerate((590, 606, 621, 645, 710, 742, 760, 810)):
            actual = picture((folder / f'capture-{frame:06}.bin').read_bytes())
            sheet.paste(actual, (i % 4 * 256, i // 4 * 248 + 24))
            draw.text((i % 4 * 256, i // 4 * 248), str(frame), fill='white')
        sheet.save(folder / 'panels.png')
        final = Image.open(folder / 'final.ppm').convert('RGB')
        final.save(folder / 'final.png')
        if not practice:
            # The guest's unmodified native league label remains in the PPU
            # capture. The expanded host list must use exactly those pixels,
            # including the original horizontal shading and glyph shapes.
            native = picture((folder / 'capture-000810.bin').read_bytes())
            y = 87 if retail else 71
            expected = native.crop((128, y, 232, y + 8))
            assert expected.getbbox(), 'native league label is blank'
            assert final.crop((128, 71, 232, 79)).tobytes() == expected.tobytes(), \
                (name, 'league font differs from native')
        # The centered native panel must also survive wide/HD presentation.
        for frame in (621, 710, 760):
            capture = folder / f'capture-{frame:06}.bin'
            rendered = folder / f'wide-{frame}.ppm'
            proc = subprocess.run([str(build / 'FZeroRenderCapture.exe'), str(capture),
                                   '21:9', str(rendered)], env=dict(clean, FZERO_HD_SCALE='2'),
                                  capture_output=True, text=True, timeout=60)
            assert proc.returncode == 0, proc.stderr[-2000:]
            image = Image.open(rendered).convert('RGB')
            extra = (image.width - 512) // 2
            expected = picture(capture.read_bytes()).resize((512, 448), Image.Resampling.NEAREST)
            assert image.crop((extra, 0, extra + 512, 448)).tobytes() == expected.tobytes(), \
                (name, frame, 'wide/HD panel changed')
        checks = ['wide/HD panel']
        if not retail:
            checks += ['stable palettes/columns', 'native rotation']
        if not practice:
            checks += ['native panel font']
        print(f'PASS {name}: ' + ', '.join(checks), flush=True)


if __name__ == '__main__':
    main()
