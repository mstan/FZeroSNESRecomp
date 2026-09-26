"""Verify and stage the author's course-named Astra Front MSU recordings."""
import argparse
import json
from pathlib import Path
import shutil
import zipfile
from import_cgp_music import verify_file
from pack_manifest import course_source_stem

ROOT = Path(__file__).resolve().parents[1]


def manifest():
    return json.loads((ROOT / 'assets/music/astra-front.json').read_text())['tracks']


def verify_music(folder):
    tracks = manifest()
    if {p.name for p in folder.iterdir()} != tracks.keys():
        raise ValueError('Unexpected or missing Astra recordings')
    for name, entry in tracks.items():
        verify_file(folder / name, entry)
    return tracks


def stage_music(source, pack):
    tracks = verify_music(source)
    index = json.loads((pack / 'courses.json').read_text())
    expected = {course_source_stem(pack, c) + '.pcm' for c in index['courses']}
    if tracks.keys() != expected:
        raise ValueError('Astra recordings do not match the course filenames')
    destination = pack / 'music'
    destination.mkdir(exist_ok=True)
    if {p.name for p in destination.iterdir()} - expected:
        raise ValueError('Unexpected files in staged Astra music')
    for name in tracks:
        shutil.copy2(source / name, destination / name)
    verify_music(destination)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('archive', type=Path, nargs='?')
    p.add_argument('--stage-course-from', type=Path)
    p.add_argument('--out', type=Path, default=ROOT / 'music/astra-front')
    a = p.parse_args()
    if a.stage_course_from:
        stage_music(a.stage_course_from, a.out)
        print('Staged ten verified Astra course recordings')
        return
    if not a.archive:
        p.error('provide an archive or --stage-course-from')
    tracks = manifest()
    with zipfile.ZipFile(a.archive) as z:
        entries = {x.filename: x for x in z.infolist() if not x.is_dir()}
        if entries.keys() != tracks.keys():
            raise ValueError('Unexpected Astra archive contents')
        a.out.mkdir(parents=True, exist_ok=False)
        for name, entry in tracks.items():
            if entries[name].file_size != entry['bytes']:
                raise ValueError('Unexpected PCM size')
            with z.open(name) as source, (a.out / name).open('xb') as target:
                shutil.copyfileobj(source, target)
        verify_music(a.out)
    print(f'Verified {len(tracks)} Astra recordings in {a.out}')


if __name__ == '__main__':
    main()
