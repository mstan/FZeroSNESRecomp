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
same C extractor as the game, deletes the temporary image and writes only
`.ini` and `.layout`. It refuses to overwrite different existing metadata.
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
5. Run structural extraction. For a new one-cup descriptor, for example:

   ```powershell
   python tools/parse_track_pack.py --stock path/to/fzero.sfc `
     --patch path/to/custom.bps --layout reviewed.layout `
     --id custom-author-pack --name "Custom Cup" --author "Course Author" `
     --course "first|First Course|0" --course "second|Second Course|3" `
     --out mods/track-packs
   ```

   This creates metadata after structural parsing; it does not certify playability.
   Multiple cups can be expressed by extending the manifest with unique `cup`
   entries and assigning each track to one of them. Current GP cups contain
   one to five entries. Pack limits are 32 cups and 128 course entries.
6. Qualify **every** course in the common stock and/or Deluxe engine. Check
   road/collision alignment, start position, checkpoints, finish line, laps,
   AI paths, pits, jumps, magnetic/rough/void terrain, hazards, minimap, palette,
   sky and course-name intro. Check the full cup's results and transition to
   the next course, including a one-course cup and the final course.
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
   include the archives' MSU/PCM soundtrack or redundant donor variants.

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

## Gameplay source changes

The course manifest remains data-only. Do not turn it into an arbitrary ROM
write or executable-hook format. Author-supplied gameplay ASM is separately
reviewed and adapted to the common engines; see [cgp-source/README.md](cgp-source/README.md)
for source coverage, table ownership, interaction rules and validation.
Keep optional gameplay switches off by default. Required, supported course
capabilities must be declared in the layout and scoped to those courses;
never silently enable global switches, music, tuning or difficulty. Verify stock/BS car IDs, caller register widths,
return-stack balance and any shared renderer/HUD hooks before exposing a patch.
