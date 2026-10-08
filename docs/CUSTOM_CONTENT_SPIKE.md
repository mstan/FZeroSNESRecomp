# Custom Content importer spike

The prototype adds a separate **Custom Content** page to F-Zero's launcher.
Choose **Import…**, pick a source, edit its display name, and import. The host
checks it on a worker thread, shows the result, and loads accepted courses when
Play is pressed. Select an FZM beside its companions for unpacked projects.
There is one Import button; folder import and folder-opening controls are not
exposed by F-Zero. Existing Mods
still controls gameplay features. The original 0.7.4 release was not changed.

The shared recomp-ui page is opt-in through a nullable C provider. It owns the
picker, name dialog, progress and installed-content display. The game owns
formats, conversion, on-disk layout, compatibility checks and worker lifetime.
Another game can implement that provider without inheriting F-Zero parsing.

## What works

- Ready pack folders/ZIPs and single raw FZEdit projects/folders/ZIPs.
- IPS/BPS, ZIP downloads, and raw/headered SNES ROM hacks through a
  standalone Windows helper. No end-user Python installation is required.
- Automatic recognition by verified target hash. Equivalent IPS/BPS pairs in
  a ZIP are resolved automatically; incompatible/ambiguous inputs are rejected.
- New FZEdit-family revisions can be decoded from supported loader patterns.
  After course validation, a review form asks for pack/cup names and shows a
  cup dropdown for each detected course. Labels can change without changing
  record identities. No memory addresses or manifest editing are required.
- CGP, Astra Front, Bower and MAX conversions retain their reviewed course and
  cup IDs, names/order, SPC music, supported mechanics and title policy.
- Display names are separate from save identities. Duplicate IDs are rejected;
  existing folders, saves and source files are never replaced.
- Staging records included pack IDs outside user manifests. CGP, Astra Front,
  Bower and MAX are labeled as included and cannot be replaced by an import,
  even if their data folder is missing. There is no removal control for them.
- Imports validate in an isolated staging directory before publication. Course
  archives share bounded expansion limits; linked and escaping paths fail.
- Unsupported resource layouts produce an explanation and persistent report
  under `mods/import-reports`. Supported new layouts wait for review without
  installing anything; cancelling leaves the installed collection unchanged.
- `mods/CONVERSION.md` explains both input paths and how to qualify a new hack.

ZIP music follows reviewed course/menu mappings or a matching course filename.
Unmapped, ambiguous or damaged recordings warn without blocking validated
courses. A native Astra patch-plus-audio check retained all ten course ZIPs
byte-for-byte, renamed one recording and reported one unresolved recording.

## Deliberate limits

This is a spike on `spike/fzero-custom-content`, using recomp-ui's
`spike/custom-content-page`. It has not been merged into the release branch.

Supported data-loader patterns are inferred from ROM instructions rather than
download names or a list of approved hashes. Unrecognized layouts still need a
reviewed extraction profile. Arbitrary donor ASM is not translated or executed:
new mechanics need a native adapter. Interactive course-only imports explain
the exclusion of donor vehicles, menus and global game rules before import.
Raw FZEdit projects contain no ASM declaration and use standard rules unless
wrapped in a manifest declaring supported mechanics. Consequently bare HM.zip
is an editor-data import; its CGP magnet declarations come from the packaged
Huckmine example. The UI reports that distinction after raw imports.

Multi-course raw folders need a manifest defining cup membership/order. The
importer does not sort filenames and pretend that is the author's league.
Raw project IDs derive from source contents; changing the display title does
not change them. Authors maintaining revisions should supply a stable pack ID.

Title choices refresh on the next launcher opening. Course discovery refreshes
before Play. There is no in-place replacement or uninstall operation.
Audio-only ZIP import and arbitrary
non-course mod formats are not implemented; existing pack music conventions
remain unchanged. Windows helper packaging is demonstrated; other platforms
can use the documented maintainer conversion command.

## Validation

- Shared UI: 25 checks, including NULL-provider invisibility and busy-state
  launch guard; integrated launcher build and 1100×880 visual capture. The real
  Windows file picker, edited name and successful import were exercised together.
- Interactive review: host lifecycle check covers source-bound answers, edited
  labels, invalid-cup correction, partial-music success and cancellation. Shared
  UI checks preserve edits while polling/retrying and prevent Play or a second
  import while a review is pending. All review callbacks remain opt-in.
- Native importer: 21 integration cases using the original HM.zip, ready packs,
  display naming, duplicate imports, malformed inputs, bounded nested archives,
  uppercase extensions, included-ID protection, ZIP ROM routing, music-only
  guidance, visible conversion warnings and transactional failure. ROM-free subset is in CTest.
- Converter: ROM-free checks cover reviewed and inferred conversions, stale
  answers, invalid assignments and unsupported resource structures. All 75 converted editable course ZIPs exactly
  match the existing shipped examples. The 74 reconstructed courses match full
  native course bytes; original Huckmine retains its existing author source.
- Full original Astra ROM, MAX Modern, IPS/BPS, equivalent patch ZIPs and
  headered ROM inputs passed. Unsupported targets produce report-only output.
- Native importer invoked the standalone helper successfully with Python/MSYS
  absent from PATH; Bower installed correctly and unknown targets left reports.
- Both downloaded ZIPs passed the packaged helper's review/resume flow with
  Python/MSYS absent from PATH. Each installed ten validated course ZIPs in an
  isolated test directory, preserved the detected record hashes and left its
  source archive unchanged. MF copied ten recordings and flagged four;
  FZero55 flagged all thirteen recordings for manual placement.
- Existing course-parser, music-source and per-course save checks passed.

An independent adversarial review found an existing reconstruction-JSON stack
overflow and archive edge cases. Those were corrected and added to regression
checks. This validates import/data fidelity; it is not a driven-race QA pass.

Integrated testing also caught a Windows path-length failure with Astra inside
the deeper UI staging directory. Decoder caches and the converter's inspection
workspace use short temporary paths; validated output is published on the
destination volume after copying completes.

## Downloaded-pack experiments

These downloads are local test inputs, never part of the included collection:

| Download | Donor | Course resources detected | PCM recordings |
| --- | --- | ---: | ---: |
| F-Zero MF.zip | BPS | 10 | 14 |
| FZero55 (1).zip | IPS | 10 | 13 |

Both archives now pass automatic course extraction and editable-project
roundtrip validation without an exact-revision profile. Each exposes ten
detected names in the review form, initially grouped into two cups of five.
The user supplies cup labels and confirms assignments; these are not claimed
as recovered author menu labels. The form explains that custom vehicles and
global code changes are excluded. Data validation does not certify full races.

MF's recognized MSU selector provides ten course recording mappings. FZero55's
different selector is not recognized: courses can import with their original
SPC selections, with a warning to add recordings manually. Neither experiment
is part of the included collection.

The owner requested the final validation through the real picker by hand.
Leave both experiments absent from the prototype's installed packs for that
pass. The baseline included collection has 75 course entries whose normalized
record hashes match the existing extraction manifests.

## Rebuilding the prototype

Build the game plus `FZeroExportCourses`, `FZeroInspectPacks` and optionally
`FZeroImportContent` against the pinned shared UI. Build the standalone helper
as described in CONVERSION.md, then stage it and its dependencies:

```powershell
python tools/stage_content_importer.py --build build `
  --converter build-content-converter/FZeroConvertContent.exe
```

The local development build is `build-import-ui/FZeroSNESRecomp.exe`. It is a
prototype, not a new release bundle. The helper and sources remain available in
their separate development directories.
