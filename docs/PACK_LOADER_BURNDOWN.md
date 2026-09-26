# F-Zero pack loader burndown

Baseline: `002c6d4` (0.2.0), branch `f-zero-forever`.
Owning issue: `beads-8wg.5.65`; shared PCM hook: `beads-8wg.2.76`.

The approved design is one all-or-nothing Track Pack Loader. Installed folders
and ZIPs use `pack.json`; courses can use raw FZEdit exports. No old-config
compatibility layer. BS tracks exclude the loader; vehicles remain independent.
The existing course runtime is the parity oracle, not a replacement engine.

- [x] Identify committed pre-Astra-MSU baseline.
- [x] Inspect FZEdit 1.2.0 bundled documentation and export reader/writer formats.
- [x] Define versioned JSON manifests and portable course resource encoding.
- [x] Implement bounded folder/ZIP discovery and diagnostics.
- [x] Decode raw FZM/AIP/TMX/TSX/indexed image exports without external tools.
- [x] Convert CGP, Astra, MAX, Bower with full decoded-resource comparisons.
- [x] Replace track mod entries with loader; update presets and BS exclusion.
- [x] Move course mechanics and title selection data into pack metadata.
- [x] Namespace SPC/MSU mappings and support separate audio packs.
- [x] Validate records, GP/Practice, reload/rewind and synthetic MSU routing.
- [x] Package and inspect local 0.3.0 test builds with/without CGP music.

## Findings

FZM is a property file referencing component files. AIP explicitly stores main
and branch origins, checkpoint coordinates, and path/main/green/purple flags.
FZEdit map music is an SPC selection independent of venue. Raw imports must
preserve these values and reject unknown mechanics rather than run donor ASM.
Private inspection material stays under ignored `captures/fzedit-source`.
No Astra recordings were provided; only synthetic audio can be tested today.

## Validation evidence (local, 2026-09-25)

- 75 exact resource roundtrips and unchanged record hashes; folder/ZIP equivalence.
- GP start on Astra, Bower, CGP and MAX (`captures/pack-loader-qa-final`).
- Actual FZEdit 1.2.0 Mute City export decoded and raced; corrected native row
  pointer offset during visual validation (`captures/raw-fixture-fixed.png`).
- CGP and Astra records: completion, detail pages, vehicle isolation, state and
  battery reload in stock-car and expanded-car modes (`pack-records-*-01`).
- Synthetic CGP/Astra PCM routing, Practice, missing song fallback, rewind and
  shared installed audio directory (`pack-music-01`, `pack-music-installed-01`).
- All 13 CTest targets pass; shared PCM resolver test passes.
- Unsafe ZIP paths, unknown mechanics, missing resources and duplicate IDs are
  rejected without partial registration. Completed ZIP caches are reused.
- No author Astra recordings tested or bundled; those are not available yet.
- Both ZIP variants staged with payload/hash checks and recursive DLL closure.
  Packaged desktop booted for 180 frames with only bundled DLLs on PATH and
  discovered all four course packs (`captures/release-smoke-030.log`).
  Final bundles live under `release-stage/0.3.0-ready`.

## Required mechanics and defaults (2026-09-25)

Owning issue: `beads-8wg.5.67`.

- [x] Remove grip magnets, up magnets and Rainbow Road from the Mods list.
- [x] Scope their required behavior to course/pack mechanics modules; reject
  unsupported or missing modules before registering a pack.
- [x] Export CGP/Astra terrain modules and a course-only Rainbow Road module.
- [x] Default Track Pack Loader and Legend on; diagnostics remain default off.
  Preserve explicit saved choices and the Vanilla/Satellaview presets.
- [x] Document all 29 ASM sources and distinguish compiled adapters from pack
  declarations. General gameplay options remain editable across packs.
- [x] Validate all 75 course/record hashes remain unchanged; 14 headless cases
  check scoped mechanics with stock/BS cars, native/MAX/Bower isolation, and
  CGP mechanics with Legend disabled (`captures/pack-mechanics-qa-03`).
- [x] Tracks, Mods and video configuration tests pass; rebuilt launcher opened.

Developer pack payloads are in `captures/pack-mechanics-payloads`. This update
has not replaced the existing 0.3.0 tester ZIPs; it is in the local build only.

## Course filename music (2026-09-25)

Owning issue: `beads-8wg.5.68`.

- [x] Match a course source basename to `music/<basename>.pcm` in its own pack.
  No new filename maps, prefixes, MSU numbers or `.msu` descriptor are required.
- [x] Detect these recordings for Installed pack music, including packs with no
  soundtrack metadata; preserve existing custom MSU support and shared cues.
- [x] Stage the approved CGP race songs by course filename inside `cgp/music`;
  future bundles omit the separate `cgp-audio` pack.
- [x] Validate Astra GP/rewind, Bower without MSU metadata, missing-song fallback
  without borrowing another pack's same-named file, and CGP Practice playback
  (`captures/course-filename-audio-02`). Tracks/music tests and six importer
  tests pass; all 75 imported course record hashes remain unchanged.

No new release ZIP was generated. Local safety review rejected recursive
deletion of old generated `build/mods/packs/cgp-audio`; its verified files remain
in place with `pack.json` renamed to `pack.json.retired`, preventing discovery.
