# Additive course library prototype

The game imports course resources into the canonical F-Zero/BS Deluxe engine.
It adds cups to a scrolling **in-game Grand Prix league menu**. The launcher
only enables/disables the library and individual packs; it has no cup dropdown.

With BS Deluxe enabled the menu contains Knight, Queen, King, BS-1, BS-2, then
MAX. The original four cars and four BS cars remain in the native vehicle menu.
Additional manifests append additional cups. With Deluxe disabled, imports
join the original three leagues and four-car roster. With the library disabled
or no usable patches installed, the normal game menus remain active.

## Installation and contribution

[The mods README](../mods/README.md) contains player instructions followed by
instructions for LLM contributors. [PARSE_MANIFEST.md](../mods/PARSE_MANIFEST.md)
documents the metadata format, typed resource layout, deterministic tools,
unknown-patch workflow and gameplay qualification checklist.

Drop IPS/BPS files into `mods/track-packs` beside the executable. Known MAX
Classic and Modern patches are recognized by verified output SHA-256 and
contribute one identical five-course pack. User manifests and layouts live
beside their patches; the shipped registry is read-only. New registry entries
are discovered without adding another pack-specific branch in game code.

An optional ZIP installer is available:

```powershell
python tools/import_track_pack.py --stock path/to/fzero.sfc `
  --archive "path/to/F-Zero MAX League.zip" --library build/mods/track-packs
```

No MAX patch, patched ROM, decoded resource binary or generated native source
is committed. MAX League is by PowerPanda and Zephyrum25.

## Runtime design

The framework owns bounded IPS/BPS application, exact source/target hash
validation and the stable-ID catalog. Each input is patched against a fresh
original image. The game decodes typed course resources (map, tile graphics,
AI/checkpoints, minimap, environment and terrain); it discards the donor image.
Donor native dispatch, menus, physics and replacement executable bytes never
become the running cartridge. Classic/Modern boost differences are therefore
not imported.

One canonical loader binding serves every pack. It preserves the stock or BS
Deluxe engine, its car/HUD palettes, raster/HD renderer and widened opponent
projection hook. Deluxe's resource loader uses different WRAM metadata from
stock, so its binding runs after that metadata is initialized. Stable catalog
IDs and ordered course resources, rather than a patch filename, define state
compatibility.

Each imported cup has a record namespace derived from pack ID, cup ID and
ordered course IDs/content hashes. Unrelated packs do not change it. New cups
start with canonical empty times. The base SRAM and its record mirror are
restored on title, reset and exit. Corrupt imported record files are preserved
and that session is read-only. Library snapshots include the base backup,
selected cup, full catalog identity and the framework execution-state chunk,
including refresh timing needed for deterministic replay.

## Build and validation

Worktrees started from FZeroSNESRecomp `1686df4` and snesrecomp `bb37c87` on
`experiment/additive-track-packs`. Build with `SNESRECOMP_ROOT` pointing at the
paired framework worktree until its content-pack support is integrated. The
normal generated stock and Deluxe modules are still required, as on main.
The local desktop executable is `build/FZeroSNESRecompTracks.exe`;
`FZERO_DESKTOP_NAME` leaves the normal output name unchanged in ordinary builds.

ROM-free checks cover patch syntax/checksums, alternate verified hashes,
manifest identities, directory discovery, independent missing/disabled packs,
malformed resource bounds, a pit at checkpoint zero, clean record defaults,
record isolation and corrupt-file preservation. Existing renderer, video,
HDMA, gamepad and mod independence tests also run.

Private qualification (requires the owner's original ROM and patch archive):

```powershell
python tests/validate_track_packs.py --build build --stock path/to/fzero.sfc `
  --archive "path/to/F-Zero MAX League.zip" --out captures/qualification-new
```

All five MAX courses were loaded and driven in the common Deluxe engine.
Stock, King and BS-1 gameplay, in-game sixth-cup navigation, IPS/BPS discovery,
equivalent-patch deduplication, partial single-course installs, removal/restore
and library disabling were checked. Save/load replays compare ten frames with
identical WRAM and master clock, and soft reset retains SRAM. The native GP
transition routine was also exercised through all five course loads and a
one-course cup using injected completed-result states. That boundary test is
not a claim of manually driving every lap or qualifying all finish-line and
hazard behavior.

## Current limits

The decoder supports the MAX/FZEdit resource representation, not every hack.
An unfamiliar binary format or donor-only hazard/event needs a new typed
adapter and qualification. Structural parsing alone cannot prove playability.
The current GP adapter accepts one to five tracks per cup. Imported Practice
selection, cross-pack assembled cups and a combined records browser are future
work. Stock/BS Practice remains available. MSU-1 is not enabled in imported
library sessions in this prototype. Library snapshots require the same
catalog; records survive adding/removing unrelated packs.

The save-root API is limited to 95 bytes; imported cup subdirectories consume
41 of those. Use a short root override for this experiment.
