# Qualify a course manifest

This is the contributor workflow for the `fzero-course-v1` adapter. It is also
the analysis checklist for an LLM handling a submitted patch. Save findings in
the owning issue and put validated, ROM-free descriptors in
`assets/track-packs` (known registry) or `mods/track-packs` (local pack).

## Automatic path

The game fingerprints the verified result of each loose IPS/BPS against the
registry and neighboring manifests. MAX Classic and Modern are registered as
equivalent course donors. Their normalized course hashes were compared;
accepting equivalent revisions must never depend on their filenames or titles.
Historical CGP P1/P2/P3 share identical course resources. The bundled manifest
now accepts only the author's revised P3test image, which replaces the retired
Volcania venue; the old images are not accepted alternates. It exposes 55: slots 15-24 are the
corrected BS courses and slots 25-54 are the new courses. Slots 0-14 have
authored revisions and appear as separate Knight/Queen/King CGP cups, preserving
the native originals. CGP and the original BS track provider are
mutually exclusive; BS vehicles are an independent option.
The new courses follow the donor's GP permutation, not numeric resource order.
No new CGP course matches MAX's normalized tile pool/block/grid resources.
Bower is registered separately: seven internal slots, but its five-course GP
table selects `6,4,5,3,2`. Do not import unused slots `0,1`. Its same-named
CGP counterparts have different resource data. CGP's authored menu labels are
decoded through its pointer table, retaining stable manifest IDs; see
[the Bower/CGP audit](../docs/BOWER_AND_CGP_LEAGUES.md) for addresses and evidence.

To validate and emit known metadata explicitly:

```powershell
python tools/parse_track_pack.py --stock path/to/fzero.sfc `
  --patch path/to/patch.ips --out mods/track-packs
```

The tool uses `build/FZeroInspectCourses.exe` (override `--inspector` on other
platforms). It creates a temporary private patched image, parses it with the
same C extractor as the game, deletes the temporary image and writes
`.ini`, `.layout` and a ROM-free `.audit.json`. It refuses to overwrite different existing metadata.
For the reviewed FZEdit loader family, it decodes the music/order table addresses
from donor instructions, checks original intro names, validates race order and
records each donor song versus any explicit override. A layout must supply the
matching music table. Unknown instruction layouts or text encodings stop for
review; they are not guessed. Cup-name verification remains a manual step and
is explicitly marked as such in the audit report.
The game itself needs neither that tool nor an installed Python interpreter.

## New revision or unknown format

1. Obtain the author's patch/readme and identify the exact required source
   ROM. Apply the patch to a fresh private source. Verify source and target
   SHA-256 and BPS checksums. Keep qualification ROMs and decoded assets private.
   Preserve author attribution with any bundled patch.
2. Identify the track format from editor documentation or the donor's loader.
   Trace pointers and consumers; a changed-byte list alone does not establish
   a resource's ownership. Compare donor data with WRAM/VRAM after an actual
   load. Inspect any code changes for course features the common adapter
   cannot express. Do not add arbitrary executable ranges to a manifest.
3. If it matches the typed format below, supply its pointer-table addresses.
   Otherwise implement a separately versioned resource decoder and tests.
   Preserve the canonical engine contract. If a course depends on unsupported
   behavior, report it and leave that pack unavailable until implemented.
4. Choose stable lowercase pack/cup/course IDs. Do not reuse another pack's ID
   to replace it. Within a cup, manifest track order is race order. Source
   indices select courses from the donor, not slots in the canonical game.
   Preserve authored course and cup names; read the actual donor menu or its
   accompanying documentation. Decode its original music selector independently
   of venue graphics. Record names, cup membership, race order and songs in the
   audit. Document any intentional deviation in [MODS.md](../MODS.md).
5. Run structural extraction. For a new one-cup descriptor, for example:

   ```powershell
   python tools/parse_track_pack.py --stock path/to/fzero.sfc `
     --patch path/to/custom.bps --layout reviewed.layout `
     --id custom-author-pack --name "Custom Cup" --author "Course Author" `
     --source-cup 0 --course "first|First Course|0" --course "second|Second Course|3" `
     --out mods/track-packs
   ```

   This creates metadata after structural parsing; it does not certify playability.
   `--source-cup` identifies the zero-based donor cup for a selected subset,
   including single-course packs. The audit preserves that cup's relative race
   order and lists omitted slots. Course names in this example are placeholders;
   use the donor's real names. Rearranged compilations or different order loaders
   need explicit review rather than bypassing a failing audit.
   Multiple cups can be expressed by extending the manifest with unique `cup`
   entries and assigning each track to one of them. Current GP cups contain
   one to five entries. Pack limits are 32 cups and 128 course entries.
6. Qualify **every** course in the common stock and/or Deluxe engine. Check
   road/collision alignment, start position, checkpoints, finish line, laps,
   AI paths, pits, jumps, magnetic/rough/void terrain, hazards, minimap, palette,
   sky and course-name intro. Check the full cup's results and transition to
   the next course, including a one-course cup and the final course.
   Check the song actually uploaded to the SPC with MSU disabled and with a
   missing PCM. Test declared mechanics with their switches both off and on,
   then race a stock and unrelated imported course with the same settings.
   For CGP-style magnets, test DOWNPULL alone versus DOWNPULL+MAGNET: only the
   latter damages. Other packs keep native magnet semantics unless their
   reviewed layout declares these capabilities. Record such differences in
   `MODS.md` and the owning Beads issue, with evidence and remaining limitations.
7. Check original Knight/Queen/King and both BS leagues with the pack present;
   all enabled cars must remain selectable. Check 4:3, widescreen and HD,
   especially HUD grouping and opponents near both side edges. Confirm the
   canonical visibility hook remains installed.
8. Check valid IPS and equivalent BPS, partial installs, unrelated invalid
   files, duplicate IDs, disabled packs, removal/restoration, catalog changes,
   save/load, reset and record isolation. Never allow imported times to become
   original times. Inspect snapshots' catalog identity checks.
9. Submit descriptors, decoder changes if needed, tests and evidence. Record
   known limitations rather than filling gaps with guessed addresses. To add
   an `alternate_target_sha256`, prove all extracted resources match for the
   declared courses and repeat relevant gameplay checks. Never commit patched
  ROMs, decoded resource binaries or generated code. MAX Classic and the current
  CGP and Bower IPS files are explicitly bundled under `assets/track-packs` with attribution;
  preserve verified manifest identities and record approved revisions. Do not
   include arbitrary soundtrack files or redundant donor variants. The separately
   approved CGP PC-port soundtrack has its own import and clearance workflow in
   [the music documentation](../assets/music/README.md).

## Metadata format

See `assets/track-packs/max-league.ini` for a complete working example:

```
format=1
id=stable-pack-id
name=Display Name
author=Author
adapter=fzero-course-v1
source_sha256=<64 hex digits>
target_sha256=<64 hex digits>
cup=stable-cup-id|Display Cup|0
track=stable-course-id|Display Course|stable-cup-id|0
```

Optional repeated `alternate_target_sha256` entries accept at most eight
additional exact images. Unknown fields, duplicate IDs and missing cup
references are errors. Source/target hashes describe the unheadered images.

## Typed FZEdit layout, version 1

`format=fzero-course-1` and decimal `count` are followed by the 16 required
table fields below. Addresses are hexadecimal 24-bit **CPU LoROM addresses**,
not file offsets. High-bank ROM aliases are supported. RAM/MMIO and out-of-image
reads are rejected. All table integers are little-endian. Compare with the
checked-in MAX layout; never assume another FZEdit version shares its offsets.

| Field | Per-course entry and decoded meaning |
| --- | --- |
| `pools` | 24-bit pointer to 0x2400 bytes of track tile pool |
| `settings` | One byte, canonical environment/variation flags |
| `palettes` | 24-bit pointer to 0xe0 track palette bytes; common car/HUD palettes stay native |
| `maps` | Two 5-byte records (pointer24 + row count16); block rows of 16 bytes and packed grid rows of 16 bytes expanded to 18 |
| `graphics` | Pointer24 to 256 tiles, each palette-prefix byte plus 32 packed-nibble bytes; expands to 0x4000 Mode 7 pixels |
| `paths` | Pointer24 to 9-byte segment records and six same-bank array pointers per segment; signed deltas expand checkpoint coordinates and four AI parameter arrays |
| `names` | Pointer24 to a bounded, zero-terminated native encoded intro string |
| `sky_graphics` | Pointer24 to 0x2000 bytes of sky graphics |
| `sky_back` | Pointer24 to 0x700 bytes of back-layer tilemap |
| `sky_front` | Pointer24 to 0x540 bytes of front-layer tilemap |
| `minimaps` | Bank plus bank-relative offset; add 0x8000 to resolve the pointer, then read 0x200 bytes |
| `map_positions` | Two packed 16-bit native minimap sprite adjustments, not plain screen x/y |
| `terrain` | Pointer24 to four 256-byte tile behavior tables |
| `gradients` | One byte selecting the native sky gradient |
| `opponents` | Three class-dependent opponent parameters |
| `shortcuts` | Pointer24 to at most 16 rectangle-crossing records of 17 bytes; signed-negative 16-bit sentinel terminates |

Optional `palette_cycles` is a table of pointer16 entries in the table's own
bank. Each points to a bounded list of little-endian palette offsets, ended
by a signed-negative 16-bit word. Offsets must be distinct multiples of 16
from `0x20` through `0xf0`. Each selects eight colors in the road palette;
the shared HUD and vehicle colors cannot be addressed. An empty list
explicitly disables native road cycling. With this field absent the previous
native behavior and course hash are preserved, so existing MAX records keep
their namespace. A specified cycle list becomes part of the course hash.

AI segment marker zero terminates, 255 marks the finish-closing segment.
Checkpoint arrays are bounded to avoid overlap in native WRAM. A layout does
not carry a program, native dispatch address, hook PC or general memory-write
instruction. The game supplies one shared set of canonical loader bindings.

Optional repeated `require=<source-index-or-all>|<feature>` entries declare
course mechanics implemented by the shared adapter. Supported features are
`grip-magnets`, `up-magnets`, and `rainbow-road`. For example, CGP requires the
first two for all its courses, and `require=52|rainbow-road` for Rainbow Road.
These capabilities run only while the declaring course is active. They do not
change any user mod switches or enable music, car tuning or difficulty.
Unknown features, invalid indices and duplicate declarations are errors.
Requirements are included in course hashes, separating incompatible records
and snapshots. Layouts without requirements keep their existing hashes.

This decoder covers the MAX/CGP FZEdit resource representation, not every
F-Zero hack. These donors disable the separate native mine-list loader;
terrain-encoded mines can still animate and modify road cells during a race.
Compare initial loaded geometry before racing, or compare a mutation with
the actual donor. Hacks with additional hazards, physics, vehicles or custom
scripted events need explicit support. A successful structural parse cannot
prove those semantic features are compatible.

### Native music for imported courses

An optional `music=<CPU LoROM address>` table supplies one byte per resource
slot: the canonical SPC song index multiplied by nine (valid values 0, 9,
through 81). It selects the native upload list at `$02CB00`; it does not import
donor executable code or custom SPC instruments/sequences. Find the donor's
selection in `$00F7E0` before declaring this table; do not infer music from sky
art, cup order, or the setting/venue nibble. Both retail and Deluxe consume the
extracted value through the shared course loader, including missing-PCM fallback.

An optional repeated `spc=<decimal resource slot>|<theme>` overrides one entry
or supplies a mapping when no table is declared. Supported themes are
`mute-city`, `big-blue`, `sand-ocean`, `silence`, `port-town`, `red-canyon`,
`white-land-1`, `white-land-2`, `fire-field`, and `death-wind`. Unknown themes,
duplicate overrides, out-of-range slots and malformed donor values are errors.
Layouts without either field retain native behavior. Music metadata is separate
from course record hashes: correcting a song must not strand existing times.
See `docs/CGP_SNES_MUSIC.md` for the current complete mapping and validation.

## Records gate for every added course

Always include new cups in records validation. Finish a cup through the native
completion path, inspect its overview and every course detail, page away/back,
switch vehicles when an expanded roster is enabled, and reload the battery
save. Confirm that another vehicle has separate times and that merely browsing
does not write records. Check stock records alongside added cups. Stable
pack/cup/course IDs and normalized course data own record identity; menu order,
display labels, music and title artwork must not own it. Disabling/re-enabling
or reordering unrelated packs must not strand existing times.

`tests/validate_imported_pack.py --build build --stock path/to/fzero.sfc
--pack <id> --out captures/<new-directory>` provides course-start, music and
completed-cup records fixtures on both engines. Document fixture limitations
and supplement with visual checks; scripted finishes are not driven laps.

For a ROM-only submission, do not ship the ROM. Identify the actual published
cups first, omit unused resource slots, and derive a resource-only patch against
the stock input. Compare normalized C-extractor results before/after packing.
The reviewed Astra profile in `tools/import_astra_front.py` is an example.

## Title artwork for every imported hack

Compare the donor's title resources with stock and already ingested screens.
Keep one independent Title screen override mod; never add per-pack title
toggles. Identical artwork reuses an existing choice. For distinct artwork,
verify the native sprite layout and DMA descriptors before extracting only
the tile/palette changes with `tools/extract_title_patch.py`. Register the
reviewed patch in `assets/track-packs/presentation/screens.txt`, preserve its
attribution in that directory's README, and visually check both engines.
Different title layouts need explicit adapter work. Title changes must not
alter records, course selection or gameplay. Keep F-Zero 55 hidden until the
owner requests its return.

## Gameplay source changes

The course manifest remains data-only. Do not turn it into an arbitrary ROM
write or executable-hook format. Author-supplied gameplay ASM is separately
reviewed and adapted to the common engines; see [cgp-source/README.md](cgp-source/README.md)
for source coverage, table ownership, interaction rules and validation.
Keep optional gameplay switches off by default. Required, supported course
capabilities must be declared in the layout and scoped to those courses;
never silently enable global switches, music, tuning or difficulty. Verify stock/BS car IDs, caller register widths,
return-stack balance and any shared renderer/HUD hooks before exposing a patch.
