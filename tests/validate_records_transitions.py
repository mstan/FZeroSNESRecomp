"""Private-ROM visual regression: no donor labels leak through Records fades.

Compare every presented transition frame against the two completed pages,
including the actual pack skyline. Uses disposable saves and native pad input.
"""
import argparse
import hashlib
import os
from pathlib import Path
import re
import shutil
import subprocess

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for key in ('build', 'stock', 'packs', 'out'):
        p.add_argument('--'+key, type=Path, required=True)
    a = p.parse_args()
    build, stock, packs, out = (getattr(a, k).resolve() for k in ('build', 'stock', 'packs', 'out'))
    out.mkdir(parents=True, exist_ok=False)
    clean = {k: v for k, v in os.environ.items()
             if not k.startswith(('FZERO_', 'SNESRECOMP_', 'SDL_', 'LNG_'))}
    # A standalone Astra copy gives known cup order and exercises donor art.
    shutil.copytree(packs/'astra-front', out/'catalog/astra-front', ignore=shutil.ignore_patterns('music', '.cache'))
    (out/'empty').mkdir()
    for name in ('retail', 'bs', 'packs', 'packs-wide'):
        folder = out/name
        folder.mkdir()
        shutil.copytree(ROOT/'assets', folder/'assets')
        imported, wide = name.startswith('packs'), name.endswith('wide')
        env = dict(clean, FZERO_DELUXE_DATA='embedded', FZERO_BS_CARS=str(int(name == 'bs')),
                   FZERO_BS_TRACKS=str(int(name == 'bs')), FZERO_ALWAYS_RECORDS='1',
                   FZERO_CGP_CARS='7' if imported else '0', FZERO_CGP_REBALANCE='15' if imported else '0',
                   FZERO_RULES='all' if imported else '', FZERO_TRACK_PACKS='settings',
                   FZERO_PACKS_DIR=str(out/('catalog' if imported else 'empty')),
                   SNESRECOMP_SAVE_ROOT='s', FZERO_TEST_SAVE_SRAM='1', FZERO_SCENE_TRACE='1')
        if wide:
            env['FZERO_VIEWPORT_SCRIPT'] = '0:16:9'

        def run(label, frames, **extra):
            e = dict(env, FZERO_TEST_SRAM_DUMP=str(folder/f'{label}.sram'), **extra)
            r = subprocess.run([str(build/'FZeroSNESRecompHeadless.exe'), str(stock), str(frames)],
                               cwd=folder, env=e, capture_output=True, text=True, timeout=120)
            log = r.stdout+r.stderr
            (folder/f'{label}.log').write_text(log)
            assert r.returncode == 0, (name, label, log[-2000:])
            return log

        enter = '300-306:32,360-366:32,420-426:8'+(',510-516:2048' if imported else '')
        run('overview', 901, SNESRECOMP_INPUT_SCRIPT=enter,
            FZERO_STATE_SAVE=str(folder/'overview.sav'), FZERO_TEST_SAVE_FRAME='900')
        files = lambda: {str(f.relative_to(folder)): hashlib.sha256(f.read_bytes()).hexdigest()
                         for f in (folder/'s').rglob('*') if f.is_file() and f.suffix != '.bak'}
        before = files()
        frames = folder/'frames'
        frames.mkdir()
        log = run('navigation', 350, FZERO_STATE_LOAD=str(folder/'overview.sav'),
                  SNESRECOMP_INPUT_SCRIPT='10-16:8,100-106:128,180-186:64,260-266:1',
                  FZERO_TEST_FRAME_DIR=str(frames), FZERO_STATE_SAVE=str(folder/'mid-fade.sav'),
                  FZERO_TEST_SAVE_FRAME='110')
        if imported:
            assert 'course=u-zero-1' in log and 'course=candany' in log, log[-2000:]
        brightness = {int(f): int(b, 16) for f, b in
                      re.findall(r'scene (\d+) .*brightness=([0-9a-f]+)', log)}
        images = [Image.open(frames/f'{i:06}.ppm').convert('RGB') for i in range(350)]
        extra = (images[0].width-256)//2
        if imported:
            # U Zero's transparent star field uses the native black backdrop,
            # never unused gray palette zero or an invented blue car heading.
            colors = set(images[99].crop((extra, 0, extra+256, 56)).getdata())
            assert (123, 123, 140) not in colors, 'Palette-zero blocks behind stars'
            assert (184, 232, 255) not in colors, 'Unwanted vehicle/mode heading'
        region = (extra+8, 76, extra+128, 114)
        crops = [im.crop(region) for im in images]
        first_switch = next(i for i in range(100, 180) if brightness[i] & 15 == 0)
        second_switch = next(i for i in range(180, 260) if brightness[i] & 15 == 0)
        assert crops[99].tobytes() != crops[179].tobytes(), 'Course navigation did not change labels'
        for i in range(100, 260):
            ref = 99 if i < first_switch else 179 if i < second_switch else 259
            level = 0 if brightness[i] & 128 else brightness[i] & 15
            expected = crops[ref].point([n*level//15 for n in range(256)]*3)
            assert crops[i].tobytes() == expected.tobytes(), (name, 'labels flashed', i)
            strip = (extra, 0, extra+256, 56)
            expected = images[ref].crop(strip).point([n*level//15 for n in range(256)]*3)
            assert images[i].crop(strip).tobytes() == expected.tobytes(), (name, 'skyline changed early', i)
        # Entering/backing out must fade the complete labels/art too, rather
        # than showing half an overview and half a course during setup.
        for start, end, old, new in ((10, 99, 9, 99), (260, 349, 259, 349)):
            switch = next(i for i in range(start, end) if brightness[i] & 15 == 0)
            for i in range(start, end):
                ref = old if i < switch else new
                level = 0 if brightness[i] & 128 else brightness[i] & 15
                for region_check in (region, (extra, 0, extra+256, 56)):
                    expected = images[ref].crop(region_check).point([n*level//15 for n in range(256)]*3)
                    assert images[i].crop(region_check).tobytes() == expected.tobytes(), (name, 'entry/back flash', i)
        # The same presentation must survive loading halfway through the fade.
        resumed = folder/'resumed'
        resumed.mkdir()
        run('resume', 45, FZERO_STATE_LOAD=str(folder/'mid-fade.sav'), FZERO_TEST_FRAME_DIR=str(resumed))
        for i in range(45):
            restored = Image.open(resumed/f'{i:06}.ppm').convert('RGB')
            assert restored.crop(region).tobytes() == crops[110+i].tobytes(), (name, 'fade reload', i)
        assert files() == before, 'Read-only browsing changed persistent records'
        print(f'PASS {name}: both directions, every fade frame, loaded-page art, snapshot, save preservation', flush=True)


if __name__ == '__main__':
    main()
