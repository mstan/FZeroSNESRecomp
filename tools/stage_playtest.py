"""Stage a private local playtest with the complete built launcher assets.

Optional --profile copies local user settings/saves. Such a directory is for
the owner's testing only; use make_release.py for a redistributable archive.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--profile", type=Path)
    parser.add_argument("--exe", default="FZeroSNESRecomp55.exe")
    args = parser.parse_args()
    build, out = args.build.resolve(), args.output.resolve()
    if Path(args.exe).name != args.exe:
        parser.error("--exe must be a filename")
    required = [args.exe, "assets/fonts/LatoLatin-Regular.ttf", "assets/fonts/LatoLatin-Bold.ttf",
                "assets/fonts/NotoSansSymbols2-Regular.ttf", "assets/fonts/OpenMoji-black-glyf.ttf",
                "assets/img/brand_mark.tga", "assets/img/pad.tga", "assets/img/boxart.tga"]
    for name in required:
        if not (build / name).is_file():
            parser.error(f"Incomplete build: missing {name}. Build the desktop target before staging.")
    if args.profile and not args.profile.is_dir():
        parser.error("--profile directory does not exist")
    out.mkdir(parents=True, exist_ok=False)
    shutil.copy2(build / args.exe, out / args.exe)
    for dll in build.glob("*.dll"):
        shutil.copy2(dll, out / dll.name)
    # Source-tree assets omit the shared UI's fonts, branding and controller.
    # CMake assembles the complete runtime asset tree beside the executable.
    shutil.copytree(build / "assets", out / "assets")
    if args.profile:
        for name in ("config.ini", "fzero-video.ini", "keybinds.ini", "input.ini", "rom.cfg"):
            source = args.profile / name
            if source.is_file():
                shutil.copy2(source, out / name)
        for name in ("mods", "saves"):
            source = args.profile / name
            if source.is_dir():
                shutil.copytree(source, out / name)
    # Keep the copied player's settings, but refresh instructions for this build.
    for directory in ("mods", "mods/track-packs"):
        for name in ("README.md", "PARSE_MANIFEST.md"):
            source = build / directory / name
            if source.is_file():
                (out / directory).mkdir(parents=True, exist_ok=True)
                shutil.copy2(source, out / directory / name)
    hashes = {}
    for name in required:
        digest = hashlib.sha256((out / name).read_bytes()).hexdigest()
        assert digest == hashlib.sha256((build / name).read_bytes()).hexdigest(), name
        hashes[name] = digest
    (out / "playtest-build.json").write_text(json.dumps(hashes, indent=2) + "\n", encoding="utf-8")
    print(out)


if __name__ == "__main__":
    main()
