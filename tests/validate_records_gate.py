"""Private-ROM title RECORDS gate: fresh saves, both engines, no fake records."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for key in ('build', 'stock', 'out'):
        p.add_argument('--'+key, type=Path, required=True)
    a = p.parse_args()
    build, stock, out = a.build.resolve(), a.stock.resolve(), a.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    clean = {k: v for k, v in os.environ.items()
             if not k.startswith(('FZERO_', 'SNESRECOMP_', 'SDL_', 'LNG_'))}
    for deluxe in (0, 1):
        for enabled in (0, 1):
            folder = out/f'{deluxe}-{enabled}'
            folder.mkdir()
            shutil.copytree(ROOT/'assets', folder/'assets')
            (folder/'empty-packs').mkdir()
            (folder/'settings').mkdir()
            (folder/'settings/loader.cfg').write_text('0\n')
            env = dict(clean, FZERO_DELUXE_DATA='embedded', FZERO_BS_CARS=str(deluxe),
                       FZERO_BS_TRACKS=str(deluxe), FZERO_CGP_CARS='0', FZERO_RULES='',
                       FZERO_PACKS_DIR=str(folder/'empty-packs'), FZERO_TRACK_PACKS='settings',
                       FZERO_ALWAYS_RECORDS=str(enabled), SNESRECOMP_SAVE_ROOT='s',
                       FZERO_TEST_SAVE_SRAM='1')

            def run(label, inputs='', frames=900, **extra):
                e = dict(env, SNESRECOMP_INPUT_SCRIPT=inputs,
                         SNESRECOMP_WRAM_DUMP=str(folder/f'{label}.ram'),
                         SNESRECOMP_FRAME_DUMP=str(folder/f'{label}.ppm'),
                         FZERO_TEST_SRAM_DUMP=str(folder/f'{label}.sram'), **extra)
                result = subprocess.run([str(build/'FZeroSNESRecompHeadless.exe'), str(stock), str(frames)],
                                        cwd=folder, env=e, capture_output=True, text=True, timeout=90)
                (folder/f'{label}.log').write_text(result.stdout+result.stderr)
                assert result.returncode == 0, result.stderr[-2000:]
                ram = (folder/f'{label}.ram').read_bytes()
                sram = (folder/f'{label}.sram').read_bytes()
                print(folder.name, label, ram[0x54:0x57].hex(), flush=True)
                return ram, sram

            _, fresh = run('title')
            assert (folder/'s'/('bs-deluxe' if deluxe else '')/'save.srm').exists(), list((folder/'s').rglob('*'))
            inputs = '300-306:32,360-366:32,420-426:8'
            ram, _ = run('enter', inputs)
            assert ram[0x54:0x57] == (b'\0\3\0' if enabled else b'\1\1\0'), (folder, ram[0x54:0x57])
            if enabled:
                ram, _ = run('detail', inputs+',540-546:8')
                assert ram[0x54:0x57] == (b'\0\3\4' if deluxe else b'\0\5\0')
                run('save', inputs, FZERO_STATE_SAVE=str(folder/'records.sav'), FZERO_TEST_SAVE_FRAME='800')
                ram, _ = run('reload', frames=180, FZERO_STATE_LOAD=str(folder/'records.sav'))
                assert ram[0x54:0x57] == b'\0\3\0'
                ram, after = run('exit', inputs+',680-686:1')
                assert ram[0x54:0x56] == b'\0\1'
                assert after == fresh, 'Opening empty records changed SRAM'
                assert not list((folder/'s').rglob('records.bin')), 'Browsing created records'
    print('PASS: empty-save title visibility/selection, exit and SRAM preservation on both engines')


if __name__ == '__main__':
    main()
