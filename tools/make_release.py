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
from import_cgp_music import verify_music

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument("--build", default="build-release")
p.add_argument("--mingw", default="C:/msys64/mingw64")
p.add_argument("--output", default="release-stage", help="Parent for a fresh versioned staging directory")
p.add_argument("--label", default="", help="Optional local build label, such as fzero-55")
p.add_argument("--exe", default="FZeroSNESRecomp.exe", help="Desktop executable filename in the build directory")
p.add_argument("--music", choices=("bundled", "external"), default="bundled",
               help="Include the approved CGP soundtrack, or retain MSU support without audio files")
p.add_argument("--packs", type=Path, required=True, help="Reviewed exported course packs; never copy arbitrary installed packs")
p.add_argument("--deluxe-mods", type=Path, default=ROOT / "captures/bs-deluxe/mods",
               help="Imported Deluxe directory; its credits and provenance ship with the build")
a = p.parse_args()
bundled_music = a.music == "bundled"
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
flavor = "with-msu" if bundled_music else "without-msu"
name = f"FZeroSNESRecomp-{release_version}-windows-x64-{flavor}"
stage = ROOT / a.output / name
stage.mkdir(parents=True, exist_ok=False)
shutil.copy2(exe, stage / "FZeroSNESRecomp.exe")
# Build trees can contain privately imported shaders; never redistribute them.
shutil.copytree(build / "assets", stage / "assets", ignore=shutil.ignore_patterns("shaders", "music", "track-packs"))
shutil.copytree(ROOT / "assets/shaders", stage / "assets/shaders")
# Release inputs are explicit: do not package the user's installation/cache.
pack_root = a.packs.resolve()
inspection = subprocess.check_output([str(build.resolve() / "FZeroInspectPacks.exe"), str(pack_root)], text=True)
for ident in ("astra-front","bower-league","cgp","max-league"):
    source=pack_root/ident
    descriptor=json.loads((source/"pack.json").read_text(encoding="utf-8"))
    if descriptor["id"]!=ident: raise SystemExit("Pack identity mismatch")
    extraction = json.loads((source / "extraction.json").read_text())
    for course, digest in extraction["record_hashes"].items():
        if f"{ident}/{course} {digest}" not in inspection:
            raise SystemExit(f"Course parity mismatch: {ident}/{course}")
    shutil.copytree(source,stage/"mods/packs"/ident,ignore=shutil.ignore_patterns(".cache","*.pcm","*.msu"))
# Only the hidden future F-Zero 55 artwork remains in the internal registry.
(stage/"assets/track-packs/presentation").mkdir(parents=True)
for filename in ("screens.txt","fzero-55.ips"):
    shutil.copy2(ROOT/"assets/track-packs/presentation"/filename,stage/"assets/track-packs/presentation"/filename)
# Reviewed CGP soundtrack only; never copy arbitrary user music from a build.
music = ROOT / "music/cgp"
if bundled_music:
    music_manifest = verify_music(music)
    shutil.copytree(music, stage / "mods/packs/cgp-audio")
    shutil.copy2(ROOT/"assets/music/cgp-pack.json",stage/"mods/packs/cgp-audio/pack.json")
(stage / "assets/music").mkdir(parents=True, exist_ok=True)
shutil.copy2(ROOT / "assets/music/cgp.json", stage / "assets/music/cgp.json")
shutil.copy2(ROOT / "assets/music/README.md", stage / "assets/music/README.md")
shutil.copy2(ROOT / "assets/music/CGP_ATTRIBUTION.md", stage / "assets/music/CGP_ATTRIBUTION.md")
if bundled_music and music_manifest.get("attribution_review", {}).get("status") == "incomplete":
    print("WARNING: soundtrack attribution is incomplete; not cleared for release. See assets/music/CGP_ATTRIBUTION.md.")
patches = ROOT / "patches"
if patches.is_dir():
    shutil.copytree(patches, stage / "patches")
for filename in ("README.md", "MODS.md", "CHANGELOG.md", "VERSION", "LICENSE"):
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
(stage / "mods").mkdir(exist_ok=True)
for filename in ("bs-deluxe-import.json", "BS-Deluxe-credits.txt"):
    shutil.copy2(deluxe_mods / filename, stage / "mods" / filename)
(stage / "mods/track-packs").mkdir()
for filename in ("README.md", "PARSE_MANIFEST.md"):
    shutil.copy2(ROOT / "mods" / filename, stage / "mods" / filename)
    text = (ROOT / "mods" / filename).read_text(encoding="utf-8")
    (stage / "mods/track-packs" / filename).write_text(text.replace("(cgp-source/README.md)", "(../cgp-source/README.md)").replace("(../docs/", "(../../docs/").replace("(../assets/", "(../../assets/").replace("(../MODS.md)", "(../../MODS.md)"), encoding="utf-8")
shutil.copytree(ROOT / "mods/cgp-source", stage / "mods/cgp-source")
shutil.copy2(ROOT / "docs/ADDITIVE_TRACK_PACKS.md", stage / "docs/ADDITIVE_TRACK_PACKS.md")
shutil.copy2(ROOT / "docs/BOWER_AND_CGP_LEAGUES.md", stage / "docs/BOWER_AND_CGP_LEAGUES.md")
shutil.copy2(ROOT / "docs/TESTER_NOTES_FZERO55.md", stage / "TESTER_NOTES.md")
shutil.copy2(ROOT / "docs/CGP_MUSIC_AND_PRESETS.md", stage / "docs/CGP_MUSIC_AND_PRESETS.md")
for filename in ("CGP_COURSE_CAPABILITIES.md", "CGP_LANDING_AUDIT.md", "CGP_SNES_MUSIC.md", "ASTRA_FRONT_IMPORT.md"):
    shutil.copy2(ROOT / "docs" / filename, stage / "docs" / filename)
shutil.copy2(ROOT/"docs/PACK_FORMAT.md",stage/"docs/PACK_FORMAT.md")
(stage/"README.txt").write_text(
    f"F-Zero Forever {release_version} - Windows x64\n\n"
    "Extract the entire ZIP and run FZeroSNESRecomp.exe. Select your own F-Zero (USA) ROM.\n"
    "Mods > Track Pack Loader loads every folder/ZIP in mods/packs. Restart after installing packs.\n"
    "The original 15 courses remain available. Loader and BS Satellaview Tracks exclude each other.\n"
    "The included CGP, Astra, Bower and MAX packs add 15 cups / 75 course versions.\n"
    "Vehicle packs, rules and screen override remain separate options. CGP preset enables the full experience.\n\n"
    + ("CGP audio is included as mods/packs/cgp-audio.\n" if bundled_music else "Audio is not included; you can add music packs separately.\n") +
    "Enable MSU-1 in Settings > Audio; Installed pack music uses discovered recordings.\n"
    "Custom selects loose MSU files. Missing songs use their course's native SNES music.\n"
    "Astra recordings are not included. See docs/PACK_FORMAT.md for the audio-pack manifest.\n\n"
    "F7: save-state menu. R: rewind. D/C: left/right shoulder. Alt+Enter: fullscreen.\n"
    "See README.md, MODS.md and docs/PACK_FORMAT.md for authoring and options.\n",encoding="utf-8")

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
for notice in (ROOT / "licenses").glob("*.txt"):
    shutil.copy2(notice, notices / notice.name)
for label, source in {
    "snesrecomp": dependency_roots["snesrecomp"] / "LICENSE",
    "recomp-ui": dependency_roots["recomp-ui"] / "LICENSE",
    "imgui": dependency_roots["recomp-ui"] / "src/third_party/imgui/LICENSE.txt",
}.items():
    shutil.copy2(source, notices / (label + ".txt"))
for package in ("gcc-libs", "libiconv", "libwinpthread", "winpthreads", "SDL3", "crt", "headers", "libxml2", "libpng", "rapidjson", "zlib", "zstd", "bzip2", "brotli", "openssl", "expat"):
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
manifest = {"version": version, "label": a.label, "music": a.music,
            "commit": commit, "dependencies": pins, "files": {}}
for path in sorted(stage.rglob("*")):
    if path.is_file():
        if (path.suffix.lower() in (".sfc", ".smc", ".srm", ".sav", ".bin", ".c")
                or path.name.lower() in ("config.ini", "rom.cfg", "fzero-video.ini", "f-zero_msu1.ips")
                or path.name.lower().startswith("crt-geom")):
            raise SystemExit(f"Forbidden payload: {path}")
        if path.suffix.lower() in (".pcm", ".msu") and (
                not bundled_music or path.parent != stage / "mods/packs/cgp-audio"):
            raise SystemExit(f"Unapproved music payload: {path}")
        manifest["files"][path.relative_to(stage).as_posix()] = hashlib.sha256(path.read_bytes()).hexdigest()
(stage / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
archive = stage.parent / (stage.name + ".zip")
with zipfile.ZipFile(archive, "x", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
    for path in sorted(stage.rglob("*")):
        if path.is_file():
            z.write(path, path.relative_to(stage.parent))
with archive.open("rb") as archive_file:
    digest = hashlib.file_digest(archive_file, "sha256").hexdigest()
archive.with_suffix(".zip.sha256").write_text(f"{digest}  {archive.name}\n", encoding="ascii")
print(f"{archive}\nSHA256 {digest}\nSource {commit}")
