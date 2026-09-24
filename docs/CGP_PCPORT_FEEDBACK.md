# CGP PC-port feedback burndown

Owner: `beads-8wg.5.54`; branch: `fzero-55`.

- [x] Replace the soundtrack with **F-Zero CGP P1 MSU PCPORT.zip** (29 recordings), physically remove superseded staged recordings, and keep attribution exclusions enforced.
- [x] Restore the CGP energy HUD lightning symbol and speed underline while preserving stock presentation for stock configurations.
- [x] Use CGP's own Community GP title artwork. Retain the F-Zero 55 backend and artwork but hide its launcher choice, per the owner's follow-up.
- [x] Retire the ending-credits rule from runtime, presets and visible mods without shifting saved rule identities.
- [x] Reproduce and fix malformed race-results lap/rank text (separate from ending credits).
- [x] Reproduce and fix MAX League Metal Forest palette flashing.
- [ ] Reproduce MAX League Port Canyon flashing; direct starts and native GP progression have remained stable in captures.
- [x] Run focused checks, build and record evidence.
- [x] Commit the completed changes on `fzero-55` (preview.10).

## Evidence and constraints

The replacement archive contains only an empty MSU descriptor and 29 PCM files,
566,847,024 PCM bytes in total. Their hashes match the corresponding older
recordings. None of the eight confirmed CosmicTailz exclusions is present.
The owner subsequently confirmed that this replacement archive already
reconciles attribution, resolving `beads-8wg.5.53`. Its exact digest and
clearance are recorded in `assets/music/CGP_ATTRIBUTION.md`.

## Findings

- Removed 24 superseded recordings from each of nine staging/build/capture
  directories: 216 physical files deleted after verifying their hashes.
  `music/cgp` and `build/assets/music/cgp` now match the replacement exactly.
- CGP contains its own **Community GP** title artwork. The title patch imports
  only its reviewed title graphics/palette regions. The retained F-Zero 55
  patch is still available to the backend for a future pack.
- CGP vehicle images now import the authored lightning symbol and speed
  underline, without changing HUD layout, digits or shared palettes.
- The ending-credits rule is hidden, masked out of saved settings/presets and
  omitted from generated executable patches. Its bit stays reserved and its
  source stays available for attribution.
- MAX's manifest omitted its native palette-cycle table at `$10:8711`.
  Metal Forest deliberately has an empty cycle list; inheriting Deluxe's
  default list animated unrelated background colors. Importing the actual
  table stops that flashing. Port Canyon's authored cycles were already
  equivalent to the fallback; no difference was observed there.
- Successful results retain the frozen course behind the table. Native setup
  at `$03:99C5` sets `$5F` bit 7. The renderer previously recognized results
  only by the disabled background layers used after a loss. Continuing from
  a successful finish into phase 5 therefore selected race-HUD positioning
  and split the lap/rank sprites across the viewport. Results now keep their
  table centered, ignore stale car-sprite ownership, and retain the native
  color-window placing numeral throughout the fade.

## Focused validation and private fixtures

Private files below are under ignored `captures/cgp-pcport-feedback/`; they
contain ROM-derived state and are not distribution assets.

- `livewide-cgp-crossing-resultfade/before-continue.sav`: completed CGP race,
  immediately before Start continues. Reloaded successfully and advanced to
  the next course. This was created with memory editing; it is a transition
  fixture, not evidence of a legitimately completed race or record.
- The fixture moves the player across the native finish line on the last lap,
  providing cumulative BCD times in `$0E90`, ranks in `$0F30`, and a final
  clock in `$00C0`. Native finish processing produces the results screen.
  `FZERO_STATE_SAVE` with `FZERO_TEST_SAVE_FRAME` exports such fixtures without
  the lifecycle test's reset. `FZERO_STATE_LOAD` checks mode compatibility.
- Captures `002199` through `002224` cover stable results, Start, and the fade.
  The lap/rank area is pixel-identical between centered 4:3 and 21:9 output
  over all 26 frames, at both 1x and 4x HD, with interpolation alpha 0.5.
  Evidence: `livewide-cgp-crossing-resultfade/focused-validation.json` and
  `results-fade-fixed.png`.
- Renderer regression checks cover successful/failed results, stale vehicle
  reservations, placing-window alignment, all supported aspects and fades.
- MAX Metal Forest captures with/without the palette table demonstrate the
  unwanted rotations disappearing. Port Canyon was checked at the starting
  grid both directly and after completing MAX's first course. Private
  `progress-max2-cars/grid.sav` preserves that second-course starting grid.
- Stock and BS stock configurations retain the retail POWER/underline tiles;
  an independently rebalanced Blue Falcon uses the CGP donor HUD tiles.
- Focused music-import, mods, video, course-parser, track and renderer checks
  pass. Desktop, headless and capture-renderer builds succeed.

Port Canyon remains an open reproduction item. The soundtrack attribution
review is resolved by the owner's confirmation of the replacement source.
