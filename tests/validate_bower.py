"""Private-ROM qualification of Bower extraction, race order and CGP cup labels.

Checks loaded resources and injected GP progression, not complete driven laps.
Donor ROMs and extracted resources remain under --out, never in Git.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from parse_track_pack import donor, fields, recognize


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--stock", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    build, stock, out = args.build.resolve(), args.stock.resolve(), args.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    original = stock.read_bytes()
    registry = ROOT / "assets/track-packs"
    images, descriptors = {}, {}
    for pack in ("bower-league", "cgp", "max-league"):
        images[pack] = donor(original, (registry / f"{pack}.ips").read_bytes())
        assert recognize(images[pack]).stem == pack
        descriptors[pack] = fields(registry / f"{pack}.ini")
        image = out / f"{pack}.sfc"
        image.write_bytes(images[pack])
        report = subprocess.check_output([str(build / "FZeroInspectCourses.exe"), str(image),
                  str(registry / f"{pack}.layout"), str(out / pack)], text=True)
        (out / f"{pack}-extract.txt").write_text(report, encoding="utf-8")

    # Small native menu tile IDs (not ASCII), read through the donor's pointer
    # table. Preserve the authored spelling, independent of our manifest.
    glyphs = {0xaf: "A", 0xbf: "B", 0xc4: "C", 0xc6: "E", 0xc7: "F", 0xc8: "G",
              0xc9: "H", 0xca: "I", 0xcc: "K", 0xcd: "L", 0xce: "N", 0xd0: "O",
              0xd1: "P", 0xd2: "Q", 0xd3: "R", 0xd4: "S", 0xd5: "T", 0xd6: "U",
              0xd8: "W", 0xdb: "Z", 0xe1: "1", 0xe2: "2", 0xff: " "}
    cgp = images["cgp"]
    names = []
    for i in range(11):
        ptr = int.from_bytes(cgp[0x80747 + 2*i:0x80749 + 2*i], "little")
        offset = 0x80000 + ptr - 0x8000
        names.append("".join(glyphs[b] for b in cgp[offset:offset+26:2]).strip())
    assert names[5:] == ["BARON", "SCEPTER", "CROWN", "ZENITH", "FALCON", "TRUE"], names
    assert [c.split("|")[1].upper() for c in descriptors["cgp"]["cup"]][5:] == names[5:]

    tracks = [t.split("|") for t in descriptors["bower-league"]["track"]]
    order = [int(t[3]) for t in tracks]
    assert order == list(images["bower-league"][0x80612:0x80617]) == [6, 4, 5, 3, 2]
    comparison = {}
    for _, name, _, slot in tracks:
        geometry = (out / f"bower-league-{slot}.bin").read_bytes()[:0xd600]
        matches = [(pack, t.split("|")[0]) for pack in ("cgp", "max-league")
                   for t in descriptors[pack]["track"]
                   if geometry == (out / f"{pack}-{t.split('|')[3]}.bin").read_bytes()[:0xd600]]
        assert not matches, (name, matches)
        comparison[name] = {"geometry_sha256": hashlib.sha256(geometry).hexdigest(), "duplicates": matches}

    clean = {k: v for k, v in os.environ.items()
             if not k.startswith(("FZERO_", "SNESRECOMP_", "SDL_", "LNG_"))}
    results = {}

    def run(case):
        ordinal, deluxe, progress = case
        name = "progress" if progress else f"course-{ordinal}-{'deluxe' if deluxe else 'retail'}"
        folder = out / name
        folder.mkdir()
        shutil.copytree(registry, folder / "assets/track-packs")
        env = dict(clean, FZERO_DELUXE_DATA="embedded" if deluxe else "", FZERO_BS_TRACKS="0",
                   FZERO_BS_CARS="1" if deluxe else "0", FZERO_CGP_CARS="0", FZERO_RULES="",
                   FZERO_CUP="bower-league/bower", FZERO_TRACK_PACKS="packs", SNESRECOMP_SAVE_ROOT="s",
                   FZERO_ASPECT="21:9", SNESRECOMP_WRAM_DUMP=str(folder / "ram.bin"),
                   SNESRECOMP_FRAME_DUMP=str(folder / "frame.ppm"),
                   SNESRECOMP_INPUT_SCRIPT="320-326:8,440-446:8,560-566:8,730-736:8,790-796:8")
        if progress:
            env["FZERO_LIBRARY_PROGRESS_TEST"] = "1"
        else:
            env["FZERO_TEST_COURSE"] = str(ordinal)
        proc = subprocess.run([str(build / "FZeroSNESRecompHeadless.exe"), str(stock),
                               "6900" if progress else "1100"], cwd=folder, env=env,
                              capture_output=True, text=True, timeout=240)
        log = proc.stdout + proc.stderr
        (folder / "run.log").write_text(log, encoding="utf-8")
        assert proc.returncode == 0 and "fzero_native: PASS" in log, (name, log[-2500:])
        assert "extracted bower-league: 5 courses; donor code discarded" in log
        if progress:
            assert log.count("completed result ordinal=") == 5
            assert all(f"ordinal={i} setting=" in log for i in range(5))
        else:
            ram = (folder / "ram.bin").read_bytes()
            assert ram[0x54] == 2 and ram[0x55] in (2, 3), (name, ram[0x54:0x57].hex())
            course = (out / f"bower-league-{order[ordinal]}.bin").read_bytes()
            assert ram[0x10000:0x12400] == course[:0x2400], (name, "pool")
            # Only the populated grid/block lengths are copied by the loader.
            report = (out / "bower-league-extract.txt").read_text().splitlines()[order[ordinal]]
            sizes = dict(item.split("=") for item in report.split() if "=" in item)
            for address, offset, length in [(0x14e00, 0x2400, int(sizes["blocks"])),
                                             (0x17000, 0x4600, int(sizes["grid"]))]:
                assert ram[address:address+length] == course[offset:offset+length], (name, hex(address))
        results[name] = "PASS"
        print(name, "PASS", flush=True)

    with ThreadPoolExecutor(max_workers=3) as pool:
        list(pool.map(run, [(i, deluxe, False) for deluxe in (False, True) for i in range(5)] + [(0, True, True)]))
    assert stock.read_bytes() == original
    (out / "validation.json").write_text(json.dumps(dict(cgp_names=names, bower_order=order,
                  geometry=comparison, cases=results, stock_unchanged=True), indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
