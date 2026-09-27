"""Private-ROM regression for Astra's Z leaking into the first-lap S OK! sprite.

Uses native Practice navigation and finish handling; only position/checkpoint
and lap time are seeded. Captures, saves and ROM-derived bytes stay in --out.
"""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

from validate_practice_catalog import route

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'tools'))
from audit_intro_font import atlas


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for key in ('build', 'stock', 'packs', 'out'):
        p.add_argument('--'+key, type=Path, required=True)
    a = p.parse_args()
    build, stock, packs, out = (getattr(a, k).resolve() for k in ('build', 'stock', 'packs', 'out'))
    out.mkdir(parents=True, exist_ok=False)
    # Test only Astra, so its first cup is always after the three retail cups.
    shutil.copytree(packs/'astra-front', out/'packs/astra-front')
    native = atlas(stock.read_bytes())
    clean = {k: v for k, v in os.environ.items()
             if not k.startswith(('FZERO_', 'SNESRECOMP_', 'SDL_', 'LNG_'))}
    for player in (5, 11):  # Dragon Bird and Red Gazelle, as reported.
        folder = out/str(player)
        folder.mkdir()
        shutil.copytree(ROOT/'assets', folder/'assets')
        env = dict(clean, FZERO_DELUXE_DATA='embedded', FZERO_BS_CARS='0', FZERO_BS_TRACKS='0',
                   FZERO_CGP_CARS='7', FZERO_CGP_REBALANCE='15', FZERO_RULES='all',
                   FZERO_PACKS_DIR=str(out/'packs'), FZERO_TRACK_PACKS='settings',
                   FZERO_TRACE_LIBRARY='1', SNESRECOMP_SAVE_ROOT='s')

        def run(label, frames, **extra):
            e = dict(env, SNESRECOMP_WRAM_DUMP=str(folder/f'{label}.ram'), **extra)
            result = subprocess.run([str(build/'FZeroSNESRecompHeadless.exe'), str(stock), str(frames)],
                                    cwd=folder, env=e, capture_output=True, text=True, timeout=120)
            log = result.stdout+result.stderr
            (folder/f'{label}.log').write_text(log)
            assert result.returncode == 0, log[-2000:]
            return (folder/f'{label}.ram').read_bytes(), log

        ram, log = run('before', 2501,
            SNESRECOMP_INPUT_SCRIPT=route(player, 255)+
                ',1200-1206:8,1300-1303:32,1330-1333:32,1360-1363:32,1450-1456:8,1800-1806:8',
            FZERO_CAPTURE_FRAMES='1600,2500', FZERO_CAPTURE_PREFIX=str(folder/'before'),
            FZERO_STATE_SAVE=str(folder/'before.sav'), FZERO_TEST_SAVE_FRAME='2500')
        assert 'menu 4/5: Astra' in log and 'ordinal=0 setting=c1' in log, log[-2000:]
        assert ram[0x54:0x57] == b'\2\3\0'
        (folder/'cross.txt').write_text('0 0 d40 0080\n0 0 d00 '+ram[0xad:0xae].hex()+
            '\n0 0 b70 90'+ram[0xae:0xaf].hex()+'\n0 0 b90 80'+ram[0xaf:0xb0].hex()+
            '\n0 0 c3 00\n0 0 c0 002052\n')
        ram, _ = run('lap', 80, FZERO_STATE_LOAD=str(folder/'before.sav'),
            FZERO_TEST_WRAM_SCRIPT=str(folder/'cross.txt'), FZERO_CAPTURE_FRAMES='2502,2520,2580',
            FZERO_CAPTURE_PREFIX=str(folder/'lap'))
        assert ram[0xd40] == ram[0xf53] == 1, 'Native first-lap crossing did not execute'
        for label in ('before-001600', 'before-002500', 'lap-002502', 'lap-002520', 'lap-002580'):
            capture = folder/f'{label}.bin'
            data = capture.read_bytes()
            scene = json.loads(capture.with_suffix('.json').read_text())
            intro = label == 'before-001600'
            assert scene['state'][:2] == ([2, 1] if intro else [2, 3]), (label, scene['state'])
            for tile in (0x8f, 0x9f, 0xa7, 0xb7):
                offset = 224*1120+0xa000+tile*32
                expected = native[tile][2]+bytes(16)
                if not intro:
                    assert data[offset:offset+32] == expected, (player, label, hex(tile))
                elif tile in (0xa7, 0xb7):
                    assert data[offset:offset+32] != expected, 'Astra intro lost its custom Z'
            if label == 'lap-002502':
                assert any(o[0] == 19 and o[3] & 511 == 0x1a6 and o[4] for o in scene['oam']), 'S OK! missing'
            subprocess.run([str(build/'FZeroRenderCapture.exe'), str(capture), '4:3',
                            str(capture.with_suffix('.ppm'))], check=True, capture_output=True)
        print(f'PASS: vehicle {player}, custom Astra intro, native first-lap message, state reload', flush=True)


if __name__ == '__main__':
    main()
