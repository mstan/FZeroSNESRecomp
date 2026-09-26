"""Wrap an unmodified, single-course FZEdit export in a runtime pack.

The original archive is never changed. Authors' twelve source files remain
byte-for-byte intact; only the pack descriptors and optional credits are added.
"""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import tempfile
import zipfile

from pack_manifest import write_index


def package(source, output, ident, author, requires=(), credits=None):
    with zipfile.ZipFile(source) as archive:
        files = {}
        for entry in archive.infolist():
            path = PurePosixPath(entry.filename)
            if (path.is_absolute() or '..' in path.parts or '\\' in entry.filename
                    or ':' in entry.filename or entry.file_size > 16 * 1024 * 1024):
                raise ValueError('Unsafe or oversized project file')
            if not entry.is_dir():
                if entry.filename in files:
                    raise ValueError('Duplicate project file')
                files[entry.filename] = archive.read(entry)
        projects = [p for p in files if p.lower().endswith('.fzm')]
        if len(projects) != 1:
            raise ValueError('Expected exactly one FZM project')
        project = projects[0]
        props = dict(line.split('=', 1) for line in files[project].decode('utf-8-sig').splitlines() if '=' in line)
        name = props['InGameMapName']
        index = dict(format=1, id=ident, name=name, author=author,
                     cups=[dict(id='course', name=name, courses=['course'])],
                     courses=[dict(id='course', name=name, source=project,
                                   requires=list(requires))])
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            write_index(root, index)
            files['pack.json'] = (root / 'pack.json').read_bytes()
            files['courses.json'] = (root / 'courses.json').read_bytes()
        if credits:
            files['CREDITS.txt'] = Path(credits).read_bytes()
        with zipfile.ZipFile(output, 'x', compression=zipfile.ZIP_DEFLATED) as target:
            for name, data in files.items():
                target.writestr(name, data)
    return dict(source_archive_sha256=hashlib.sha256(Path(source).read_bytes()).hexdigest(),
                project=project, guid=props.get('GUID'), files=len(files))


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('source', type=Path)
    p.add_argument('output', type=Path)
    p.add_argument('--id', required=True)
    p.add_argument('--author', required=True)
    p.add_argument('--requires', nargs='*', default=[])
    p.add_argument('--credits', type=Path)
    a = p.parse_args()
    print(json.dumps(package(a.source, a.output, a.id, a.author, a.requires, a.credits), indent=2))
