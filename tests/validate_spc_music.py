"""Verify non-MSU course music at the real SPC upload destination ($07fe).

Requires the owner's stock ROM. No audio/ROM data is committed. Expectations
are independent of the extraction manifest and the runtime's music selector.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
import json
import os
from pathlib import Path
import shutil
import subprocess

from validate_native_menus import player_route, press
from validate_practice_catalog import route as practice_route

ROOT = Path(__file__).resolve().parents[1]
SONGS = ["Mute City", "Big Blue", "Sand Ocean", "Silence", "Port Town",
         "Red Canyon", "White Land I", "White Land II", "Fire Field", "Death Wind"]
# CGP resource slots, not GP display order. Slot 10 restores Mute City III.
EXPECTED = [0, 1, 2, 9, 3, 0, 4, 5, 6, 7, 0, 9, 4, 5, 8,
            1, 1, 1, 1, 3, 0, 1, 1, 1, 1,
            1, 1, 6, 0, 2, 9, 5, 3, 6, 2, 7, 8, 4, 2, 3,
            4, 9, 2, 4, 0, 7, 4, 2, 1, 8, 3, 4, 7, 3, 2]


def tracks(pack):
    counts = {}
    for line in (ROOT / f"assets/track-packs/{pack}.ini").read_text().splitlines():
        if line.startswith("track="):
            identity, name, cup, slot = line[6:].split("|")
            order = counts.get(cup, 0)
            counts[cup] = order + 1
            yield dict(name=identity, label=name, cup=f"{pack}/{cup}",
                       order=order, slot=int(slot))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ("build", "stock", "out"):
        parser.add_argument("--" + key, type=Path, required=True)
    parser.add_argument("--filter", default="")
    args = parser.parse_args()
    build, out, stock = args.build.resolve(), args.out.resolve(), args.stock.resolve()
    out.mkdir(parents=True, exist_ok=False)
    shutil.copytree(ROOT / "assets", out / "assets")
    (out / "empty").mkdir()
    (out / "empty/none.msu").write_bytes(b"")
    clean = {k: v for k, v in os.environ.items()
             if not k.startswith(("FZERO_", "SNESRECOMP_", "SDL_", "LNG_"))}
    cgp = list(tracks("cgp"))
    cases = [dict(t, name="off-" + t["name"], song=EXPECTED[t["slot"]]) for t in cgp]
    # Every native song on the retail engine and with a missing MSU recording.
    representatives = [next(t for t in cgp if EXPECTED[t["slot"]] == song) for song in range(10)]
    for mode in ("stock", "missing"):
        cases += [dict(t, name=mode + "-" + t["name"], song=EXPECTED[t["slot"]],
                       stock=mode == "stock", missing=mode == "missing") for t in representatives]
    for pack, songs in (("max-league", [0, 4, 5, 9, 8]),
                        ("bower-league", [9, 3, 2, 5, 4, 3, 0])):
        cases += [dict(t, name=pack + "-" + t["name"], song=songs[t["slot"]]) for t in tracks(pack)]
    # Imported metadata must not affect the original three leagues.
    for cup, theme in (("knight", 0), ("queen", 0), ("king", 0)):
        cases.append(dict(name="native-" + cup, cup="bs-deluxe/" + cup, order=0, song=theme))
    cases += [dict(name="practice-big-blue-3", cup="cgp/cgp-1", order=1, song=1, practice=True),
              dict(name="practice-missing-big-blue-3", cup="cgp/cgp-1", order=1, song=1,
                   practice=True, missing=True, rewind=True)]
    if args.filter:
        cases = [c for c in cases if any(k in c["name"] for k in args.filter.split(","))]
    assert cases
    for index, case in enumerate(cases):
        case["save_root"] = f"s{index}"

    def run(case):
        folder = out / case["name"]
        folder.mkdir()
        (out / case["save_root"]).mkdir()
        (folder / "packs").mkdir()
        (folder / "packs/max-league.disabled").write_text("0\n")
        cars = 0 if case.get("stock") else 7
        inputs = player_route(0, cars) + ",600-606:8,730-736:8,900-906:8,960-966:8"
        if not cars:
            inputs = "320-326:8,440-446:8,560-566:8,730-736:8,790-796:8"
        env = dict(clean, FZERO_DELUXE_DATA="embedded", FZERO_BS_CARS="0", FZERO_BS_TRACKS="0",
                   FZERO_CGP_CARS=str(cars), FZERO_CGP_REBALANCE="0",
                   FZERO_RULES="cgp-msu" if case.get("missing") else "",
                   FZERO_CUP=case["cup"], FZERO_TEST_COURSE=str(case["order"]),
                   FZERO_TRACK_PACKS=str(folder / "packs"), SNESRECOMP_SAVE_ROOT=case["save_root"],
                   SNESRECOMP_INPUT_SCRIPT=inputs,
                   SNESRECOMP_WRAM_DUMP=str(folder / "ram.bin"),
                   FZERO_TEST_APURAM_DUMP=str(folder / "apu.bin"))
        if case.get("missing"):
            env["SNESRECOMP_MSU1"] = str(out / "empty/none")
        frames = 1750
        if case.get("practice"):
            env.pop("FZERO_TEST_COURSE")
            env.pop("FZERO_CUP")
            env["SNESRECOMP_INPUT_SCRIPT"] = (practice_route(0, 255) + ",1200-1206:8" +
                "".join(press(1300 + i * 24, 32) for i in range(9)) +
                ",1620-1626:8,1800-1806:32,1900-1906:8")
            frames = 3100
        if case.get("rewind"):
            env.update(FZERO_REWIND_TEST="1", FZERO_VEHICLE_CROSS_STATE="1", FZERO_TEST_SAVE_FRAME="2600")
        proc = subprocess.run([str(build / "FZeroSNESRecompHeadless.exe"), str(stock), str(frames)],
                              cwd=out, env=env, capture_output=True, text=True, timeout=150)
        log = proc.stdout + proc.stderr
        (folder / "run.log").write_text(log)
        assert proc.returncode == 0, (case["name"], log[-1800:])
        ram = (folder / "ram.bin").read_bytes()
        apu = (folder / "apu.bin").read_bytes()
        assert ram[0x54:0x56] == b"\x02\x03", (case["name"], ram[0x54:0x57].hex())
        assert bool(ram[0x58]) == bool(case.get("practice"))
        if case.get("practice"):
            assert ram[0x53] == case["order"], (case["name"], "Practice order", ram[0x53])
        if case.get("rewind"):
            assert "resimulation identical" in log
        assert apu[0x7fe] == 8 + case["song"], (case["name"], "SPC song", apu[0x7fe], case["song"])
        if case.get("missing"):
            assert ram[0x182] == 1, (case["name"], "missing-PCM fallback", ram[0x180:0x185].hex())
        print(case["name"], "PASS:", SONGS[case["song"]], flush=True)
        return dict(name=case["name"], song=SONGS[case["song"]], spc_song_byte=apu[0x7fe])

    passed, failed = [], []
    with ThreadPoolExecutor(max_workers=3) as pool:
        for future in as_completed([pool.submit(run, case) for case in cases]):
            try:
                passed.append(future.result())
            except Exception as error:
                failed.append(str(error))
                print("FAIL:", error, flush=True)
    (out / "validation.json").write_text(json.dumps(dict(passed=passed, failed=failed), indent=2))
    assert not failed, "\n".join(failed)


if __name__ == "__main__":
    main()
