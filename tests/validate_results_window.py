"""Reproduce the race-to-results window-latch regression with a private ROM.

Optionally check the owner's original CGP crash snapshot without rewriting it.
Requires Pillow for checking the rendered output in stock, wide and HD modes.
"""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--build", type=Path, required=True)
    p.add_argument("--stock", type=Path, required=True)
    p.add_argument("--state", type=Path)
    p.add_argument("--out", type=Path, required=True)
    a = p.parse_args()
    build, stock, out = a.build.resolve(), a.stock.resolve(), a.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    clean = {k: v for k, v in os.environ.items()
             if not k.startswith(("FZERO_", "SNESRECOMP_", "SDL_", "LNG_"))}
    report = {}
    for name in ["fresh-bs"] + (["reported-save"] if a.state else []):
        folder = out / name
        folder.mkdir(exist_ok=True)
        shutil.copytree(ROOT / "assets", folder / "assets", dirs_exist_ok=True)
        saved = name == "reported-save"
        frames = 30 if saved else 2500
        env = dict(clean, FZERO_DELUXE_DATA="embedded", FZERO_TRACK_PACKS="packs",
                   FZERO_BS_CARS="0" if saved else "1", FZERO_BS_TRACKS="0" if saved else "1",
                   FZERO_CGP_CARS="7" if saved else "0", FZERO_CGP_REBALANCE="15" if saved else "0",
                   FZERO_RULES="all,cgp-msu,cgp-credits" if saved else "",
                   FZERO_CAPTURE_FRAME=str(6881 if saved else frames - 1),
                   FZERO_CAPTURE_PREFIX=str(folder / "capture"),
                   SNESRECOMP_SAVE_ROOT="s", SNESRECOMP_WRAM_DUMP=str(folder / "ram.bin"),
                   SNESRECOMP_FRAME_DUMP=str(folder / "frame.ppm"))
        if saved:
            env.update(FZERO_STATE_LOAD=str(a.state.resolve()), FZERO_REWIND_TEST="1", FZERO_TEST_SAVE_FRAME="10")
        else:
            fixture = folder / "death.txt"
            fixture.write_text("1500 1500 c3 40\n", encoding="ascii")
            env.update(FZERO_TEST_WRAM_SCRIPT=str(fixture),
                       SNESRECOMP_INPUT_SCRIPT="320-326:8,560-566:8,640-646:8,730-736:8,790-796:8,1160-1499:1,2300-2306:8")
        proc = subprocess.run([str(build / "FZeroSNESRecompHeadless.exe"), str(stock), str(frames)],
                              cwd=folder, env=env, capture_output=True, text=True, timeout=180)
        log = proc.stdout + proc.stderr
        (folder / "run.log").write_text(log, encoding="utf-8")
        assert proc.returncode == 0, (name, log[-3000:])
        ram = (folder / "ram.bin").read_bytes()
        assert ram[0x54] == 3, (name, ram[0x54:0x57].hex())
        if saved:
            assert "resimulation identical" in log, name
        images = [folder / "frame.ppm"]
        for aspect, scale in (("4:3", 1), ("21:9", 1), ("21:9", 4)):
            output = folder / f"render-{aspect.replace(':', '-')}-{scale}.ppm"
            rendered = subprocess.run([str(build / "FZeroRenderCapture.exe"), str(folder / "capture.bin"), aspect, str(output)],
                                      cwd=folder, env=dict(clean, FZERO_HD_SCALE=str(scale)) if scale > 1 else clean,
                                      capture_output=True, text=True, timeout=60)
            assert rendered.returncode in (0, 1), (name, rendered.stderr)
            images.append(output)
        for path in images:
            image = Image.open(path).convert("RGB")
            scale = image.height // 224
            extra = (image.width - 256 * scale) // 2
            # This rectangle was a solid red/grey bar. It is unused black
            # space in the native results screen, also checked in Snes9x.
            area = image.crop((extra + 100 * scale, 18 * scale, extra + 145 * scale, 27 * scale))
            assert area.getbbox() is None, (name, path.name, "stale results window")
            assert image.getpixel((0, 20 * scale)) == (0, 0, 0), (name, "window not fully collapsed")
            image.save(path.with_suffix(".png"))
        report[name] = dict(scene=list(ram[0x54:0x57]), render_modes=4, rewind=saved)
        print(name, "PASS", flush=True)
    (out / "validation.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
