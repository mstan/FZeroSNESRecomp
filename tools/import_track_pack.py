"""Install a recognized user-owned patch into the additive course directory.

The game recognizes loose IPS/BPS files itself. This optional convenience tool
also extracts MAX's ZIP and validates inputs without installing patched ROMs.
"""
import argparse
from pathlib import Path
import zipfile
from parse_track_pack import donor, recognize, digest, write_new


def install(stock, patch, library):
    target = donor(stock, patch)
    manifest = recognize(target)
    if not manifest:
        raise ValueError("Unrecognized donor; use parse_track_pack.py with a reviewed layout")
    pack_id = manifest.stem
    extension = ".bps" if patch.startswith(b"BPS1") else ".ips"
    library.mkdir(parents=True, exist_ok=True)
    path = library / f"{pack_id}-{digest(patch)[:16]}{extension}"
    write_new(path, patch)
    print(f"Installed {path.name}; the in-game library adds this pack once")


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--stock", type=Path, required=True)
    source = p.add_mutually_exclusive_group(required=True)
    source.add_argument("--archive", type=Path)
    source.add_argument("--patch", type=Path)
    p.add_argument("--variant", choices=("classic", "modern"))
    p.add_argument("--library", type=Path, required=True)
    a = p.parse_args()
    stock = a.stock.read_bytes()
    if a.patch:
        if a.patch.stat().st_size > 32 * 1024 * 1024:
            raise ValueError("Oversized patch")
        install(stock, a.patch.read_bytes(), a.library)
    else:
        with zipfile.ZipFile(a.archive) as archive:
            for variant in (a.variant,) if a.variant else ("classic", "modern"):
                entry = archive.getinfo(f"MAX_League_{variant.title()}.ips")
                if entry.file_size > 32 * 1024 * 1024:
                    raise ValueError("Oversized archive member")
                install(stock, archive.read(entry), a.library)


if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError, KeyError, zipfile.BadZipFile) as error:
        raise SystemExit(str(error))
