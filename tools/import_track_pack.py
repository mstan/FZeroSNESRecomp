"""Install user-supplied MAX League patches without storing a patched ROM.

Only the patch bytes and public manifests are installed. Existing selections
and other packs are preserved. Equivalent BPS patches work through --patch.
The runtime revalidates both ROM hashes when launching a cup.
"""
import argparse
import hashlib
from pathlib import Path
import sys
import zipfile

from inspect_bs_deluxe import apply_bps, apply_ips, STOCK_SHA256

ROOT = Path(__file__).resolve().parents[1]


def install(stock, patch, manifest, library):
    fields = dict(line.split("=", 1) for line in manifest.read_text().splitlines()
                  if "=" in line and not line.startswith("#"))
    pack_id = fields["id"]
    if hashlib.sha256(stock).hexdigest() != fields["source_sha256"]:
        raise ValueError("A verified original F-Zero USA ROM is required")
    target = apply_bps(stock, patch) if patch.startswith(b"BPS1") else apply_ips(stock, patch)
    if hashlib.sha256(target).hexdigest() != fields["target_sha256"]:
        raise ValueError(f"Patch does not produce the pinned {pack_id} revision")
    library.mkdir(parents=True, exist_ok=True)
    # Preserve a previous installation unless it is precisely the same input.
    # New revisions need a new ID; users retain their old patch and records.
    outputs = {
        library / f"{pack_id}.ini": manifest.read_bytes(),
        library / f"{pack_id}.patch": patch,
        library / f"{pack_id}.path": (str((library / f"{pack_id}.patch").resolve()) + "\n").encode(),
    }
    for path, content in outputs.items():
        if path.exists() and path.read_bytes() != content:
            raise ValueError(f"Refusing to replace existing content: {path}")
    for path, content in outputs.items():
        if not path.exists():
            with path.open("xb") as f:
                f.write(content)
    print(f"Installed {pack_id}/max (5 tracks); other packs and selection unchanged")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--stock", type=Path, required=True)
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument("--archive", type=Path)
    source.add_argument("--patch", type=Path)
    parser.add_argument("--variant", choices=("classic", "modern"))
    parser.add_argument("--library", type=Path, required=True)
    args = parser.parse_args()
    stock = args.stock.read_bytes()
    if len(stock) == 0x80200:
        stock = stock[512:]
    if hashlib.sha256(stock).hexdigest() != STOCK_SHA256:
        parser.error("A verified original F-Zero USA ROM is required")
    if args.patch and not args.variant:
        parser.error("--patch requires --variant classic or modern")
    variants = (args.variant,) if args.variant else ("classic", "modern")
    for variant in variants:
        if args.archive:
            with zipfile.ZipFile(args.archive) as z:
                info = z.getinfo(f"MAX_League_{variant.title()}.ips")
                if info.file_size > 32 * 1024 * 1024:
                    raise ValueError("Oversized patch archive member")
                patch = z.read(info)
        else:
            if args.patch.stat().st_size > 32 * 1024 * 1024:
                raise ValueError("Oversized patch")
            patch = args.patch.read_bytes()
        install(stock, patch, ROOT / "assets/track-packs" / f"max-league-{variant}.ini", args.library)


if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError, KeyError, zipfile.BadZipFile) as error:
        print(f"Import failed: {error}", file=sys.stderr)
        sys.exit(1)
