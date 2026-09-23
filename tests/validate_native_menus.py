"""Exercise native car columns and the additive vertical league list with pad input.
Requires the owner's verified stock ROM; all captures stay in --out.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from inspect_bs_deluxe import apply_ips
# Expected native row order from each author's four-car menu; unassigned
# originals follow as one separate column when a CGP group is disabled.
ROSTERS = {
    0: [0, 2, 1, 3],
    1: [4, 6, 5, 7, 0, 2, 1, 3],
    2: [0, 2, 8, 9, 1, 3],
    3: [4, 6, 5, 7, 0, 2, 8, 9, 1, 3],
    4: [10, 11, 1, 3, 0, 2],
    5: [4, 6, 5, 7, 10, 11, 1, 3, 0, 2],
    6: [0, 2, 8, 9, 10, 11, 1, 3],
    7: [4, 6, 5, 7, 0, 2, 8, 9, 10, 11, 1, 3],
}
ORDER = ROSTERS[7]
NAMES = ["blue-falcon", "wild-goose", "golden-fox", "fire-stingray",
         "moon-shadow", "dragon-bird", "great-star", "death-anchor",
         "p-emerald", "black-bull", "white-cat", "red-gazelle"]


def press(frame, button):
    return f",{frame}-{frame + 3}:{button}"


def player_route(identity, packs=7):
    roster = ROSTERS[packs]
    index = roster.index(identity)
    pages = (len(roster) + 3) // 4
    # Blue Falcon is the initial identity even when its group is column two.
    rights = (index // 4 - roster.index(0) // 4) % pages
    return ("320-326:8" + "".join(press(400 + i * 30, 128) for i in range(rights)) +
            "".join(press(480 + i * 20, 32) for i in range(index % 4)))


def enabled_cup_count(all_packs=False):
    """Read the same shipped pack defaults as the runtime (original BS off)."""
    count = 3
    for manifest in (ROOT / "assets/track-packs").glob("*.ini"):
        def flag(suffix):
            path = manifest.with_suffix(suffix)
            return path.exists() and path.read_text().strip() == "1"
        if not flag(".hidden") and (all_packs or not flag(".disabled")):
            count += sum(line.startswith("cup=") for line in manifest.read_text().splitlines())
    return count


def enable_all_packs(folder):
    folder.mkdir(exist_ok=True)
    for manifest in (ROOT / "assets/track-packs").glob("*.ini"):
        (folder / (manifest.stem + ".disabled")).write_text("0\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--stock", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--all-track-packs", action="store_true")
    args = parser.parse_args()
    cup_count = enabled_cup_count(args.all_track_packs)
    build, stock, out = args.build.resolve(), args.stock.resolve(), args.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    clean = {k: v for k, v in os.environ.items()
             if not k.startswith(("FZERO_", "SNESRECOMP_", "SDL_", "LNG_"))}
    original = stock.read_bytes()
    art = [original] + [apply_ips(original, (ROOT / f"assets/vehicle-packs/cgp-p{g}.ips").read_bytes()) for g in (1, 2, 3)]
    groups, slots = [0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 3, 3], [0, 1, 2, 3, 0, 1, 2, 3, 1, 3, 0, 2]
    cases = []
    for i, identity in enumerate(ORDER):
        route = player_route(identity)
        cases.append(dict(name="ship-" + NAMES[identity], route=route, frames=570, identity=identity))
        cases.append(dict(name="race-" + NAMES[identity], identity=identity, frames=1700,
                          route=route + ",600-606:8,730-736:8,900-906:8,960-966:8,1300-1699:1"))
    for mask in range(7):
        for identity in ROSTERS[mask]:
            cases.append(dict(name=f"partial-{mask}-" + NAMES[identity],
                              route=player_route(identity, mask), frames=570,
                              packs=mask, rebalance=15 if not mask else 0, identity=identity))
    cases += [dict(name="right-wrap", route="320-326:8,400-403:128,430-433:128,460-463:128", frames=570, identity=0),
              dict(name="left-wrap", route="320-326:8,400-403:64", frames=570, identity=4),
              dict(name="select-all", route="320-326:8" + "".join(press(400 + i * 14, 4) for i in range(11)), frames=600, identity=7)]
    base = "320-326:8,440-446:8,560-566:8"
    cases += [dict(name="all-leagues", route=base + "".join(press(650 + i * 16, 32) for i in range(cup_count)),
                   frames=700 + cup_count * 16, leagues=list(range(2, cup_count + 1)) + [1]),
              dict(name="reverse-leagues", route=base + press(650, 16), frames=700, leagues=[cup_count]),
              dict(name="horizontal-leagues", route=base + press(650, 128) + press(670, 64), frames=710, leagues=[]),
              dict(name="held-leagues", route=base + ",650-710:32", frames=730, leagues=list(range(2, 10)))]
    results = {}

    def run(case):
        folder = out / case["name"]
        folder.mkdir()
        shutil.copytree(ROOT / "assets", folder / "assets")
        if args.all_track_packs:
            enable_all_packs(folder / "packs")
        env = dict(clean, FZERO_TRACK_PACKS=str(folder / "packs"), FZERO_DELUXE_DATA="embedded",
                   FZERO_BS_CARS="0", FZERO_BS_TRACKS="0", FZERO_CGP_CARS=str(case.get("packs", 7)),
                   FZERO_CGP_REBALANCE=str(case.get("rebalance", 0)), FZERO_RULES="", FZERO_CUP="bs-deluxe/knight",
                   SNESRECOMP_SAVE_ROOT="s", SNESRECOMP_INPUT_SCRIPT=case["route"],
                   SNESRECOMP_WRAM_DUMP=str(folder / "ram.bin"), SNESRECOMP_FRAME_DUMP=str(folder / "frame.ppm"),
                   FZERO_CART_DUMP=str(folder / "cart.bin"))
        proc = subprocess.run([str(build / "FZeroSNESRecompHeadless.exe"), str(stock), str(case["frames"])],
                              cwd=folder, env=env, capture_output=True, text=True, timeout=180)
        log = proc.stdout + proc.stderr
        (folder / "run.log").write_text(log, encoding="utf-8")
        assert proc.returncode == 0, (case["name"], log[-2000:])
        ram = (folder / "ram.bin").read_bytes()
        if "identity" in case:
            assert ram[0x14dff] == case["identity"], (case["name"], ram[0x14dff])
            if case["frames"] == 570:
                roster = ROSTERS[case.get("packs", 7)]
                page = roster.index(case["identity"]) // 4
                column = (ram[0x14c84] & 4) // 4
                cart = (folder / "cart.bin").read_bytes()
                for row in range(4):
                    index = page * 4 + row
                    expected = NAMES[roster[index]].replace("-", " ").upper() if index < len(roster) else ""
                    if expected == "P EMERALD": expected = "P_ EMERALD"
                    start = 0xf5b20 + (column * 4 + row) * 16
                    actual = cart[start:start + 15].decode("ascii").strip()
                    assert actual == expected, (case["name"], row, actual, expected)
                    position = column * 4 + row
                    if index < len(roster):
                        member = roster[index]
                        group = groups[member] or ((2 if member in (0, 2) else 3) if case.get("rebalance", 0) & (1 << member) else 0)
                        source = art[group]
                        start = 0x40000 + slots[member] * 0x8000
                        pixels = source[start:start + 0x8000]
                        normal = source[0x7cd80 + slots[member] * 32:][:32]
                        dim = source[0x76180 + slots[member] * 32:][:32]
                    else:
                        pixels, normal, dim = bytes(0x8000), bytes(32), bytes(32)
                    assert cart[0x300000 + position * 0x8000:][:0x8000] == pixels, (case["name"], row, "column artwork")
                    assert cart[0x340000 + position * 32:][:32] == normal, (case["name"], row, "selected palette")
                    assert cart[0x340100 + position * 32:][:32] == dim, (case["name"], row, "dim palette")
        if case["name"].startswith("race-"):
            assert ram[0x54:0x56] == bytes([2, 3]), (case["name"], ram[0x54:0x59].hex())
        if "leagues" in case:
            import re
            visited = [int(n) for n in re.findall(rf"\[track-library\] menu (\d+)/{cup_count}:", log)]
            assert visited == case["leagues"], (case["name"], visited, case["leagues"])
        results[case["name"]] = dict(identity=ram[0x14dff], scene=list(ram[0x54:0x57]))
        print(case["name"], "PASS", flush=True)

    with ThreadPoolExecutor(max_workers=3) as pool:
        list(pool.map(run, cases))
    (out / "validation.json").write_text(json.dumps(results, indent=2) + "\n")


if __name__ == "__main__":
    main()
