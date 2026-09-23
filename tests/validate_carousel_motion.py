"""Private-ROM checks for continuous native car-column presentation.

Checks intermediate frames, not just final selections. Keeps raw captures and
a visual transition contact sheet in --out. Requires Pillow.
"""
import argparse
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
RASTER = 224 * 1120
PIXELS = RASTER + 65536
RAM = PIXELS + 256 * 224 * 4


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--stock", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    build, stock, out = args.build.resolve(), args.stock.resolve(), args.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    clean = {k: v for k, v in os.environ.items()
             if not k.startswith(("FZERO_", "SNESRECOMP_", "SDL_", "LNG_"))}
    presses = [400, 440, 480, 520, 560, 600]
    frames = [n for press in presses for n in range(press - 1, press + 13)]
    inputs = "320-326:8" + "".join(f",{p}-{p+3}:{128 if i < 3 else 64}" for i, p in enumerate(presses))
    outputs = []
    for rewind in (False, True):
        folder = out / ("rewind" if rewind else "forward")
        folder.mkdir()
        shutil.copytree(ROOT / "assets", folder / "assets")
        env = dict(clean, FZERO_DELUXE_DATA="embedded", FZERO_BS_CARS="0", FZERO_BS_TRACKS="0",
                   FZERO_CGP_CARS="7", FZERO_CGP_REBALANCE="0", FZERO_RULES="", FZERO_TRACK_PACKS="packs",
                   SNESRECOMP_SAVE_ROOT="s", SNESRECOMP_INPUT_SCRIPT=inputs, FZERO_CAPTURE_FRAME="400",
                   FZERO_CAPTURE_FRAMES=",".join(map(str, frames)), FZERO_CAPTURE_PREFIX=str(folder / "capture"))
        if rewind:
            env.update(FZERO_REWIND_TEST="1", FZERO_TEST_SAVE_FRAME="443", FZERO_VEHICLE_CROSS_STATE="1")
        proc = subprocess.run([str(build / "FZeroSNESRecompHeadless.exe"), str(stock), "630"],
                              cwd=folder, env=env, capture_output=True, text=True, timeout=120)
        (folder / "run.log").write_text(proc.stdout + proc.stderr, encoding="utf-8")
        assert proc.returncode == 0, proc.stderr[-3000:]
        if rewind:
            assert "resimulation identical" in proc.stderr
        outputs.append(folder)
    report = []
    text_reference = None
    moving_frames = []
    sheet = Image.new("RGB", (256 * 5, 242 * 6))
    draw = ImageDraw.Draw(sheet)
    for i, press in enumerate(presses):
        target = [10, 4, 0, 4, 10, 0][i]
        moving = []
        for frame in range(press - 1, press + 13):
            path = outputs[0] / f"capture-{frame:06}.bin"
            data = path.read_bytes()
            assert data == (outputs[1] / path.name).read_bytes(), (frame, "rewind changed presentation")
            ram = data[RAM:RAM + 0x20000]
            picture = Image.frombytes("RGBA", (256, 224), data[PIXELS:RAM], "raw", "BGRA")
            for caption in ((112, 85, 240, 105), (112, 125, 240, 145)):
                assert picture.crop(caption).convert("RGB").getbbox(), (frame, "native text disappeared")
            text_area = picture.crop((104, 0, 256, 224)).tobytes()
            if text_reference is None:
                text_reference = text_area
            assert text_area == text_reference, (frame, "moving car crossed into the text pane")
            if ram[0x14dff] != target:
                continue
            native = data[60 * 1120:61 * 1120]
            x = struct.unpack_from("<H", native, 576)[0] & 255
            x |= (native[1088] & 1) << 8
            if x >= 256:
                x -= 512
            if not moving or x != moving[-1][1]:
                moving.append((frame, x))
            # Exactly one cursor, same side and orientation on every page.
            vram = struct.unpack_from("<32768H", data, RASTER)
            assert 0x920 <= vram[0x400 + 7 * 32 + 2] < 0x930
            assert not vram[0x400 + 7 * 32 + 16]
        positions = [x for _, x in moving]
        assert len(positions) >= 4 and positions[-1] == 28, (press, positions)
        assert positions == sorted(positions, reverse=i < 3), (press, "wrong-way animation", positions)
        assert (positions[0] > 28) == (i < 3), (press, positions)
        report.append(dict(direction="right" if i < 3 else "left", identity=target, positions=positions))
        moving_frames.extend(frame for frame, _ in moving)
        for j, (frame, _) in enumerate(moving[:5]):
            data = (outputs[0] / f"capture-{frame:06}.bin").read_bytes()
            image = Image.frombytes("RGBA", (256, 224), data[PIXELS:RAM], "raw", "BGRA").convert("RGB")
            sheet.paste(image, (j * 256, i * 242 + 18))
            draw.text((j * 256 + 8, i * 242 + 2), f"{report[-1]['direction']} / frame {frame}", fill="white")
    sheet.save(out / "motion.png")
    # The desktop uses the enhanced compositor even outside a race. Verify
    # native, wide and HD paths against the PPU throughout the animation.
    for aspect, scale in (("4:3", 1), ("21:9", 1), ("21:9", 2)):
        rendered = out / f"render-{aspect.replace(':', '-')}-{scale}"
        rendered.mkdir()
        captures = [outputs[0] / f"capture-{frame:06}.bin" for frame in moving_frames]
        env = dict(clean)
        if scale > 1:
            env["FZERO_HD_SCALE"] = str(scale)
        proc = subprocess.run([str(build / "FZeroRenderCapture.exe"), "--sequence", aspect, str(rendered),
                               *map(str, captures)], env=env, capture_output=True, text=True, timeout=120)
        assert proc.returncode == 0, proc.stderr[-2000:]
        for capture in captures:
            image = Image.open(rendered / capture.with_suffix(".ppm").name).convert("RGB")
            extra = (image.width - 256 * scale) // 2
            native = Image.frombytes("RGBA", (256, 224), capture.read_bytes()[PIXELS:RAM], "raw", "BGRA").convert("RGB")
            expected = native.resize((256 * scale, 224 * scale), Image.Resampling.NEAREST)
            assert image.crop((extra, 0, extra + 256 * scale, 224 * scale)).tobytes() == expected.tobytes(), \
                (capture.name, aspect, scale, "enhanced/HD differs from native menu")
    (out / "validation.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print("Six directional transitions, both wraps, stable cursor and mid-slide rewind: PASS")


if __name__ == "__main__":
    main()
