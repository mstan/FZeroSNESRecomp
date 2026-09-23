# Add more courses to F-Zero

Community Grand Prix is included. Enable or disable it in **Mods**; every
enabled pack adds its cups automatically. Bundled packs have
an enable checkbox, with no file selection to configure. Start the game,
choose Grand Prix, then move through the league list with the direction
buttons. Imported cups appear after the original leagues.

The `fzero-55` build has **11 cups / 55 courses** with CGP enabled: the
15 original courses, CGP's 10 corrected BS courses, and 30 new courses.
MAX League's files are retained, but its mod and cups are hidden and disabled.

**BS Satellaview vehicles** adds the four extra cars independently of tracks.
**BS Satellaview tracks** adds the ten original BS courses. That track option
and CGP are mutually exclusive; enabling either turns off the other. The
vehicle option works with either track set, or just the original 15 courses.
CGP and BS vehicles are enabled by default; original BS tracks are off.

**CGP P1, P2 and P3 vehicles** are three independent, opt-in packs. Enable any
combination to add their ships to the car selector; all three give twelve
distinct identities including the original four. Each new ship has its own
artwork, handling, energy boost and exhaust. Left/right changes car pages;
up/down changes the selected ship. **Vehicle Rebalances** separately offers
the author's versions of Blue Falcon, Golden Fox, Wild Goose and Fire Stingray.
Those options modify the existing identities and default off.

Stock BS vehicles conflict with all three CGP vehicle packs and the stock-car
rebalances. Enabling one mode disables the other. This does not affect the
selected courses. Old independent tuning/boost/exhaust profiles are retired;
old selections do not silently enable new ships or rebalances.

CGP's **Title screen** dropdown defaults to **Original**. Select **F-Zero 55**
to use the 55 logo and its title colors while CGP is enabled. Disabling CGP
restores the original title and remembers your choice. This changes only the
title artwork; cars, handling, cups and course records are unaffected.

For additional packs, put extracted `.ips` or `.bps` files in
**`mods/track-packs` beside the game**, along with their manifest and layout
when they are not already recognized. You still supply your own original ROM.
Turn off course packs, BS vehicles and other enhancements for the stock
experience. There is no separate Track Library switch.

MAX League by PowerPanda and Zephyrum25 includes the Classic IPS and original
credits in `assets/track-packs`. Classic and Modern supply the same five
courses; installing both adds MAX once. Races use the common game's rules,
including its boost behavior.

**Community Grand Prix (CGP)** includes a regenerated IPS for the author's
P3test venue revision and credits in `assets/track-packs`. Historical P1/P2/P3
had identical courses, but their retired Volcania artwork is no longer an
accepted course source. The current import supplies **eight cups / 40 courses**
(30 new plus 10 corrected BS). MAX's five courses are distinct but parked.

The remaining **16 optional gameplay mods** work independently of vehicle and
course packs. Required up/grip magnet and Rainbow Road behavior now accompanies
the declaring courses automatically; optional switches are not prerequisites
for those courses. Course packs do not select car tuning or difficulty.
See [the source/options table](cgp-source/README.md).
The optional CGP MSU adapter requires your own music folder. No MSU/PCM audio
files are installed or enabled by these packs.

MAX and CGP need no separate download. To import additional or equivalent
patches from ZIP archives without extracting the soundtrack, use the optional
tool (the game itself expects loose patches):

```powershell
python tools/import_track_pack.py --stock path/to/fzero.sfc `
  --archive "path/to/another-qualified-pack.zip" --library build/mods/track-packs
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
base engine and gameplay rules; adding/removing packs preserves records but
invalidates old library snapshots. Changed gameplay rules use a separate save
and records namespace. Native original BS Practice courses are unavailable
when their track mod is off.

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
table separately. Exclude unchanged native courses explicitly in the manifest. Corrected BS
revisions in CGP are deliberate alternatives, with mutually exclusive track
providers; do not deduplicate them against originals solely by name.
Compare normalized geometry against other packs before claiming duplicates;
names alone cannot distinguish a revision from another track. Accept multiple
donor hashes under one pack ID only after comparing every declared resource.

A shipped `<pack-id>.hidden` file containing `1` parks a pack: it remains in
the catalog with its assets and record identities intact, but is omitted from
Mods and kept off regardless of old settings. This branch ships
`max-league.hidden`. Removing that marker or changing it to `0` makes the
pack visible again; its enable setting can then be changed normally.
