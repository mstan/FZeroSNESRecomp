# Legend acceleration integration

The previous host hook recomputed a shared acceleration index for every CPU on
every call. That bypassed the donor's opening Straightaway phase and explains
the reported identical starts.

The adapter now reads each actor's `$0D71+X` index. Normal vehicle indices retain
their own acceleration. The author's routines select shared indices `$94..$9A`
when a main CPU leaves the last Straightaway checkpoint (`$00:E334`), when the
purple starter is initialized (`$00:D410`), after a finish (`$00:F077`), and at
the lap/rank updates (`$00:99A9`, `$00:E9A9`). The adapter consumes those indices
without predicting them from the current rank. It bounds reads to the 29-byte
shared table, whose last entry is zero.

Legend still applies after vehicle tables. Independently enabling it also
includes the movement-range change at `$00:96F6`; this does not select any
vehicle rebalance. Inspection of the canonical engine shows CPUs already skip
that component clamp, while a non-boosting player normally does not. The shared
support prevents the player's movement from retaining that lower component
limit under the higher-speed rules.

Validation in both engines covers per-car player/CPU acceleration equality
before handoff, all shared indices over 32 speed steps, all five checkpoint
handoff difficulty values, actual extended movement instructions, difficulty
menu selection and bounded practice opponent inputs. Each P1/P2/P3 combined
configuration passes snapshot resimulation and reset. Private evidence is in
`captures/feedback-20260923/legend-validation`.

Longer matched driving traces, collision behavior and the expanded roster
remain part of final qualification; these focused checks do not certify all
AI behavior on all courses.
