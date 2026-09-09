"""Export a deterministic retail F-Zero race sequence for the NR experiment."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build', type=Path, required=True)
p.add_argument('--rom', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--first', type=int, default=1500)
p.add_argument('--count', type=int, default=300)
a = p.parse_args()
if a.first < 0 or not 1 <= a.count <= 300:
    p.error('first must be nonnegative and count must be 1..300')
build, rom, output = a.build.resolve(), a.rom.resolve(), a.output.resolve()
output.mkdir(parents=True, exist_ok=False)
frames = output / 'frames'
frames.mkdir()
route = '180:8,300:8,450:8,600:8,750:8,900:8,1000-10000:1'
env = {k: v for k, v in os.environ.items() if not k.startswith(('FZERO_', 'SNESRECOMP_'))}
env['PATH'] = 'C:/msys64/mingw64/bin;' + env['PATH']
env.update(FZERO_CAPTURE_FRAMES=','.join(map(str, range(a.first, a.first + a.count))),
           FZERO_CAPTURE_PREFIX=str(output / 'frame'),
           SNESRECOMP_SAVE_ROOT=str(output / 'saves'), SNESRECOMP_INPUT_SCRIPT=route)
with (output / 'capture.log').open('w') as log:
    subprocess.run([str(build / 'FZeroSNESRecompHeadless.exe'), str(rom), str(a.first + a.count)],
                   cwd=output, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=120, check=True)
    for number in range(a.first, a.first + a.count):
        subprocess.run([str(build / 'FZeroRenderCapture.exe'), str(output / f'frame-{number:06d}.bin'),
                        '4:3', str(frames / f'{number:06d}.ppm')],
                       env=env, stdout=log, stderr=subprocess.STDOUT, timeout=10, check=True)
revision = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=Path(__file__).resolve().parents[1], text=True).strip()
with (build / 'FZeroSNESRecompHeadless.exe').open('rb') as stream:
    executable_digest = hashlib.file_digest(stream, 'sha256').hexdigest()
with rom.open('rb') as stream:
    digest = hashlib.file_digest(stream, 'sha256').hexdigest()
(output / 'manifest.json').write_text(json.dumps(dict(first=a.first, count=a.count,
    width=256, height=224, simulation_hz=60.098811862, source_revision=revision,
    rom_sha256=digest, executable_sha256=executable_digest, route=route, stock_diff_pixels=0), indent=2))
print(frames)
