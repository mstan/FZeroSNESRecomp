# Contributing

## Before you start

Run `bash tools/bootstrap.sh`. It syncs and initializes the pinned
`snesrecomp/` and `recomp-ui/` submodules and fails loudly if either is
at the wrong revision. The gitlinks in this repository *are* the
dependency pins — there is no separate SHA to keep in sync.

You need a legally obtained *F-Zero (USA)* ROM staged as `fzero.sfc` at
the repo root before anything will build. `tools/regen.sh` verifies its
SHA-256 and refuses to run on anything else.

## Ground rules

**Never hand-edit generated files.** `src/gen/*.c` and `recomp/funcs.h`
are recompiler output and are regenerated from the ROM. A fix that
belongs in generated C belongs in the recompiler (`snesrecomp/`) or in a
`recomp/bank*.cfg` directive instead.

**The interpreter is the correctness floor, not a tier to live on.** A
statically compiled body must be an exact materialization of what the
interpreter does. When they disagree, the AOT body is wrong until
proven otherwise.

**Fallbacks are loud.** If the recompiler cannot resolve something, the
runtime says so on stderr and the site lands in the post-mortem report's
authorization worklist. Do not silence those messages; resolve them.

## Framework changes

Changes to `snesrecomp/` affect every game built on it. Develop them in
a git worktree of the engine and point the game build at it:

```powershell
cmake -S . -B build-dev -G Ninja -DSNESRECOMP_ROOT="F:/path/to/engine-worktree"
```

Before proposing an engine change, regenerate and soak at least this
game, and say in the PR which other titles you did or did not check.

## Debugging a divergence

The tool of first resort is the AOT deny gate — an interpreter oracle
that needs no second build and no rebuilds to bisect:

1. Regenerate with `SNESRECOMP_EMIT_AOT_DENY_GATE=1` set at generation
   time. Confirm it took: `rtl_aot_node_denied(` must appear in
   `src/gen/*.c`.
2. Rebuild `FZeroSNESRecompHeadless`.
3. Build a deny-all list from `src/gen/program_manifest.json` (every
   node's `key.pc24`, one 6-hex-digit value per line).
4. Run with `SNESRECOMP_LLE_INTERP_TARGET_FILE=<list>`. Every AOT body
   tiers down at its prologue on every entry path, so this is a sound
   whole-program interpreter oracle.
5. Compare against a deny-nothing run. Because the deny list is just a
   text file, delta-debugging which node diverges needs no rebuilds —
   edit the list and rerun.
6. **Regenerate without the variable before packaging** and verify
   `rtl_aot_node_denied` no longer appears in the emitted C. It is easy
   to ship the instrumented build by accident.

For rendering problems specifically, `SNESRECOMP_RASTER_TRACE=1` prints
the per-frame raster-IRQ replay (how many splits fired, on which lines,
and how many PPU registers each changed). Diffing that between an
oracle run and an AOT run localizes a Mode 7 corruption far faster than
comparing framebuffers.

## Validation expectations

Any change that touches the frame loop, the recompiler, or the engine
should come with a headless soak:

```bash
SNESRECOMP_FRAME_DUMP=out.ppm ./build/FZeroSNESRecompHeadless fzero.sfc 10800
```

Report the `fzero_native:` summary line and say whether the final frame
still renders correctly. A `PASS` with a garbled frame is not a pass —
the qualification gate measures activity, not correctness.

For BS car-select or HDMA changes, also run the rendered carousel regression
with your local stock ROM:

```bash
python tests/validate_car_select.py --build build --stock fzero.sfc --out captures/car-select
```

It checks every selection on both pages, stable colors and visibility for
unselected cars, and unchanged menu graphics with all three CGP profiles.
Its ROM and rendered captures stay local.

## Windows release

Update `VERSION` and `CHANGELOG.md`, regenerate using the native analysis backend
without the AOT deny gate, then configure a Release build with
`-DSNESRECOMP_BUILD_VERSION=<version>`. Run CTest and the relevant bounded game
checks. Commit the source before packaging so the manifest records its revision.
Run `python tools/make_release.py --build build-release`; this resolves runtime
DLL imports and creates a fresh ROM-free ZIP and SHA-256 file in `release-stage`.
Do not reuse or distribute a development folder containing personal ROM/config/save
files. Publish an annotated `v<version>` tag and attach the ZIP/checksum to its
GitHub release after owner authorization.

## Commit descriptions

Describe the defect and its evidence, not just the edit. If you
disproved a plausible cause along the way, say so in the commit body;
an unrecorded wrong theory gets re-investigated by the next person.
