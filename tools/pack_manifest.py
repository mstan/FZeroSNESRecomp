"""F-Zero course index wrapped in the shared snesrecomp pack envelope."""
import importlib.util
import json
import os
import zipfile
from pathlib import Path

BASE_SHA256 = 'bf16c3c867c58e2ab061c70de9295b6930d63f29f81cc986f5ecae03e0ad18d2'
ROOT = Path(__file__).resolve().parents[1]


def course_source_stem(root, course):
    """Audio names follow the source project, including a single-course ZIP."""
    source = Path(course['source'])
    if source.suffix.lower() != '.zip':
        return source.stem
    with zipfile.ZipFile(Path(root) / source) as archive:
        names = archive.namelist()
        manifests = [name for name in names if name == 'pack.json' or
                     (name.count('/') == 1 and name.endswith('/pack.json'))]
        if len(manifests) != 1:
            raise ValueError('Expected one course ZIP manifest')
        prefix = manifests[0][:-len('pack.json')]
        manifest = json.loads(archive.read(manifests[0]))
        index = json.loads(archive.read(prefix + manifest['payload']['file']))
        if len(index['courses']) != 1:
            raise ValueError('Expected one course in source ZIP')
        source = Path(index['courses'][0]['source'])
        if source.suffix.lower() not in ('.fzm', '.fzc'):
            raise ValueError('Nested course ZIPs are not supported')
        return source.stem

def write_index(root, index):
    engine = Path(os.environ.get('SNESRECOMP_ROOT', ROOT/'snesrecomp'))
    spec = importlib.util.spec_from_file_location('snes_data_pack', engine/'tools/data_pack.py')
    shared = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(shared)
    payload = (json.dumps(index, indent=2)+'\n').encode('utf-8')
    root = Path(root)
    (root/'courses.json').write_bytes(payload)
    (root/'pack.json').write_bytes(shared.manifest_bytes(
        game='f-zero', ident=index['id'], title=index['name'], base_sha256=BASE_SHA256,
        payload_format='fzero.course-index', payload_name='courses.json', payload=payload,
        requires=['fzero-course-v1']))
