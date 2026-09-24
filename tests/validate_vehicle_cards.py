"""Private-ROM card validation against independent Snes9x information screens.

--oracles contains <source 0..3>-<native row 0..3>/{frame.png,ram.bin,vram.bin}.
Source 0 is retail; 1..3 are the author's graphics donors. Each oracle must be
captured on the confirmed car screen. No ROM-derived fixtures are committed.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import json
import os
from pathlib import Path
import shutil
import subprocess
from PIL import Image, ImageChops

from validate_native_menus import NAMES, ROSTERS, player_route
from validate_vehicles import GROUPS, SLOTS

ROOT = Path(__file__).resolve().parents[1]
ROWS = [0, 2, 1, 3]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ("build", "stock", "oracles", "out"):
        p.add_argument("--" + name, type=Path, required=True)
    args = p.parse_args()
    build, stock, oracles, out = (getattr(args, n).resolve() for n in ("build", "stock", "oracles", "out"))
    out.mkdir(parents=True, exist_ok=False)
    clean = {k: v for k, v in os.environ.items()
             if not k.startswith(("FZERO_", "SNESRECOMP_", "SDL_", "LNG_"))}
    cases = [dict(name="all-" + name, identity=i, packs=7, rebalance=15, rewind=True)
             for i, name in enumerate(NAMES)]
    cases += [dict(name="original-" + NAMES[i], identity=i, packs=7, rebalance=0) for i in range(4)]
    # Independently opted-in originals, with the other three left untouched.
    cases += [dict(name=f"rebalance-{i}-view-{j}", identity=j, packs=0, rebalance=1 << i)
              for i in range(4) for j in range(4)]
    cases += [dict(name=f"partial-{mask}-" + NAMES[i], identity=i, packs=mask, rebalance=0)
              for mask in (1, 2, 4) for i in ROSTERS[mask]]
    cases += [dict(name="practice-dragon", identity=5, packs=7, rebalance=0, practice=True, rewind=True)]
    results = {}

    def run(case):
        name, identity = case["name"], case["identity"]
        folder = out / name
        folder.mkdir()
        shutil.copytree(ROOT / "assets", folder / "assets")
        route = player_route(identity, case["packs"]) + ",600-606:8"
        if case.get("practice"):
            route = "300-306:32," + route
        env = dict(clean, FZERO_DELUXE_DATA="embedded", FZERO_BS_CARS="0", FZERO_BS_TRACKS="0",
                   FZERO_CGP_CARS=str(case["packs"]), FZERO_CGP_REBALANCE=str(case["rebalance"]),
                   FZERO_RULES="", FZERO_TRACK_PACKS="packs", SNESRECOMP_SAVE_ROOT="s",
                   SNESRECOMP_INPUT_SCRIPT=route, SNESRECOMP_FRAME_DUMP=str(folder / "frame.ppm"),
                   SNESRECOMP_WRAM_DUMP=str(folder / "ram.bin"), FZERO_CART_DUMP=str(folder / "cart.bin"),
                   FZERO_CAPTURE_FRAME="799", FZERO_CAPTURE_PREFIX=str(folder / "capture"))
        if case.get("rewind"):
            env.update(FZERO_REWIND_TEST="1", FZERO_VEHICLE_CROSS_STATE="1", FZERO_TEST_SAVE_FRAME="750")
        proc = subprocess.run([str(build / "FZeroSNESRecompHeadless.exe"), str(stock), "800"],
                              cwd=folder, env=env, capture_output=True, text=True, timeout=180)
        log = proc.stdout + proc.stderr
        (folder / "run.log").write_text(log, encoding="utf-8")
        assert proc.returncode == 0, (name, log[-2000:])
        if case.get("rewind"):
            assert "resimulation identical" in log, name
        ram = (folder / "ram.bin").read_bytes()
        assert ram[0x54:0x57] == bytes([1, 1, 1]), (name, "not on information screen")
        assert ram[0x14dff] == identity, (name, "wrong identity")
        assert bool(ram[0x58]) == bool(case.get("practice")), name
        group = GROUPS[identity] or ((2 if identity in (0, 2) else 3)
                                    if case["rebalance"] & (1 << identity) else 0)
        reference = oracles / f"{group}-{ROWS[SLOTS[identity]]}"
        source_ram = (reference / "ram.bin").read_bytes()
        assert ram[0x532:0x538] == ram[0x544:0x54a] == source_ram[0x506:0x50c], (name, "card colors")
        cart = (folder / "cart.bin").read_bytes()
        label = cart[0xf5b20 + ram[0x14c84] * 16:][:15].decode("ascii").strip()
        assert label.replace("_", "") == NAMES[identity].replace("-", " ").upper(), (name, label)
        image = Image.open(folder / "frame.ppm").convert("RGB")
        source = Image.open(reference / "frame.png").convert("RGB")
        # Compare owned values/graph and frame colors. The shared Deluxe labels
        # keep their native spacing (some donors move the colons by one pixel).
        # Normalize only 555/565 expansion differences between the two cores.
        for box in ((176, 56, 232, 112), (120, 136, 224, 176), (106, 48, 112, 176)):
            actual = image.crop(box).point(lambda v: v >> 3)
            expected = source.crop(box).point(lambda v: v >> 3)
            assert ImageChops.difference(actual, expected).getbbox() is None, (name, "donor pixels", box)
        image.save(folder / "frame.png")
        if name in ("all-dragon-bird", "all-red-gazelle"):
            for scale in (1, 2):
                target = folder / f"wide-{scale}.ppm"
                rend_env = dict(clean, FZERO_HD_SCALE=str(scale)) if scale > 1 else clean
                proc = subprocess.run([str(build / "FZeroRenderCapture.exe"), str(folder / "capture.bin"),
                                       "21:9", str(target)], env=rend_env, capture_output=True, text=True, timeout=60)
                assert proc.returncode in (0, 1), proc.stderr
                rendered = Image.open(target).convert("RGB")
                center = (rendered.width - 256 * scale) // 2
                box = (center + 120 * scale, 56 * scale, center + 232 * scale, 176 * scale)
                expected = image.crop((120, 56, 232, 176)).resize((112 * scale, 120 * scale), Image.Resampling.NEAREST)
                assert ImageChops.difference(rendered.crop(box), expected).getbbox() is None, (name, "wide/HD card")
        results[name] = dict(identity=identity, source=group, rewind=bool(case.get("rewind")))
        print(name, "PASS", flush=True)

    with ThreadPoolExecutor(max_workers=3) as pool:
        list(pool.map(run, cases))
    (out / "validation.json").write_text(json.dumps(results, indent=2) + "\n")


if __name__ == "__main__":
    main()
