# F-Zero Forever packs (format 1)

Put a pack folder or ZIP in `mods/packs`, then restart the game. **Track Pack
Loader** is enabled by default and adds every valid installed course pack.
An explicitly saved off setting stays off. There are no individual
pack switches. Remove a folder/ZIP to uninstall it. The original 15 courses
remain available. BS Satellaview Tracks and the loader exclude each other;
vehicle options remain separate. Invalid packs appear in the Mods diagnostics.

ZIPs contain `pack.json` at their root, or inside one enclosing directory.
Use relative paths with forward slashes. Keep each FZEdit export's referenced
files together; filenames do not have to match course IDs. Stable IDs identify
content, while names are the labels players see. Never assign another pack's ID
to an unrelated pack. Duplicate installed IDs exclude both copies.

```json
{
  "format": 1,
  "id": "my-project",
  "name": "My Project",
  "author": "Course Author",
  "order": 100,
  "cups": [{"id": "first", "name": "First League", "courses": ["coast"]}],
  "courses": [{
    "id": "coast", "name": "Coastal Run", "source": "courses/coast/Coast.fzm",
    "requires": [],
    "music": {"spc": 1, "soundtrack": "my-project", "track": 10}
  }],
  "soundtracks": [{"id": "my-project", "prefix": "My Project", "directory": "music"}]
}
```

Cup order and each cup's course order are authoritative. Cups contain 1–5
entries. Reusing a course in multiple cups does not require duplicating its
files. Pack order sorts by `order`, then installation entry name. Current
capacity is 64 catalog packs, 32 cups per pack and 128 course entries per pack.
These bounds produce diagnostics; they are not silently truncated.

## Course sources

`source` supports `.fzm` exported by FZEdit 1.2.0 (its internal file version is
`FZEdit Version 0.9`) and portable `.fzc` extracted course resources. FZM input
uses its AIP checkpoint file, CSV TMX layers, TSX properties, indexed tilesets,
palette, horizon and minimap. Image signatures determine the format: FZEdit
can save PNG data with a BMP/GIF filename. Cache files are disposable.

The source supplies SPC music unless `music.spc` overrides it. SPC indices are:
0 Mute City, 1 Big Blue, 2 Sand Ocean, 3 Silence, 4 Port Town, 5 Red Canyon,
6 White Land I, 7 White Land II, 8 Fire Field, 9 Death Wind.

`requires` declares native engine capabilities: `grip-magnets`, `up-magnets`,
and `rainbow-road`. These apply to this course, not every installed track.
Unknown capabilities reject the pack. This is not an ASM execution interface.
New data can load without a game update; a genuinely new mechanic requires
an engine implementation first. See MODS.md for the existing mechanic details.

### Pack-owned mechanics modules

Required terrain behavior is part of the pack, with no global Mods checkbox.
Put reusable module files inside the pack and reference them with `mechanics`:

```json
"mechanics": ["mechanics/terrain.json"]
```

At the top level this applies to every course in that pack. The same field on
one course adds behavior only to that course. Paths are relative to the pack
root, even when the course's FZM lives in a subdirectory. A module looks like:

```json
{
  "format": 1,
  "id": "cgp-magnets",
  "engine": "fzero-course-v1",
  "requires": ["grip-magnets", "up-magnets"]
}
```

CGP and Astra exports include a shared terrain module. CGP Rainbow Road adds
its own `rainbow-road` module on that course only. Multiple modules combine;
a course's `requires` cannot turn off its pack's mandatory mechanics. Required
modules are validated before the pack is registered; missing files, unknown
formats/engines, and unsupported requirements reject the pack with a diagnostic.

These files bundle declarations of reviewed ASM behavior. The native adapters
that implement that behavior remain in the engine; raw `.asm` is not assembled
or executed at launch. New packs may reuse supported mechanics without an EXE
update. New ASM behavior still needs a reviewed adapter. Developer distributions
retain the original ASM sources for that work.

For reviewed ROM hacks without editor source, `tools/export_runtime_packs.py`
applies the known patches privately, extracts resources, writes manifests and
checks every resource and existing record hash through a readback. The result
contains no ROM or executable donor code. `extraction.json` identifies the
source revision and record hashes. It is reconstructed data, not the original
author's FZEdit project or editing history. Unknown patches still need format
and mechanics qualification; their filenames cannot establish compatibility.

The converted examples contain CGP's 55 course versions (15 revised originals,
10 revised BS courses, 30 additional courses), Astra's 10, MAX's 5, and Bower's
5. Extraction recovers layout, tile/palette/horizon/minimap resources,
checkpoints, opponents/shortcuts, intro text, native music and reviewed terrain
requirements. It does not recover author layers, filenames, editing history,
ASM source, or absent MSU recordings. These examples preserve the 0.2.0 decoded
resources and record keys; they are not newly reinterpreted course designs.

### Portable resource encoding

`.fzc` starts with the nine bytes `FZCOURSE` followed by byte `1`. Each section
has a little-endian uint16 ID and uint16 byte length. All 18 sections are
required exactly once; unknown sections are rejected in version 1. IDs 1–16
hold pool, blocks, grid, tile graphics, palette, horizon graphics, back/front
horizon maps, minimap, terrain, intro name, AI path, opponents, shortcuts,
palette cycles, and soundtrack ID respectively. Section 17 stores explicit
little-endian scalar metadata; section 18 stores the count and 28 reserved
intro glyph slots (one code plus 32 pixel bytes each). The authoritative sizes
and scalar offsets are in `src/fzero_course_file.c`; no compiler struct layout
or donor pointers are serialized. Authors normally use FZEdit exports instead
of constructing this engine-oriented resource by hand.

Building the importer requires C++20, RapidJSON, libarchive, libxml2, libpng,
and giflib in addition to the existing game dependencies. Runtime packages
include the Windows DLL dependencies; end users do not install FZEdit or Python.

## Music

Music is optional and independently enabled in Audio settings. **Installed pack
music** uses the discovered audio; **Custom** uses a selected loose MSU source.
Use one naming rule: **the PCM matches the course source filename**, in that
pack's `music/` folder. Drop it in, restart, enable MSU-1, and select Installed
pack music. Create `music/` if it is absent.

```text
mods/packs/astra-front/
    courses/u-zero-1.fzc
    music/u-zero-1.pcm
```

For raw FZEdit input, `courses/coast/Coast.fzm` uses `music/Coast.pcm`.
Match the filename exactly, including case, without the `.fzc`/`.fzm` extension.
No MSU number, soundtrack prefix, JSON music mapping, or `.msu` file is needed.
The course display name and ID do not select the recording. Two source files
with the same basename in one pack share its PCM; different packs stay separate.
Missing recordings use SNES music. Adding music does not change course records.

CGP recordings now ship in `mods/packs/cgp/music`, following the same rule for
race songs. There is no separate `cgp-audio` folder in new bundles. Shared menu
cues retain the existing adapter filenames in that folder. Original numbered
MSU packs remain supported through the existing custom-source adapter, but are
not required for course-file matching. No Astra recordings are bundled yet.

This convention is implemented after the 0.3.0 tester ZIPs; those existing ZIPs
still need their original prefixed recordings. A rebuilt executable is required.

## Optional title artwork

`titles` is an array of `{ "id": "my-project", "name": "My Project",
"source": "title.fzt" }`. Artwork is selectable in the existing independent
screen-override dropdown; installing a pack does not select it automatically.
The reviewed extractor produces FZTITLE version-1 resources for the current
native title layout. Arbitrary donor title code is not supported.

## Maintainers and automated contributors

Preserve author names, cup/course order, native music, and capability requirements.
Do not infer music from venue graphics. Do not combine vehicle families while
extracting courses. Keep local ROMs, editor inspection files and recordings out
of Git. Qualify all course resources, then test GP, Practice and records before
publishing a pack. Compare extracted packs with the original resource hashes;
for raw sources, compare decoded layout and gameplay rather than compressor
byte ordering. Updating music alone must not reset course records.
