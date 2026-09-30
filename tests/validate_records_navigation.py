"""Private-ROM browser navigation, persistence, snapshot and rewind regression.

Use the CGP fixture directory produced by validate_records.py. This copies its
saves into a NEW output directory; it never operates on a player's save root.
"""
import argparse
import hashlib
import os
from pathlib import Path
import shutil
import subprocess

from validate_records import ROOT, BEST, EMPTY, ROUTE, totals


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--build', type=Path, required=True)
    p.add_argument('--stock', type=Path, required=True)
    p.add_argument('--fixture', type=Path, required=True)
    p.add_argument('--out', type=Path, required=True)
    p.add_argument('--navigation-only', action='store_true', help='Skip repeating the long native/MAX completion fixtures')
    a = p.parse_args()
    build, stock, fixture, out = (v.resolve() for v in (a.build, a.stock, a.fixture, a.out))
    assert (fixture/'complete.log').is_file() and (fixture/'records.sav').is_file()
    out.mkdir(parents=True, exist_ok=False)
    shutil.copytree(ROOT/'assets', out/'assets')
    shutil.copytree(fixture/'s', out/'s')
    shutil.copy2(fixture/'records.sav', out/'records.sav')
    (out/'packs').mkdir()
    for name in ('cgp', 'max-league', 'bower-league'):
        (out/f'packs/{name}.disabled').write_text('0\n' if name == 'cgp' else '1\n')
    env = {k: v for k, v in os.environ.items() if not k.startswith(('FZERO_', 'SNESRECOMP_', 'SDL_', 'LNG_'))}
    env.update(FZERO_DELUXE_DATA='embedded', FZERO_BS_CARS='0', FZERO_BS_TRACKS='0',
               FZERO_CGP_CARS='7', FZERO_CGP_REBALANCE='15', FZERO_RULES='all',
               FZERO_TRACK_PACKS='packs', SNESRECOMP_SAVE_ROOT='s', FZERO_TEST_SAVE_SRAM='1')

    def files():
        return {str(p.relative_to(out)): hashlib.sha256(p.read_bytes()).hexdigest()
                for p in (out/'s').rglob('records.bin')}

    before = files()

    def run(label, inputs='', frames=300, state='records.sav', **extra):
        e = dict(env, SNESRECOMP_INPUT_SCRIPT=inputs, SNESRECOMP_WRAM_DUMP=str(out/f'{label}.ram'),
                 SNESRECOMP_FRAME_DUMP=str(out/f'{label}.ppm'), FZERO_TEST_SRAM_DUMP=str(out/f'{label}.sram'))
        if state:
            e['FZERO_STATE_LOAD'] = str(out/state)
        e.update(extra)
        r = subprocess.run([str(build/'FZeroSNESRecompHeadless.exe'), str(stock), str(frames)],
                           cwd=out, env=e, capture_output=True, text=True, timeout=240)
        log = r.stdout+r.stderr
        (out/f'{label}.log').write_text(log, encoding='utf-8')
        assert r.returncode == 0, (label, log[-2000:])
        ram, sram = (out/f'{label}.ram').read_bytes(), (out/f'{label}.sram').read_bytes()
        print(label, ram[0x54:0x57].hex(), flush=True)
        return ram, sram, log

    def baron(sram):
        return totals(sram[0x153-5:])

    for label, inputs, expected in (
        ('next-car', '60-66:512', EMPTY),
        ('return-car', '60-66:512,120-126:2', BEST),
        ('next-page', '60-66:2048', EMPTY),
        ('return-page', '60-66:2048,120-126:1024', BEST),
        ('imported-practice', '60-66:4', BEST),
    ):
        ram, sram, _ = run(label, inputs)
        assert ram[0x54:0x57] == b'\0\3\0' and baron(sram) == [expected]*5, label

    ram, _, log = run('detail-next-course', '60-66:8,180-186:32', frames=400,
                      FZERO_CAPTURE_FRAME='6699', FZERO_CAPTURE_PREFIX=str(out/'detail-raster'))
    assert ram[0x54:0x57] == b'\0\3\4' and ram[0x14c25] == 11
    assert 'course=big-blue-3' in log
    # Native detail DMA copies the WRAM tilemap verbatim. Before the VMAIN
    # restore fix, a cold-loaded state displaced every high VRAM byte by one
    # word. Validate actual rendered VRAM against the native source buffer.
    capture = (out/'detail-raster.bin').read_bytes()
    vram = 224 * 1120
    wram = vram + 0x10000 + 256 * 224 * 4
    assert capture[vram+0x1000:vram+0x1800] == capture[wram+0x19000:wram+0x19800]
    ram, sram, _ = run('detail-back', '60-66:8,180-186:1', frames=400)
    assert ram[0x54:0x57] == b'\0\3\0' and baron(sram) == [BEST]*5
    ram, sram, _ = run('exit', '60-66:1')
    assert ram[0x54:0x56] == b'\0\1' and baron(sram) == [EMPTY]*5

    _, sram, _ = run('all-cars', ','.join(f'{60+i*45}-{66+i*45}:512' for i in range(13)), frames=700)
    assert baron(sram) == [BEST]*5
    _, sram, _ = run('all-pages', ','.join(f'{60+i*45}-{66+i*45}:2048' for i in range(5)), frames=400)
    assert baron(sram) == [BEST]*5
    _, _, log = run('rewind-navigation', '62-66:512', FZERO_REWIND_TEST='1', FZERO_TEST_SAVE_FRAME='60')
    assert 'rewind: actual ring restore and ten-frame resimulation identical' in log
    run('save-view', '60-66:512', FZERO_STATE_SAVE=str(out/'other-car.sav'), FZERO_TEST_SAVE_FRAME='140')
    ram, sram, _ = run('restore-view', state='other-car.sav')
    assert ram[0x54:0x57] == b'\0\3\0' and baron(sram) == [EMPTY]*5
    assert files() == before, 'Browsing changed/created persistent record files'
    if a.navigation_only:
        print('PASS: navigation, records detail DMA, rewind, snapshots and save preservation')
        return

    # Native courses have their own per-vehicle namespace too.
    ram, sram, log = run('native-car-completion', ROUTE, frames=6400, state=None,
                      FZERO_CUP='bs-deluxe/knight', FZERO_TEST_RECORDS_CUP='1',
                      FZERO_TEST_VEHICLE='moon-shadow', FZERO_STATE_SAVE=str(out/'native-car.sav'),
                      FZERO_TEST_SAVE_FRAME='6300')
    assert ram[0x54:0x57] == b'\0\3\0'
    assert 'vehicle=MOON SHADOW' in log
    # Moon Shadow's original guest slot sets the same BCD valid flag as Falcon.
    assert totals(sram) == [BEST]*5
    _, sram, _ = run('native-car-select-keeps-records', '60-66:4', state='native-car.sav')
    assert totals(sram) == [BEST]*5
    _, sram, _ = run('native-car-gp-return', '60-66:4,120-126:4', state='native-car.sav')
    assert totals(sram) == [BEST]*5
    assert len(files()) == len(before)+1
    # Additive packs also run on the retail engine without the Deluxe car mod.
    out = out/'max-stock'
    out.mkdir()
    shutil.copytree(ROOT/'assets', out/'assets')
    (out/'packs').mkdir()
    for name in ('cgp', 'max-league', 'bower-league'):
        (out/f'packs/{name}.disabled').write_text('0\n' if name == 'max-league' else '1\n')
    env.update(FZERO_CGP_CARS='0', FZERO_CGP_REBALANCE='0', FZERO_RULES='', FZERO_CUP='max-league/max')
    ram, sram, _ = run('max-completion', ROUTE, frames=6400, state=None,
                      FZERO_TEST_RECORDS_CUP='1', FZERO_STATE_SAVE=str(out/'records.sav'),
                      FZERO_TEST_SAVE_FRAME='6300')
    assert ram[0x54:0x57] == b'\0\3\0' and totals(sram) == [BEST]*5
    ram, sram, _ = run('max-detail', '60-66:8')
    assert ram[0x54:0x57] == b'\0\5\0' and totals(sram) == [BEST]*5
    ram, sram, _ = run('max-page-back', '60-66:1024,120-126:2048')
    assert ram[0x54:0x57] == b'\0\3\0' and totals(sram) == [BEST]*5
    # Empty native courses are selectable as well; no fake record is inserted.
    ram, sram, _ = run('max-native-empty', '60-66:1024,120-126:8')
    assert ram[0x54:0x57] == b'\0\5\0' and totals(sram) == [EMPTY]*5
    print('PASS: navigation, every car/page, detail/back, unified records, rewind, snapshots and save preservation')


if __name__ == '__main__':
    main()
