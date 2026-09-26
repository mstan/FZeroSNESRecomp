"""Private-source regression: original FZEdit BMP, ZIP and disposable cache."""
import argparse
import io
import json
from pathlib import Path
import subprocess
import sys
import zipfile
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from package_fzedit_course import package


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source', type=Path, required=True)
    p.add_argument('--build', type=Path, required=True)
    p.add_argument('--out', type=Path, required=True)
    a = p.parse_args()
    out = a.out.resolve(); out.mkdir(parents=True, exist_ok=False)
    build = a.build.resolve()
    inputs = out / 'inputs'; inputs.mkdir()
    archive = inputs / 'huckmine.zip'
    package(a.source, archive, 'huckmine-test', 'Author-supplied test fixture')
    with zipfile.ZipFile(a.source) as original, zipfile.ZipFile(archive) as packed:
        for entry in original.infolist():
            if not entry.is_dir():
                assert original.read(entry) == packed.read(entry.filename)
        assert len([e for e in original.infolist() if not e.is_dir()]) == 12
    def inspect(directory, good=True):
        run = subprocess.run([str(build / 'FZeroInspectPacks.exe'), str(directory)],
                             cwd=out, capture_output=True, text=True, timeout=30)
        assert (run.returncode == 0) == good, run.stdout + run.stderr
        return run.stdout, run.stderr
    expected, errors = inspect(inputs)
    assert not errors and '1 course entries' in expected
    cache = list((out / 'mods/packs/.cache/courses').glob('*.fzc'))
    assert len(cache) == 1
    stamp = cache[0].stat().st_mtime_ns
    assert inspect(inputs) == (expected, '')
    assert cache[0].stat().st_mtime_ns == stamp
    extracted = out / 'extracted'; (extracted / 'hm').mkdir(parents=True)
    with zipfile.ZipFile(archive) as packed:
        packed.extractall(extracted / 'hm')  # Previously validated test fixture.
    assert inspect(extracted) == (expected, '')
    mini = extracted / 'hm/HM/hm_Minimap.bmp'
    original_bmp = mini.read_bytes()
    # Independent standard BMP encoding must produce identical course data.
    Image.open(io.BytesIO(original_bmp)).convert('RGB').save(mini)
    assert inspect(extracted) == (expected, '')
    malformed = bytearray(original_bmp)
    malformed[58:62] = malformed[54:58]  # Overlapping red/green masks.
    mini.write_bytes(malformed)
    assert 'Invalid BMP color masks' in inspect(extracted, False)[1]
    mini.write_bytes(original_bmp[:60])
    assert 'BMP' in inspect(extracted, False)[1]
    mini.write_bytes(original_bmp)
    assert inspect(extracted) == (expected, '')
    report = dict(source_files_unchanged=12, zip_folder_parity=True,
                  cold_warm_cache=True, bmp_encoding_parity=True,
                  invalid_masks_and_truncation_rejected=True)
    (out / 'validation.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
