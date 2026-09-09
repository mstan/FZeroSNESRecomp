"""Bounded Vulkan/DLSS validation, with child timeout and captured output."""
import argparse
import os
from pathlib import Path
import subprocess

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
p.add_argument('--rom', type=Path, required=True)
p.add_argument('--frames', type=int, default=1800)
p.add_argument('--dlss', action='store_true')
p.add_argument('--screenshot', type=Path)
p.add_argument('--input-script', default='')
p.add_argument('--backend', choices=['Vulkan', 'SDL'], default='Vulkan')
p.add_argument('--runtime', type=Path)
p.add_argument('--viewport-script', default='')
a = p.parse_args()
b = a.build.resolve()
env = os.environ.copy()
env.update(FZERO_OUTPUT_METHOD=a.backend, FZERO_DLSS=str(int(a.dlss)),
           FZERO_DLSS_PYTHON=str(b / 'dlss-deps/venv/Scripts/python.exe'),
           FZERO_DLSS_ROOT=str(b / 'dlss-deps/bridge/ComfyUI-DLSS5-NR'),
           SNESRECOMP_AUTOCLOSE_FRAMES=str(a.frames),
           SNESRECOMP_SAVE_ROOT=str(b / 'saves'),
           SNESRECOMP_INPUT_SCRIPT=a.input_script,
           FZERO_VIEWPORT_SCRIPT=a.viewport_script,
           SNESRECOMP_WRAM_DUMP=str(b / ('dlss-final.wram' if a.dlss else 'baseline-final.wram')))
if a.runtime:
    env['FZERO_DLSS_ROOT'] = str(a.runtime.resolve())
env['PATH'] = 'C:/msys64/mingw64/bin;' + env['PATH']
if a.screenshot:
    env['FZERO_PRESENT_CAPTURE'] = str(a.screenshot.resolve())
    env['FZERO_PRESENT_CAPTURE_FRAME'] = str(a.frames - 5)
with (b / ('dlss-run.log' if a.dlss else 'vulkan-run.log')).open('w') as log:
    child = subprocess.Popen([str(b / 'FZeroSNESRecomp.exe'), str(a.rom.resolve())],
                             cwd=b, env=env, stdout=log, stderr=subprocess.STDOUT)
    try:
        code = child.wait(timeout=max(60, a.frames / 30 + 30))
    except subprocess.TimeoutExpired:
        child.kill()
        child.wait()
        raise
print('exit:', code)
raise SystemExit(code)
