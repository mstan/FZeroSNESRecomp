"""Import the retained CGP soundtrack, enforcing documented contributor exclusions.
No ROM, patch, unrelated archive files or alternate soundtrack is extracted.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import zipfile
from pack_manifest import course_source_stem

ROOT = Path(__file__).resolve().parents[1]


def load_manifest():
    return json.loads((ROOT / "assets/music/cgp.json").read_text())


def verify_file(path, item):
    if path.stat().st_size != item["bytes"]:
        raise ValueError(f"Unexpected size: {path}")
    with path.open("rb") as f:
        if hashlib.file_digest(f, "sha256").hexdigest() != item["sha256"]:
            raise ValueError(f"Unexpected content: {path}")


def prune_excluded(folder, *, superseded=False):
    """Physically remove identified obsolete files from this exact folder.

    Superseded means absent from the PC-port replacement, not author-excluded.
    """
    manifest = load_manifest()
    retired = dict(manifest.get("excluded_tracks", {}))
    if superseded:
        retired.update(manifest.get("superseded_tracks", {}))
    excluded = []
    for track, item in retired.items():
        path = folder / f"cgp-{track}.pcm"
        if path.exists():
            # Do not delete a user's replacement just because its number matches.
            verify_file(path, item)
            excluded.append(path)
    for path in excluded:
        path.unlink()
    return len(excluded)


def verify_music(folder):
    manifest = load_manifest()
    for track, item in manifest["tracks"].items():
        verify_file(folder / f"cgp-{track}.pcm", item)
    if (folder / "cgp.msu").read_bytes() != b"":
        raise ValueError("Unexpected MSU descriptor")
    expected = {f"cgp-{n}.pcm" for n in manifest["tracks"]} | {"cgp.msu"}
    if (folder / "pack.json").exists():
        if json.loads((folder / "pack.json").read_text()) != json.loads((ROOT / "assets/music/cgp-pack.json").read_text()):
            raise ValueError("Unexpected soundtrack pack manifest")
        expected.add("pack.json")
    if {p.name for p in folder.iterdir()} != expected:
        raise ValueError("Unexpected soundtrack files (including excluded recordings)")
    return manifest


def stage_music(source, destination):
    manifest = verify_music(source)
    destination.mkdir(parents=True, exist_ok=True)
    # copy_directory alone would retain excluded recordings from older builds.
    prune_excluded(destination, superseded=True)
    for track in manifest["tracks"]:
        name = f"cgp-{track}.pcm"
        shutil.copy2(source / name, destination / name)
    shutil.copy2(source / "cgp.msu", destination / "cgp.msu")
    verify_music(destination)
    return len(manifest["tracks"])


def stage_course_music(source, pack):
    """Stage reviewed recordings as music/<course source basename>.pcm."""
    manifest = verify_music(source)
    courses = json.loads((pack / "courses.json").read_text())["courses"]
    files, used = {}, set()
    for course in courses:
        number = str(course.get("music", {}).get("track", ""))
        if number not in manifest["tracks"]:
            continue  # The approved replacement intentionally uses SPC here.
        name = course_source_stem(pack, course) + ".pcm"
        if name in files and files[name] != number:
            raise ValueError(f"Conflicting music filename: {name}")
        files[name] = number
        used.add(number)
    # Non-race cues retain the existing MSU adapter's names, in the same folder.
    for number in manifest["tracks"].keys() - used:
        if int(number) >= 10:
            raise ValueError(f"Approved recording has no course: {number}")
        files[f"cgp-{number}.pcm"] = number
    directory = pack / "music"
    directory.mkdir(exist_ok=True)
    if {p.name for p in directory.iterdir()} - files.keys():
        raise ValueError("Unexpected files in staged course music")
    for name, number in files.items():
        shutil.copy2(source / f"cgp-{number}.pcm", directory / name)
        verify_file(directory / name, manifest["tracks"][number])
    return len(files)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("archive", type=Path, nargs="?")
    operations = p.add_mutually_exclusive_group()
    operations.add_argument("--stage-from", type=Path)
    operations.add_argument("--stage-course-from", type=Path,
                            help="Stage into --out course pack, matching course filenames")
    operations.add_argument("--prune-excluded", type=Path,
                            help="Remove hash-verified excluded files from an existing import/build")
    operations.add_argument("--prune-retired", type=Path,
                            help="Remove hash-verified excluded and superseded PC-port recordings")
    p.add_argument("--out", type=Path, default=ROOT / "music/cgp")
    a = p.parse_args()
    if sum(bool(x) for x in (a.archive, a.stage_from, a.stage_course_from, a.prune_excluded, a.prune_retired)) != 1:
        p.error("Choose an archive or one staging/pruning operation")
    if a.stage_course_from:
        print(f"Staged {stage_course_music(a.stage_course_from, a.out)} course/menu recordings in {a.out / 'music'}")
        return
    if a.prune_retired:
        print(f"Removed {prune_excluded(a.prune_retired, superseded=True)} retired CGP files from {a.prune_retired}")
        return
    if a.prune_excluded:
        print(f"Removed {prune_excluded(a.prune_excluded)} excluded CGP files from {a.prune_excluded}")
        return
    if a.stage_from:
        print(f"Staged {stage_music(a.stage_from, a.out)} retained CGP files in {a.out}")
        return
    manifest = load_manifest()
    excluded = manifest.get("excluded_tracks", {})
    known = manifest["tracks"] | excluded
    a.out.mkdir(parents=True, exist_ok=False)
    with zipfile.ZipFile(a.archive) as archive:
        entries = {}
        for item in archive.infolist():
            name = Path(item.filename).name
            if not name.endswith(".pcm"):
                continue
            track = name.rsplit("-", 1)[-1][:-4]
            if track not in known or track in entries:
                raise ValueError("Unrecognized or duplicate PCM track")
            entries[track] = item
        if not manifest["tracks"].keys() <= entries.keys():
            raise ValueError("Incomplete soundtrack")
        for track, item in entries.items():
            if item.file_size != known[track]["bytes"]:
                raise ValueError("Unexpected PCM size")
            if track in excluded:
                with archive.open(item) as src:
                    if hashlib.file_digest(src, "sha256").hexdigest() != excluded[track]["sha256"]:
                        raise ValueError("Unexpected excluded PCM content")
                continue
            with archive.open(item) as src, (a.out / f"cgp-{track}.pcm").open("xb") as dst:
                shutil.copyfileobj(src, dst)
    (a.out / "cgp.msu").write_bytes(b"")
    verify_music(a.out)
    print(f"Verified {len(manifest['tracks'])} retained CGP tracks in {a.out}")
    if manifest.get("attribution_review", {}).get("status") == "incomplete":
        print("Contributor attribution is incomplete; see assets/music/CGP_ATTRIBUTION.md before release.")


if __name__ == "__main__":
    main()
