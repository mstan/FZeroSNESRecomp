"""Private-ROM checks for BS isolation and per-ship boost HUD ownership."""
import argparse
import os, json, shutil, subprocess
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
root=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build',type=Path,required=True)
parser.add_argument('--stock',type=Path,required=True)
parser.add_argument('--out',type=Path,required=True)
args=parser.parse_args();build=args.build.resolve();stock=args.stock.resolve();out=args.out.resolve();out.mkdir(parents=True,exist_ok=True)
clean={k:v for k,v in os.environ.items() if not k.startswith(('FZERO_','SNESRECOMP_','SDL_','LNG_'))}
route='320-326:8,560-566:8,640-646:8,730-736:8,790-796:8,1160-1599:1'
def run(c):
 name=c['name'];folder=out/name;folder.mkdir(exist_ok=True);shutil.copytree(root/'assets',folder/'assets',dirs_exist_ok=True)
 ownroute=route
 if 'row' in c:
  row=c['row']
  if row>=4:ownroute+=',400-406:128'
  for n in range(row%4):ownroute+=f',{430+n*25}-{436+n*25}:32'
 e=dict(clean,FZERO_DELUXE_DATA='embedded',FZERO_BS_CARS=str(c.get('bs',0)),FZERO_BS_TRACKS='0',FZERO_CGP_CARS=str(c.get('packs',0)),FZERO_CGP_REBALANCE=str(c.get('rebalance',0)),FZERO_RULES='',FZERO_TRACK_PACKS=str(folder/'packs'),FZERO_CUP=c.get('cup','cgp/knight-cgp'),SNESRECOMP_SAVE_ROOT='s',SNESRECOMP_INPUT_SCRIPT=ownroute,SNESRECOMP_WRAM_DUMP=str(folder/'ram.bin'),SNESRECOMP_FRAME_DUMP=str(folder/'frame.ppm'),FZERO_CAPTURE_FRAME='1599',FZERO_CAPTURE_PREFIX=str(folder/'capture'))
 if 'vehicle' in c:e['FZERO_TEST_VEHICLE']=c['vehicle']
 p=subprocess.run([str(build/'FZeroSNESRecompHeadless.exe'),str(stock),'1600'],cwd=folder,env=e,capture_output=True,text=True,timeout=180);(folder/'run.log').write_text(p.stdout+p.stderr);assert p.returncode==0,(name,p.stderr[-1500:])
 r=(folder/'ram.bin').read_bytes();assert r[0x54:0x56]==b'\x02\x03',(name,r[0x54:0x57].hex())
 if 'row' in c:assert r[0x52]==[0,2,1,3,4,6,5,7][c['row']],(name,r[0x52])
 # Authored boost OAM relocation is isolated to energy-boost ships.
 energy=c.get('vehicle') in ('great-star','white-cat')
 assert (r[0x2b1]==0xef and r[0x2b5]==0xef and r[0x2b9]==0x14)==energy,(name,r[0x2b0:0x2bc].hex())
 if c.get('render'):
  for aspect,scale in [('4:3',1),('21:9',1),('21:9',4)]:
   ppm=folder/f'render-{aspect.replace(":","-")}-{scale}.ppm';re=dict(clean)
   if scale>1:re['FZERO_HD_SCALE']=str(scale)
   render=subprocess.run([str(build/'FZeroRenderCapture.exe'),str(folder/'capture.bin'),aspect,str(ppm)],cwd=folder,env=re,capture_output=True,text=True,timeout=60)
   assert render.returncode in (0,1),(name,render.stderr)
 print(name,'PASS',flush=True)
cases=[dict(name=f'bs-{i}-{mode}',row=i,bs=1,packs=7 if mode else 0,rebalance=15 if mode else 0,cup='bs-deluxe/knight') for i in range(8) for mode in (0,1)]
cases += [dict(name='stock',render=True),dict(name='catalog-stock',packs=7,vehicle='blue-falcon',render=True),dict(name='great-star',packs=7,vehicle='great-star',render=True),dict(name='white-cat',packs=7,vehicle='white-cat',render=True)]
with ThreadPoolExecutor(max_workers=3) as pool:list(pool.map(run,cases))
for i in range(8):
 for file in ('ram.bin','frame.ppm'):
  assert (out/f'bs-{i}-0'/file).read_bytes()==(out/f'bs-{i}-1'/file).read_bytes(),(i,file,'conflicting CGP settings leaked into BS baseline')
(out/'validation.json').write_text(json.dumps({'cases':cases,'bs_conflicts_identical':True},indent=2))
