# Additive track library experiment

This prototype adds a combined launcher cup library containing retail,
BS Deluxe, and user-supplied MAX League content. MAX Classic and Modern are
separate packs, each with the five courses Sand City, Port Canyon, Metal
Forest, Death Storm and White Fire. No MAX patch or patched ROM is committed.

The experiment starts from FZeroSNESRecomp `1686df4` and snesrecomp `bb37c87`.
Both worktrees use branch `experiment/additive-track-packs`. The paired
framework supplies `content_pack.h/.c`, `content_patch.c`, and the explicit
interpreter-program selection API; the game's original pinned submodule does
not contain those additions. Build with `SNESRECOMP_ROOT` pointing at the
paired framework worktree until the framework changes are integrated.

## Try it

The local Windows build is at
`F:\Projects\snesrecomp\_wt-fzero-track-packs\build\FZeroSNESRecomp.exe`.
`Launch-Track-Library.cmd` in that build directory opens the launcher using the
owner's existing original USA ROM. Its library already has both supplied MAX
patches. This is a separate build and saves directory from the normal install.

In **Mods → Track Packs**, select **Track Library**, then choose a cup.
Type `Track Library` in the search box to bring the group into view. The course
list appears in the details pane. Play normally through the title and vehicle
selection; the adapter highlights the chosen Grand Prix cup using the game's
own input/navigation routine. Difficulty and vehicle choices remain yours.
Return to the launcher to select a cup backed by a different pack.

For another installation, select an extracted `.ips` or `.bps` through the
Classic or Modern feature's file selector. Both formats are verified against
the original ROM and exact expected output when that pack is launched. Or use:

```powershell
python tools/import_track_pack.py --stock 'path/to/fzero.sfc' `
  --archive 'path/to/F-Zero MAX League.zip' --library 'build/track-packs'
```

`--variant classic` or `--variant modern` imports only one. For a loose patch,
use `--patch path/to/patch.bps --variant modern`. The importer preserves other
packs, the current cup selection, and existing resources. It refuses to
overwrite a different installed input. Headered original USA ROMs are accepted.

For headless or scripted launches:

```powershell
$env:FZERO_TRACK_PACKS = 'absolute/path/to/build/track-packs'
$env:FZERO_CUP = 'max-league-classic/max'
& build/FZeroSNESRecompHeadless.exe 'path/to/fzero.sfc' 1600
```

An empty cup selection preserves the existing BS Deluxe setting and ordinary
menus. A named cup overrides that setting for the current session. Selecting a
retail cup runs retail; selecting a Deluxe cup runs Deluxe. BS Deluxe's native
alternate league controls and Practice remain available within its program.

## Additive model

The framework's manifest/catalog is independent of F-Zero's guest memory.
Each pack has a stable ID, each cup and track has a local stable ID, and slots
are adapter metadata. Adding a compatible pack means supplying another `.ini`
manifest and patch resource, not extending parallel compiled menu arrays.

`assets/track-packs/*.ini` contains the MAX metadata and exact target hashes.
At runtime, `track-packs/*.ini` is scanned; each external pack gets its own
patch selector and contributes cups only when enabled and its input exists.
Its `.path` stores the owner's file path; `.disabled` preserves its toggle.
`selection.txt` stores a key such as `max-league-classic/max`.

Missing, disabled or removed content never renumbers a saved selection. A
missing selected cup reports an error; unrelated cups still launch. Restoring
the manifest/patch restores access to its existing records. Duplicate external
IDs are quarantined and reported, with no directory-order winner. Built-in
retail and Deluxe identities cannot be replaced by external manifests.

The catalog accepts a single-course pack and arbitrary cup sizes. That is the
unit to use when an author supplies only one independent course: its own
identity, patch, manifest and compatible adapter. It does **not** extract
independent courses out of an opaque whole-ROM patch or pretend that partially
supplied patch bytes constitute a valid course. MAX's five courses currently
share one indivisible patch resource per ruleset.

## Execution and records

Each imported patch is applied to a fresh verified original image in memory.
MAX changes code as well as track data, so imported packs execute with an empty
native dispatch table and the interpreter scheduler. The stock-only widening
hook is disabled for them; the shared rendering/presentation path remains
available. No downloaded native code or recompilation is needed to try MAX.

SRAM and snapshots for imported packs live under
`saves/<SHA256(pack ID + target ROM hash)>/`. This includes the full identity,
so a different pack or revision cannot inherit another pack's times. Retail
and Deluxe retain their existing save paths. Imported snapshots also include
the full identity and refuse a different pack even if copied/renamed by hand.
The existing runtime's save-root limit is 95 bytes; use a short root override.

## Current boundary and next extension

This is an additive **launcher** library, with one cartridge context per
session. It does not expand the SNES game's own cup selector, transition across
cartridges during a race, compose a Grand Prix from tracks from different
packs, merge records into one leaderboard, or add MAX courses to Deluxe's
internal ROM tables. Individual course names are browsable metadata; the
prototype selects Grand Prix cups, not individual Practice courses.

The next layer is a host-owned race controller: resolve a sequence of stable
track IDs, load the appropriate adapter at safe race boundaries, and keep
track-level records keyed by identity. A shared cup with optional tracks
should expose its available entries and mark incomplete GP lineups explicitly.
Adapters must define course loading, vehicle/rules compatibility and results
extraction. Do not treat different boosting systems as interchangeable races.

The `fzero-max-v1` adapter is qualified here for the two pinned MAX outputs.
New code-changing hacks need adapter qualification even when their IPS/BPS
syntax is valid. MSU-1 is not applied to imported packs; stock and Deluxe retain
their existing MSU support. Performance promotion to native MAX code is future
work after correctness and course coverage.

## Validation

- Ten game CTest suites, including partial/absent/disabled/removed/restored
  pack combinations, duplicate quarantine, and preservation of old settings.
- Framework patch/catalog suite and native dispatch isolation contract test.
- Four 1,600-frame gameplay runs: MAX Classic IPS, equivalent MAX Modern BPS,
  retail King and BS-1. Each reached live gameplay with changing video/audio.
- Missing and corrupt selected patches fail; unrelated content continues to
  boot. Restoring a patch restores its cup. Source ROM bytes remain identical.
- Desktop Classic race with widescreen presentation, save and reload; the
  snapshot loads in Classic and is refused in Modern, retail and Deluxe.
- Actual launcher and guest-menu screenshots inspected. Cup navigation goes
  through guest input so cursor, palettes, DMA and window masks stay in sync.

Reproduce the private gameplay checks after building with Deluxe enabled:

```powershell
python tests/validate_track_packs.py --build build --stock 'path/to/fzero.sfc' `
  --archive 'path/to/F-Zero MAX League.zip' --out captures/qualification-new
```

The output directory must be new. No full-cup completion or all-five-track
qualification is claimed. The experiment is ready for further playtesting.

MAX League is by PowerPanda and Zephyrum25. Their supplied readme also credits
Grego/Catador's FZEdit, Tiled, Alejandro/Fennor's fixes and modern boost code,
and BS Deluxe's recovered track work by GuyPerfect, Porthor and PowerPanda.
