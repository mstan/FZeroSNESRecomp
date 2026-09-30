"""Private-ROM regression: class navigation, confirmation fade and intro colors.

Uses native pad input and disposable saves. Captures remain under --out.
"""
import argparse
import hashlib
import os
from pathlib import Path
import shutil
import subprocess

from validate_native_menus import player_route

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ('build', 'stock', 'packs', 'out'):
        parser.add_argument('--'+key, type=Path, required=True)
    args = parser.parse_args()
    build, stock, packs, out = (getattr(args, k).resolve() for k in ('build', 'stock', 'packs', 'out'))
    out.mkdir(parents=True, exist_ok=False)
    clean = {k: v for k, v in os.environ.items()
             if not k.startswith(('FZERO_', 'SNESRECOMP_', 'SDL_', 'LNG_'))}
    original = hashlib.sha256(stock.read_bytes()).digest()
    # Five Downs stop at Legend; Select cycles; Up stops at Beginner.
    inputs = ',600-606:8,730-736:8,850-856:8'
    inputs += ',1070-1073:32,1110-1113:32,1150-1153:32,1190-1193:32,1230-1233:32'
    inputs += ',1270-1273:4,1310-1313:16,1350-1353:32,1390-1393:32,1430-1436:8'
    levels = {1081: 1, 1121: 2, 1161: 3, 1201: 4, 1241: 4,
              1281: 0, 1321: 0, 1361: 1, 1401: 2, 1451: 2}
    colors = (1491, 1501, 1511, 1521, 1531, 1551)
    # Two different imported cohorts; the initial stock colors used to last
    # through frame 1521, then change at the first exhaust palette update.
    for identity in (4, 11):
        folder = out/str(identity)
        folder.mkdir()
        shutil.copytree(ROOT/'assets', folder/'assets')
        env = dict(clean, FZERO_DELUXE_DATA='embedded', FZERO_CGP_CARS='7',
                   FZERO_CGP_REBALANCE='15', FZERO_RULES='all', FZERO_PACK_LOADER='1',
                   FZERO_PACKS_DIR=str(packs), FZERO_TRACK_PACKS='settings', FZERO_CUP='cgp/cgp-4',
                   SNESRECOMP_SAVE_ROOT='s', FZERO_VIEWPORT_SCRIPT='0:16:9',
                   SNESRECOMP_INPUT_SCRIPT=player_route(identity)+inputs,
                   FZERO_CAPTURE_FRAMES=','.join(map(str, (*levels, *colors))),
                   FZERO_CAPTURE_PREFIX=str(folder/'capture'))
        result = subprocess.run([str(build/'FZeroSNESRecompHeadless.exe'), str(stock), '1600'],
                                cwd=folder, env=env, capture_output=True, text=True, timeout=300)
        (folder/'run.log').write_text(result.stdout+result.stderr)
        assert result.returncode == 0, result.stderr[-2000:]

        def ram(frame):
            return (folder/f'capture-{frame:06d}.bin').read_bytes()[-0x20000-8:-8]

        for frame, level in levels.items():
            assert ram(frame)[0x57] == level, (identity, frame, 'wrong class')
        assert ram(1451)[0x54:0x57] == bytes([1, 7, 2]), 'Did not exercise confirmation fade'
        reference = ram(colors[-1])
        for frame in colors:
            current = ram(frame)
            assert current[0x54] == 2 and current[0x55] == 0, 'Did not exercise course intro'
            for base in (0x600, 0x700, 0x800):
                for row in range(4):
                    offset = base + 6 + row*32
                    assert current[offset:offset+2] == reference[offset:offset+2], \
                        (identity, frame, 'intro color changed')
        print(f'PASS car {identity}: class limits, Select cycle, confirmed Expert, stable intro colors', flush=True)
    assert hashlib.sha256(stock.read_bytes()).digest() == original


if __name__ == '__main__':
    main()
