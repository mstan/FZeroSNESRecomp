"""Local all-or-nothing BS Deluxe conversion. Requires the user's archive/ROM.

Produces a private guarded cartridge delta and verifies its generated module.
No installed patched ROM is required. Generated output must not be committed.
"""
import argparse
import json
from pathlib import Path
import struct
import zipfile

from inspect_bs_deluxe import (STOCK_SHA256, apply_bps, apply_ips, sha,
                               usa_patch_names, readme_version)

# Pinned upstream revision. The native module, runtime target hash in
# src/fzero_deluxe.c and the tracked patches/bs-deluxe-usa.ips must all move
# together when this changes.
DELUXE_VERSION = "1.1"
DELUXE_SHA256 = "552159a19955e88a8337f7c473ccc53e5dcef15b87daab8e89e1894694542fe6"
# Previous pin, USA 1.0 (February 10, 2024): 77bb37bcdedd3e17321727d5ed6a14792aa7a45ac04e9040b16bf564bd6dea24


def validate_namespace(gen):
    # The framework owns the complete namespace, including module identity.
    # Do not regenerate a partial game-specific list of C symbols.
    header = gen / "module_namespace.h"
    descriptor = gen / "module_v2.c"
    if not header.is_file() or not descriptor.is_file() or \
            "#define g_program_module deluxe_g_program_module" not in header.read_text() or \
            'g_program_module.id = "bs-deluxe";' not in descriptor.read_text():
        raise ValueError("Regenerate Deluxe with --module-id bs-deluxe --module-prefix deluxe")


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--archive", type=Path, required=True)
    p.add_argument("--stock", type=Path, required=True)
    p.add_argument("--out", type=Path, required=True)
    p.add_argument("--gen", type=Path, required=True)
    a = p.parse_args()
    source = a.stock.read_bytes()
    if len(source) == 524800:
        source = source[512:]
    if sha(source) != STOCK_SHA256:
        raise ValueError("Unsupported stock ROM")
    with zipfile.ZipFile(a.archive) as z:
        bps_name, ips_name = usa_patch_names(z)
        target = apply_bps(source, z.read(bps_name))
        if target != apply_ips(source, z.read(ips_name)):
            raise ValueError("Patch formats disagree")
        credits = z.read("readme.txt")
        version = readme_version(z)
    if sha(target) != DELUXE_SHA256 or version != DELUXE_VERSION:
        raise ValueError(f"Unsupported Deluxe revision {version}: native module is pinned to USA {DELUXE_VERSION}")
    baseline = source + bytes(len(target) - len(source))
    records = []
    start = None
    for i, (old, new) in enumerate(zip(baseline, target)):
        if old != new and start is None:
            start = i
        if old == new and start is not None:
            records.append((start, target[start:i]))
            start = None
    if start is not None:
        records.append((start, target[start:]))
    payload = b"BSDELX1\0" + struct.pack("<II", len(target), len(records))
    payload += bytes.fromhex(STOCK_SHA256) + bytes.fromhex(DELUXE_SHA256)
    for offset, data in records:
        payload += struct.pack("<II", offset, len(data)) + data
    validate_namespace(a.gen)
    a.out.mkdir(parents=True, exist_ok=True)
    (a.out / "bs-deluxe.dat").write_bytes(payload)
    (a.out / "BS-Deluxe-credits.txt").write_bytes(credits)
    (a.out / "bs-deluxe-import.json").write_text(json.dumps({
        "format": 1, "package": "bs-deluxe", "version": f"{DELUXE_VERSION}-USA",
        "ownership": "Entire Deluxe cartridge delta; indivisible launch-time feature",
        "archive_sha256": sha(a.archive.read_bytes()), "stock_sha256": sha(source),
        "bps_member": bps_name, "ips_member": ips_name,
        "target_sha256": sha(target), "delta_sha256": sha(payload),
        "records": len(records), "payload_bytes": len(payload),
    }, indent=2) + "\n")
    print(f"Imported {len(records)} guarded delta records ({len(payload)} bytes); native namespace ready")


if __name__ == "__main__":
    main()
