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

Validation: all eight courses changed by P3test enter races with every optional
rule disabled. Each passes 1,024 landing/recovery decisions and 720 up-magnet
cases. With the optional author ASM installed, all 720 magnet results match
its accumulator and carry results. This is not a claim of complete CPU-register
equivalence or full-route playability. The reported trampoline and railroad
routes still require dedicated replays.

Matched native-course runs with required features declared versus omitted
produce identical WRAM and rendered frames in both stock and Deluxe engines.
Both engines pass required-Rainbow save/load resimulation and reset. The
comparison keeps the same installed catalog and execution backend; removing
the catalog itself can change AOT/interpreter timing and is a different test.

Private evidence: `captures/feedback-20260923/{required-probes,magnet-parity,feature-scoping}`.
