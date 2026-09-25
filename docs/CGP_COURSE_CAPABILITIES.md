# CGP course behavior audit

The course adapter imports resources, not the donor executable. This audit
records the behavior those resources require in the canonical engine.

| Donor behavior | Common runtime ownership |
| --- | --- |
| FZEdit geometry, graphics, scenery, palettes, paths, start/finish, opponents and minimaps | Typed `FzeroCourse` resources and loader hooks |
| Four terrain-property tables (`$10:8518` in the revised donor) | Shared terrain adapter at `$00:8E36`, including forced minimum speed |
| Landing and recovery on custom surfaces | `$00:9C9A` uses decoded surface flags, `$00:EB90` uses decoded recovery depth; native tile-number predicates remain for native courses |
| Rectangular shortcuts | Decoded records evaluated at `$00:DA04`, retaining canonical shortcut effects |
| Road palette animation | Bounded palette-cycle lists; no shared car/HUD palette writes |
| Up magnets and grip magnets | `require=all` declarations in CGP's layout; shared scoped adapters |
| Rainbow Road gravity and illusion collision | `require=52|rainbow-road`; scoped to that course |
| Car tuning, boost, exhaust, difficulty, music and donor menu/HUD edits | Separate work; never inferred from a course payload or silently enabled globally |

Required features are part of the normalized course hash. Adding these
declarations deliberately changes CGP record/snapshot identities even for
courses whose geometry is unchanged. Older record files remain on disk.
Unknown required features reject the layout instead of loading a course with
incomplete mechanics. Layouts without requirements retain their old hashes.

## Magnet scope correction, September 25

The optional switches previously installed the raw `No_DMag_Damage` and
`Reverse_DMAG` patches over the common engine. Returning early from a scoped
hook then executed those patched instructions on unrelated courses. Both
patches are now supplied entirely by the existing course-aware adapters,
even when the CGP preset enables their switches.

`DOWNPULL_MAGNET` is surface bit `$08`; damage is surface bit `$04` (`MAGNET`).
CGP grip surfaces with `$08` alone are harmless; `$0C` still damages. Native
courses and imported courses without this capability keep the original `$0C`
damage mask and downward pull for tile IDs that CGP reinterprets as up magnets.
The author's independent ASM remains an oracle in the private tests, temporarily
installed and fully restored around each reference fragment only.

`tests/validate_cgp_rules.py --filter magnet-` passes 22 scenarios on retail
and Deluxe: optional switches on/off for native Knight, Bower Red Canyon III,
MAX Metal Forest and CGP Red Canyon III, plus the existing up-magnet switch
checks. Each runs 16 actual damage calls (player/CPU, grounded/airborne and
all combinations of the two flags). CGP Red Canyon III additionally validates
11 actual terrain tiles: `$A6` decodes to `$0C` and reduces energy from `$0800`
to `$07FB`. Required-only and enabled cases agree. Non-CGP courses pass 48
native downpull cases, including CGP's special tile IDs. Enabled CGP cases
retain the 720-case up-magnet ASM comparison and all five damage-mask comparisons.
Four additional runs repeat stock/MAX/Bower/CGP with all current gameplay
rules, all CGP vehicles and all four original-car rebalances enabled, matching
the gameplay portion of the CGP preset.

Evidence: `captures/magnet-scope-02` and `captures/magnet-preset-02`. This checks semantics at guest routine
boundaries and race entry; it does not certify every full hazard route. The
broader route follow-up remains tracked in `beads-8wg.5.42`. Player-facing
differences are in [MODS.md](../MODS.md).

Validation: all eight courses changed by P3test enter races with every optional
rule disabled. Each passes 1,024 landing/recovery decisions and 720 up-magnet
cases. With the optional author ASM installed, all 720 magnet results match
its accumulator and carry results. This is not a claim of complete CPU-register
equivalence or full-route playability. Dedicated Marine City I trampoline and
Lightning railroad replays now reproduce both old deaths and validate the fix;
see [landing audit](CGP_LANDING_AUDIT.md).

Matched native-course runs with required features declared versus omitted
produce identical WRAM and rendered frames in both stock and Deluxe engines.
Both engines pass required-Rainbow save/load resimulation and reset. The
comparison keeps the same installed catalog and execution backend; removing
the catalog itself can change AOT/interpreter timing and is a different test.

Private evidence: `captures/feedback-20260923/{required-probes,magnet-parity,feature-scoping}`.
