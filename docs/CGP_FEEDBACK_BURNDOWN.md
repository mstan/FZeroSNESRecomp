# CGP author-feedback burndown

Date: 2026-09-23. Branch: `fzero-55`. Baseline: `ced5d72` (preview.3).

**Status: approved and in progress (2026-09-23). Implementation: 2/12 complete.**
Matthew approved beginning the full burndown, including fixes, asset replacement
and a validated local Windows build. Work solo. Keep commits local; no pushes.

Planning issue: `beads-8wg.5.34`. The 12 work items below belong directly under
`beads-8wg.5` (Game: F-Zero), under `beads-8wg` (System: SNES), in the central
database at `F:\Software\beads\issues`. Beads owns status; this file is the
requested human-readable mirror. Update both at meaningful milestones. A
checkbox requires the item's acceptance evidence, not merely a successful build.

## What initial inspection establishes

- Original CGP P1/P2/P3 have identical normalized data for all 55 course slots.
  That establishes course equivalence between those three donors, **not**
  vehicle equivalence, full engine equivalence, or equality with stock courses.
- The three newly supplied 512 KiB ROMs contain distinct graphics/palette
  replacements. Banks 00/01/02, including stock stat code/data examined, are
  unchanged from retail. They are useful art references, not full CGP ROMs.
  Over file offsets `0x40000..0x5DFFF`, each matches one old CGP graphics set
  within five bytes: unsuffixed -> P1, `(1)` -> P2, `(2)` -> P3. Cross-set
  distances are about 69-71 thousand bytes. This is a mapping lead, not proof
  of every ship's identity or of a final unique roster count.
- Current code selects **one** tuning, boost and exhaust profile each (P3 by
  default). It does not apply all three sequentially. The actual design flaw
  is allowing those independent profiles to operate on stock vehicle slots
  without their matching donor ship artwork/identity. Repeated boost values
  for BS cars were an adapter choice, not verified author balance.
- ASM comments are insufficient for identity: P1 names Moon Shadow, Dragon
  Bird, Great Star and Death Anchor, while P3 contains contradictory old names
  and abbreviations. The author's later White Cat correction must be reconciled
  with the actual graphics, menu data and profile sources.
- Our course import retains typed resource data and separately selected ASM
  rules. It discards donor code. Thus “all mods enabled” is not a guarantee that
  every FZEdit/custom engine behavior in the donor has been ported.
- Legend is already applied after tuning. However, the host acceleration
  overrides at `$00:94CE` and `$1E:ACF0` apply a common difficulty/rank/lap
  calculation to CPU actors without testing the opening Straightaway phase.
  This is a concrete suspect for identical starts, pending donor comparison.
- Up magnets and grip magnets already have mod entries (`Reverse magnets`
  and `Harmless grip magnets`). Their presence does not prove correct tile
  interpretation, landing behavior or interactions with imported maps.
- Knight, Queen and King currently use native courses. CGP imports slots
  15-54 only. Any CGP revisions of the first 15 courses are currently omitted.
- P3test extracts successfully with the current layout but changes eight
  normalized payloads: Forest IV (27), Lethal Cave (30), Gold District (34),
  Sunset Drive VR (46), Volcania (49), Lightning (51), Rainbow Road (52) and
  Mercury Sea (53). The ROM differs from old P3 by 3,011 bytes, including four
  bytes in the first `0x20000`. These may include shared-resource effects;
  review before claiming that only Volcania changed.
- The release ships `assets/track-packs/cgp.ips`, `.ini` and `.layout`, not a
  CGP `.sfc` or standalone `.dat`. BS Deluxe data is embedded separately.
  The venue replacement must update the CGP payload/registry, not replace the
  unrelated BS payload. No full ROM should enter a release.
- `image.webp` shows different vehicle artwork, a lightning energy symbol,
  and noisy/corrupted-looking terrain/scenery. The second image circles POWER,
  the speed display, pixels beside the ship and S-jet indicators. It is annotated
  feedback, not an established clean reference. Identify which capture came
  from which application/configuration before deciding the intended HUD.

## Approved behavior

1. A vehicle is a stable identity with matching artwork, animation, stats,
   boost parameters, exhaust geometry, icons and menu metadata. P1/P2/P3 are
   donor sets to import coherently, not unrelated global replacement tables.
2. Keep original cars available. The owner's roster clarification below
   supersedes the earlier proposal to mix non-overlapping stock BS and CGP
   ships. Stock BS cars and CGP car packs are separate modes; CGP packs may
   coexist with one another. Determine identities/counts from evidence.
3. A CGP rebalance of a stock car is an opt-in modification of that identity,
   separate from adding ships. Shared engine support and per-ship balance are
   separate concepts. Existing settings need deliberate migration.
   CGP vehicle tuning must not leak into the stock BS eight-car experience.
4. Required course behavior accompanies the enabled pack and is scoped to its
   declared courses/features. A user should not need to guess which optional
   switches prevent a valid landing from killing them. Optional difficulty,
   balance, cosmetic choices and music remain distinct.
5. Preserve original cups and record identities. Offer genuine CGP revisions
   clearly, without creating duplicate cups for unchanged courses. Keep the
   common HUD, widescreen/culling and rendering path for all packs.
6. Replace the revoked venue before another shareable build. Review the whole
   new donor revision and prevent accepted older donors from restoring the
   retired artwork. Preserve private source evidence; do not delete user ROMs.

### Owner clarification: stock BS versus CGP cars (2026-09-23)

- The stock BS cars mod retains the existing four original cars plus four BS
  cars, each with its original handling, boost, exhaust and presentation.
  "Behave identically" is interpreted as matching that established eight-car
  experience, not giving all eight cars identical stats. Independently selected
  global rules remain separate from CGP vehicle tuning.
- Do not convert the legacy BS cars to CGP vehicle behavior. If CGP includes
  versions of those identities, import the matching CGP resources and balance
  for those versions; the legacy BS data is not their tuning specification.
- The entire stock BS cars mod conflicts with **each** of CGP car packs P1,
  P2 and P3, irrespective of how much their ship identities overlap. Enabling
  any CGP car pack disables stock BS cars. Enabling stock BS cars disables
  all three CGP car packs. Apply the same rule when loading saved settings.
- P1, P2 and P3 do **not** conflict with each other. Any subset should work;
  enabling one must preserve the others. The target is all 12 CGP donor ships
  available together, subject to the identity audit and technical validation.
  Original cars remain available; stock-identity rebalances stay explicit.
  Do not promise a total roster count before resolving duplicate identities.
- These are vehicle-pack conflicts, not track-pack conflicts. Stock BS cars
  remain usable on CGP courses. CGP cars remain usable on stock or BS courses;
  the existing conflict between original and CGP-corrected BS tracks is separate.
- Qualify all nine valid combinations of the four car-pack switches: BS off
  with each of the eight CGP subsets, plus BS on with all CGP car packs off.
  Verify transitions both ways and normalize the seven conflicting saved
  combinations deterministically, with a clear explanation in the UI.

The owner subsequently approved the full implementation burndown on 2026-09-23.

## Checklist

<a id="venue"></a>
- [x] **B01 — Replace revoked Volcania venue** — `beads-8wg.5.35`, P1.
  Audit all eight changed course payloads and code deltas in P3test. Update the
  bundled patch, registry hashes, compatibility policy and credits. Verify the
  approved venue and any shared resources; review record compatibility and
  supersede the old share build. No full ROM packaged.
  Completed source replacement: [revision audit](CGP_REVISION_AUDIT.md).
  Exact patch reconstruction, three IPS writer tests, all nine bundled-pack
  cases and race entry on all eight revised courses pass. Volcania frame
  inspected. Retired hashes are rejected; previous sharing ZIP is superseded
  pending B12's corrected build. Full hazard/vehicle qualification remains below.

<a id="roster-audit"></a>
- [x] **B02 — Identify the donor rosters** — `beads-8wg.5.36`, P1.
  Produce a 12-slot identity table covering art, names, menu previews, race
  frames, palette, stats, boost and blast pipes. Compare against retail and BS
  identities. Resolve stale names, exact set mapping and the unique-ship count.
  Identify CGP versions of BS ships from CGP donors; do not repurpose legacy BS
  balance or use the overlap audit to relax the whole-pack conflict rule.
  Completed: [12-slot identity/source table](CGP_VEHICLE_AUDIT.md), captured
  running donor information screens and verified stat/curve association. Eight
  additions plus four retail rebalances; no stock BS identities. Menu order is
  0,2,1,3, not stat-table order. Resource integration remains B03/B04.

<a id="roster"></a>
- [ ] **B03 — Add coherent, expandable vehicle packs** — `beads-8wg.5.37`, P1.
  Depends on B02. Import complete ship records, add pages/scrolling to the shared
  selector, and bind all resources by identity. Enforce stock BS cars versus
  every CGP car pack, while allowing P1/P2/P3 together and preserving originals.
  Use CGP donors for CGP versions of BS identities. Validate all nine valid
  car-pack combinations, conflicting saved settings, player/CPU identities,
  menus/HUD, partial installations, all course sources, save/load and rewind.
  More selectable ships does not itself require more simultaneous racers.

<a id="retail-tuning"></a>
- [ ] **B04 — Separate stock rebalances from added ships** — `beads-8wg.5.38`, P2.
  Depends on B02. Provide explicit opt-in changes for any retail identities CGP
  modifies. Separate required speed/boost support from stat data. Replace the
  mix-and-match profile UX and migrate saved choices; do not guess BS boost
  balance by repeating a four-slot table. Preserve all eight cars' individual
  baseline behavior in stock BS mode, without CGP vehicle tuning. Prove the
  disabled baseline is stock and toggling modes restores the correct data.

<a id="course-runtime"></a>
- [ ] **B05 — Audit missing embedded course behavior** — `beads-8wg.5.39`, P1.
  Compare complete donor code and consumers with the ASM archive and current
  adapter. Inventory tile/property, landing, death, checkpoint, hazard and event
  gaps. Record required versus optional behavior and reusable manifest/native
  capabilities. Unsupported requirements must be visible, not silently ignored.
  In progress: found and fixed two missing property-table consumers (landing
  and recovery) and the forced-surface speed floor. Existing shortcut adapter
  covers the donor's rectangle crossing through canonical checkpoint effects.
  Required up/grip magnet and Rainbow behavior now uses scoped layout
  capabilities. See `CGP_COURSE_CAPABILITIES.md` for coverage and remaining gaps.

<a id="trampoline"></a>
- [ ] **B06 — Fix trampoline-exit death** — `beads-8wg.5.40`, P1.
  Depends on B05. Identify the exact course/ship/route from the report, then
  compare height, landing, surface and boundary decisions against the donor.
  Require a replay that survives the valid exit while genuine off-track deaths
  still work. Include mod combinations and save/rewind/reset.
  Shared landing fix passes 1,024 surface/phase/recovery probes on each of the
  eight revised courses. Native tile-number checks were ignoring custom ground
  properties. Full reported-route reproduction remains pending.

<a id="railroad"></a>
- [ ] **B07 — Fix railroad-landing death** — `beads-8wg.5.41`, P1.
  Depends on B05. Identify the course and railroad property, establish donor
  landing semantics, and fix the mismatch. Keep a separate regression even if
  it shares B06's root cause. Test adjacent invalid landing surfaces too.
  Shares the confirmed tile-number/property mismatch. Probes cover high-numbered
  safe tiles and lower-numbered pits; reported railroad route still pending.

<a id="magnets"></a>
- [ ] **B08 — Validate up and grip magnets** — `beads-8wg.5.42`, P1.
  Depends on B05. Match actual donor tile flags, tilt/lift, negative height,
  grounded grip, turning, strafe, damage and landing transitions. Include CPU
  behavior where applicable and preserve ordinary stock magnets. Declare any
  necessary course dependencies rather than merely exposing more switches.
  Required-only race entry and 720 up-magnet cases pass on eight revised
  courses; optional-ASM runs match accumulator/carry results. Native-course
  frames/WRAM are unchanged by capability declarations in both engines;
  required-Rainbow save/load resimulation and reset pass. Full hazard routes
  and broader collision/landing transitions remain pending.

<a id="legend"></a>
- [ ] **B09 — Correct Legend AI and dependencies** — `beads-8wg.5.43`, P1.
  Match per-ship launch acceleration until the appropriate non-Straightaway
  checkpoint, then the intended difficulty/rank/lap logic. Preserve Legend-after-
  stats ordering and audit overlapping tables/host hooks. Check early CPU
  fighting, lead changes, path flags and higher-speed support, including Legend
  with non-CGP courses or untuned cars. Compare per-racer traces, not just feel.
  Fixed the unconditional common-acceleration override: consume the author's
  per-actor phase index instead. Standalone Legend includes movement-range
  support without vehicle stat replacement. Both engines pass focused launch,
  handoff, shared-table and movement checks, plus all three combined-profile
  lifecycle tests. Expanded-roster and longer AI traces remain pending.

<a id="visuals"></a>
- [ ] **B10 — Fix garble and reconcile HUD/exhaust/menu presentation** — `beads-8wg.5.44`, P1.
  Establish capture provenance and reproduce each highlighted difference. Audit
  terrain/sky/palette data, ship-side/exhaust sprites, energy/boost indicators
  and the deliberately omitted donor S-jet relocation. Use matched captures in
  stock 4:3, widescreen and HD. Preserve shared HUD ownership/culling; expand
  menu labels, previews and icons with the roster rather than slot substitutions.

<a id="original-cups"></a>
- [ ] **B11 — Compare CGP's first three leagues** — `beads-8wg.5.45`, P2.
  Depends on B05. Audit all first-15-course differences against stock, separating
  course edits from global car/AI changes. If genuine revisions exist, make them
  clearly selectable while retaining untouched originals and isolated records.
  Do not assume 15 additional unique courses or duplicate identical ones.

<a id="qualification"></a>
- [ ] **B12 — Qualify and package the corrected build** — `beads-8wg.5.46`, P1.
  Depends on B01-B11. Compare donor and recomp through reported hazards, starts,
  checkpoints and affected course/cup completion. Cover rosters, partial packs,
  valid Legend dependencies, rendering modes, title option and save/rewind/reset.
  Include all nine valid car-pack combinations, all three CGP packs together,
  both directions of BS/CGP exclusion and all eight stock BS baseline cars.
  Update README/PARSE_MANIFEST with mandatory capability checks and vehicle
  identity rules. Commit locally and verify a fresh Windows ZIP with no ROM,
  music or retired venue; explicitly document remaining limits.

Suggested sequence after approval: B01 and B02 first; B05 before hazard fixes;
B03/B04 after the roster table; B09/B10 can be investigated independently;
B11 before final catalog counts; B12 only after the other accepted items pass.
This is a solo sequence, not authorization to delegate to subagents.

## Evidence and validation limits

Initial inspection is recorded privately under `captures/feedback-20260923`:
`triage.json` plus the four donor course-inspection logs. No ROM or decoded
course payload is checked into this plan. The images were inspected directly.

| Input | SHA-256 |
| --- | --- |
| `E:\Downloads\F-Zero (USA).sfc` | `7266ff8b43456a6627ba4d73d6cb233e57b914f0d1170f165f4ffef4be3df1db` |
| `E:\Downloads\F-Zero (USA) (1).sfc` | `8ffc1dbe13746da35f0c8d5647b73d3cdb3fa678b775a94668798157eb538a0e` |
| `E:\Downloads\F-Zero (USA) (2).sfc` | `fc32ffa67b6a5bc31126b9adf089622c12510e201396f6f5624945fa31fd3f50` |
| `E:\Downloads\F-Zero CGP P3test.sfc` | `25383a9090265e6a4f007dc349fa224b6305c028e899c60069d63dc0bfc5ffb7` |

Prior structural extraction, race-entry and soak tests did not establish full
donor parity for hazards, starts or all vehicle combinations. No reported bug
is marked reproduced or fixed by this planning pass. The exact trampoline and
railroad courses/routes and the intended meaning of each image remain to be
established; use the supplied donors and geometry before requesting more detail.
