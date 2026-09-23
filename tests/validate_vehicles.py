"""Private-ROM vehicle integration checks. Does not certify full-race playability."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import json
import os
from pathlib import Path
import re
import shutil
import subprocess

ROOT=Path(__file__).resolve().parents[1]
NAMES=['blue-falcon','wild-goose','golden-fox','fire-stingray','moon-shadow','dragon-bird',
       'great-star','death-anchor','p-emerald','black-bull','white-cat','red-gazelle']
GROUPS=[0,0,0,0,1,1,1,1,2,2,3,3]
SLOTS=[0,1,2,3,0,1,2,3,1,3,0,2]
FIELDS=[(0xfa81,1),(0xfa85,1),(0xfa89,1),(0xfa8d,1),(0xfa91,2),(0xfa99,2),
        (0xfaa1,2),(0xfaa9,1),(0xfaad,1),(0xfab1,1),(0xfab5,1),(0xfab9,1),
        (0xfabd,1),(0xfac1,1),(0xfac5,2),(0xfacd,2),(0xfad5,2),(0xfadd,2),(0xfae5,2),(0xfaed,1)]
ROUTE='320-326:8,440-446:8,560-566:8,730-736:8,790-796:8,1160-1599:1'

def authored_stats(group,stock):
    data=bytearray(stock)
    text=(ROOT/f'mods/cgp-source/CGP/{group}/CGP.asm').read_text(encoding='cp1252')
    for match in re.finditer(r'org\s+\$([0-9A-Fa-f]+)\s+db\s+([^\r\n]+)',text):
        address=int(match[1],16);offset=(address>>16)*32768+(address&32767)
        values=bytes(int(part.strip().lstrip('$'),16) for part in match[2].split(';')[0].split(','))
        data[offset:offset+len(values)]=values
    return data

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--build',type=Path,required=True);p.add_argument('--stock',type=Path,required=True)
    p.add_argument('--out',type=Path,required=True);p.add_argument('--filter',default='')
    a=p.parse_args();build=a.build.resolve();stock=a.stock.resolve();out=a.out.resolve();out.mkdir(parents=True,exist_ok=True)
    original=stock.read_bytes();sources=[original]+[authored_stats(g,original) for g in (1,2,3)]
    clean={k:v for k,v in os.environ.items() if not k.startswith(('FZERO_','SNESRECOMP_','SDL_','LNG_'))}
    results={}
    def run(case):
        name=case['name']
        if a.filter and a.filter not in name:return
        folder=out/name;folder.mkdir(exist_ok=True);(folder/'s').mkdir(exist_ok=True);(folder/'packs').mkdir(exist_ok=True)
        shutil.copytree(ROOT/'assets',folder/'assets',dirs_exist_ok=True)
        packs=case.get('packs',7);rebalance=case.get('rebalance',0);bs=case.get('bs',0)
        vehicle=case.get('vehicle','blue-falcon');ident=NAMES.index(vehicle)
        frames=case.get('frames',1600)
        env=dict(clean,FZERO_TRACK_PACKS=str(folder/'packs'),FZERO_DELUXE_DATA='embedded',FZERO_BS_CARS=str(bs),
                 FZERO_BS_TRACKS=str(case.get('tracks',0)),FZERO_CGP_CARS=str(packs),FZERO_CGP_REBALANCE=str(rebalance),FZERO_RULES=case.get('rules',''),
                 FZERO_CUP=case.get('cup','cgp/cgp-1'),SNESRECOMP_SAVE_ROOT='s',
                 SNESRECOMP_INPUT_SCRIPT=case.get('route',ROUTE),SNESRECOMP_WRAM_DUMP=str(folder/'ram.bin'),
                 SNESRECOMP_FRAME_DUMP=str(folder/'frame.ppm'))
        if case.get('navigate') is None:env['FZERO_TEST_VEHICLE']=vehicle
        if case.get('lifecycle'):env.update(FZERO_LIFECYCLE_TEST='1',FZERO_VEHICLE_CROSS_STATE='1')
        if case.get('rewind'):env.update(FZERO_REWIND_TEST='1',FZERO_VEHICLE_CROSS_STATE='1')
        if case.get('probe'):env['FZERO_RULE_PROBE']='1'
        proc=subprocess.run([str(build/'FZeroSNESRecompHeadless.exe'),str(stock),str(frames)],
                            cwd=folder,env=env,capture_output=True,text=True,timeout=180)
        log=proc.stdout+proc.stderr;(folder/'run.log').write_text(log,encoding='utf-8')
        assert proc.returncode==0,(name,log[-2500:])
        ram=(folder/'ram.bin').read_bytes()
        if packs or rebalance:
            count=4+4*bool(packs&1)+2*bool(packs&2)+2*bool(packs&4)
            assert f'[vehicles] {count} identities' in log,name
        if case.get('lifecycle'):
            assert 'resimulation identical' in log and 'soft reset, SRAM retained' in log,name
        else:
            assert ram[0x54:0x56]==(b'\x01\x01' if frames==570 else b'\x02\x03'),(name,ram[0x54:0x57].hex())
            if packs or rebalance:
                assert ram[0x14dff]==ident,(name,ram[0x52],ram[0x14dff])
                group=GROUPS[ident] or (2 if ident in (0,2) else 3) * bool(rebalance&(1<<ident))
                slot=SLOTS[ident];source=sources[group]
                expected=b''.join(source[address-0x8000+slot*width:address-0x8000+(slot+1)*width] for address,width in FIELDS)
                assert frames == 570 or ram[0x14d47:0x14d47+len(expected)]==expected,(name,'handling record differs from authored slot')
        if case.get('rewind'):assert 'rewind: actual ring restore and ten-frame resimulation identical' in log,name
        if case.get('practice'):assert ram[0x58],(name,'not in Practice')
        if case.get('probe'):assert 'rules-probe: PASS' in log,name
        assert '[MSU-1] enabled:' not in log,name
        records=[p.parent.name for p in (folder/'s').rglob('records.bin')]
        results[name]={'packs':packs,'rebalance':rebalance,'vehicle':vehicle,'frames':frames,'records':records}
        print(name,'PASS',flush=True)
    cases=[dict(name='all-'+v,vehicle=v) for v in NAMES]
    cases += [dict(name='rebalance-'+v,vehicle=v,packs=0,rebalance=15) for v in NAMES[:4]]
    cases += [dict(name=f'partial-{mask}',packs=mask) for mask in range(1,8)]
    cases += [dict(name='lifecycle-'+v,vehicle=v,rules='cgp-legend',lifecycle=True,frames=1850) for v in NAMES]
    cases += [dict(name='navigate-'+v,vehicle=v,navigate=True,frames=570,
                   route='320-326:8'+''.join(f',{400+j*30}-{403+j*30}:128' for j in range(i//4))+''.join(f',{480+j*20}-{483+j*20}:32' for j in range(i%4)))
              for i,v in enumerate(['blue-falcon','golden-fox','wild-goose','fire-stingray','moon-shadow',
                                     'great-star','dragon-bird','death-anchor','p-emerald','black-bull','white-cat','red-gazelle'])]
    cases += [dict(name='rewind-'+v,vehicle=v,rewind=True,rules='cgp-legend') for v in NAMES]
    cases += [dict(name='practice-'+v,vehicle=v,practice=True,rewind=True,
                   route=ROUTE+',300-306:32',rules='cgp-legend') for v in NAMES]
    cases += [dict(name='provider-'+v+'-'+provider,vehicle=v,tracks=int(provider=='bs'),
                   cup='bs-deluxe/bs-1' if provider=='bs' else 'bs-deluxe/knight',rewind=True)
              for v in ('moon-shadow','p-emerald','white-cat') for provider in ('stock','bs')]
    cases += [dict(name='probe-'+v,vehicle=v,rules='all',probe=True)
              for v in ('moon-shadow','p-emerald','white-cat')]
    with ThreadPoolExecutor(max_workers=3) as pool:list(pool.map(run,cases))
    for prefix in ('all-', 'practice-'):
        keys=[results[prefix+v]['records'] for v in NAMES if prefix+v in results]
        if len(keys)==len(NAMES):
            assert all(len(k)==1 for k in keys),(prefix,keys)
            assert len({k[0] for k in keys})==len(NAMES),(prefix,'vehicle records collide')
    assert stock.read_bytes()==original
    (out/('validation'+('-'+a.filter if a.filter else '')+'.json')).write_text(json.dumps(results,indent=2)+'\n')

if __name__=='__main__':main()
