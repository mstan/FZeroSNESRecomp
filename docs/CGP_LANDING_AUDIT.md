# Reported CGP landings

The owner identified Marine City I's trampoline and Lightning's railroad.
Both failures are reproduced with the previous native landing predicate and
survive with the property-driven predicate from the author-supplied P3test ROM.

| Replay | Approach coordinate | Actual landing tile | Old check | Fixed / donor |
| --- | --- | --- | --- | --- |
| Marine City I trampoline exit | 6996, 1468 | DB at 6996, 1480 | Fatal | Survives |
| Lightning railroad | 824, 1448 | F7 | Fatal | Survives |

Both tiles have ground flags `00`. The stock engine assumed every tile ID at
or above D0 was a fatal landing; custom courses use those IDs for valid road.
The fix follows the decoded surface properties, including the separate pit
and jumping-state rules. It does not disable death or add per-course immunity.

The full Marine City trampoline section was also traversed from 6972, 724:
the car repeatedly bounces, exits beyond Y=1480 and lands safely. Matching
local replays in the P3test ROM corroborate the bounce/landing transitions.
The comparison uses real course geometry, ordinary surface sampling and the
race loop after injecting an approach state. It is not a manually driven lap.

`tests/validate_hazards.py` passes 40 cases: both reported landings, the
trampoline crossing, and neighboring invalid surfaces, across retail, stock
BS and combined CGP vehicle modes, with optional rules off or all enabled.
Four negative controls restore the native predicate and reproduce the deaths.
Six snapshot cases save immediately before landing, resimulate ten frames
identically, and reset while retaining SRAM. CGP cases also switch cartridge
cohorts before restoring the state. Required course behavior works without
enabling optional ASM switches. The adjacent invalid surfaces remain fatal.

Private evidence: `captures/feedback-20260923/hazard-qualified`,
`hazard-replay` and `hazard-maps`. Only test code and coordinates are committed;
the donor ROM, decoded maps, state files and captures remain private.
