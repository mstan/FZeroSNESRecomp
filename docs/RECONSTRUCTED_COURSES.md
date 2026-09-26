# Editable course archives

Branch: `f-zero-forever`. Beads: `beads-8wg.5.74`. Tester version: 0.6.0.

- [x] Keep `huckmine.zip` and its twelve original author files unchanged.
- [x] Recover editor projects for all 74 other bundled course versions.
- [x] Preserve filenames, pack IDs, cup order, names, audio and mechanics.
- [x] Put compiled FZC files in the disposable runtime cache.
- [x] Compare all 75 resulting course resources byte-for-byte with 0.5.0.
- [x] Compile all 74 recovered editor projects with native preservation off;
  compare map tiles, artwork, palettes, minimaps, AI paths and opponents.
- [x] Exercise layout, intro, terrain, AI and companion metadata edits.
- [x] Check cold/warm cache behavior, damaged-cache recovery and bad metadata.
- [x] Avoid rescanning every neighboring course ZIP for each course.
- [x] Add stronger row compression for Death City, Forest IV and White Land I
  when the ordinary raw-source compiler exceeds the native row-address window.
- [x] Build and package both tester variants; retain developer files.

Evidence: `captures/reconstructed-projects-01/baseline` contains the previous
runtime course dumps. `captures/reconstructed-projects-03/validation-release`
contains exact roundtrips, independent editor-only decodes, edited-course dumps
and the validation report. The final cold 75-course scan took 11.22 seconds
here; the warm scan took 2.88 seconds. All 13 CTest checks passed.

Implementation commit: `b2a7d88da7f1fb799510a0316a5557800ab79e53`.
Both player bundles are in `release-stage/0.6.0/`, with full file hashes and
ZIP CRCs checked. Each includes 75 source ZIPs and no compiled FZC/cache files.
The no-music bundle is 17.7 MiB; the 39-recording bundle is 862.5 MiB.
`build-shared-packs` retains the developer build with generated course caches.
Obsolete installed FZC sources were backed up under
`captures/reconstructed-projects-03/previous-installed`.

A concurrent QA scan sharing the CTest extraction cache produced one
transient partial-metadata read; the subsequent single-process scan passed.
Shared-cache publication under concurrent processes is tracked separately as
`beads-8wg.2.79`. These corpus tests use their own working/cache directory.

The recovered files use FZEdit's actual formats, including CRLF checkpoint
files and 16-pixel Tiled track previews. They are not the author's original
project history. A guarded data-only companion preserves native compression,
extra terrain bits, custom intro glyphs and other details the editor does not
express. Editing a component uses normal editor compilation for that group.
The companion is validated and included in the source cache fingerprint.
No FZC, source ROM or executable ASM is hidden inside a source archive.

Course hashes and IDs are unchanged. The records key derives from pack ID,
cup ID, course IDs/hashes and vehicle/mode; renaming source files therefore
does not reset existing records. Huckmine retains its already-established
0.5.0 source-project identity. Music filenames remain unchanged, including
the original Huckmine project's `hm.pcm`.

Run the corpus check with `tests/validate_reconstructed_projects.py --build
<build> --packs <new packs> --baseline <previous runtime dumps> --out <fresh
validation directory>`. `FZeroInspectPacks <packs> --dump <existing directory>`
writes portable decoded resources for comparisons without loading a ROM.
