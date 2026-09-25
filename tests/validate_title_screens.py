"""Private-ROM checks for independent title artwork, snapshots and records.

Leaves captures/saves in a fresh private output directory. Does not alter a
player's configuration or source ROM. Uses scripted native race completion.
"""
import argparse
import os
from pathlib import Path
import shutil
import subprocess

from validate_records import BEST, ROUTE, totals

ROOT = Path(__file__).resolve().parents[1]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for key in ('build', 'stock', 'out'):
        p.add_argument('--'+key, type=Path, required=True)
    a = p.parse_args()
    build, stock, out = a.build.resolve(), a.stock.resolve(), a.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    shutil.copytree(ROOT/'assets/track-packs', out/'assets/track-packs')
    (out/'packs').mkdir()
    for file in (ROOT/'assets/track-packs').glob('*.ini'):
        (out/f'packs/{file.stem}.disabled').write_text('1\n')
    choice = out/'packs/title-screen.choice'
    clean = {k:v for k,v in os.environ.items() if not k.startswith(('FZERO_', 'SNESRECOMP_', 'SDL_', 'LNG_'))}
    base = dict(clean, FZERO_DELUXE_DATA='embedded', FZERO_CGP_CARS='0',
                FZERO_CGP_REBALANCE='0', FZERO_RULES='', FZERO_TRACK_PACKS='packs',
                FZERO_TEST_SAVE_SRAM='1')

    def run(label, env, frames, expected=0, **extra):
        proc = subprocess.run([str(build/'FZeroSNESRecompHeadless.exe'), str(stock), str(frames)],
            cwd=out, env=dict(env, SNESRECOMP_FRAME_DUMP=str(out/f'{label}.ppm'),
                             SNESRECOMP_WRAM_DUMP=str(out/f'{label}.ram'),
                             FZERO_TEST_SRAM_DUMP=str(out/f'{label}.sram'), **extra),
            capture_output=True, text=True, timeout=240)
        log = proc.stdout+proc.stderr
        (out/f'{label}.log').write_text(log, encoding='utf-8')
        assert proc.returncode == expected, (label, proc.returncode, log[-2000:])
        if not expected:
            assert 'fzero_native: PASS' in log
        return log

    for bs in (0, 1):
        env = dict(base, FZERO_BS_CARS=str(bs), FZERO_BS_TRACKS=str(bs), SNESRECOMP_SAVE_ROOT=f's{bs}')
        frames = {}
        for screen in ('original', 'cgp', 'max-league', 'fzero-55'):
            label = f'{bs}-{screen}'
            choice.write_text('1|'+screen+'\n')
            log = run(label, env, 300, FZERO_STATE_SAVE=str(out/f'{label}.sav'), FZERO_TEST_SAVE_FRAME='299')
            assert 'extracted ' not in log, 'Title must not require any course pack'
            assert ('title artwork enabled' in log) == (screen != 'original')
            frames[screen] = (out/f'{label}.ppm').read_bytes()
            run(label+'-resume', env, 90, FZERO_STATE_LOAD=str(out/f'{label}.sav'))
        assert len(set(frames.values())) == 4, 'Screens must produce distinct artwork'
        choice.write_text('0|max-league\n')
        run(f'{bs}-disabled', env, 300)
        assert (out/f'{bs}-disabled.ppm').read_bytes() == frames['original']
        choice.write_text('1|max-league\n')
        bad = run(f'{bs}-wrong-art-state', env, 1, expected=3,
                  FZERO_STATE_LOAD=str(out/f'{bs}-cgp.sav'))
        assert 'unable to load compatible state' in bad
        print(f'engine {bs}: titles/disable/snapshot identity PASS', flush=True)

    # Title-only mode must preserve stock SRAM and native records/menu behavior.
    env = dict(base, FZERO_BS_CARS='0', FZERO_BS_TRACKS='0', SNESRECOMP_SAVE_ROOT='records')
    choice.write_text('0|original\n')
    run('records-original', env, 6400, SNESRECOMP_INPUT_SCRIPT=ROUTE, FZERO_TEST_RECORDS_CUP='1')
    assert totals((out/'records-original.sram').read_bytes()) == [BEST]*5
    saved = (out/'records/save.srm').read_bytes()
    choice.write_text('1|max-league\n')
    run('records-max', env, 1400, SNESRECOMP_INPUT_SCRIPT=ROUTE)
    assert totals((out/'records-max.sram').read_bytes()) == [BEST]*5
    assert (out/'records/save.srm').read_bytes() == saved
    assert not list((out/'records').rglob('records.bin'))
    print('title-only mode: original records survive unchanged PASS', flush=True)

    # Missing custom artwork falls back to Original, with a visible diagnostic.
    patch = out/'assets/track-packs/presentation/max-league.ips'
    patch.rename(patch.with_suffix('.missing'))
    log = run('missing-art', dict(env, SNESRECOMP_SAVE_ROOT='missing'), 300)
    assert 'Missing title artwork patch; using Original' in log
    assert (out/'missing-art.ppm').read_bytes() == (out/'0-original.ppm').read_bytes()
    print('missing artwork: original fallback PASS', flush=True)


if __name__ == '__main__':
    main()
