"""Private integration checks for independent CGP rules and BS content toggles.

Requires the owner's stock ROM. All saves/captures remain in --out. Race-entry
and injected results checks do not claim full-lap playability of every course.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import json
import os
from pathlib import Path
import re
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
ROUTE = '320-326:8,440-446:8,560-566:8,730-736:8,790-796:8,1160-1599:1'


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--build', type=Path, required=True)
    p.add_argument('--stock', type=Path, required=True)
    p.add_argument('--out', type=Path, required=True)
    p.add_argument('--filter', default='')
    a = p.parse_args()
    build, stock, out = a.build.resolve(), a.stock.resolve(), a.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    original = stock.read_bytes()
    clean = {k: v for k, v in os.environ.items()
             if not k.startswith(('FZERO_', 'SNESRECOMP_', 'SDL_', 'LNG_'))}
    rules = re.findall(r'\{"(cgp-[a-z-]+)"', (ROOT/'src/fzero_gameplay_settings.c').read_text())
    results = {}

    def run(name, cars=0, tracks=0, cgp=False, rule='', profile=3,
            cup=None, frames=1600, route=ROUTE, car=0, level=None, practice=False, **extra):
        if a.filter and a.filter not in name:
            return
        folder = out/name
        folder.mkdir(exist_ok=True)
        packs = folder/'packs'
        packs.mkdir(exist_ok=True)
        shutil.copytree(ROOT/'assets/track-packs', folder/'assets/track-packs', dirs_exist_ok=True)
        (packs/'cgp.disabled').write_text('0\n' if cgp else '1\n')
        cup = cup or ('bs-deluxe' if cars or tracks else 'retail')+'/knight'
        env = dict(clean, FZERO_TRACK_PACKS=str(packs), FZERO_DELUXE_DATA='embedded',
                   FZERO_BS_CARS=str(cars), FZERO_BS_TRACKS=str(tracks),
                   FZERO_RULES=rule, FZERO_CGP_PROFILE=str(profile), FZERO_CUP=cup,
                   FZERO_ASPECT='21:9', SNESRECOMP_INPUT_SCRIPT=route,
                   SNESRECOMP_WRAM_DUMP=str(folder/'ram.bin'),
                   SNESRECOMP_FRAME_DUMP=str(folder/'frame.ppm'), SNESRECOMP_SAVE_ROOT='s',
                   FZERO_RULE_PROBE='1', FZERO_TEST_LEGACY_PROFILES='1')
        env.update(extra)
        with (folder/'run.log').open('w') as log:
            proc = subprocess.run([str(build/'FZeroSNESRecompHeadless.exe'), str(stock), str(frames)],
                                  cwd=folder, env=env, stdout=log, stderr=log, timeout=300)
        log = (folder/'run.log').read_text()
        assert proc.returncode == 0 and 'fzero_native: PASS' in log, (name, log[-3000:])
        ram = (folder/'ram.bin').read_bytes()
        if 'SNESRECOMP_MSU1' in extra:
            assert ram[0x183] == 1 and ram[0x182] == 1, (name,'MSU SPC fallback')
        else:
            assert '[MSU-1] enabled:' not in log, (name,'unexpected music activation')
        if frames == 1600:
            assert ram[0x54:0x56] == b'\x02\x03', (name, ram[0x54:0x57].hex())
            assert ram[0x52] == car, (name, 'car', ram[0x52])
            assert bool(ram[0x58]) == practice, (name,'practice',ram[0x58])
            if not practice:
                assert '[fzero-visibility]' in log, name
        if level is not None:
            assert ram[0x57] == level, (name, 'class', ram[0x57])
        if 'FZERO_LIFECYCLE_TEST' in extra:
            assert 'resimulation identical' in log and 'soft reset, SRAM retained' in log
        assert 'rules-probe: PASS' in log, name
        if cgp and not tracks:
            assert 'extracted cgp: 40 courses' in log
        else:
            assert 'extracted cgp:' not in log
        results[name] = {'cars': cars, 'original_bs': tracks, 'cgp': cgp, 'rules': rule,
                         'profile': profile, 'frames': frames, 'car': ram[0x52], 'class': ram[0x57]}
        print(name, 'PASS', flush=True)

    cases = []
    for cars in (0, 1):
        cases.append(dict(name=f'msu-fallback-cars{cars}', cars=cars, rule='cgp-msu',
                          cgp=True, cup='cgp/cgp-1',
                          SNESRECOMP_MSU1=str(out/'missing-music'/'soundtrack')))
        cases.append(dict(name=f'practice-cars{cars}', cars=cars, rule='all', practice=True,
                          route=ROUTE+',300-306:32'))
        for rule in rules:
            cases.append(dict(name=f'{rule}-cars{cars}', cars=cars, rule=rule))
        for profile in (1, 2, 3):
            cases.append(dict(name=f'tuning-p{profile}-cars{cars}', cars=cars,
                              rule='cgp-tuning', profile=profile, frames=5))
            cases.append(dict(name=f'combined-p{profile}-cars{cars}', cars=cars,
                              rule='all,cgp-credits', profile=profile, cgp=True, cup='cgp/bs-1',
                              frames=1850, FZERO_LIFECYCLE_TEST='1'))
        cases.append(dict(name=f'legend-cars{cars}', cars=cars, rule='cgp-legend', level=4,
                          route=ROUTE.replace('790-796:8', '760-762:16,790-796:8')))
    cases += [dict(name='original-bs-four-cars', tracks=1, cup='bs-deluxe/bs-1',
                   route=ROUTE+',400-406:128'),
              dict(name='corrected-bs-eight-cars', cars=1, cgp=True, cup='cgp/bs-2', car=4,
                   route=ROUTE+',400-406:128'),
              dict(name='all-rules-extra-car', cars=1, cgp=True, cup='cgp/cgp-1', car=4,
                   rule='all', route=ROUTE+',400-406:128'),
              dict(name='corrected-bs-four-cars', cgp=True, cup='cgp/bs-1'),
              dict(name='stock-no-rules'), dict(name='bs-cars-stock-courses', cars=1)]
    with ThreadPoolExecutor(max_workers=3) as pool:
        list(pool.map(lambda case: run(**case), cases))
    for prefix, metric in [('cgp-exhaust','exhaust'), ('cgp-tuning','acceleration')]+[
            (f'combined-p{i}','exhaust') for i in (1,2,3)]+[
            (f'tuning-p{i}','acceleration') for i in (1,2,3)]:
        pair=[f'{prefix}-cars{i}' for i in (0,1)]
        if all(name in results for name in pair):
            hashes=[re.search(rf'rules-probe: {metric}=([0-9a-f]+)', (out/name/'run.log').read_text())[1]
                    for name in pair]
            assert hashes[0]==hashes[1], (prefix, metric, hashes)
    assert stock.read_bytes() == original
    (out/('validation'+('-'+a.filter if a.filter else '')+'.json')).write_text(json.dumps(results, indent=2)+'\n')


if __name__ == '__main__':
    main()
