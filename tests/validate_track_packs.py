"""Private gameplay qualification; requires the owner's stock ROM and archive.

Creates fresh local captures, never changes the source ROM or another install.
Run after building the paired framework/game worktrees with BS Deluxe enabled.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import zipfile
import zlib

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from import_track_pack import install
from inspect_bs_deluxe import apply_ips


def literal_bps(source, target):
    patch = bytearray(b"BPS1")
    def number(value):
        while True:
            byte = value & 127
            value >>= 7
            if not value:
                patch.append(byte | 128)
                break
            patch.append(byte)
            value -= 1
    number(len(source)); number(len(target)); number(0)
    number(((len(target) - 1) << 2) | 1)
    patch.extend(target)
    patch.extend(struct.pack("<II", zlib.crc32(source), zlib.crc32(target)))
    patch.extend(struct.pack("<I", zlib.crc32(patch)))
    return bytes(patch)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--build", type=Path, required=True)
    p.add_argument("--stock", type=Path, required=True)
    p.add_argument("--archive", type=Path, required=True)
    p.add_argument("--out", type=Path, required=True)
    a = p.parse_args()
    build, stock_path, out = a.build.resolve(), a.stock.resolve(), a.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    original = stock_path.read_bytes()
    source = original[512:] if len(original) == 0x80200 else original
    library = out / "library"
    with zipfile.ZipFile(a.archive) as archive:
        for variant in ("classic", "modern"):
            patch = archive.read(f"MAX_League_{variant.title()}.ips")
            # Exercise an equivalent BPS against the same pinned Modern hash.
            if variant == "modern":
                patch = literal_bps(source, apply_ips(source, patch))
            install(source, patch, ROOT / f"assets/track-packs/max-league-{variant}.ini", library)
    env = {k: v for k, v in os.environ.items()
           if not k.startswith(("FZERO_", "SNESRECOMP_", "SDL_", "LNG_"))}
    env.update(FZERO_TRACK_PACKS=str(library), SNESRECOMP_SAVE_ROOT="saves")
    route = "320-326:8,440-446:8,560-566:8,730-736:8,790-796:8,1160-1599:1"
    results = {}
    def run(name, cup, frames=1600, expected=0):
        folder = out / name
        folder.mkdir()
        run_env = dict(env, FZERO_CUP=cup, SNESRECOMP_INPUT_SCRIPT=route,
                       SNESRECOMP_FRAME_DUMP="frame.ppm", SNESRECOMP_WRAM_DUMP="ram.bin")
        with (folder / "run.log").open("w") as log:
            result = subprocess.run([str(build / "FZeroSNESRecompHeadless.exe"), str(stock_path), str(frames)],
                                    cwd=folder, env=run_env, stdout=log, stderr=log, timeout=45)
        text = (folder / "run.log").read_text()
        assert result.returncode == expected, text[-3000:]
        if expected == 0:
            assert "fzero_native: PASS" in text
            if frames == 1600:
                ram = (folder / "ram.bin").read_bytes()
                assert ram[0x54:0x56] == b"\x02\x03", (cup, ram[0x54:0x57].hex())
            if cup.startswith("max-"):
                assert "isolated interpreter program" in text
        results[name] = {"cup": cup, "frames": frames, "exit": result.returncode}
        print(name, "PASS", flush=True)
    run("classic-ips", "max-league-classic/max")
    run("modern-bps", "max-league-modern/max")
    run("retail-king", "retail/king")
    run("bs-one", "bs-deluxe/bs-1")
    # Missing packs must not break unrelated content or silently rebind a key.
    classic = library / "max-league-classic.patch"
    payload = classic.read_bytes()
    classic.rename(library / "held.patch")
    run("missing-selected", "max-league-classic/max", 10, 2)
    run("missing-unrelated", "max-league-modern/max", 10)
    classic.write_bytes(payload[:-1] + bytes([payload[-1] ^ 1]))
    run("corrupt-selected", "max-league-classic/max", 10, 2)
    run("corrupt-unrelated", "retail/knight", 10)
    classic.write_bytes(payload)
    run("restored", "max-league-classic/max", 10)
    assert stock_path.read_bytes() == original
    (out / "validation.json").write_text(json.dumps({
        "stock_unchanged": True, "stock_sha256": hashlib.sha256(source).hexdigest(),
        "cases": results}, indent=2) + "\n")


if __name__ == "__main__":
    main()
