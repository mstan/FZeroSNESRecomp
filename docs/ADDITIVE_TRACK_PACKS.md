# Additive course library prototype

The local `fzero-55` release parks MAX League with a shipped
`assets/track-packs/max-league.hidden` marker. Its implementation, manifest,
patch and records are retained; it is hidden in Mods and held off, including
when old settings enabled it. CGP defaults to **11 cups / 55
courses**, with BS vehicles independently enabled. Remove the marker or set it to `0` to expose MAX again.

The game imports course resources into the canonical F-Zero/BS Deluxe engine.
It adds cups to a scrolling **in-game Grand Prix league menu**. The launcher
enables/disables individual packs; there is no master mod or cup dropdown.

The menu contains Knight, Queen and King, then CGP's two corrected BS cups
and six new cups. Original BS tracks have their own switch and are mutually
exclusive with CGP. The four BS cars have a separate switch that works with
either track set or stock courses. MAX adds five courses when unhidden,
bringing the total to 60.
Additional manifests append additional cups. With BS vehicles disabled, imports
use the four-car roster. With all imported packs
disabled or unavailable, the normal game menus remain active. Obsolete
`library.disabled` settings are ignored; each pack controls its own cups.

## Installation and contribution

[The mods README](../mods/README.md) contains player instructions followed by
instructions for LLM contributors. [PARSE_MANIFEST.md](../mods/PARSE_MANIFEST.md)
documents the metadata format, typed resource layout, deterministic tools,
unknown-patch workflow and gameplay qualification checklist.

MAX Classic and CGP P1 IPS patches are bundled with attribution in
`assets/track-packs`. They are discovered automatically alongside their
manifests. Drop additional IPS/BPS files into `mods/track-packs` beside the
executable. Bundled packs use their fixed included patch and expose only an
enable checkbox, ignoring old file selections and duplicate user copies. Known MAX
Classic and Modern patches are recognized by verified output SHA-256 and
contribute one identical five-course pack. User manifests and layouts live
beside their patches; the shipped registry is read-only. New registry entries
are discovered without adding another pack-specific branch in game code.

An optional ZIP installer is available:

```powershell
python tools/import_track_pack.py --stock path/to/fzero.sfc `
  --archive "path/to/F-Zero MAX League.zip" --library build/mods/track-packs
```

The bundled IPS patches are the original files from the supplied archives.
No patched ROM, decoded resource binary, generated native source or MSU audio
is committed. MAX League is by PowerPanda and Zephyrum25; its original readme
is preserved as `assets/track-packs/MAX-League-credits.txt`. CGP attribution
and patch provenance are in `assets/track-packs/CGP-credits.txt`.

CGP's three patches are equivalent course donors, so any one is sufficient.
The manifest excludes its 15 retail courses, includes its 10 corrected BS
courses, and retains the original race order for its 30 new courses. None match MAX geometry. Its custom palette
cycles use one bounded typed resource and a common engine callback; no
donor executable code is installed by the course importer. The original
MAX hash region remains unchanged, preserving existing imported record keys.
The ZIP installer accepts repeated `--archive` arguments and scans nested
IPS/BPS entries; it does not copy MSU audio or other archive contents.

CGP is by Worthy MF, Fennor Virastar and its contributors; full credits and
the release description are on the [author's release page](https://romhackplaza.org/romhacks/f-zero-community-grand-prix-cgp-super-nintendo-romhack/).
The course adapter retains the common roster and rules. The author's ASM
is exposed separately as 19 opt-in gameplay mods; see
[source coverage and adaptation](../mods/cgp-source/README.md).

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
`experiment/additive-track-packs`, now preserved on local `fzero-55` branches.
The game pins its paired framework and launcher commits. Build with
`SNESRECOMP_ROOT` and `RECOMP_UI_ROOT` pointing at the corresponding local
worktrees until their changes are integrated. The
normal generated stock and Deluxe modules are still required, as on main.
The current local desktop executable is `build/FZeroSNESRecomp55.exe`;
`FZERO_DESKTOP_NAME` leaves the normal output name unchanged in ordinary builds.

ROM-free checks cover patch syntax/checksums, alternate verified hashes,
manifest identities, directory discovery, independent missing/disabled packs,
malformed resource bounds, a pit at checkpoint zero, clean record defaults,
record isolation and corrupt-file preservation. Existing renderer, video,
HDMA, gamepad and mod independence tests also run.

Validate the staged bundled defaults with no user patches:

```powershell
python tests/validate_bundled_tracks.py --build build --stock path/to/fzero.sfc `
  --out captures/bundled-qualification-new
```

This checks the bundled IPS digests and attribution files, pack toggles,
hidden-pack enforcement, stock/BS gameplay, active imported packs, duplicate
user copies and obsolete master settings. Bundled `<pack-id>.ips` or `.bps` companions appear
as checkbox-only mods before ROM selection; hashes are verified on Play.

Private qualification (requires the owner's original ROM and patch archive):

```powershell
python tests/validate_track_packs.py --build build --stock path/to/fzero.sfc `
  --archive "path/to/F-Zero MAX League.zip" --out captures/qualification-new
```

All five MAX courses were loaded and driven in the common Deluxe engine.
Stock, King and BS-1 gameplay, in-game sixth-cup navigation, IPS/BPS discovery,
equivalent-patch deduplication, partial single-course installs, removal/restore
and individual pack disabling were checked. Save/load replays compare ten frames with
identical WRAM and master clock, and soft reset retains SRAM. The native GP
transition routine was also exercised through all five course loads and a
one-course cup using injected completed-result states. That boundary test is
not a claim of manually driving every lap or qualifying all finish-line and
hazard behavior.

CGP qualification is reproducible with:

```powershell
python tests/validate_cgp.py --build build --stock path/to/fzero.sfc `
  --archive "path/to/F-Zero CGP P1.zip" --archive "path/to/F-Zero CGP P2.zip" `
  --archive "path/to/F-Zero CGP P3.zip" --max-archive "path/to/F-Zero MAX League.zip" `
  --out captures/cgp-qualification-new
```

The suite compares all 55 extracted resource hashes across all three variants,
checks the 40-course manifest and MAX geometry, loads every imported course,
and compares loaded tile pools/blocks/grids with extraction. For road cells
changed by mine explosions, it compares the initial road before driving.
It exercises all eight cup transitions, native leagues, MAX, stock engine,
snapshot replay/reset, single-variant installs, BPS, disabling/removal, and
identical record namespaces across variant changes. Inputs and evidence remain
private under `captures`; patched ROMs and decoded assets are not committed.
The two bundled IPS files are the explicitly included distribution inputs.

The local CGP run passed 52 integration cases (`captures/cgp-q3`) and all
12 normal CTests. A separate donor interpreter run corroborated Port Town III's
loaded blocks/grid, checkpoint coordinates and computed heading array, including
the eight bytes rewritten by a mine explosion. Rebuilding the extractor from
the prior committed source confirmed all five MAX hashes remain unchanged.
These checks do not claim full manual completion of every course or every
hazard interaction. The MSU soundtrack remains outside this prototype.

## Current limits

The decoder supports the MAX/CGP FZEdit resource representation, not every hack.
An unfamiliar binary format or donor-only hazard/event needs a new typed
adapter and qualification. Structural parsing alone cannot prove playability.
The current GP adapter accepts one to five tracks per cup. Imported Practice
selection, cross-pack assembled cups and a combined records browser are future
work. Native Practice includes only enabled native courses. The CGP MSU
adapter is separately opt-in and falls back to SPC without matching audio. Library snapshots require the same
catalog; records survive adding/removing unrelated packs.

The save-root API is limited to 95 bytes; imported cup subdirectories consume
41 of those. Use a short root override for this experiment.

## Independent gameplay mods

The course provider and gameplay settings are independent. `BSVehicles` and
`BSTracks` replace the old combined `BSDeluxe` setting; old configurations
migrate both flags, then enabled CGP takes precedence over original BS tracks.
Changes to mechanics use a rules-specific save root and snapshots include the
assembled canonical image digest plus an adapter version. Imported cup record
identities remain stable when unrelated packs change.

The reviewed source patches are compiled from blank images at build time and
applied only when their individual mod is enabled. They do not execute a donor
engine or replace the common rendering/HUD hooks. The implementation composes
Legend's CPU tables and magnet steering explicitly with vehicle tuning.

Run `tests/validate_cgp_rules.py --build build --stock path/to/fzero.sfc --out
captures/rules-new` for the private rule matrix, instruction probes and replay
checks. Imported Practice selection remains outside the course prototype;
disabled original BS courses cannot be selected through native Practice.

The split-content implementation passed 62 gameplay cases (including both
engines, standalone options, all profiles, Practice, Legend, eight-car energy
boost and six save/load/reset replays), 64 course qualification cases covering
all 40 imports and eight cup transitions, nine bundled-install cases, and all
12 CTests. The shared interpreter redirect regression harness passed 113/113
checks. MSU with a missing soundtrack correctly returned to SPC on both engines;
no soundtrack is enabled by default. Local evidence is under
`captures/rules-qualified`, `captures/cgp40-validation` and
`captures/bundled40-validation`.
