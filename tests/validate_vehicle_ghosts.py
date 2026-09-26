"""Record/save/reboot/play back CGP ghosts through the native Practice menus.

The finish fixture seeds the last lap, not a physically driven full race. The
native recording stream, checksum, Save Ghost dialog and playback still run.
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
sys.path.insert(0, str(ROOT / 'tools'))
from inspect_bs_deluxe import apply_ips


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('build', 'stock', 'packs', 'out'):
        parser.add_argument('--' + name, type=Path, required=True)
    a = parser.parse_args()
    build, stock, out = a.build.resolve(), a.stock.resolve(), a.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    clean = {k: v for k, v in os.environ.items()
             if not k.startswith(('FZERO_', 'SNESRECOMP_', 'SDL_', 'LNG_'))}
    original = stock.read_bytes()
    report = {}
    for identity, group, source_slot in ((5, 1, 1), (10, 3, 0)):
        folder = out / str(identity)
        folder.mkdir()
        shutil.copytree(ROOT / 'assets', folder / 'assets')
        env = dict(clean, FZERO_DELUXE_DATA='embedded', FZERO_BS_CARS='0',
                   FZERO_BS_TRACKS='0', FZERO_CGP_CARS='7', FZERO_CGP_REBALANCE='15',
                   FZERO_RULES='all', FZERO_CUP='bs-deluxe/knight',
                   FZERO_PACKS_DIR=str(a.packs.resolve()), FZERO_PACK_LOADER='1',
                   FZERO_TRACK_PACKS='packs', SNESRECOMP_SAVE_ROOT='s',
                   FZERO_TEST_SAVE_SRAM='1')

        def run(label, frames, inputs, **extra):
            settings = dict(env, SNESRECOMP_INPUT_SCRIPT=inputs,
                            SNESRECOMP_WRAM_DUMP=str(folder / (label + '.ram')),
                            SNESRECOMP_FRAME_DUMP=str(folder / (label + '.ppm')),
                            FZERO_TEST_SRAM_DUMP=str(folder / (label + '.sram')), **extra)
            proc = subprocess.run([str(build / 'FZeroSNESRecompHeadless.exe'), str(stock), str(frames)],
                                  cwd=folder, env=settings, capture_output=True, text=True, timeout=180)
            (folder / (label + '.log')).write_text(proc.stdout + proc.stderr, encoding='utf-8')
            assert proc.returncode == 0, (label, proc.stderr[-2000:])
            return (folder / (label + '.ram')).read_bytes(), (folder / (label + '.sram')).read_bytes()

        enter = ',1200-1206:8,1300-1306:8,1700-1706:8'
        _, saved = run('record', 5000, route(identity, 255) + enter +
                       ',4100-4106:256,4600-4606:64,4700-4706:256', FZERO_TEST_RECORDS_CUP='1')
        assert saved[0x3aa] == 0x80 | identity, (identity, 'saved identity', saved[0x3a0:0x3b0].hex())
        start = int.from_bytes(saved[0x3a2:0x3a4], 'little')
        size = int.from_bytes(saved[0x3a4:0x3a6], 'little')
        assert size > 0 and sum(saved[start:start + size]) & 65535 == int.from_bytes(saved[0x3a6:0x3a8], 'little')
        cart_path = folder / 'playback.cart'
        ram, replayed = run('playback', 1995, route(identity, 254) + enter, FZERO_CART_DUMP=str(cart_path))
        assert ram[0x54:0x56] == b'\2\3' and ram[0x14ca0] & 0x80, (identity, 'ghost inactive')
        slot = ram[0xcf2]
        assert slot == ram[0x14ceb] == ram[0x1133] != ram[0x52], (identity, 'slot collision')
        assert ram[0x14ce8] == identity and ram[0x14cea] == 1, (identity, 'codec overwrote identity')
        cart = cart_path.read_bytes()
        art = apply_ips(original, (ROOT / f'assets/vehicle-packs/cgp-p{group}.ips').read_bytes())
        start = 0x40000 + source_slot * 0x8000
        assert cart[0x350000:0x355000] == art[start:start + 0x5000], (identity, 'wrong ghost art')
        assert cart[0xf00ff + slot * 256] == identity, (identity, 'wrong ghost metadata')
        assert replayed[0x3a0:0x3b0] == saved[0x3a0:0x3b0], (identity, 'saved ghost changed')
        report[str(identity)] = dict(saved_identity=saved[0x3aa], slot=slot, stream_bytes=size,
                                     native_checksum=True, artwork=True, reboot=True)
        print(identity, 'native ghost save/playback PASS', flush=True)
    assert stock.read_bytes() == original
    (out / 'validation.json').write_text(json.dumps(report, indent=2) + '\n')


if __name__ == '__main__':
    main()
