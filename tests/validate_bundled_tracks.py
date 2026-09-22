"""Qualify staged track defaults with an empty user directory and the owner's ROM."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--stock", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    build, stock, out = args.build.resolve(), args.stock.resolve(), args.out.resolve()
    original = stock.read_bytes()
    out.mkdir(parents=True, exist_ok=False)
    registry = out / "assets/track-packs"
    shutil.copytree(build / "assets/track-packs", registry)
    expected = {
        "max-league.ips": "8b1b4ee5abc2ea0eee180e539cf56cce6fa583ed1be17fb6f8c5715d63089716",
        "cgp.ips": "4ad667cda0841eb15c849f49ea22e2254d7852475a4e9c168da00974c1dc0d8e",
    }
    for name, digest in expected.items():
        assert hashlib.sha256((registry / name).read_bytes()).hexdigest() == digest
    assert (registry / "MAX-League-credits.txt").is_file()
    assert (registry / "CGP-credits.txt").is_file()
    hidden = {p.stem for p in registry.glob("*.hidden") if p.read_text(encoding="utf-8").strip() == "1"}
    assert not any(p.suffix.lower() in (".msu", ".pcm", ".sfc", ".smc") for p in registry.rglob("*"))
    user = out / "mods/track-packs"
    user.mkdir(parents=True)
    env = {k: v for k, v in os.environ.items()
           if not k.startswith(("FZERO_", "SNESRECOMP_", "SDL_", "LNG_"))}
    env.update(FZERO_TRACK_PACKS=str(user), FZERO_DELUXE_DATA="embedded", FZERO_ASPECT="21:9")
    route = "320-326:8,440-446:8,560-566:8,730-736:8,790-796:8,1160-1599:1"
    menu_route = "320-326:8,440-446:8,560-566:8,650-652:64"
    results = {}

    def run(name, enabled, cups=None, cup="", deluxe=True, configure=True):
        if configure:
            for pack in ("max-league", "cgp"):
                (user / f"{pack}.disabled").write_text("0\n" if pack in enabled else "1\n", encoding="utf-8")
        enabled = enabled - hidden
        folder = out / name
        folder.mkdir()
        frames = 700 if cups else 1600
        run_env = dict(env, FZERO_CUP=cup, FZERO_DELUXE_DATA="embedded" if deluxe else "",
                       SNESRECOMP_INPUT_SCRIPT=menu_route if cups else route,
                       SNESRECOMP_WRAM_DUMP=str(folder / "ram.bin"),
                       SNESRECOMP_SAVE_ROOT=f"{name}/saves")
        result = subprocess.run([str(build / "FZeroSNESRecompHeadless.exe"), str(stock), str(frames)],
                                cwd=out, env=run_env, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, text=True, encoding="utf-8", timeout=180)
        log = result.stdout
        (folder / "run.log").write_text(log, encoding="utf-8")
        assert result.returncode == 0 and "fzero_native: PASS" in log, log[-4000:]
        for pack, count in (("max-league", 5), ("cgp", 30)):
            assert log.count(f"extracted {pack}: {count} courses") == (pack in enabled), (name, pack)
        if cups:
            assert f"menu {cups}/{cups}:" in log, log[-4000:]
        else:
            ram = (folder / "ram.bin").read_bytes()
            assert ram[0x54:0x56] == b"\x02\x03", (name, ram[0x54:0x57].hex())
        results[name] = {"enabled": sorted(enabled), "cups": cups, "deluxe": deluxe, "frames": frames}
        print(name, "PASS", flush=True)

    both = {"max-league", "cgp"}
    cup_count = 11 if "max-league" in hidden else 12
    run("fresh", both, cups=cup_count, configure=False)
    (user / "library.disabled").write_text("1\n", encoding="utf-8")
    run("old-setting", both, cups=cup_count)
    run("max-only", {"max-league"}, cups=None if "max-league" in hidden else 6)
    run("cgp-only", {"cgp"}, cups=11)
    run("native-bs", set(), cup="bs-deluxe/knight")
    run("native-stock", set(), cup="retail/knight", deluxe=False)
    run("stock-packs", both, cups=cup_count-2, deluxe=False)
    if "max-league" not in hidden:
        run("max-race", both, cup="max-league/max")
    run("cgp-race", both, cup="cgp/cgp-1")
    for name in expected:
        shutil.copy2(registry / name, user / ("duplicate-" + name))
    run("duplicates", both, cups=cup_count)
    assert stock.read_bytes() == original
    (out / "validation.json").write_text(json.dumps({"stock_unchanged": True,
        "bundled_sha256": expected, "hidden": sorted(hidden), "cases": results}, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
