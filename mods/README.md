# Add more courses to F-Zero

MAX League and Community Grand Prix are included. Enable or disable each pack
in **Mods**; every enabled pack adds its cups automatically. Bundled packs have
only an enable checkbox, with no file selection to configure. Start the game,
choose Grand Prix, then move through the league list with the direction
buttons. Imported cups appear after the original leagues.

For additional packs, put extracted `.ips` or `.bps` files in
**`mods/track-packs` beside the game**, along with their manifest and layout
when they are not already recognized. You still supply your own original ROM.

With BS Deluxe enabled, its four extra cars and two extra leagues remain
available alongside the original content and imported courses. With BS Deluxe
off, imports join the original four-car game. Turn off the individual track
packs to use the native league list; turn off BS Deluxe and other enhancements
for the stock experience. There is no separate Track Library switch.

MAX League by PowerPanda and Zephyrum25 includes the Classic IPS and original
credits in `assets/track-packs`. Classic and Modern supply the same five
courses; installing both adds MAX once. Races use the common game's rules,
including its boost behavior.

**Community Grand Prix (CGP) 1.0** includes the P1 IPS and credits in
`assets/track-packs`. P1, P2 and P3 contain identical courses with different
donor vehicles: any one patch supplies the
same **six new cups / 30 courses**, and installing all three adds them once.
CGP's original and Satellaview courses are omitted because the native leagues
already provide them. MAX's five courses are distinct and remain available.
With BS Deluxe, MAX and CGP installed, the game has **12 cups / 60 courses**
and the existing eight-car roster. CGP's replacement vehicles, boost rules,
Legend difficulty and MSU soundtrack are not imported by this course adapter.
No MSU/PCM audio files are installed or enabled by these packs.

MAX and CGP need no separate download. To import additional or equivalent
patches from ZIP archives without extracting the soundtrack, use the optional
tool (the game itself expects loose patches):

```powershell
python tools/import_track_pack.py --stock path/to/fzero.sfc `
  --archive "path/to/F-Zero CGP P1.zip" --archive "path/to/F-Zero CGP P2.zip" `
  --archive "path/to/F-Zero CGP P3.zip" --library build/mods/track-packs
```

Other compatible packs supply three files:

```
mods/track-packs/
  someone-created.ips
  my-pack.ini
  my-pack.layout
```

The manifest supplies stable pack, cup and course identities and exact ROM
hashes. The layout describes course resources. Filenames of patches do not
matter. Bundled defaults remain in `assets/track-packs`; user patches and
settings remain in `mods/track-packs`. Bundled packs always use their included
patch; old file selections and extra copies cannot replace it.
Patches are applied separately to your original ROM in memory. The
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
For compilations, inventory the donor's resource slots and its race-order
table separately. Exclude existing native courses explicitly in the manifest.
Compare normalized geometry against other packs before claiming duplicates;
names alone cannot distinguish a revision from another track. Accept multiple
donor hashes under one pack ID only after comparing every declared resource.
