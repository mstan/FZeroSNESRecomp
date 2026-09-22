# Add more courses to F-Zero

Put extracted `.ips` or `.bps` files in **`mods/track-packs` beside the game**.
Start the game, choose Grand Prix, then move through the league list with the
direction buttons. Installed cups appear after the original leagues. You do
not select a replacement cartridge or cup in the launcher.

With BS Deluxe enabled, its four extra cars and two extra leagues remain
available alongside the original content and imported courses. With BS Deluxe
off, imports join the original four-car game. Turn Track Library off in Mods
to use the original menus; turn off other enhancements for stock presentation.

MAX League by PowerPanda and Zephyrum25 is recognized automatically. Either
Classic or Modern supplies the same five courses. Installing both adds MAX
once. Races use the common game's rules, including its boost behavior.

Other compatible packs supply three files:

```
mods/track-packs/
  someone-created.ips
  my-pack.ini
  my-pack.layout
```

The manifest supplies stable pack, cup and course identities and exact ROM
hashes. The layout describes course resources. Filenames of patches do not
matter. Patches are applied separately to your original ROM in memory. The
game imports course data and discards the donor's replacement engine code.
Your original ROM is never modified.

Missing, disabled, unrecognized or invalid packs do not remove other cups.
Duplicate manifest IDs are excluded instead of arbitrarily choosing one.
Restore a removed patch to get its cups back. Existing save files are retained;
imported cup records have their own stable namespace and start with empty
times. Changes to other installed packs do not change that namespace.

This prototype supports Grand Prix cups containing **one to five courses**.
A contributor can supply a single-course pack, several cups, or a selected
subset of a donor's courses. Each independently supplied patch needs its own
manifest. Imported Practice selection and a combined records browser are not
implemented yet. Save states require the same installed course catalog and
base engine; adding/removing packs preserves records but invalidates old
library snapshots. MSU-1 remains outside this prototype's qualification.

If a patch is unrecognized, see [PARSE_MANIFEST.md](PARSE_MANIFEST.md). File an
issue with the author's patch link, revision, source-ROM checksum and desired
course list. Do not attach a ROM. An unknown IPS/BPS does not itself describe
which bytes are tracks, physics, menus or executable code.

## For LLMs and pack contributors

Read [PARSE_MANIFEST.md](PARSE_MANIFEST.md) before creating a descriptor.
Use `tools/parse_track_pack.py` to fingerprint known revisions or validate a
reviewed resource layout. Runtime loading does not require Python or an LLM.

Preserve the additive contract: stable identities, independent patch inputs,
common engine, original and BS content, shared HUD/culling hooks, isolated
records. Do not execute the donor cartridge, patch the running engine with a
donor's bytes, add another launcher cup selector, or silently substitute a
different course when data is missing. Structural parsing is only one part
of qualification; record gameplay evidence and unresolved features explicitly.
