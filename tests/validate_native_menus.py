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

ROOT = Path(__file__).resolve().parents[1]
ORDER = [0, 2, 1, 3, 4, 6, 5, 7, 8, 9, 10, 11]
NAMES = ["blue-falcon", "wild-goose", "golden-fox", "fire-stingray",
         "moon-shadow", "dragon-bird", "great-star", "death-anchor",
         "p-emerald", "black-bull", "white-cat", "red-gazelle"]


def press(frame, button):
    return f",{frame}-{frame + 3}:{button}"


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
    cases = []
    for i, identity in enumerate(ORDER):
        route = "320-326:8"
        for page in range(i // 4):
            route += press(400 + page * 30, 128)
        for row in range(i % 4):
            route += press(480 + row * 20, 32)
        cases.append(dict(name="ship-" + NAMES[identity], route=route, frames=570, identity=identity))
        cases.append(dict(name="race-" + NAMES[identity], identity=identity, frames=1700,
                          route=route + ",600-606:8,730-736:8,900-906:8,960-966:8,1300-1699:1"))
    for mask in range(1, 8):
        roster = [i for i in ORDER if i < 4 or mask & (1 << (0 if i < 8 else 1 if i < 10 else 2))]
        route = "320-326:8,400-403:64,450-453:32,480-483:32,510-513:32"
        cases.append(dict(name=f"partial-{mask}", route=route, frames=570, packs=mask, identity=roster[-1]))
    cases += [dict(name="right-wrap", route="320-326:8,400-403:128,430-433:128,460-463:128", frames=570, identity=0),
              dict(name="left-wrap", route="320-326:8,400-403:64", frames=570, identity=8),
              dict(name="select-all", route="320-326:8" + "".join(press(400 + i * 14, 4) for i in range(11)), frames=600, identity=11)]
    base = "320-326:8,440-446:8,560-566:8"
    cases += [dict(name="all-leagues", route=base + "".join(press(650 + i * 16, 32) for i in range(14)),
                   frames=950, leagues=list(range(2, 15)) + [1]),
              dict(name="reverse-leagues", route=base + press(650, 16), frames=700, leagues=[14]),
              dict(name="horizontal-leagues", route=base + press(650, 128) + press(670, 64), frames=710, leagues=[]),
              dict(name="held-leagues", route=base + ",650-710:32", frames=730, leagues=list(range(2, 10)))]
    results = {}

    def run(case):
        folder = out / case["name"]
        folder.mkdir()
        shutil.copytree(ROOT / "assets", folder / "assets")
        env = dict(clean, FZERO_TRACK_PACKS=str(folder / "packs"), FZERO_DELUXE_DATA="embedded",
                   FZERO_BS_CARS="0", FZERO_BS_TRACKS="0", FZERO_CGP_CARS=str(case.get("packs", 7)),
                   FZERO_CGP_REBALANCE="0", FZERO_RULES="", FZERO_CUP="bs-deluxe/knight",
                   SNESRECOMP_SAVE_ROOT="s", SNESRECOMP_INPUT_SCRIPT=case["route"],
                   SNESRECOMP_WRAM_DUMP=str(folder / "ram.bin"), SNESRECOMP_FRAME_DUMP=str(folder / "frame.ppm"))
        proc = subprocess.run([str(build / "FZeroSNESRecompHeadless.exe"), str(stock), str(case["frames"])],
                              cwd=folder, env=env, capture_output=True, text=True, timeout=180)
        log = proc.stdout + proc.stderr
        (folder / "run.log").write_text(log, encoding="utf-8")
        assert proc.returncode == 0, (case["name"], log[-2000:])
        ram = (folder / "ram.bin").read_bytes()
        if "identity" in case:
            assert ram[0x14dff] == case["identity"], (case["name"], ram[0x14dff])
        if case["name"].startswith("race-"):
            assert ram[0x54:0x56] == bytes([2, 3]), (case["name"], ram[0x54:0x59].hex())
        if "leagues" in case:
            import re
            visited = [int(n) for n in re.findall(r"\[track-library\] menu (\d+)/14:", log)]
            assert visited == case["leagues"], (case["name"], visited, case["leagues"])
        results[case["name"]] = dict(identity=ram[0x14dff], scene=list(ram[0x54:0x57]))
        print(case["name"], "PASS", flush=True)

    with ThreadPoolExecutor(max_workers=3) as pool:
        list(pool.map(run, cases))
    (out / "validation.json").write_text(json.dumps(results, indent=2) + "\n")


if __name__ == "__main__":
    main()
