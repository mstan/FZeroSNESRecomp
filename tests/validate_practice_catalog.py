"""Private-ROM integration: native Practice navigation, rivals and course catalog.

Uses actual pad input and compares composed handling/acceleration with the
author's ASM data. Raw cartridge/frame artifacts stay exclusively in --out.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

from validate_vehicles import NAMES, GROUPS, SLOTS, FIELDS, authored_stats
from validate_native_menus import ORDER, press, enabled_cup_count, enable_all_packs

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from inspect_bs_deluxe import apply_ips


def route(player, rival):
    result = "300-306:32,320-326:8"
    index = ORDER.index(player)
    result += "".join(press(400 + i * 35, 128) for i in range(index // 4))
    result += "".join(press(480 + i * 25, 32) for i in range(index % 4))
    result += ",600-606:8,760-766:8"
    if rival < 12:
        index = ORDER.index(rival)
        result += "".join(press(900 + i * 35, 128) for i in range(index // 2))
        if index % 2:
            result += press(1100, 32)
    else:
        result += press(900, 16)  # Native No Rival beneath the icon rows.
        if rival == 254:
            result += press(950, 32)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--stock", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--filter", default="")
    parser.add_argument("--all-track-packs", action="store_true")
    args = parser.parse_args()
    cup_count = enabled_cup_count(args.all_track_packs)
    course_count = cup_count * 5
    build, stock, out = args.build.resolve(), args.stock.resolve(), args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    original = stock.read_bytes()
    stats = [original] + [authored_stats(g, original) for g in (1, 2, 3)]
    art = [original] + [apply_ips(original, (ROOT / f"assets/vehicle-packs/cgp-p{g}.ips").read_bytes())
                        for g in (1, 2, 3)]
    clean = {k: v for k, v in os.environ.items()
             if not k.startswith(("FZERO_", "SNESRECOMP_", "SDL_", "LNG_"))}
    cases = [dict(name="rival-" + NAMES[rival], player=0, rival=rival) for rival in ORDER]
    cases += [dict(name="player-" + NAMES[player], player=player, rival=ORDER[(i + 4) % 12], rewind=True)
              for i, player in enumerate(ORDER)]
    cases += [dict(name="rebalanced-" + NAMES[rival], player=(rival + 1) % 4, rival=rival, rebalance=15)
              for rival in range(4)]
    cases += [dict(name="no-rival", player=0, rival=255), dict(name="ghost", player=0, rival=254, menu=True)]
    base = route(0, 0) + ",1200-1206:8"
    cases += [dict(name="all-leagues", route=base + "".join(press(1300 + i * 20, 32) for i in range(cup_count)),
                   frames=1350 + cup_count * 20, leagues=list(range(2, cup_count + 1)) + [1]),
              dict(name="reverse-leagues", route=base + press(1300, 16), frames=1400, leagues=[cup_count]),
              dict(name="all-courses", route=base + ",1300-1306:8" +
                   "".join(press(1800 + i * 100, 32) for i in range(course_count)), frames=1850 + course_count * 100,
                   courses=[(i // 5 + 1, i % 5 + 1) for i in range(1, course_count)] + [(1, 1)]),
              dict(name="reverse-courses", route=base + ",1300-1306:8,1800-1803:16", frames=1950,
                   courses=[(cup_count, 5)])]
    for mask in range(1, 8):
        roster = [i for i in ORDER if i < 4 or mask & (1 << (GROUPS[i] - 1))]
        cases.append(dict(name=f"partial-{mask}", player=0, rival=roster[-1], packs=mask, menu=True,
                          route=route(0, 0) + "".join(press(900 + i * 20, 4) for i in range(len(roster) - 1))))
    results = {}

    def run(case):
        name = case["name"]
        if args.filter and args.filter not in name:
            return
        folder = out / name
        folder.mkdir(exist_ok=True)
        shutil.copytree(ROOT / "assets", folder / "assets", dirs_exist_ok=True)
        if args.all_track_packs:
            enable_all_packs(folder / "packs")
        player, rival = case.get("player", 0), case.get("rival", 0)
        inputs = case.get("route", route(player, rival))
        menu = case.get("menu", False)
        if "route" not in case and not menu:
            inputs += ",1200-1206:8,1300-1306:8,1700-1706:8,1900-2099:1"
        frames = case.get("frames", 1160 if menu else 2100)
        env = dict(clean, FZERO_DELUXE_DATA="embedded", FZERO_BS_CARS="0", FZERO_BS_TRACKS="0",
                   FZERO_CGP_CARS=str(case.get("packs", 7)), FZERO_CGP_REBALANCE=str(case.get("rebalance", 0)),
                   FZERO_RULES="", FZERO_CUP="bs-deluxe/knight", FZERO_TRACK_PACKS="packs",
                   SNESRECOMP_SAVE_ROOT="s", SNESRECOMP_INPUT_SCRIPT=inputs,
                   SNESRECOMP_WRAM_DUMP=str(folder / "ram.bin"),
                   SNESRECOMP_FRAME_DUMP=str(folder / "frame.ppm"))
        race = "route" not in case and not menu
        if race and rival < 12:
            env.update(FZERO_CART_DUMP=str(folder / "cart.bin"), FZERO_PRACTICE_PROBE=str(folder / "acceleration.bin"))
        if case.get("rewind"):
            env.update(FZERO_REWIND_TEST="1", FZERO_VEHICLE_CROSS_STATE="1", FZERO_TEST_SAVE_FRAME="1900")
        proc = subprocess.run([str(build / "FZeroSNESRecompHeadless.exe"), str(stock), str(frames)],
                              cwd=folder, env=env, capture_output=True, text=True, timeout=240)
        log = proc.stdout + proc.stderr
        (folder / "run.log").write_text(log, encoding="utf-8")
        assert proc.returncode == 0, (name, log[-3000:])
        ram = (folder / "ram.bin").read_bytes()
        assert ram[0x58], (name, "not Practice")
        assert ram[0x14dff] == player, (name, "player", ram[0x14dff], player)
        if "leagues" in case:
            visited = [int(n) for n in re.findall(rf"\[track-library\] menu (\d+)/{cup_count}:", log)]
            assert visited == case["leagues"], (name, visited)
        elif "courses" in case:
            visited = [(int(cup), int(course)) for cup, course in
                       re.findall(rf"\[track-library\] practice (\d+)/{cup_count} course=(\d+)", log)]
            assert visited == case["courses"], (name, visited)
            assert ram[0x54:0x57] == bytes([2, 1, 1]), (name, ram[0x54:0x57].hex())
        else:
            assert ram[0x14ce3] == rival, (name, "rival", ram[0x14ce3], rival)
        if race:
            assert ram[0x54:0x56] == bytes([2, 3]), (name, ram[0x54:0x57].hex())
            if rival < 12:
                assert ram[0xcf2] == ram[0x14ce6] != ram[0x52], (name, "physical slot collision")
                cart = (folder / "cart.bin").read_bytes()
                expected_acceleration = b""
                for who, identity in enumerate((player, rival)):
                    group, slot = GROUPS[identity], SLOTS[identity]
                    if not group and case.get("rebalance", 0) & (1 << identity):
                        group = 2 if identity in (0, 2) else 3
                    source = stats[group]
                    expected = b"".join(source[a - 0x8000 + slot * w:a - 0x8000 + (slot + 1) * w]
                                        for a, w in FIELDS)
                    record = ram[0x14d47:0x14d47 + len(expected)] if who == 0 else \
                        cart[0xf0047 + ram[0xcf2] * 256:0xf0047 + ram[0xcf2] * 256 + len(expected)]
                    assert record == expected, (name, who, "handling differs from author's ASM")
                    start = 0x14a42 + slot * 29 if group else 0x14a37 + slot * 19
                    expected_acceleration += source[start:start + (29 if group else 19)] + (b"" if group else bytes(10))
                    if who:
                        start = 0x40000 + slot * 0x8000
                        assert cart[0x350000:0x355000] == art[group][start:start + 0x5000], (name, "rival artwork")
                assert (folder / "acceleration.bin").read_bytes() == expected_acceleration, (name, "acceleration consumer")
            else:
                assert ram[0xcf2] == 255, (name, "No Rival did not reach race")
        if case.get("rewind"):
            assert "rewind: actual ring restore and ten-frame resimulation identical" in log, name
        assert "[MSU-1] enabled:" not in log, name
        results[name] = dict(player=ram[0x14dff], rival=ram[0x14ce3], scene=list(ram[0x54:0x57]), frames=frames)
        print(name, "PASS", flush=True)

    with ThreadPoolExecutor(max_workers=3) as pool:
        list(pool.map(run, cases))
    assert stock.read_bytes() == original
    (out / ("validation" + ("-" + args.filter if args.filter else "") + ".json")).write_text(
        json.dumps(results, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
