# F-Zero Forever release versioning

Owner decision: 2026-09-24. Tracked by `beads-8wg.5.55`.

Development continues on `f-zero-forever`, created from `fzero-55` at
`eba1adcfb1cc457400094e15e929f84b31532bb0`. The earlier branch and all existing
preview bundles keep their history. This change does not produce a new release.

## Version policy

`VERSION` is the shared source of truth for this branch's game and bundles.
The new baseline is **0.1.10**, corresponding to the work already completed.
There is no new 0.1.10 download.

Before the next release, review the changes and update `VERSION`:

- Fixes, maintenance or packaging changes: **0.1.11**.
- Any new feature: **0.2.0**.

Continue that rule afterward: fixes increment the patch number; a feature
increments the minor number and resets the patch number to zero. A release
containing both features and fixes takes the feature increment. Branch creation
and this versioning setup do not themselves count as a new gameplay feature.

Both Windows variants (`with-msu` and `without-msu`) use the same version,
as do any other platform bundles produced for that release. Do not continue
the old `1.8.3-fzero-55-preview.N` sequence or give variants independent numbers.
Use the plain version without a `fzero-55-preview` label.

## Preparing the next release

1. Update `VERSION`, add the matching changelog entry and refresh tester notes.
2. Reconfigure the existing build so `SNESRECOMP_BUILD_VERSION` matches
   `VERSION`; an old CMake cache can retain the prior preview version. In
   PowerShell, quote the full CMake definition to preserve dotted numbers:

   ```powershell
   $releaseVersion = (Get-Content -LiteralPath VERSION -Raw).Trim()
   & 'C:\Program Files\CMake\bin\cmake.exe' -S . -B build "-DSNESRECOMP_BUILD_VERSION=$releaseVersion"
   ```

3. Build and perform checks appropriate to the actual changes, then commit.
4. Package both music variants with `tools/make_release.py`, omitting
   `--label`. Its version checks require the cache, executable and `VERSION`
   to agree. A fix release produces filenames such as
   `FZeroSNESRecomp-0.1.11-windows-x64-with-msu.zip` and
   `FZeroSNESRecomp-0.1.11-windows-x64-without-msu.zip`.

The Linux build script also reads `VERSION`; do not override it with an old
preview version. Keep all already distributed bundles unchanged.
