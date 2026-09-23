"""Check authored minimap colors through both native exhaust-palette phases.

Requires the owner's ROM and three original artwork donors. Captures remain
private. Checks guest palettes and native pixels, plus wide/HD presentation.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess

from PIL import Image
from validate_vehicles import NAMES, GROUPS, SLOTS, ROUTE
from validate_practice_catalog import route as practice_route

ROOT = Path(__file__).resolve().parents[1]
PIXELS = 224 * 1120 + 65536
RAM = PIXELS + 256 * 224 * 4
# Native HUD palette order: Wild Goose, Fire Stingray, Golden Fox, Blue Falcon.
HUD_ROWS = [3, 0, 2, 1]
FRAMES = [1536, 1537, 1540, 1541, 1552, 1553, 1556, 1557]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--build", type=Path, required=True)
    p.add_argument("--stock", type=Path, required=True)
    p.add_argument("--donors", type=Path, nargs=3, required=True)
    p.add_argument("--out", type=Path, required=True)
    args = p.parse_args()
    build, stock, out = args.build.resolve(), args.stock.resolve(), args.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    sources = [stock.read_bytes()] + [path.read_bytes() for path in args.donors]
    clean = {k: v for k, v in os.environ.items()
             if not k.startswith(("FZERO_", "SNESRECOMP_", "SDL_", "LNG_"))}
    cases = [(name, i, 7, 15, 0, False) for i, name in enumerate(NAMES)]
    cases += [("original-" + NAMES[i], i, 7, 0, 0, False) for i in range(4)]
    cases += [("stock", 0, 0, 0, 0, False), ("bs-stock", 0, 0, 0, 1, False),
              ("moon-rewind", 4, 7, 15, 0, True)]
    practice = {"practice-moon-white": (4, 10), "practice-dragon-gazelle": (5, 11),
                "practice-stock-moon": (0, 4)}
    cases += [(name, player, 7, 0, 0, True) for name, (player, _) in practice.items()]
    results = {}

    def run(case):
        name, identity, packs, rebalance, bs, rewind = case
        folder = out / name
        folder.mkdir()
        shutil.copytree(ROOT / "assets", folder / "assets")
        frames = [f + 700 for f in FRAMES] if name in practice else FRAMES
        env = dict(clean, FZERO_DELUXE_DATA="embedded", FZERO_BS_CARS=str(bs), FZERO_BS_TRACKS="0",
                   FZERO_CGP_CARS=str(packs), FZERO_CGP_REBALANCE=str(rebalance), FZERO_RULES="",
                   FZERO_TRACK_PACKS="packs", FZERO_CUP="bs-deluxe/knight", SNESRECOMP_SAVE_ROOT="s",
                   FZERO_TEST_VEHICLE=NAMES[identity], SNESRECOMP_INPUT_SCRIPT=ROUTE,
                   FZERO_CAPTURE_FRAMES=",".join(map(str, frames)), FZERO_CAPTURE_PREFIX=str(folder / "capture"),
                   FZERO_CART_DUMP=str(folder / "cart.bin"))
        if name in practice:
            env.pop("FZERO_TEST_VEHICLE")
            env["SNESRECOMP_INPUT_SCRIPT"] = practice_route(*practice[name]) + ",1200-1206:8,1300-1306:8,1700-1706:8,1900-2299:1"
        if rewind:
            env.update(FZERO_REWIND_TEST="1", FZERO_VEHICLE_CROSS_STATE="1", FZERO_TEST_SAVE_FRAME=str(frames[2]))
        if name == "stock":
            env["FZERO_DELUXE_DATA"] = ""
            env["FZERO_CUP"] = "retail/knight"
        proc = subprocess.run([str(build / "FZeroSNESRecompHeadless.exe"), str(stock), str(2300 if name in practice else 1600)],
                              cwd=folder, env=env, capture_output=True, text=True, timeout=180)
        log = proc.stdout + proc.stderr
        (folder / "run.log").write_text(log, encoding="utf-8")
        assert proc.returncode == 0, (name, log[-3000:])
        if rewind:
            assert "resimulation identical" in log
        group = GROUPS[identity] or ((2 if identity in (0, 2) else 3) if rebalance else 0)
        offset = 0x7cd06 + HUD_ROWS[SLOTS[identity]] * 32
        expected = struct.unpack_from("<H", sources[group], offset)[0]
        rgb = tuple((value << 3) | (value >> 2)
                    for value in ((expected >> shift) & 31 for shift in (0, 5, 10)))
        phases, exhaust = set(), set()
        cart = (folder / "cart.bin").read_bytes()
        for f in frames:
            capture = (folder / f"capture-{f:06}.bin").read_bytes()
            ram = capture[RAM:RAM + 0x20000]
            assert ram[0x54:0x56] == bytes([2, 3]), (name, "not racing")
            # Marker palette follows the physical slot, including both phases.
            actual = struct.unpack_from("<H", ram, 0x606 + HUD_ROWS[SLOTS[identity]] * 32)[0]
            assert actual == expected, (name, f, hex(actual), hex(expected))
            if packs or rebalance:
                assert ram[0x14dff] == identity
                for slot in range(4):
                    member = cart[0xf00ff + slot * 256]
                    source_group = GROUPS[member] or ((2 if member in (0, 2) else 3) if rebalance & (1 << member) else 0)
                    color = sources[source_group][0x7cd06 + HUD_ROWS[SLOTS[member]] * 32:][:2]
                    assert ram[0x606 + HUD_ROWS[slot] * 32:][:2] == color, (name, slot, "cohort/rival marker")
                if name in practice:
                    assert ram[0x58] and ram[0x14ce3] == practice[name][1]
            phases.add(ram[0x51] & 4)
            exhaust.add(ram[0x668:0x66e])
            image = Image.frombytes("RGB", (256, 224), capture[PIXELS:RAM], "raw", "BGRX")
            assert rgb in {color for _, color in image.crop((0, 144, 64, 224)).getcolors()}, (name, f, "marker pixels missing")
        assert phases == {0, 4} and len(exhaust) > 1, (name, "exhaust animation lost")
        if identity == 4:
            for scale in (1, 2):
                rendered = folder / f"wide-{scale}"
                rendered.mkdir()
                rend_env = dict(clean, FZERO_HD_SCALE=str(scale)) if scale > 1 else clean
                proc = subprocess.run([str(build / "FZeroRenderCapture.exe"), "--sequence=0.5", "21:9",
                                       str(rendered), *[str(folder / f"capture-{f:06}.bin") for f in frames]],
                                      env=rend_env, capture_output=True, text=True, timeout=90)
                assert proc.returncode == 0, proc.stderr
                for f in frames:
                    image = Image.open(rendered / f"capture-{f:06}.ppm").convert("RGB")
                    assert rgb in {color for _, color in image.crop((0, 144 * scale, 64 * scale, 224 * scale)).getcolors(maxcolors=65536)}
        results[name] = dict(marker=f"{expected:04x}", phases=sorted(phases), exhaust_phases=len(exhaust))
        print(name, "PASS", flush=True)

    with ThreadPoolExecutor(max_workers=3) as pool:
        list(pool.map(run, cases))
    (out / "validation.json").write_text(json.dumps(results, indent=2) + "\n")


if __name__ == "__main__":
    main()
