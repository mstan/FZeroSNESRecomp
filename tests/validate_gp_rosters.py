"""Check the actual GP starting field against the authored four-ship groups.

Requires an owner-supplied stock ROM. Captures and ROM-derived data stay in
--out. Practice freedom is covered separately by validate_practice_catalog.py.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

from validate_vehicles import NAMES, GROUPS, SLOTS, FIELDS, ROUTE, authored_stats

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from inspect_bs_deluxe import apply_ips

COHORTS = [[0, 1, 2, 3], [4, 5, 6, 7], [0, 8, 2, 9], [10, 1, 11, 3]]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--stock", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    build, stock, out = args.build.resolve(), args.stock.resolve(), args.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    original = stock.read_bytes()
    stats = [original] + [authored_stats(g, original) for g in (1, 2, 3)]
    art = [original] + [apply_ips(original, (ROOT / f"assets/vehicle-packs/cgp-p{g}.ips").read_bytes())
                        for g in (1, 2, 3)]
    clean = {k: v for k, v in os.environ.items()
             if not k.startswith(("FZERO_", "SNESRECOMP_", "SDL_", "LNG_"))}
    # Every member with all packs, then retail members across partial installs,
    # then rebalances without additions. No CPU should leak out of a disabled pack.
    cases = [(i, 7, 0) for i in range(12)]
    cases += [(i, mask, 0) for mask in range(1, 7) for i in range(4)]
    cases += [(i, 0, 15) for i in range(4)] + [(i, 7, 15) for i in range(4)]
    results = {}

    def run(case):
        player, packs, rebalance = case
        name = f"{NAMES[player]}-packs{packs}-rebalance{rebalance}"
        folder = out / name
        folder.mkdir()
        shutil.copytree(ROOT / "assets", folder / "assets")
        env = dict(clean, FZERO_DELUXE_DATA="embedded", FZERO_BS_CARS="0", FZERO_BS_TRACKS="0",
                   FZERO_CGP_CARS=str(packs), FZERO_CGP_REBALANCE=str(rebalance), FZERO_RULES="",
                   FZERO_TEST_VEHICLE=NAMES[player], FZERO_CUP="cgp/knight-cgp", FZERO_TRACK_PACKS="packs",
                   SNESRECOMP_SAVE_ROOT="s", SNESRECOMP_INPUT_SCRIPT=ROUTE,
                   FZERO_TEST_WRAM_TRACE=str(folder / "trace.bin"), FZERO_CART_DUMP=str(folder / "cart.bin"))
        if packs == 7:
            env.update(FZERO_REWIND_TEST="1", FZERO_VEHICLE_CROSS_STATE="1")
        proc = subprocess.run([str(build / "FZeroSNESRecompHeadless.exe"), str(stock), "1600"],
                              cwd=folder, env=env, capture_output=True, text=True, timeout=240)
        log = proc.stdout + proc.stderr
        (folder / "run.log").write_text(log, encoding="utf-8")
        assert proc.returncode == 0, (name, log[-3000:])
        if packs == 7:
            assert "rewind: actual ring restore and ten-frame resimulation identical" in log, name
        cart = (folder / "cart.bin").read_bytes()
        identity_group = GROUPS[player] or (2 if player in (0, 2) else 3)
        use_donor = packs & (1 << (identity_group - 1))
        expected = COHORTS[identity_group] if use_donor else COHORTS[0]
        composed = [cart[0xf00ff + 256 * slot] for slot in range(4)]
        assert composed == expected, (name, composed, expected)
        # Check each rival's matching art and handling, not only group labels.
        for slot, identity in enumerate(expected):
            source = GROUPS[identity]
            if identity < 4 and rebalance & (1 << identity):
                source = 2 if identity in (0, 2) else 3
            handling = b"".join(stats[source][address-0x8000+slot*width:address-0x8000+(slot+1)*width]
                                for address, width in FIELDS)
            assert cart[0xf0047+slot*256:0xf0047+slot*256+len(handling)] == handling, (name, identity, "stats")
            start = 0x40000 + slot * 0x8000
            assert cart[start:start+0x5000] == art[source][start:start+0x5000], (name, identity, "art")
        trace = (folder / "trace.bin").read_bytes()
        opening = next((trace[i:i+8192] for i in range(0, len(trace), 8192)
                        if trace[i+0x54:i+0x56] == bytes([2, 3])), None)
        assert opening is not None and not opening[0x58], (name, "not Grand Prix")
        player_slot = opening[0x52]
        cpu_slots = [opening[0xd71 + actor] for actor in (2, 4, 6)]
        assert player_slot == SLOTS[player] and sorted(cpu_slots + [player_slot]) == [0, 1, 2, 3], (name, cpu_slots)
        rivals = [composed[slot] for slot in cpu_slots]
        assert set(rivals) == set(expected) - {player}, (name, rivals)
        results[name] = dict(player=NAMES[player], rivals=[NAMES[i] for i in rivals], rewind=packs == 7)
        print(name, "PASS", flush=True)

    with ThreadPoolExecutor(max_workers=3) as pool:
        list(pool.map(run, cases))
    assert stock.read_bytes() == original
    (out / "validation.json").write_text(json.dumps(results, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
