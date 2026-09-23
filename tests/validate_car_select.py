"""Private-ROM regression for BS carousel palettes across all eight selections.

Checks rendered pixels, not just successful menu/race entry. Unselected cars
must retain their colors and silhouette when a different row is highlighted;
the gameplay profiles must not change the menu's native graphics.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
import os
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--stock', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    build, stock, out = args.build.resolve(), args.stock.resolve(), args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    original_hash = hashlib.sha256(stock.read_bytes()).hexdigest()
    clean = {k: v for k, v in os.environ.items()
             if not k.startswith(('FZERO_', 'SNESRECOMP_', 'SDL_', 'LNG_'))}
    frames = {}

    def run(case):
        profile, car = case
        name = f'p{profile}-car{car}'
        folder = out/name
        folder.mkdir(exist_ok=True)
        route = '320-326:8'
        if car >= 4:
            route += ',400-406:128'
        for step in range(car % 4):
            route += f',{440+step*30}-{446+step*30}:32'
        env = dict(clean, FZERO_TRACK_PACKS=str(folder/'packs'),
                   FZERO_DELUXE_DATA='embedded', FZERO_BS_CARS='1', FZERO_BS_TRACKS='0',
                   FZERO_RULES='all,cgp-msu,cgp-credits' if profile else '',
                   FZERO_CGP_PROFILE=str(profile or 3), FZERO_ASPECT='4:3',
                   SNESRECOMP_INPUT_SCRIPT=route, SNESRECOMP_SAVE_ROOT='s',
                   SNESRECOMP_WRAM_DUMP=str(folder/'ram.bin'),
                   SNESRECOMP_FRAME_DUMP=str(folder/'frame.ppm'))
        with (folder/'run.log').open('w') as log:
            proc = subprocess.run([str(build/'FZeroSNESRecompHeadless.exe'), str(stock), '570'],
                                  cwd=folder, env=env, stdout=log, stderr=log, timeout=120)
        assert proc.returncode == 0, (name, (folder/'run.log').read_text()[-2000:])
        ram = (folder/'ram.bin').read_bytes()
        assert ram[0x54:0x57] == b'\x01\x01\x00' and ram[0x14c84] == car, name
        magic, dimensions, maximum, pixels = (folder/'frame.ppm').read_bytes().split(b'\n', 3)
        assert (magic, dimensions, maximum) == (b'P6', b'256 224', b'255'), name
        assert len(pixels) == 256*224*3, name
        frames[case] = pixels
        print(name, 'PASS', flush=True)

    with ThreadPoolExecutor(max_workers=3) as pool:
        list(pool.map(run, [(profile, car) for profile in range(4) for car in range(8)]))

    def car_row(pixels, row):
        # Only the large preview column. Exclude the selector arrow, the
        # adjacent carousel page, and the independently flashing menu text.
        return b''.join(pixels[(y*256+28)*3:(y*256+76)*3]
                        for y in range(40+row*40, 80+row*40))

    for page in (0, 4):
        for row in range(4):
            unselected = [car_row(frames[0, page+selected], row)
                          for selected in range(4) if selected != row]
            assert all(p == unselected[0] for p in unselected), (page, row, 'unstable car')
            colors = list(zip(unselected[0][0::3], unselected[0][1::3], unselected[0][2::3]))
            assert sum(max(rgb) > 0 for rgb in colors) > 100, (page, row, 'missing car')
            if page == 0 and row == 1:
                assert sum(r > 30 and g > 30 and b*2 < min(r, g)
                           for r, g, b in colors) > 50, 'Golden Fox must stay yellow'
        for car in range(page, page+4):
            for profile in (1, 2, 3):
                for row in range(4):
                    assert car_row(frames[profile, car], row) == car_row(frames[0, car], row), (
                        profile, car, row, 'gameplay mod changed the menu')
    assert hashlib.sha256(stock.read_bytes()).hexdigest() == original_hash
    result = {f'p{profile}-car{car}': hashlib.sha256(pixels).hexdigest()
              for (profile, car), pixels in sorted(frames.items())}
    (out/'validation.json').write_text(json.dumps(result, indent=2)+'\n')
    print('All eight previews retain their colors and visibility in every profile: PASS')


if __name__ == '__main__':
    main()
