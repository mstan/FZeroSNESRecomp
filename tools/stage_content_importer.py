"""Add the opt-in importer helper and its DLL closure to a developer build.

Run after building the game, FZeroExportCourses and FZeroInspectPacks. Never
copies ROMs or changes installed packs, saves, or settings.
"""
import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess


def stage(build, converter, mingw):
    build, converter, mingw = map(Path, (build, converter, mingw))
    required = [build / 'FZeroSNESRecomp.exe', build / 'FZeroExportCourses.exe',
                build / 'FZeroInspectPacks.exe', converter]
    for path in required:
        if not path.is_file():
            raise ValueError(f'Build or provide {path} first')
    target = build / 'FZeroConvertContent.exe'
    if converter.resolve() != target.resolve():
        shutil.copy2(converter, target)
    pending = required[:3] + [target]
    seen = set()
    system = Path(os.environ.get('SystemRoot', 'C:/Windows')) / 'System32'
    while pending:
        binary = pending.pop()
        imports = subprocess.check_output([str(mingw / 'bin/objdump.exe'), '-p', str(binary)], text=True)
        for name in re.findall(r'DLL Name:\s*(\S+)', imports):
            key = name.lower()
            if key in seen:
                continue
            seen.add(key)
            if key.startswith(('api-ms-', 'ext-ms-')) or (system / name).is_file():
                continue
            source = next((p / name for p in (build, mingw / 'bin') if (p / name).is_file()), None)
            if source is None:
                raise ValueError(f'Missing dependency {name}')
            dest = build / name
            if source.resolve() != dest.resolve():
                shutil.copy2(source, dest)
            pending.append(dest)
    print(f'Importer helper and {len(seen)} checked dependencies: {build.resolve()}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--converter', type=Path, required=True)
    parser.add_argument('--mingw', type=Path, default=Path('C:/msys64/mingw64'))
    args = parser.parse_args()
    stage(args.build, args.converter, args.mingw)
