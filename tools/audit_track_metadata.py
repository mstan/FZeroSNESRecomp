"""Read-only metadata audit for the reviewed FZEdit loader family.

Addresses are decoded from recognized donor instructions, never guessed from
the venue art or our display names. Other loaders need a reviewed adapter.
"""
import hashlib
import re

SONGS = ("mute-city", "big-blue", "sand-ocean", "silence", "port-town",
         "red-canyon", "white-land-1", "white-land-2", "fire-field", "death-wind")
# Intro-text glyphs observed in the reviewed donors; unknown glyphs fail closed.
GLYPHS = dict(zip((0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x8e,
                  0x6e, 0x6f, 0x8a, 0x8b, 0x8c, 0x8d, 0x8f, 0xa0, 0xa1,
                  0xa2, 0xa3, 0xa4, 0xa5, 0xa7, 0xff, 0xfe),
                 "ABCDEFGHIKLMNOPRSTUVWYZ I"))


def span(rom, address, length):
    if not 0 <= address <= 0xffffff or (address & 0xffff) < 0x8000 or (address & 0x7f0000) >= 0x7e0000:
        raise ValueError(f"Invalid donor ROM address {address:06x}")
    offset = ((address & 0x7f0000) >> 1) | (address & 0x7fff)
    data = rom[offset:offset + length]
    if len(data) != length:
        raise ValueError("Donor metadata exceeds ROM bounds")
    return data


def course_name(rom, table, slot):
    address = int.from_bytes(span(rom, table + 3 * slot, 3), "little")
    data = span(rom, address, 128).split(b"\0", 1)[0]
    if len(data) < 6 or data[1:6] != bytes.fromhex("53 01 82 1b ff"):
        raise ValueError("Unrecognized course-name encoding; review the donor text loader")
    try:
        return "".join(GLYPHS[b] for b in data[6:]).strip()
    except KeyError as error:
        raise ValueError(f"Unreviewed course-name glyph {error.args[0]:02x}") from error


def audit_metadata(rom, layout, manifest, source_cup=None):
    """Return serializable evidence, or reject missing/mismatched donor metadata."""
    selector = span(rom, 0x00f7e0, 17)
    if selector[:8] != bytes.fromhex("08 e2 30 c2 10 ae 59 10") or selector[8] != 0xbf or selector[12:] != bytes.fromhex("e2 10 ea ea ea"):
        raise ValueError("Unreviewed music selector; qualify its original soundtrack before importing")
    music_table = int.from_bytes(selector[9:12], "little")
    if layout.get("music") != [f"{music_table:06x}"]:
        # Accept different spelling of the same hexadecimal CPU address.
        if len(layout.get("music", [])) != 1 or int(layout["music"][0], 16) != music_table:
            raise ValueError(f"Layout must import the donor music table at {music_table:06x}")
    overrides = {}
    for entry in layout.get("spc", []):
        slot, theme = entry.split("|")
        if not slot.isdecimal() or theme not in SONGS or int(slot) in overrides:
            raise ValueError("Invalid or duplicate SPC override")
        overrides[int(slot)] = theme
    loader = span(rom, 0x10824d, 21)
    if loader[:11] != bytes.fromhex("8a c2 30 29 ff 00 8d 5b 10 bb bf") or loader[14:] != bytes.fromhex("29 ff 00 8d 59 10 20"):
        raise ValueError("Unreviewed league-order loader; qualify authored race order")
    order_table = int.from_bytes(loader[11:14], "little")
    count = int(layout["count"][0])
    if not 1 <= count <= 128 or any(slot >= count for slot in overrides):
        raise ValueError("Invalid resource count or SPC override index")
    music = span(rom, music_table, count)
    if any(song > 81 or song % 9 for song in music):
        raise ValueError("Donor uses unsupported native music data")
    tracks = [t.split("|") for t in manifest["track"]]
    cups = [c.split("|") for c in manifest["cup"]]
    ordered = [t for cup in cups for t in tracks if t[2] == cup[0]]
    if len(ordered) != len(tracks):
        raise ValueError("Missing or duplicate cup identities")
    indices = [int(t[3]) for t in ordered]
    if source_cup is None:
        if any(sum(t[2] == cup[0] for t in tracks) != min(5, count - i * 5)
               for i, cup in enumerate(cups)):
            raise ValueError("Manifest cup membership differs from the donor; use --source-cup for a selected subset")
        source_order = list(span(rom, order_table, len(ordered)))
        if indices != source_order:
            raise ValueError("Manifest race order differs from the donor; use --source-cup for a selected subset")
    else:
        if len(cups) != 1 or source_cup < 0 or source_cup * 5 >= count:
            raise ValueError("A subset must identify one valid source cup")
        source_order = list(span(rom, order_table + source_cup * 5, min(5, count - source_cup * 5)))
        # Subsets are supported without silently reordering or adding courses.
        if (any(slot >= count for slot in source_order) or len(set(indices)) != len(indices)
                or indices != [slot for slot in source_order if slot in indices]):
            raise ValueError("Selected courses must follow their source cup's authored race order")
    names = int(layout["names"][0], 16)
    result = []
    for identity, display, cup, index in ordered:
        slot = int(index)
        if not 0 <= slot < count:
            raise ValueError("Course index outside donor resources")
        original = course_name(rom, names, slot)
        # CGP provenance suffixes distinguish additive revisions of stock cups.
        if re.sub(r" CGP$", "", display.upper()) != original:
            raise ValueError(f"Course name mismatch: {display!r}; donor says {original!r}")
        result.append(dict(id=identity, name=display, donor_name=original, cup=cup, slot=slot,
                           donor_music=SONGS[music[slot] // 9],
                           snes_music=overrides.get(slot, SONGS[music[slot] // 9]),
                           override=slot in overrides))
    return dict(donor_sha256=hashlib.sha256(rom).hexdigest(),
                music_table=f"{music_table:06x}", order_table=f"{order_table:06x}",
                source_cup=source_cup, source_order=source_order,
                omitted_slots=[slot for slot in source_order if slot not in indices],
                cup_names="manual-review-required",
                cups=[dict(id=c[0], name=c[1]) for c in cups], tracks=result)
