# Records investigation

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
- [ ] Fix added-cup/vehicle records browsing (`beads-8wg.5.57`).

The existing adapter gives imported cups separate native SRAM images keyed by
stable cup/course identity, with per-vehicle contexts when applicable. The
native records menu has no combined additive-library browser. These are
separate concerns: preserved files alone do not prove that the menu displays
the correct courses or times.

## Confirmed defect

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

There is no new renderer defect demonstrated by this audit. The stock and
Satellaview control captures have the correct course names and accessible
record detail pages; direct/reference-renderer checks agree. The earlier
suspicion that the small BS capture had blank text was an inspection error.
The right-hand pink arrow belongs to Deluxe's native records paging.

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
| CGP completed-cup overview and title-menu reopen | **Fail: base records shown** |
| Original Knight with CGP cars/rebalances/rules | **Same context-switch failure** |
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

The audit deliberately returns **1** with a `CONFIRMED DEFECT` message and
`audit.json` while the CGP browser failure remains. Passing native/storage
checks must not be mistaken for an all-clear. Run the relevant synthetic
checks with `ctest --test-dir build -R 'fzero_(tracks|course_parser|course_saves)$' --output-on-failure`.

## Browser fix burndown

- [ ] Keep the completed cup and vehicle context through the records transition.
- [ ] Resolve overview **and detail** names/course resources from the live catalog.
- [ ] Make all enabled cups and applicable vehicle records reachable from title
      and after completion, retaining the native menu aesthetic.
- [ ] Keep original retail and BS records accessible without assigning imported
      times to their slots or labels.
- [ ] Cover CGP cars on native courses as well as imported courses: both can
      use private vehicle record contexts.
- [ ] Verify GP and Practice record browsing, best lap/total, multiple cars,
      exit/back navigation, session restart, snapshots and rewind.
- [ ] Turn the completed-cup audit green with correct names and times, then
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
  it has no human-readable cup/car index. A combined browser needs explicit
  context discovery rather than guessing which hash directory belongs to a car.
- Existing malformed-file tests verify preservation/read-only behavior. They
  are not a general recovery/migration implementation.

No production records behavior was changed in the investigation commit.
The missing browser integration remains a release blocker; the new private
test fixture and this report are the validated outcome of the investigation.
