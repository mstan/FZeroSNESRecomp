"""Replay the reported CGP landings from controlled approach states.

Requires a private retail ROM and built headless host. Fixtures place the car
at real course coordinates; the ordinary race loop then resolves motion,
surface sampling, jumps and landing. This is not a complete driven lap.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
ROUTE = '320-326:8,440-446:8,560-566:8,730-736:8,790-796:8,1400-1619:1'
CASES = [
    dict(name='marine-exit', cup='cgp-1', ordinal=0, x=6996, y=1468,
         heading=0x6000, height=0x800, velocity=0xfc00, speed=0x600, checkpoint=35),
    dict(name='marine-traverse', cup='cgp-1', ordinal=0, x=6972, y=724,
         heading=0x6000, height=0x400, velocity=0, speed=0x600, checkpoint=32),
    dict(name='railroad', cup='cgp-5', ordinal=4, x=824, y=1448,
         heading=0x3000, height=0x800, velocity=0xfc00, speed=0, checkpoint=126),
]
CASES += [dict(CASES[0], name='marine-pit', x=7200, fatal=True),
          dict(CASES[2], name='railroad-pit', y=1400, fatal=True)]


def fixture(case):
    entries = []
    def write(address, value, width=2):
        entries.append(f'1400 1400 {address:x} {value.to_bytes(width, "little").hex()}')
    for address, key in [(0xb70, 'x'), (0xb90, 'y'), (0xbd0, 'heading'), (0xbe0, 'heading'),
                         (0xbc0, 'height'), (0xbb0, 'velocity'), (0xb20, 'speed')]:
        write(address, case[key])
    for address in [0xb80, 0xba0, 0xb30, 0xb40, 0xb50, 0xb60, 0xc20, 0xcc0, 0xc00]:
        write(address, 0)
    write(0xd51, 0x80 if case['velocity'] else 0, 1)
    write(0xc3, 0, 1)
    write(0xd00, case['checkpoint'], 1)
    return '\n'.join(entries) + '\n'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--stock', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    build, stock, out = args.build.resolve(), args.stock.resolve(), args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    clean = {k: v for k, v in os.environ.items()
             if not k.startswith(('FZERO_', 'SNESRECOMP_', 'SDL_', 'LNG_'))}
    results = {}

    def run(item):
        case, engine, rules, old, lifecycle = item
        name = f'{case["name"]}-{engine}-{rules or "required"}' + ('-old' if old else '') + ('-state' if lifecycle else '')
        folder = out / name
        folder.mkdir(exist_ok=True)
        shutil.copytree(ROOT / 'assets', folder / 'assets', dirs_exist_ok=True)
        (folder / 'events.txt').write_text(fixture(case), encoding='ascii')
        env = dict(clean, FZERO_DELUXE_DATA='embedded', FZERO_BS_CARS=str(int(engine == 'bs')),
                   FZERO_BS_TRACKS='0', FZERO_CGP_CARS='7' if engine == 'cgp' else '0',
                   FZERO_CGP_REBALANCE='15' if engine == 'cgp' else '0', FZERO_TEST_VEHICLE='white-cat',
                   FZERO_RULES=rules, FZERO_TRACK_PACKS=str(folder / 'packs'), FZERO_CUP='cgp/' + case['cup'],
                   FZERO_TEST_COURSE=str(case['ordinal']), SNESRECOMP_INPUT_SCRIPT=ROUTE, SNESRECOMP_SAVE_ROOT='s',
                   FZERO_TEST_WRAM_SCRIPT=str(folder / 'events.txt'), FZERO_TEST_WRAM_TRACE=str(folder / 'trace.bin'),
                   SNESRECOMP_FRAME_DUMP=str(folder / 'frame.ppm'), SNESRECOMP_WRAM_DUMP=str(folder / 'ram.bin'))
        if old:
            env['FZERO_TEST_STOCK_LANDING'] = '1'
        if lifecycle:
            env.update(FZERO_LIFECYCLE_TEST='1', FZERO_TEST_SAVE_FRAME='1400', FZERO_VEHICLE_CROSS_STATE='1')
        proc = subprocess.run([str(build / 'FZeroSNESRecompHeadless.exe'), str(stock), '1850' if lifecycle else '1620'],
                              cwd=folder, env=env, capture_output=True, text=True, timeout=180)
        log = proc.stdout + proc.stderr
        (folder / 'run.log').write_text(log, encoding='utf-8')
        assert proc.returncode == 0, (name, log[-2500:])
        data = (folder / 'trace.bin').read_bytes()
        frames = [data[f*8192:(f+1)*8192] for f in range(1400, 1560)]
        fatal = bool(case.get('fatal') or old)
        deaths = [1400+i for i, r in enumerate(frames) if r[0xc3] & 64]
        assert bool(deaths) == fatal, (name, deaths)
        if fatal:
            assert deaths[0] <= 1403, (name, 'did not reach the landing check')
        else:
            assert any(not r[0xd51] & 128 for r in frames), (name, 'never landed')
            if case['name'] == 'marine-traverse':
                assert any(r[0xd50] & 64 and r[0xd51] & 128 for r in frames), (name, 'never bounced')
                assert max(int.from_bytes(r[0xb90:0xb92], 'little') for r in frames) > 1480, (name, 'never exited')
            elif case['name'] == 'railroad':
                assert frames[0][0xcd0] == 0xf7, (name, 'missed railroad surface')
            elif case['name'] == 'marine-exit':
                assert frames[1][0xcd0] == 0xdb, (name, 'missed trampoline exit')
        if lifecycle:
            assert 'resimulation identical' in log and 'soft reset, SRAM retained' in log, name
        results[name] = dict(fatal=fatal, first_death=deaths[0] if deaths else None, lifecycle=lifecycle)
        print(name, 'PASS', flush=True)

    matrix = [(c, e, r, False, False) for c in CASES for e in ('stock', 'bs', 'cgp') for r in ('', 'all')]
    matrix += [(c, e, 'all', True, False) for c in (CASES[0], CASES[2]) for e in ('stock', 'cgp')]
    matrix += [(c, e, 'all', False, True) for c in (CASES[0], CASES[2]) for e in ('stock', 'bs', 'cgp')]
    with ThreadPoolExecutor(max_workers=3) as pool:
        list(pool.map(run, matrix))
    (out / 'validation.json').write_text(json.dumps(results, indent=2) + '\n')


if __name__ == '__main__':
    main()
