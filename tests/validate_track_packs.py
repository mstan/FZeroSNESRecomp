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
    library = out / "library"; library.mkdir()
    registry = out / "assets/track-packs"; registry.mkdir(parents=True)
    for ext in ("ini", "layout"):
        (registry / f"max-league.{ext}").write_bytes((ROOT / f"assets/track-packs/max-league.{ext}").read_bytes())
    with zipfile.ZipFile(a.archive) as archive:
        classic = archive.read("MAX_League_Classic.ips")
        modern = literal_bps(source, apply_ips(source, archive.read("MAX_League_Modern.ips")))
    (library / "arbitrary.IPS").write_bytes(classic)
    (library / "equivalent.bps").write_bytes(modern)
    (library / "corrupt.ips").write_bytes(b"PATCHbad")
    env = {k: v for k, v in os.environ.items()
           if not k.startswith(("FZERO_", "SNESRECOMP_", "SDL_", "LNG_"))}
    env.update(FZERO_TRACK_PACKS=str(library), FZERO_DELUXE_DATA="embedded", FZERO_ASPECT="21:9")
    route = "320-326:8,440-446:8,560-566:8,730-736:8,790-796:8,1160-1599:1"
    results = {}
    def run(name, cup="max-league/max", frames=1600, **overrides):
        folder = out / name; folder.mkdir()
        run_env = dict(env, FZERO_CUP=cup, SNESRECOMP_INPUT_SCRIPT=route,
                       SNESRECOMP_FRAME_DUMP=str(folder / "frame.ppm"),
                       SNESRECOMP_WRAM_DUMP=str(folder / "ram.bin"),
                       SNESRECOMP_SAVE_ROOT=f"{name}/saves")
        run_env.update(overrides)
        with (folder / "run.log").open("w") as log:
            result = subprocess.run([str(build / "FZeroSNESRecompHeadless.exe"), str(stock_path), str(frames)],
                                    cwd=out, env=run_env, stdout=log, stderr=log, timeout=300)
        text = (folder / "run.log").read_text()
        assert result.returncode == 0 and "fzero_native: PASS" in text, text[-4000:]
        if frames == 1600:
            ram = (folder / "ram.bin").read_bytes()
            assert ram[0x54:0x56] == b"\x02\x03", (name, ram[0x54:0x57].hex())
        if "FZERO_LIFECYCLE_TEST" in overrides:
            assert "resimulation identical" in text and "soft reset, SRAM retained" in text
        results[name] = {"cup": cup, "frames": frames, "exit": result.returncode}
        print(name, "PASS", flush=True)
        return text
    for i in range(5):
        text = run(f"course{i}", FZERO_TEST_COURSE=str(i))
        assert text.count("extracted max-league:")==1 and "donor code discarded" in text
    run("lifecycle", frames=1850, FZERO_LIFECYCLE_TEST="1")
    text=run("progress", frames=6900, FZERO_LIBRARY_PROGRESS_TEST="1")
    assert all(f"ordinal={i} setting=" in text for i in range(5))
    assert text.count("completed result ordinal=")==5
    run("stock", "retail/knight", FZERO_DELUXE_DATA="")
    run("king", "bs-deluxe/king")
    run("bs1", "bs-deluxe/bs-1")
    text = run("menu", cup="", frames=700,
               SNESRECOMP_INPUT_SCRIPT="320-326:8,440-446:8,560-566:8,650-652:64")
    assert "menu 6/6: MAX League" in text
    # Two independent, one-course manifests can coexist with the full donor pack.
    template=(registry / "max-league.ini").read_text()
    for ident,track in [("solo-a",0),("solo-b",4)]:
        lines=[line for line in template.splitlines() if not line.startswith(("id=","name=","cup=","track="))]
        lines += [f"id={ident}",f"name={ident}",f"cup=solo|{ident}|0",f"track=only|Only Course|solo|{track}"]
        (library / f"{ident}.ini").write_text("\n".join(lines)+"\n")
        (library / f"{ident}.layout").write_bytes((registry / "max-league.layout").read_bytes())
    run("solo", "solo-b/solo")
    text=run("solo-progress", "solo-b/solo", frames=2800, FZERO_LIBRARY_PROGRESS_TEST="1")
    assert text.count("completed result ordinal=")==1 and "ordinal=1 setting=" not in text
    (library / "solo-a.ini").rename(library / "solo-a.held")
    run("partial", "solo-b/solo", frames=10)
    (library / "arbitrary.IPS").rename(library / "classic.held")
    run("bps", frames=10)
    (library / "equivalent.bps").rename(library / "modern.held")
    text=run("absent", "bs-deluxe/knight", frames=10)
    assert "extracted" not in text
    (library / "classic.held").rename(library / "arbitrary.IPS")
    text=run("restored", frames=10);assert "extracted max-league" in text
    (library / "library.disabled").write_text("1\n")
    text=run("old-master-ignored", frames=10);assert "extracted max-league" in text
    (library / "max-league.disabled").write_text("1\n")
    text=run("pack-disabled", "bs-deluxe/knight", frames=10);assert "extracted max-league" not in text
    assert stock_path.read_bytes() == original
    (out / "validation.json").write_text(json.dumps({"stock_unchanged": True,
        "stock_sha256": hashlib.sha256(source).hexdigest(), "cases": results}, indent=2)+"\n")


if __name__ == "__main__":
    main()
