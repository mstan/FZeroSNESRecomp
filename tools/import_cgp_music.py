"""Import the approved CGP soundtrack once, verifying every PCM against provenance.
No ROM, patch, unrelated archive files or alternate soundtrack is extracted.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import zipfile

ROOT = Path(__file__).resolve().parents[1]

def verify_music(folder):
    manifest = json.loads((ROOT / "assets/music/cgp.json").read_text())
    for track, item in manifest["tracks"].items():
        path = folder / f"cgp-{track}.pcm"
        if path.stat().st_size != item["bytes"]:
            raise ValueError(f"Unexpected size: {path}")
        with path.open("rb") as f:
            if hashlib.file_digest(f, "sha256").hexdigest() != item["sha256"]:
                raise ValueError(f"Unexpected content: {path}")
    if (folder / "cgp.msu").read_bytes() != b"":
        raise ValueError("Unexpected MSU descriptor")
    expected = {f"cgp-{n}.pcm" for n in manifest["tracks"]} | {"cgp.msu"}
    if {p.name for p in folder.iterdir()} != expected:
        raise ValueError("Unexpected soundtrack files")
    return manifest

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("archive", type=Path)
    p.add_argument("--out", type=Path, default=ROOT / "music/cgp")
    a = p.parse_args()
    manifest = json.loads((ROOT / "assets/music/cgp.json").read_text())
    a.out.mkdir(parents=True, exist_ok=False)
    with zipfile.ZipFile(a.archive) as archive:
        entries = {}
        for item in archive.infolist():
            name = Path(item.filename).name
            if not name.endswith(".pcm"):
                continue
            track = name.rsplit("-", 1)[-1][:-4]
            if track not in manifest["tracks"] or track in entries:
                raise ValueError("Unrecognized or duplicate PCM track")
            entries[track] = item
        if entries.keys() != manifest["tracks"].keys():
            raise ValueError("Incomplete soundtrack")
        for track, item in entries.items():
            if item.file_size != manifest["tracks"][track]["bytes"]:
                raise ValueError("Unexpected PCM size")
            with archive.open(item) as src, (a.out / f"cgp-{track}.pcm").open("xb") as dst:
                shutil.copyfileobj(src, dst)
    (a.out / "cgp.msu").write_bytes(b"")
    verify_music(a.out)
    print(f"Verified {len(entries)} CGP tracks in {a.out}")

if __name__ == "__main__":
    main()
