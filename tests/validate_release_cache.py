"""Relocate a player ZIP: reuse its course cache, rebuild only an edited course.

Uses the shipped editable projects and real loader, without a ROM or gameplay
fixture. Work stays in a fresh output directory; the release is never modified.
"""
import argparse
import json
import os
from pathlib import Path
import subprocess
import time
import zipfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ('build', 'bundle', 'out'):
        parser.add_argument('--' + key, type=Path, required=True)
    args = parser.parse_args()
    exe = args.build.resolve() / 'FZeroInspectPacks.exe'
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    with zipfile.ZipFile(args.bundle.resolve()) as archive:
        # This is our release archive, not untrusted user input.
        archive.extractall(out)
    install = next(p for p in out.iterdir() if p.is_dir())
    packs = install / 'mods/packs'
    cache = packs / '.cache'
    expected_courses = set()
    for index in packs.glob('*/courses.json'):
        document = json.loads(index.read_text())
        expected_courses.update(index.parent.name + '/' + c['id'] for c in document['courses'])
    compiled = list((cache / 'courses').glob('*.fzc'))
    ready = list((cache / 'sources').glob('*.ready'))
    assert len(compiled) == len(ready) == len(expected_courses) > 0
    # Make any first-launch rewrite visible regardless of ZIP timestamp precision.
    for path in cache.rglob('*'):
        if path.is_file():
            os.utime(path, (1000000000, 1000000000))

    def stamps():
        return {str(p.relative_to(cache)): (p.stat().st_size, p.stat().st_mtime_ns)
                for p in cache.rglob('*') if p.is_file()}

    def inspect(label):
        dest = out / label
        dest.mkdir()
        start = time.monotonic()
        run = subprocess.run([str(exe), 'mods/packs', '--dump', str(dest)],
                             cwd=install, capture_output=True, text=True, timeout=120)
        (dest / 'inspect.log').write_text(run.stdout + run.stderr)
        assert run.returncode == 0, run.stdout + run.stderr
        return dest, round(time.monotonic() - start, 2)

    shipped = stamps()
    warm, warm_seconds = inspect('first-launch')
    assert stamps() == shipped, 'Shipped cache was regenerated after relocation'
    decoded = {p.name: p.read_bytes() for p in warm.glob('*.fzc')}
    assert len(decoded) == len(expected_courses)

    # Edit a source ZIP, not its disposable extracted copy. Preserve the rest
    # of the archive, including its course identity and companion metadata.
    source = packs / 'cgp/courses/marine-city-1.zip'
    temp = source.with_suffix('.tmp')
    changed = 0
    with zipfile.ZipFile(source) as original, zipfile.ZipFile(temp, 'x') as updated:
        for entry in original.infolist():
            data = original.read(entry)
            if entry.filename.endswith('.fzm'):
                lines = data.splitlines(keepends=True)
                for i, line in enumerate(lines):
                    if line.startswith(b'InGameMapName='):
                        ending = b'\r\n' if line.endswith(b'\r\n') else b'\n'
                        lines[i] = b'InGameMapName=EDITED COURSE' + ending
                        changed += 1
                data = b''.join(lines)
            updated.writestr(entry, data)
    assert changed == 1
    temp.replace(source)
    edited, edit_seconds = inspect('edited-course')
    after = {p.name: p.read_bytes() for p in edited.glob('*.fzc')}
    assert after.keys() == decoded.keys()
    differences = [name for name in decoded if decoded[name] != after[name]]
    assert differences == ['cgp--marine-city-1.fzc'], differences
    now = stamps()
    assert all(now.get(name) == stamp for name, stamp in shipped.items())
    assert len(list((cache / 'courses').glob('*.fzc'))) == len(compiled) + 1
    assert len(list((cache / 'sources').glob('*.ready'))) == len(ready) + 1
    report = dict(cached_courses=len(compiled), extracted_archives=len(ready),
                  relocated_first_launch_reused_every_cache_file=True,
                  edited_course_rebuilt=True, unchanged_courses_reused=True,
                  first_launch_seconds=warm_seconds, edited_scan_seconds=edit_seconds)
    (out / 'validation.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2), flush=True)


if __name__ == '__main__':
    main()
