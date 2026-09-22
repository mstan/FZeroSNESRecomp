"""Private CGP qualification. Inputs and extracted data stay in --out, never Git.

Requires the owner's stock ROM, three CGP archives and MAX archive. Tests all
30 imported courses, native transitions, variant equivalence and partial installs.
Completed-result injection checks transitions, not manual full-lap playability.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from parse_track_pack import donor, fields
from validate_track_packs import literal_bps


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--build", type=Path, required=True)
    p.add_argument("--stock", type=Path, required=True)
    p.add_argument("--archive", type=Path, action="append", required=True)
    p.add_argument("--max-archive", type=Path, required=True)
    p.add_argument("--out", type=Path, required=True)
    a = p.parse_args()
    build, stock_path, out = a.build.resolve(), a.stock.resolve(), a.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    stock = stock_path.read_bytes()
    library = out / "library"; library.mkdir()
    registry = out / "assets/track-packs"; registry.mkdir(parents=True)
    for file in (ROOT / "assets/track-packs").iterdir():
        if file.suffix in (".ini", ".layout"):
            (registry / file.name).write_bytes(file.read_bytes())
    descriptor = fields(registry / "cgp.ini")
    tracks = [entry.split("|") for entry in descriptor["track"]]
    assert len(tracks) == 30 and {int(t[3]) for t in tracks} == set(range(25, 55))
    extracted, patches, sizes = [], [], {}
    inspector = build / "FZeroInspectCourses.exe"
    for i, archive_path in enumerate(a.archive):
        with zipfile.ZipFile(archive_path) as archive:
            entries = [e for e in archive.infolist() if e.filename.lower().endswith(".ips")]
            assert len(entries) == 1
            patch = archive.read(entries[0])
        patches.append(patch)
        image = donor(stock, patch)
        image_path = out / f"variant{i}.sfc"; image_path.write_bytes(image)
        report = subprocess.check_output([str(inspector), str(image_path),
                    str(registry / "cgp.layout"), str(out / f"variant{i}-course")], text=True)
        (out / f"variant{i}-extraction.txt").write_text(report, encoding="utf-8")
        hashes = re.findall(r"hash=([0-9a-f]{64})", report)
        if i == 0:
            sizes = {int(slot): (int(blocks), int(grid)) for slot, blocks, grid in
                     re.findall(r"course (\d+): setting=\w+ blocks=(\d+) grid=(\d+)", report)}
        assert len(hashes) == 55
        extracted.append(hashes)
        (library / f"variant{i}.ips").write_bytes(patch)
    assert len(extracted) == 3 and extracted[0] == extracted[1] == extracted[2]
    with zipfile.ZipFile(a.max_archive) as archive:
        max_patch = archive.read("MAX_League_Classic.ips")
    (library / "max.ips").write_bytes(max_patch)
    max_donor = out / "max.sfc"; max_donor.write_bytes(donor(stock, max_patch))
    subprocess.check_output([str(inspector), str(max_donor), str(registry / "max-league.layout"),
                             str(out / "max-course")], text=True)
    # Pool/block/grid comparisons ignore names, palettes and opponent tuning.
    max_geometry = [(out / f"max-course-{i}.bin").read_bytes()[:0xd600] for i in range(5)]
    for _, _, _, slot in tracks:
        assert (out / f"variant0-course-{slot}.bin").read_bytes()[:0xd600] not in max_geometry
    env = {k: v for k, v in os.environ.items()
           if not k.startswith(("FZERO_", "SNESRECOMP_", "SDL_", "LNG_"))}
    env.update(FZERO_TRACK_PACKS=str(library), FZERO_DELUXE_DATA="embedded", FZERO_ASPECT="21:9")
    route = "320-326:8,440-446:8,560-566:8,730-736:8,790-796:8,1160-1599:1"
    results = {}

    def run(name, cup="cgp/cgp-1", frames=1600, **overrides):
        folder = out / name; folder.mkdir()
        run_env = dict(env, FZERO_CUP=cup, SNESRECOMP_INPUT_SCRIPT=route,
                       SNESRECOMP_FRAME_DUMP=str(folder / "frame.ppm"),
                       SNESRECOMP_WRAM_DUMP=str(folder / "ram.bin"),
                       SNESRECOMP_SAVE_ROOT=f"{name}/saves")
        run_env.update(overrides)
        with (folder / "run.log").open("w") as log:
            result = subprocess.run([str(build / "FZeroSNESRecompHeadless.exe"), str(stock_path), str(frames)],
                                    cwd=out, env=run_env, stdout=log, stderr=log, timeout=300)
        text = (folder / "run.log").read_text(encoding="utf-8")
        assert result.returncode == 0 and "fzero_native: PASS" in text, text[-4000:]
        if frames == 1600:
            ram = (folder / "ram.bin").read_bytes()
            assert ram[0x54:0x56] == b"\x02\x03", (name, ram[0x54:0x57].hex())
        results[name] = {"cup": cup, "frames": frames, "exit": result.returncode}
        print(name, "PASS", flush=True)
        return text

    def course_case(item):
        index, (ident, _, cup, slot) = item
        text = run(ident, f"cgp/{cup}", FZERO_TEST_COURSE=str(index % 5))
        assert text.count("extracted cgp: 30 courses")==1
        # Verify the selected donor's actual geometry reached the canonical loader.
        ram = (out / ident / "ram.bin").read_bytes()
        course = (out / f"variant0-course-{slot}.bin").read_bytes()
        assert ram[0x10000:0x12400] == course[:0x2400], (ident, "tile pool")
        blocks, grid = sizes[int(slot)]
        assert ram[0x14e00:0x14e00 + blocks] == course[0x2400:0x2400 + blocks], (ident, "blocks")
        if ram[0x17000:0x17000 + grid] != course[0x4600:0x4600 + grid]:
            # Exploded mines rewrite four grid cells during a race. Compare the
            # initial loaded road before driving, rather than freezing hazards.
            run(ident + "-load", f"cgp/{cup}", frames=1100, FZERO_TEST_COURSE=str(index % 5))
            ram = (out / (ident + "-load") / "ram.bin").read_bytes()
        assert ram[0x17000:0x17000 + grid] == course[0x4600:0x4600 + grid], (ident, "grid")

    with ThreadPoolExecutor(max_workers=4) as executor:
        list(executor.map(course_case, enumerate(tracks)))

    def progress(cup):
        text = run(f"progress-{cup}", f"cgp/{cup}", frames=6900, FZERO_LIBRARY_PROGRESS_TEST="1")
        assert all(f"ordinal={i} setting=" in text for i in range(5))
        assert text.count("completed result ordinal=")==5

    with ThreadPoolExecutor(max_workers=3) as executor:
        list(executor.map(progress, [f"cgp-{i}" for i in range(1, 7)]))
    text = run("lifecycle", frames=1850, FZERO_LIFECYCLE_TEST="1")
    assert "resimulation identical" in text and "soft reset, SRAM retained" in text
    for cup in ("knight", "queen", "king", "bs-1", "bs-2"):
        run(f"native-{cup}", f"bs-deluxe/{cup}")
    run("max-present", "max-league/max")
    run("stock-engine", FZERO_DELUXE_DATA="", FZERO_ASPECT="4:3")
    text = run("menu", cup="", frames=700,
               SNESRECOMP_INPUT_SCRIPT="320-326:8,440-446:8,560-566:8,650-652:64")
    assert "menu 12/12:" in text
    for i in range(3):
        (library / f"variant{i}.ips").rename(library / f"variant{i}.held")
    for i in range(3):
        (library / f"variant{i}.held").rename(library / f"variant{i}.ips")
        text = run(f"only-variant{i}")
        assert text.count("extracted cgp: 30 courses")==1
        (library / f"variant{i}.ips").rename(library / f"variant{i}.held")
    roots = [sorted(p.name for p in (out / f"only-variant{i}/saves/bs-deluxe/courses").iterdir())
             for i in range(3)]
    assert roots[0] == roots[1] == roots[2]
    text = run("cgp-absent", "max-league/max", frames=10)
    assert "extracted cgp" not in text and "extracted max-league" in text
    (library / "equivalent.bps").write_bytes(literal_bps(stock, donor(stock, patches[0])))
    run("bps-only", frames=10)
    (library / "cgp.disabled").write_text("1\n")
    text = run("cgp-disabled", "max-league/max", frames=10)
    assert "extracted cgp" not in text and "extracted max-league" in text
    assert stock_path.read_bytes() == stock
    (out / "validation.json").write_text(json.dumps({"stock_unchanged": True,
        "stock_sha256": hashlib.sha256(stock).hexdigest(), "variant_course_hashes": extracted,
        "courses_imported": 30, "native_slots_excluded": list(range(25)),
        "max_geometry_duplicates": [], "cases": results}, indent=2)+"\n", encoding="utf-8")


if __name__ == "__main__":
    main()
