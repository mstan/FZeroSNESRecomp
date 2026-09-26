"""Private corpus check: exact reconstruction, editable sources, cache lifecycle."""
import argparse
import json
from pathlib import Path
import subprocess
import sys
import time
import xml.etree.ElementTree as ET
import zipfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from reconstruct_fzedit_course import sections, unpack_layout


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--build',type=Path,required=True)
    p.add_argument('--packs',type=Path,required=True)
    p.add_argument('--baseline',type=Path,required=True)
    p.add_argument('--out',type=Path,required=True)
    a=p.parse_args()
    exe=a.build.resolve()/'FZeroInspectPacks.exe'
    packs=a.packs.resolve();out=a.out.resolve();out.mkdir(parents=True,exist_ok=False)
    def inspect(root,label,good=True):
        target=out/label;target.mkdir()
        t=time.monotonic()
        run=subprocess.run([str(exe),str(root),'--dump',str(target)],cwd=out,capture_output=True,text=True,timeout=90)
        (target/'inspect.log').write_bytes((run.stdout+run.stderr).encode())
        assert (run.returncode==0)==good,run.stdout+run.stderr
        return target,round(time.monotonic()-t,2)
    count=0
    for folder in packs.iterdir():
        if not (folder/'courses.json').is_file(): continue
        index=json.loads((folder/'courses.json').read_text())
        for course in index['courses']:
            assert course['source'].endswith('.zip')
            with zipfile.ZipFile(folder/course['source']) as archive:
                assert not any(n.endswith('.fzc') for n in archive.namelist())
                fzm=next(n for n in archive.namelist() if n.endswith('.fzm'))
                if course['id']!='huckmine':
                    assert Path(fzm).stem==course['id']
                    aip=archive.read(next(n for n in archive.namelist() if n.endswith('.aip')))
                    assert aip.startswith(b'MAIN_PATH:\r\n')
                    assert b'\n' not in aip.replace(b'\r\n',b'')
            count+=1
    cold,seconds=inspect(packs,'cold')
    expected=list(a.baseline.glob('*.fzc')); assert len(expected)==count==75
    for old in expected:
        assert old.read_bytes()==(cold/old.name).read_bytes(),old.name
    cache=out/'mods/packs/.cache/courses'
    stamps={p.name:p.stat().st_mtime_ns for p in cache.glob('*.fzc')}
    assert len(stamps)==75
    warm,warm_seconds=inspect(packs,'warm')
    assert stamps=={p.name:p.stat().st_mtime_ns for p in cache.glob('*.fzc')}
    for old in expected: assert old.read_bytes()==(warm/old.name).read_bytes()
    # A damaged cache is disposable and rebuilt from the course ZIP.
    damaged=next(cache.glob('*.fzc'));damaged.write_bytes(b'broken cache')
    healed,_=inspect(packs,'healed')
    for old in expected: assert old.read_bytes()==(healed/old.name).read_bytes()
    # Decode every recovered editor project with native preservation disabled.
    # This independently checks the editable map/AI/artwork, rather than merely
    # proving that the companion can restore the previous runtime bytes.
    raw_count=0
    for folder in packs.iterdir():
        if not (folder/'courses.json').is_file(): continue
        raw=out/('editor-only-'+folder.name);raw.mkdir()
        index=json.loads((folder/'courses.json').read_text())
        for course in index['courses']:
            if course['id']=='huckmine': continue
            dest=raw/course['id'];dest.mkdir()
            with zipfile.ZipFile(folder/course['source']) as archive:
                for n in archive.namelist(): (dest/n).write_bytes(archive.read(n))
            metadata=next(dest.glob('*_Reconstruction.json'))
            metadata.write_bytes(b'{"format":"fzero.reconstruction","version":1,"groups":{}}')
        decoded,_=inspect(raw,'editor-only-dump-'+folder.name)
        for course in index['courses']:
            if course['id']=='huckmine': continue
            before=sections((a.baseline/(folder.name+'--'+course['id']+'.fzc')).read_bytes())
            after=sections((decoded/(folder.name+'-'+course['id']+'--course.fzc')).read_bytes())
            assert unpack_layout(before)==unpack_layout(after),course['id']
            for section in [4,5,9,12,13]: assert before[section]==after[section],(course['id'],section)
            raw_count+=1
    # Exercise edits through the actual loader, including the native-layout
    # bypass: editing the TMX must compile its new tiles, not reuse saved bytes.
    source=packs/'cgp/courses/marine-city-1.zip'
    inputs=out/'edit-inputs';project=inputs/'marine-city';project.mkdir(parents=True)
    with zipfile.ZipFile(source) as archive:
        originals={n:archive.read(n) for n in archive.namelist()}
    def restore():
        for n,data in originals.items(): (project/n).write_bytes(data)
    restore()
    base,_=inspect(inputs,'edit-base')
    name=next(base.glob('*.fzc')).name
    baseline=sections((base/name).read_bytes())
    fzm=next(project.glob('*.fzm'))
    props=dict(line.split('=',1) for line in fzm.read_text().splitlines() if '=' in line)
    track=project/props['TrackFile']
    doc=ET.parse(track);data=doc.getroot().find('layer/data')
    values=[int(v.strip()) for v in data.text.split(',') if v.strip()]
    values[0]=2 if values[0]!=2 else 3
    data.text=','.join(map(str,values));doc.write(track,encoding='utf-8')
    changed,_=inspect(inputs,'edit-layout')
    after=sections((changed/name).read_bytes())
    assert unpack_layout(after)[0]==values[0]-1
    assert unpack_layout(after)!=unpack_layout(baseline)
    assert after[4]==baseline[4]
    restore()
    fzm.write_bytes(fzm.read_bytes().replace(('InGameMapName='+props['InGameMapName']).encode(),b'InGameMapName=EDITED COURSE'))
    changed,_=inspect(inputs,'edit-name')
    after=sections((changed/name).read_bytes());assert after[11]!=baseline[11]
    assert after[1]==baseline[1] and after[12]==baseline[12]
    restore()
    terrain=project/props['TilesetTSX'];doc=ET.parse(terrain)
    prop=doc.getroot().find("tile[@id='0']/properties/property[@name='DIRT']")
    prop.set('value',str(1-int(prop.get('value'))));doc.write(terrain,encoding='utf-8')
    changed,_=inspect(inputs,'edit-terrain')
    after=sections((changed/name).read_bytes());assert after[10][256]!=baseline[10][256]
    assert after[1]==baseline[1]
    restore()
    ai=project/props['AiPathFile'];text=ai.read_bytes()
    start=text.index(b'\r\nPATH:')+7;end=text.index(b',',start)
    text=text[:start]+str(int(text[start:end])^16).encode()+text[end:];ai.write_bytes(text)
    changed,_=inspect(inputs,'edit-ai')
    after=sections((changed/name).read_bytes());assert after[12][0x502]==baseline[12][0x502]^16
    restore()
    metadata=project/props['ReconstructionFile'];native=json.loads(metadata.read_text())
    native['groups']['gradient']['fields']['gradient']=0
    metadata.write_bytes(json.dumps(native).encode())
    changed,_=inspect(inputs,'edit-native-metadata')
    assert sections((changed/name).read_bytes())[17][1]==0
    native['groups']['layout']['fields']['pool']='00'
    metadata.write_bytes(json.dumps(native).encode())
    rejected,_=inspect(inputs,'invalid-native',False)
    assert 'Invalid reconstruction byte field' in (rejected/'inspect.log').read_text()
    metadata.write_bytes(b'')
    rejected,_=inspect(inputs,'empty-native',False)
    assert 'Empty reconstruction metadata' in (rejected/'inspect.log').read_text()
    report=dict(courses=count,byte_exact=True,editor_only_roundtrips=raw_count,no_fzc_in_sources=True,cold_seconds=seconds,warm_seconds=warm_seconds,cache_reused_and_repaired=True,edits=['layout','intro','terrain','AI','native metadata'],malformed_metadata_rejected=True)
    (out/'validation.json').write_bytes((json.dumps(report,indent=2)+'\n').encode())
    print(json.dumps(report,indent=2))


if __name__=='__main__': main()
