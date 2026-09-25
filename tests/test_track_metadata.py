"""ROM-free checks that import audits reject guessed or reordered metadata."""
from copy import deepcopy
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from audit_track_metadata import audit_metadata


class MetadataAudit(unittest.TestCase):
    def setUp(self):
        self.rom = bytearray(0x90000)
        self.rom[0x77e0:0x77f1] = bytes.fromhex("08 e2 30 c2 10 ae 59 10 bf 00 89 10 e2 10 ea ea ea")
        self.rom[0x8024d:0x80262] = bytes.fromhex("8a c2 30 29 ff 00 8d 5b 10 bb bf 00 88 10 29 ff 00 8d 59 10 20")
        self.rom[0x80800:0x80805] = bytes([6, 4, 5, 3, 2])
        self.rom[0x80900:0x80907] = bytes([81, 27, 18, 45, 36, 27, 0])
        for slot in range(7):
            self.rom[0x80a00+3*slot:0x80a03+3*slot] = (0x109000+16*slot).to_bytes(3, "little")
            self.rom[0x81000+16*slot:0x81008+16*slot] = bytes([0x80, 0x53, 1, 0x82, 0x1b, 0xff, 0x64+slot, 0])
        self.layout = dict(count=["7"], music=["108900"], names=["108a00"])
        self.manifest = dict(cup=["example|Example League|0"],
                             track=[f"course-{slot}|{'ABCDEFG'[slot]}|example|{slot}" for slot in (6, 4, 5, 3, 2)])

    def test_donor_order_and_music(self):
        report = audit_metadata(self.rom, self.layout, self.manifest)
        self.assertEqual([t["snes_music"] for t in report["tracks"]],
                         ["mute-city", "port-town", "silence", "red-canyon", "sand-ocean"])
        self.assertEqual(report["cup_names"], "manual-review-required")

    def test_explicit_subset(self):
        self.manifest["track"] = self.manifest["track"][1:3]
        report = audit_metadata(self.rom, self.layout, self.manifest, source_cup=0)
        self.assertEqual(report["omitted_slots"], [6, 3, 2])
        self.assertEqual([t["slot"] for t in report["tracks"]], [4, 5])

    def test_single_course(self):
        self.manifest["track"] = self.manifest["track"][-1:]
        self.assertEqual(len(audit_metadata(self.rom, self.layout, self.manifest, 0)["tracks"]), 1)

    def test_reordered_subset_rejected(self):
        self.manifest["track"] = list(reversed(self.manifest["track"][-2:]))
        with self.assertRaisesRegex(ValueError, "authored race order"):
            audit_metadata(self.rom, self.layout, self.manifest, 0)

    def test_unused_resource_rejected(self):
        self.manifest["track"] = ["unused|A|example|0"]
        with self.assertRaisesRegex(ValueError, "authored race order"):
            audit_metadata(self.rom, self.layout, self.manifest, 0)

    def test_cup_boundaries_preserved(self):
        self.manifest["cup"] = ["first|First|0", "second|Second|1"]
        self.manifest["track"] = [t.replace("|example|", "|first|" if i < 2 else "|second|")
                                  for i, t in enumerate(self.manifest["track"])]
        with self.assertRaisesRegex(ValueError, "cup membership"):
            audit_metadata(self.rom, self.layout, self.manifest)

    def test_override_is_visible(self):
        self.layout["spc"] = ["6|big-blue"]
        track = audit_metadata(self.rom, self.layout, self.manifest)["tracks"][0]
        self.assertEqual((track["donor_music"], track["snes_music"], track["override"]),
                         ("mute-city", "big-blue", True))

    def test_malformed_metadata_rejected(self):
        variants = []
        layout = deepcopy(self.layout)
        del layout["music"]
        variants.append((self.rom, layout, self.manifest))
        for override in ("7|big-blue", "6|invented", "-1|big-blue"):
            variants.append((self.rom, dict(self.layout, spc=[override]), self.manifest))
        variants.append((self.rom, dict(self.layout, music=["108901"]), self.manifest))
        variants.append((self.rom, self.layout, dict(self.manifest, track=list(reversed(self.manifest["track"])))))
        variants.append((self.rom, self.layout, dict(self.manifest, track=[self.manifest['track'][0].replace('|G|', '|Wrong Name|')]+self.manifest['track'][1:])))
        variants.append((self.rom, self.layout, dict(self.manifest, cup=["missing|Missing|0"])))
        for offset, value in ((0x77e8, 0xea), (0x80257, 0xea), (0x80906, 82), (0x81066, 0x01)):
            rom = bytearray(self.rom)
            rom[offset] = value
            variants.append((rom, self.layout, self.manifest))
        for index, args in enumerate(variants):
            with self.subTest(index=index), self.assertRaises(ValueError):
                audit_metadata(*args)


if __name__ == "__main__":
    unittest.main()
