"""Boundary and independent round-trip coverage for the IPS writer."""
import random
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from make_ips import make_ips
from inspect_bs_deluxe import apply_ips


class PatchWriterTests(unittest.TestCase):
    def check_patch(self, source, target):
        patch = make_ips(source, target)
        self.assertEqual(apply_ips(source, patch), target)
        self.assertEqual(make_ips(source, target), patch)

    def test_sizes_and_runs(self):
        for a, b in [(b"original", b"orig"), (b"a", b"a" + bytes(100000)),
                     (bytes(140000), b"x" * 140000), (b"abc", b"abc"),
                     (b"", b"first"), (b"a" * 65536, b"b" * 65536)]:
            with self.subTest(sizes=(len(a), len(b))):
                self.check_patch(a, b)

    def test_reserved_offset(self):
        source = bytes(0x454f47)
        self.check_patch(source, source[:-1] + b"x")

    def test_sparse_deltas(self):
        rng = random.Random(20260923)
        for size in (1, 3, 65534, 65535, 65536, 140000):
            source = rng.randbytes(size)
            target = bytearray(source)
            for _ in range(100):
                target[rng.randrange(size)] = rng.randrange(256)
            self.check_patch(source, bytes(target))


if __name__ == "__main__":
    unittest.main()
