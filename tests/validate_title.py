"""Private-ROM check of the optional title and its separation from gameplay."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
ROUTE = '320-326:8,440-446:8,560-566:8,730-736:8,790-796:8,1160-1599:1'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--stock', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    build, stock, out = args.build.resolve(), args.stock.resolve(), args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    original = stock.read_bytes()
    clean = {k: v for k, v in os.environ.items()
             if not k.startswith(('FZERO_', 'SNESRECOMP_'))}
    results = {}

    def run(case):
        cars, screen, title = case
        name = f'cars{cars}-{screen}-title{title}'
        folder = out/name
        folder.mkdir(exist_ok=True)
        (folder/'s').mkdir(exist_ok=True)
        shutil.copytree(ROOT/'assets/track-packs', folder/'assets/track-packs', dirs_exist_ok=True)
        packs = folder/'packs'
        packs.mkdir(exist_ok=True)
        (packs/'cgp.title').write_text(f'{title}\n')
        (packs/'cgp.disabled').write_text('1\n' if screen == 'pack-off' else '0\n')
        route = '' if screen in ('title', 'pack-off', 'missing-art') else ROUTE
        # Exercise an added car in both the menu and an imported race.
        if cars and route:
            route += ',400-406:128'
        frames = 1600 if screen == 'race' else 570 if screen == 'car-select' else 300
        if screen == 'missing-art':
            (folder/'assets/track-packs/presentation/fzero-55.ips').unlink()
        enabled = bool(title) and screen not in ('pack-off', 'missing-art')
        env = dict(clean, FZERO_BS_CARS=str(cars), FZERO_BS_TRACKS='0', FZERO_RULES='',
                   FZERO_DELUXE_DATA='embedded', FZERO_TRACK_PACKS=str(packs),
                   SNESRECOMP_INPUT_SCRIPT=route, SNESRECOMP_SAVE_ROOT='s',
                   SNESRECOMP_WRAM_DUMP=str(folder/'ram.bin'),
                   SNESRECOMP_FRAME_DUMP=str(folder/'frame.ppm'))
        if screen in ('race', 'car-select'):
            env['FZERO_CUP'] = 'cgp/cgp-1'
        with (folder/'run.log').open('w') as log:
            proc = subprocess.run([str(build/'FZeroSNESRecompHeadless.exe'), str(stock), str(frames)],
                                  cwd=folder, env=env, stdout=log, stderr=log, timeout=120)
        log = (folder/'run.log').read_text()
        assert proc.returncode == 0 and 'fzero_native: PASS' in log, (name, log[-2000:])
        assert ('F-Zero 55 title artwork enabled' in log) == enabled, name
        assert '[MSU-1] enabled:' not in log and '[cgp-rules] enabled=' not in log, name
        ram = (folder/'ram.bin').read_bytes()
        if screen == 'race':
            assert ram[0x54:0x56] == b'\x02\x03' and ram[0x52] == cars*4, name
            assert '[track-library] load ' in log, name
        elif screen == 'car-select':
            assert ram[0x54] == 1, name
        else:
            assert ram[0x54] == 0, name
        pixels = (folder/'frame.ppm').read_bytes()
        results[case] = (pixels, ram)
        print(name, 'PASS', flush=True)

    cases = [(cars, screen, title) for cars in (0, 1)
             for screen in ('title', 'pack-off', 'car-select', 'race', 'missing-art')
             for title in (0, 1)]
    with ThreadPoolExecutor(max_workers=3) as pool:
        list(pool.map(run, cases))
    for cars in (0, 1):
        assert results[cars, 'title', 0][0] != results[cars, 'title', 1][0], 'Title did not change'
        for screen in ('pack-off', 'car-select', 'race', 'missing-art'):
            assert results[cars, screen, 0][0] == results[cars, screen, 1][0], (cars, screen, 'graphics leaked')
        # Unmodified CPU code and data keep the race state byte-identical too.
        assert results[cars, 'race', 0][1] == results[cars, 'race', 1][1], (cars, 'gameplay changed')
    assert stock.read_bytes() == original
    report = {'-'.join(map(str, k)): hashlib.sha256(v[0]).hexdigest() for k, v in sorted(results.items())}
    (out/'validation.json').write_text(json.dumps(report, indent=2)+'\n')
    print('Title only changes when opted in; car-select, race and disabled-pack views match: PASS')


if __name__ == '__main__':
    main()
