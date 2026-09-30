# Records investigation and browser fix

Investigation: `beads-8wg.5.56`. Browser fix: `beads-8wg.5.57` (P1).
Branch: `f-zero-forever`. Audited baseline: `2733f38`, using the current
preview.10 code and CGP cars/rebalances/all gameplay rules. Music was off.

Reported: blank/broken records-menu rows after completing a league.

- [x] Checkpoint existing work: clean at `2733f38` before investigation.
- [x] Audit existing coverage: record isolation/reset/corrupt-file unit tests
  exist, but no complete-league records-menu integration check.
- [x] Reproduce the screenshot using private-ROM completion fixtures.
- [x] Trace native record writes, league completion and records rendering.
- [x] Check stock, BS and CGP configurations, including battery-save restart.
- [x] Audit discovery and persistence when course packs are added/removed.
- [x] Record limitations and focused validation; commit with this document.
- [x] Fix added-cup/vehicle records browsing (`beads-8wg.5.57`).

## Implemented browser

The native three-cup overview now pages over the enabled catalog. L/R shoulder
buttons change cup pages and X/Y change vehicle context. GP and Practice now
share each course/car's records. Arrows select courses, A/Start opens native detail, and B
returns/exits. The selected/completed cup and car provide the initial focus.
Page/car changes retain the menu frame without replaying its fade or music.
Unplayed courses remain accessible and show empty times.

The native detail screen uses the selected cup/course's names and minimap.
Imported courses display their decoded horizon layers and palette in the
original venue strip. Native total-time and best-lap
rendering remains in use. Browsing assembles a read-only SRAM view from stable
cup/vehicle keys; it never installs that view as a writable record context.
Exiting restores the original SRAM and WRAM mirror. Missing/bad files stay
untouched. The snapshot trailer retains its original size; browser state fits
in the unused key field while viewing, so rewind and existing states still load.

0.7.2 restores the full native 256x56 scenery strip and removes the added car/mode
heading. Transparent horizon pixels now use the black menu backdrop instead of
unused palette entry 96, which caused U Zero's gray star/planet rectangles.
Mode 1 layer priority and native RGB expansion are respected. Per-frame fades,
navigation and snapshot restoration pass for retail, BS, imported and widescreen
menus. Music tests also cover the public Huckmine ZIP name independently of its
internal `hm.fzm` filename.

The separate cropped Metal Fort skyline with the BEST race HUD has not reproduced
as corruption; the visible buildings correspond to the imported course artwork.
That cropped report remains unconfirmed in `beads-8wg.5.71`. The separate
scattered result digits were reproduced and fixed on September 30 (below).

Two additional defects surfaced in validation and are fixed:

- A fresh stock-engine/track-pack session did not create its base save directory
  before creating `courses`, so its first records file could not be written.
- Legacy PPU snapshots omit VMAIN. Cold-loading a Deluxe records state left it
  at `$00`, shifting every high byte of later detail DMAs by one VRAM word.
  Restoring the native records-mode `$80` latch fixes labels, venue and minimap.
  Fresh-boot and loaded-state captures now agree on the native DMA tilemap.

## Transition and lettering follow-up (2026-09-27)

`beads-8wg.5.77`: left/right detail navigation briefly exposed a native donor
name (often Port Town I) and could replace the venue graphics before fading.

- [x] Keep the displayed course and overview/detail layout until the native
      loader finishes the next page. Store this presentation in the browser
      snapshot, separate from the course requested by input.
- [x] Retain course labels, car icons and imported scenery throughout the fade,
      using actual PPU brightness. Freeze long-name scrolling during fades.
- [x] Restore retail's native fade-out command (`$038471`, `$60 = 2`) before
      entering detail reload state 4. Deluxe already issues its own fade.
- [x] Use the original tall green/white course lettering, underline and white
      cup lettering for both stock and imported details. Read native glyphs
      from ROM and use a course's explicit glyph resources where provided.
      Unsupported characters fall back to the readable small font.
- [x] Verify every transition frame: enter detail, both navigation directions,
      return to overview, scenery and labels, mid-fade snapshot restoration,
      and preservation of persistent records.

`tests/validate_records_transitions.py` covers retail, original BS, standalone
Astra with CGP vehicles/rules, and the same imported configuration in widescreen.
All four pass; private captures are in `captures/records-transitions-04/`.
The restored **MUTE CITY** lettering also matches the exposed original native
lettering pixel for pixel. The existing 13 CTests pass.

```powershell
python tests/validate_records_transitions.py --build build-shared-packs --stock <stock-ROM> --packs <pack-directory> --out captures/records/new-transitions
```

This earlier coverage tested the Records browser; it did not cover the live
end-of-league lap recap described below.

## Shared times and live GP recap (2026-09-30)

`beads-8wg.5.81`: remove the GP/Practice dimension from native course/car keys
and the Records UI. Imported cups already shared their keys. Read better times
from the old native Practice files, merge each cup's top ten and best lap,
and write only the unified context. Existing GP keys remain unchanged.

`tests/validate_unified_records.py` completes a native Knight and imported
Astra GP, enters Practice with those times, crosses the real finish line with
a faster time, and restarts GP to check persistence. The visible Records page
must contain both times. Its 16:9 center must exactly match the native 4:3
layout, and browsing must leave persistent records unchanged.

The scattered lap digits finally reproduced when the completion fixture was
allowed to retain the final fly-away animation. This recap remains in live
race scene 2 (`$C3=$11`, `$0975` active), reusing OBJ slots 0..63 for the table.
The renderer was anchoring some of those digits as racing HUD elements. Keep
the table centered while retaining the live race projection and BG HUD.
The times themselves were intact.

The former fixture skipped exactly this sequence. Set both
`FZERO_TEST_RECORDS_CUP=1` and `FZERO_TEST_KEEP_FINISH_ANIMATION=1` to reproduce
it with the `ROUTE` from `tests/validate_records.py`. The private Zenith run
completed 10,000 frames; frame 6000 reproduced the tester's corruption before
the fix. All five tables now align. Renderer tests cover the reused slots,
including rank-like tiles, at all supported aspects and HD off/2x/4x.

Private evidence: `captures/feedback-0930/gp-full-finish/`, `recap-fixed/`,
`unified-03/` and `records-fades/`. The latter repeats every Records transition
for retail, BS, imported courses and widescreen, including snapshot reload.

At the audited baseline, the adapter already gave imported cups separate
native SRAM images keyed by stable cup/course identity, with per-vehicle
contexts when applicable. The native menu lacked the corresponding browser.
Preserved files alone did not prove that it displayed correct courses or times.

## Original failure

The completed Baron cup writes five valid BCD best times to its private
`records.bin`. The final result fade enters native scene `$54,$55 = 00,02`.
`FzeroTracksMenuTick` calls `FzeroTracksRecordsSelect(NULL)` on **every**
`$54 == 0` frame, restoring base SRAM before records initialization. Scene 00
also contains the records overview/detail states; it is not just the title.
The native completed-course marker list remains, so the screen shows the
reported Knight/Queen/King headings, empty rows and completion icons.

This is broader than that condition. Native overview/detail code has only
retail/BS course tables. `current_course()` rejects all scene-00 states and
the vehicle-record selector is also disabled there. Merely retaining CGP
SRAM would put CGP times beneath **wrong stock course names** and still omit
other cups. No such partial workaround was applied.

The initial overview audit did not demonstrate a renderer defect; the stock
and Satellaview overview controls had correct names and accessible details.
Follow-up inspection of cold-loaded detail captures exposed the separate
VMAIN restoration issue described above. The right-hand pink arrow belongs
to Deluxe's native records paging.

## Validation

`tests/validate_records.py` runs a fresh private directory for each setup.
The headless-only completion fixture seeds lap/checkpoint state, drives across
the real finish trigger and lets native record-writing code run. It skips the
final winner fly-away animation after the native writes; this does **not**
validate a physically driven full league or that animation.

| Check | Result |
| --- | --- |
| Retail Knight: five native writes, overview, record detail, battery reload | Pass |
| Original BS configuration / Knight: same checks | Pass |
| CGP Baron, all cars/rebalances/rules: five native record writes | Pass |
| CGP completed-cup overview and title-menu reopen | Pass after fix; Baron times displayed in its own cup |
| Original Knight with CGP cars/rebalances/rules | Pass after fix; Moon Shadow records accessible |
| Vehicle and page cycling, detail course navigation, back/exit | Pass |
| GP/Practice context switching; empty-context detail | Pass |
| Actual rewind ring and snapshot reload | Pass |
| Loaded detail VRAM equals native WRAM DMA source | Pass |
| MAX, stock engine: fresh save, overview/detail, paging | Pass |
| Original BS Forest I detail with MAX enabled | Pass; native BS resources retained |
| Start Baron again with the same car/settings | Saved times reload |
| Add MAX and Bower with those records present | Same record key and times |
| Disable CGP, then re-enable it | Original file preserved; times reload |
| Storage/parser/catalog unit checks | 3/3 pass |

The fixture total is `1:30.32`; the native SRAM value is `81 30 32` (the
high bit marks a valid record). Every course's written total and the reloaded
values are checked, not just file existence. The native detail capture also
shows the fixture's `0:09.92` best lap. Private evidence is under
`captures/records/audit-20260924/`; ROMs, saves and captures are not committed.
The additional native-Knight/CGP-car control is in
`captures/records/cgp-native-finish/` (same five native writes, empty base SRAM
at overview entry). Thus the failure is not limited to imported course data.

Reproduce using a new output directory (the tool refuses to reuse one):

```powershell
python tests/validate_records.py --build build --stock <stock-ROM> --out captures/records/new-audit
```

The formerly failing audit now returns **0** with no defects. Current private
evidence is in `captures/records/browser-fix-01`, with navigation/native/MAX
coverage in `browser-nav-04` and the final no-fade navigation check in
`browser-nav-05`. Each run uses fresh private directories. Run the follow-up:

```powershell
python tests/validate_records_navigation.py --build build --stock <stock-ROM> --fixture <audit>/cgp --out captures/records/new-navigation
```

This cycles every enabled car/shared context and cup page, checks detail/back,
GP/Practice separation, saved views, actual rewind/resimulation, compares the
native detail DMA against VRAM, and verifies that browsing changes no records
files. It also completes native Knight with Moon Shadow and MAX on the stock
engine. `--navigation-only` repeats the shorter checks after UI-only changes.
Synthetic checks: `ctest --test-dir build -R 'fzero_(tracks|course_parser|course_saves)$' --output-on-failure`.

## Browser fix burndown

- [x] Keep the completed cup and vehicle context through the records transition.
- [x] Resolve overview **and detail** names/course resources from the live catalog.
- [x] Make all enabled cups and applicable vehicle records reachable from title
      and after completion, retaining the native menu aesthetic.
- [x] Keep original retail and BS records accessible without assigning imported
      times to their slots or labels.
- [x] Cover CGP cars on native courses as well as imported courses: both can
      use private vehicle record contexts.
- [x] Verify GP and Practice record browsing, best lap/total, multiple cars,
      exit/back navigation, session restart, snapshots and rewind.
- [x] Turn the completed-cup audit green with correct names and times, then
      repeat the unrelated-pack and disable/re-enable preservation checks.

## Storage limits to account for

- Keys identify a whole cup and, where enabled, the selected vehicle. They are
  independent of unrelated packs and filesystem ordering.
- Records also live below a gameplay-signature directory. Changing rules or
  vehicle configuration can select another directory; this audit does not
  certify migration across settings or releases.
- Changing a member course's content changes the cup key, so older files stay
  on disk but are not automatically discoverable under the revised cup.
- `records.bin` stores a digest, native SRAM and the retail WRAM record mirror;
  it has no human-readable cup/car index. The browser computes keys from the
  current catalog and enabled vehicle identities instead of guessing filenames.
- Existing malformed-file tests verify preservation/read-only behavior. They
  are not a general recovery/migration implementation.

The browser is read-only; record deletion is deliberately not forwarded to
native guest-slot deletion commands. It covers contexts in the current save
root/catalog, not automatic migration across older gameplay signatures,
cartridge save-name prefixes, or changed course resources.
