"""Create a deterministic IPS delta from two private images; never copy a ROM.

The output is verified using the independent IPS reader before it is written.
Only use donor revisions approved for redistribution as patches.
"""
import argparse
from pathlib import Path

from inspect_bs_deluxe import apply_ips


def make_ips(source, target):
    if not target or max(len(source), len(target)) >= 0x1000000:
        raise ValueError("IPS images must be nonempty and smaller than 16 MiB")
    padded = source[:len(target)].ljust(len(target), b"\0")
    result = bytearray(b"PATCH")
    pos = 0
    while pos < len(target):
        if padded[pos] == target[pos]:
            pos += 1
            continue
        start = pos
        # EOF is reserved as a record offset; include the preceding byte.
        if start == 0x454f46:
            start -= 1
        end = pos + 1
        last = end
        while end < len(target) and end - start < 65535 and end - last <= 5:
            if padded[end] != target[end]:
                last = end + 1
            end += 1
        data = target[start:last]
        result += start.to_bytes(3, "big")
        if len(data) > 3 and data.count(data[:1]) == len(data):
            result += b"\0\0" + len(data).to_bytes(2, "big") + data[:1]
        else:
            result += len(data).to_bytes(2, "big") + data
        pos = last
    result += b"EOF" + len(target).to_bytes(3, "big")
    result = bytes(result)
    if apply_ips(source, result) != target:
        raise ValueError("IPS round-trip verification failed")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("target", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    patch = make_ips(args.source.read_bytes(), args.target.read_bytes())
    args.output.write_bytes(patch)
    print(f"Wrote {len(patch)} bytes; exact round trip verified")


if __name__ == "__main__":
    main()
