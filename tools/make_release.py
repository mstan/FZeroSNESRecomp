"""Stage a ROM-free Windows x64 release and resolve its runtime DLL closure.

Build Release first. Requires MSYS2 objdump; never edits or deletes source/build
directories. Refuses to reuse a staging directory, preventing stale payloads.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument("--build", default="build-release")
p.add_argument("--mingw", default="C:/msys64/mingw64")
p.add_argument("--output", default="release-stage", help="Parent for a fresh versioned staging directory")
p.add_argument("--label", default="", help="Optional local build label, such as fzero-55")
p.add_argument("--exe", default="FZeroSNESRecomp.exe", help="Desktop executable filename in the build directory")
p.add_argument("--deluxe-mods", type=Path, default=ROOT / "captures/bs-deluxe/mods",
               help="Imported Deluxe directory; its credits and provenance ship with the build")
a = p.parse_args()
# BS Deluxe ships in every download, and that is allowed: it is a ROM hack,
# included with its authors' permission (GuyPerfect, Porthor, PowerPanda). Only
# OFFICIAL copyrighted assets are withheld from a release -- the commercial ROM
# and anything generated from it, which is what the src/gen check below and the
# ROM-free staging policy are for.
#
# This used to refuse to package unless the GitHub repository was private. That
# guard encoded a distribution assumption rather than a licence term, and it
# went stale the moment the repository was made public: releases had already
# shipped the payload publicly, so the check was asserting something untrue
# while hard-blocking every future release. Do not reinstate it. The payload
# INTEGRITY checks immediately below are a different thing and must stay.
version = (ROOT / "VERSION").read_text().strip()
if not re.fullmatch(r"\d+\.\d+\.\d+", version):
    raise SystemExit("Invalid VERSION")
if a.label and not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9.-]*", a.label):
    raise SystemExit("Invalid release label")
if Path(a.exe).name != a.exe or not a.exe.lower().endswith(".exe"):
    raise SystemExit("Expected an executable filename")
build, mingw = ROOT / a.build, Path(a.mingw)
cache = {}
for line in (build / "CMakeCache.txt").read_text(encoding="utf-8").splitlines():
    match = re.match(r"([^:#/][^:]*):[^=]+=(.*)", line)
    if match:
        cache[match[1]] = match[2]
dependency_roots = {"snesrecomp": Path(cache.get("SNESRECOMP_ROOT", ROOT / "snesrecomp")),
                    "recomp-ui": Path(cache.get("RECOMP_UI_ROOT", ROOT / "recomp-ui"))}
exe = build / a.exe
image = exe.read_bytes()
release_version = version + ("-" + a.label if a.label else "")
if cache.get("SNESRECOMP_BUILD_VERSION") != release_version:
    raise SystemExit(f"Build version does not match {release_version}; reconfigure and rebuild")
if release_version.encode() not in image:
    raise SystemExit("Executable does not contain the full release version")
deluxe_mods = a.deluxe_mods if (a.deluxe_mods / "bs-deluxe-import.json").exists() else build / "mods"
metadata = json.loads((deluxe_mods / "bs-deluxe-import.json").read_text())
# The payload is what ships, so it has to be inside the binary - together with
# the target digest the loader verifies the patched cartridge against.
if b"BSDELX1" not in image:
    raise SystemExit("Executable has no embedded BS Deluxe payload; "
                     "configure with FZERO_DELUXE_GEN_DIR and rebuild")
if bytes.fromhex(metadata["target_sha256"]) not in image:
    raise SystemExit("Embedded BS Deluxe payload is not the expected version")
if hashlib.sha256((deluxe_mods / "bs-deluxe.dat").read_bytes()).hexdigest() != metadata["delta_sha256"]:
    raise SystemExit("Deluxe payload digest does not match import metadata")
for source in Path(cache.get("FZERO_GEN_DIR", ROOT / "src/gen")).glob("*.c"):
    if "rtl_aot_node_denied(" in source.read_text(encoding="utf-8"):
        raise SystemExit("Regenerate without the AOT deny gate before packaging")
name = f"FZeroSNESRecomp-{release_version}-windows-x64"
stage = ROOT / a.output / name
stage.mkdir(parents=True, exist_ok=False)
shutil.copy2(exe, stage / "FZeroSNESRecomp.exe")
# Build trees can contain privately imported shaders; never redistribute them.
shutil.copytree(build / "assets", stage / "assets", ignore=shutil.ignore_patterns("shaders"))
shutil.copytree(ROOT / "assets/shaders", stage / "assets/shaders")
patches = ROOT / "patches"
if patches.is_dir():
    shutil.copytree(patches, stage / "patches")
for filename in ("README.md", "CHANGELOG.md", "VERSION", "LICENSE"):
    shutil.copy2(ROOT / filename, stage / filename)
(stage / "docs").mkdir()
shutil.copy2(ROOT / "docs/ADAPTIVE_RENDERER.md", stage / "docs/ADAPTIVE_RENDERER.md")
shutil.copy2(ROOT / "docs/BS_DELUXE_EXPLORATION.md", stage / "docs/BS_DELUXE_EXPLORATION.md")
shutil.copy2(ROOT / "docs/SAVE_STATES.md", stage / "docs/SAVE_STATES.md")
shutil.copy2(ROOT / "docs/HD_MODE7.md", stage / "docs/HD_MODE7.md")
shutil.copy2(ROOT / "docs/HD_MODE7_PERFORMANCE.md", stage / "docs/HD_MODE7_PERFORMANCE.md")
shutil.copy2(ROOT / "docs/PERFORMANCE_DIAGNOSTICS.md", stage / "docs/PERFORMANCE_DIAGNOSTICS.md")
screenshots = ROOT / "docs/screenshots"
if screenshots.is_dir():
    shutil.copytree(screenshots, stage / "docs/screenshots")
shutil.copy2(ROOT / "assets/README.md", stage / "assets/README.md")
# Credits and provenance still ship as files. The payload itself does not: a
# copy beside the executable is only a development override, and a stale one
# would be tried ahead of the embedded bytes.
(stage / "mods").mkdir()
for filename in ("bs-deluxe-import.json", "BS-Deluxe-credits.txt"):
    shutil.copy2(deluxe_mods / filename, stage / "mods" / filename)
(stage / "mods/track-packs").mkdir()
for filename in ("README.md", "PARSE_MANIFEST.md"):
    shutil.copy2(ROOT / "mods" / filename, stage / "mods" / filename)
    text = (ROOT / "mods" / filename).read_text(encoding="utf-8")
    (stage / "mods/track-packs" / filename).write_text(text.replace("(cgp-source/README.md)", "(../cgp-source/README.md)"), encoding="utf-8")
shutil.copytree(ROOT / "mods/cgp-source", stage / "mods/cgp-source")
shutil.copy2(ROOT / "docs/ADDITIVE_TRACK_PACKS.md", stage / "docs/ADDITIVE_TRACK_PACKS.md")
(stage / "README.txt").write_text(
    f"FZeroSNESRecomp {release_version} - Windows x64\n\n"
    "Extract the entire ZIP and run FZeroSNESRecomp.exe. Select your own\n"
    "F-Zero (USA) ROM in the launcher. No ROM is included.\n\n"
    "Settings > Display contains aspect choices and shader presets including\n"
    "CRT Soft. Shaders start OFF (None). Browse imports a custom .glslp or\n"
    ".glsl shader; selecting one uses the OpenGL presentation path.\n\n"
    "For MSU-1, put the supported Conn/Cubear v11 f-zero_msu1.ips beside\n"
    "your pack's numbered PCM tracks, then enable MSU-1 and choose that\n"
    "folder in Settings > Sound. Keep using your unmodified USA ROM.\n"
    "The MSU patch, music and CRT-Geom shader are not bundled. See README.md\n"
    "for import instructions and save-state/audio limitations.\n\n"
    "Mods defaults to Widescreen at Fit, which follows the\n"
    "window between 4:3 and 32:9, Presentation FPS at Auto, and BS vehicles.\n"
    "Turn any of them off in Mods, or choose a fixed aspect or rate there.\n\n"
    "HD Mode 7 starts off. Enable it in Mods for sharper tracks at 2x through 10x.\n"
    "Start at 2x. Above 4x can cause severe slowdown; use at your own risk.\n"
    "Cars and HUD keep their original pixel artwork.\n"
    "See docs/HD_MODE7.md for details.\n\n"
    "Diagnostics starts off. Enable Mods > Diagnostics, reproduce a slowdown,\n"
    "then attach the newest diagnostics/performance-*.jsonl file to your report.\n"
    "Logs stay local and include no ROM or save data.\n"
    "See docs/PERFORMANCE_DIAGNOSTICS.md for details.\n\n"
    "BS vehicles and original BS tracks have independent switches. The engine keeps\n"
    "its saves apart under saves/bs-deluxe. It is included with permission\n"
    "from its authors: GuyPerfect, Porthor, and PowerPanda. The SNES patch is\n"
    "at patches/bs-deluxe-usa.ips for your own ROM, and\n"
    "mods/BS-Deluxe-credits.txt lists machines, leagues and alternate controls.\n\n"
    "Community Grand Prix adds 30 new, 10 corrected BS and 15 revised original\n"
    "courses. With untouched originals, that is 14 cups / 70 course versions.\n"
    "CGP and original BS tracks are mutually exclusive; cars are independent.\n"
    "Required course fixes apply automatically; optional rules stay opt-in.\n"
    "Three optional CGP car packs combine into twelve identities; four retail\n"
    "rebalances are separate options. Stock BS cars exclude CGP car options.\n"
    "All CGP vehicle options start off. No music is included.\n"
    "Enable or disable CGP directly in Mods. MAX League is retained but\n"
    "hidden and disabled in this branch. Bundled IPS patches and attribution\n"
    "are under assets/track-packs; no MSU audio is included.\n"
    "Other IPS/BPS course packs go in mods/track-packs; each enabled pack adds\n"
    "its cups to the in-game Grand Prix menu. See mods/README.md.\n\n"
    "F7 or Select+R opens the save-state menu: 12 slots with thumbnails,\n"
    "A loads, X saves, B or Escape backs out. Stock and BS Deluxe keep\n"
    "separate slots and a state from the other one is refused, not loaded.\n\n"
    "R or Select+L opens rewind, which is enabled by default. Disable it\n"
    "or adjust its depth and interval in the launcher's Settings. Left and\n"
    "Right scrub, A or Enter jumps there, B or Escape leaves.\n"
    "Both keys are rebindable on the launcher's Controls page.\n\n"
    "Arrows: steer; Z: accelerate; X: A; Enter: Start.\n"
    "D: left shoulder; C: right shoulder (F-Zero keyboard defaults).\n"
    "Ctrl+F6: aspect; Ctrl+F7: enable/cycle FPS; Alt+Enter: fullscreen.\n"
    "P: pause; Ctrl+R: reset.\n"
    "Shift+F1..F12 save and F1..F12 load a slot directly. F7 belongs\n"
    "to the save-state menu unless rebound; F8 is a normal quick slot.\n\n"
    "See README.md and CHANGELOG.md for more details.\n",
    encoding="utf-8")

pending, seen = [stage / "FZeroSNESRecomp.exe"], set()
system = Path(os.environ.get("SystemRoot", "C:/Windows")) / "System32"
while pending:
    binary = pending.pop()
    imports = subprocess.check_output([str(mingw / "bin/objdump.exe"), "-p", str(binary)], text=True)
    for dll in re.findall(r"DLL Name:\s*(\S+)", imports):
        key = dll.lower()
        if key in seen:
            continue
        seen.add(key)
        if key.startswith(("api-ms-", "ext-ms-")) or (system / dll).is_file():
            continue
        source = next((directory / dll for directory in (build, mingw / "bin")
                       if (directory / dll).is_file()), None)
        if source is None:
            raise SystemExit(f"Unresolved runtime DLL: {dll}")
        target = stage / dll
        shutil.copy2(source, target)
        pending.append(target)

notices = stage / "licenses"
notices.mkdir()
for label, source in {
    "snesrecomp": dependency_roots["snesrecomp"] / "LICENSE",
    "recomp-ui": dependency_roots["recomp-ui"] / "LICENSE",
    "imgui": dependency_roots["recomp-ui"] / "src/third_party/imgui/LICENSE.txt",
}.items():
    shutil.copy2(source, notices / (label + ".txt"))
for package in ("gcc-libs", "libiconv", "libwinpthread", "winpthreads", "SDL3", "crt", "headers"):
    source = mingw / "share/licenses" / package
    if source.is_dir():
        shutil.copytree(source, notices / package)

# Preserve copyright and license records embedded in the bundled font files.
for font in (stage / "assets/fonts").glob("*.ttf"):
    data = font.read_bytes()
    records = []
    for i in range(struct.unpack_from(">H", data, 4)[0]):
        tag, _, offset, _ = struct.unpack_from(">4sIII", data, 12 + 16 * i)
        if tag != b"name":
            continue
        _, count, strings = struct.unpack_from(">HHH", data, offset)
        for j in range(count):
            platform, _, _, name_id, length, location = struct.unpack_from(">6H", data, offset + 6 + 12*j)
            if name_id not in (0, 7, 8, 9, 13, 14):
                continue
            raw = data[offset + strings + location:offset + strings + location + length]
            text = raw.decode("utf-16-be" if platform in (0, 3) else "mac_roman", errors="replace")
            if text not in records:
                records.append(text)
    (notices / (font.stem + ".txt")).write_text("\n\n".join(records), encoding="utf-8")

git = shutil.which("git")
if git is None:
    raise SystemExit("Git is required to record release source and dependency pins")
commit = subprocess.check_output([git, "rev-parse", "HEAD"], cwd=ROOT, text=True).strip()
pins = {name: subprocess.check_output([git, "rev-parse", "HEAD"], cwd=root, text=True).strip()
        for name, root in dependency_roots.items()}
for name, pin in pins.items():
    recorded = subprocess.check_output([git, "ls-tree", "HEAD", name], cwd=ROOT, text=True).split()
    if len(recorded) < 3 or recorded[2] != pin:
        raise SystemExit(f"Build dependency {name} does not match committed submodule pin")
manifest = {"version": version, "label": a.label, "commit": commit, "dependencies": pins, "files": {}}
for path in sorted(stage.rglob("*")):
    if path.is_file():
        if (path.suffix.lower() in (".sfc", ".smc", ".srm", ".sav", ".bin", ".c", ".pcm", ".msu")
                or path.name.lower() in ("config.ini", "rom.cfg", "fzero-video.ini", "f-zero_msu1.ips")
                or path.name.lower().startswith("crt-geom")):
            raise SystemExit(f"Forbidden payload: {path}")
        manifest["files"][path.relative_to(stage).as_posix()] = hashlib.sha256(path.read_bytes()).hexdigest()
(stage / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
archive = stage.parent / (stage.name + ".zip")
with zipfile.ZipFile(archive, "x", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
    for path in sorted(stage.rglob("*")):
        if path.is_file():
            z.write(path, path.relative_to(stage.parent))
digest = hashlib.sha256(archive.read_bytes()).hexdigest()
archive.with_suffix(".zip.sha256").write_text(f"{digest}  {archive.name}\n", encoding="ascii")
print(f"{archive}\nSHA256 {digest}\nSource {commit}")
