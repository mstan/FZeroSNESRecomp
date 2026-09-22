"""Validate a course donor and emit metadata only; never execute donor code.

Known revisions need no analysis. A new revision requires a reviewed typed
layout and course names; see mods/track-packs/PARSE_MANIFEST.md.
"""
import argparse
import hashlib
from pathlib import Path
import re
import subprocess
import tempfile

from inspect_bs_deluxe import apply_bps, apply_ips, STOCK_SHA256

ROOT = Path(__file__).resolve().parents[1]


def digest(data):
    return hashlib.sha256(data).hexdigest()


def fields(path):
    result = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        if line and not line.startswith("#"):
            key, value = line.split("=", 1)
            result.setdefault(key, []).append(value)
    return result


def donor(stock, patch):
    if len(stock) == 0x80200:
        stock = stock[512:]
    if digest(stock) != STOCK_SHA256:
        raise ValueError("Expected the original F-Zero USA ROM")
    if len(patch) > 32 * 1024 * 1024:
        raise ValueError("Patch exceeds the input limit")
    if patch.startswith(b"BPS1"):
        target = apply_bps(stock, patch)
    elif patch.startswith(b"PATCH"):
        target = apply_ips(stock, patch)
    else:
        raise ValueError("Expected an extracted IPS or BPS file")
    if len(target) > 16 * 1024 * 1024:
        raise ValueError("Donor exceeds the image limit")
    return target


def recognize(target):
    target_hash = digest(target)
    for path in sorted((ROOT / "assets/track-packs").glob("*.ini")):
        data = fields(path)
        if target_hash in data["target_sha256"] + data.get("alternate_target_sha256", []):
            return path
    return None


def write_new(path, content):
    if path.exists():
        if path.read_bytes() != content:
            raise ValueError(f"Refusing to replace existing content: {path}")
        return
    with path.open("xb") as file:
        file.write(content)


def qualify(target, layout, inspector):
    with tempfile.TemporaryDirectory(prefix="fzero-course-") as temp:
        image = Path(temp) / "donor.sfc"
        image.write_bytes(target)
        result = subprocess.run([str(inspector.resolve()), str(image), str(layout.resolve())],
                                capture_output=True, text=True, check=False)
        if result.returncode:
            raise ValueError(f"Course extraction failed: {result.stdout} {result.stderr}")
        return result.stdout


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--stock", type=Path, required=True)
    p.add_argument("--patch", type=Path, required=True)
    p.add_argument("--out", type=Path, required=True)
    p.add_argument("--inspector", type=Path, default=ROOT / "build/FZeroInspectCourses.exe")
    p.add_argument("--layout", type=Path)
    p.add_argument("--id")
    p.add_argument("--name")
    p.add_argument("--author")
    p.add_argument("--course", action="append", help="stable-id|display name|source index; repeat in race order")
    a = p.parse_args()
    target = donor(a.stock.read_bytes(), a.patch.read_bytes())
    known = recognize(target)
    if known and not a.layout:
        manifest = known.read_bytes()
        layout = known.with_suffix(".layout")
        pack_id = fields(known)["id"][0]
    else:
        if not all((a.layout, a.id, a.name, a.author, a.course)):
            p.error("Unknown revisions need --layout, --id, --name, --author and --course; see PARSE_MANIFEST.md")
        if not re.fullmatch(r"[a-z0-9][a-z0-9-]{0,46}", a.id):
            p.error("Invalid stable pack ID")
        if max(len(a.name.encode()), len(a.author.encode())) >= 96 or any(c in a.name + a.author for c in "\r\n|"):
            p.error("Names must be single-line metadata")
        if not 1 <= len(a.course) <= 5:
            p.error("This GP adapter accepts 1 to 5 courses per cup")
        tracks, ids = [], set()
        for course in a.course:
            entry = course.split("|")
            if len(entry) != 3:
                p.error("Use --course 'stable-id|display name|source index'")
            ident, name, slot = entry
            if not re.fullmatch(r"[a-z0-9][a-z0-9-]{0,46}", ident) or ident in ids:
                p.error("Track IDs must be valid and unique")
            if not name or len(name) >= 96 or "\n" in name or "\r" in name or not slot.isdecimal() or int(slot) > 127:
                p.error("Invalid track metadata")
            ids.add(ident)
            tracks.append(f"track={ident}|{name}|cup|{slot}\n")
        pack_id, layout = a.id, a.layout
        manifest = (f"format=1\nid={a.id}\nname={a.name}\nauthor={a.author}\n"
                    f"adapter=fzero-course-v1\nsource_sha256={STOCK_SHA256}\n"
                    f"target_sha256={digest(target)}\ncup=cup|{a.name}|0\n" + "".join(tracks)).encode()
    count = int(fields(layout)["count"][0])
    declared = [line for line in manifest.decode().splitlines() if line.startswith("track=")]
    if any(int(line.rsplit("|", 1)[1]) >= count for line in declared):
        raise ValueError("Track source index is outside the reviewed layout")
    report = qualify(target, layout, a.inspector)
    # Preflight every destination before creating anything. No patch or ROM is exported.
    outputs = {a.out / f"{pack_id}.ini": manifest,
               a.out / f"{pack_id}.layout": layout.read_bytes()}
    for path, content in outputs.items():
        if path.exists() and path.read_bytes() != content:
            raise ValueError(f"Refusing to replace existing content: {path}")
    a.out.mkdir(parents=True, exist_ok=True)
    for path, content in outputs.items():
        write_new(path, content)
    print(report, end="")
    print(f"Wrote {pack_id} metadata. Structural validation passed; new revisions still need gameplay qualification.")


if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError) as error:
        raise SystemExit(str(error))
