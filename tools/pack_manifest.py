"""F-Zero course index wrapped in the shared snesrecomp pack envelope."""
import importlib.util
import json
import os
from pathlib import Path
from course_tool_paths import ROOT

BASE_SHA256 = 'bf16c3c867c58e2ab061c70de9295b6930d63f29f81cc986f5ecae03e0ad18d2'


def course_source_stem(root, course):
    """Music matches the course file visible to players, including ZIPs."""
    return Path(course['source']).stem

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
