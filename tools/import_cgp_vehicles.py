"""Build vehicle-only IPS inputs from privately supplied author artwork.

Only reviewed graphics/palette/preview resources enter the delta. Title,
track, unrelated HUD and engine edits in the supplied images are deliberately excluded.
"""
import argparse
import hashlib
from pathlib import Path
from make_ips import make_ips
from vehicle_cards import information_cards

DONORS = [
    ('7266ff8b43456a6627ba4d73d6cb233e57b914f0d1170f165f4ffef4be3df1db',
     [('moon-shadow','Moon Shadow','new'),('dragon-bird','Dragon Bird','new'),
      ('great-star','Great Star','new'),('death-anchor','Death Anchor','new')]),
    ('8ffc1dbe13746da35f0c8d5647b73d3cdb3fa678b775a94668798157eb538a0e',
     [('blue-falcon','Blue Falcon','rebalance'),('p-emerald','P. Emerald','new'),
      ('golden-fox','Golden Fox','rebalance'),('black-bull','Black Bull','new')]),
    ('fc32ffa67b6a5bc31126b9adf089622c12510e201396f6f5624945fa31fd3f50',
     [('white-cat','White Cat','new'),('wild-goose','Wild Goose','rebalance'),
      ('red-gazelle','Red Gazelle','new'),('fire-stingray','Fire Stingray','rebalance')])]
STOCK = 'bf16c3c867c58e2ab061c70de9295b6930d63f29f81cc986f5ecae03e0ad18d2'

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--stock',type=Path,required=True)
    p.add_argument('--donors',type=Path,nargs=3,required=True)
    p.add_argument('--out',type=Path,required=True)
    a=p.parse_args();stock=a.stock.read_bytes()
    assert hashlib.sha256(stock).hexdigest()==STOCK
    a.out.mkdir(parents=True,exist_ok=True)
    for group,(file,(expected,ships)) in enumerate(zip(a.donors,DONORS),1):
        donor=file.read_bytes()
        assert hashlib.sha256(donor).hexdigest()==expected,(file,'unexpected donor')
        image=bytearray(stock)
        for start,end in [(0x40000,0x60000),(0x76180,0x76800),(0x7cd80,0x7ce00)]:
            image[start:end]=donor[start:end]
        # The authored energy HUD: three underline tiles and eight POWER/bolt
        # tiles. No digits, map layout, palettes or executable donor bytes.
        for start,end in [(0x640d0,0x64100),(0x641b0,0x64230)]:
            image[start:end]=donor[start:end]
        # One minimap marker color per native HUD row. Runtime selects this by
        # stable vehicle identity; never import the surrounding shared HUD.
        for row in range(4):
            start=0x7cd06+row*32
            image[start:start+2]=donor[start:start+2]
        # These addresses are shared HUD/exhaust OAM and fog assets.
        for start,end in [(0x5ec00,0x5f000),(0x46f80,0x47000)]:
            assert donor[start:end]==stock[start:end],(file,hex(start),'shared resource changed')
        # A data-only appendix: converted numeric/graph tiles and per-slot
        # information colors. It is never installed as executable cartridge data.
        image += information_cards(donor)
        patch=make_ips(stock,bytes(image))
        (a.out/f'cgp-p{group}.ips').write_bytes(patch)
        lines=['format=fzero-vehicles-2',f'id=cgp-p{group}',f'profile={group}',
               f'source_sha256={STOCK}',f'target_sha256={hashlib.sha256(image).hexdigest()}',
               f'# Author artwork input SHA-256: {expected}']
        lines += [f'vehicle={id}|{name}|{slot}|{role}' for slot,(id,name,role) in enumerate(ships)]
        (a.out/f'cgp-p{group}.ini').write_text('\n'.join(lines)+'\n',encoding='utf-8',newline='\n')
        print(group,len(patch),hashlib.sha256(image).hexdigest())

if __name__=='__main__':main()
