# F-Zero Forever 0.7.4: tester notes

Extract the whole ZIP into a new folder and run `FZeroSNESRecomp.exe`. Select
your own original F-Zero (USA) ROM. Keep older installations and saves for
comparison. Both downloads contain the same executable, courses and prebuilt
caches. **With MSU** includes the approved CGP and Astra music; **without MSU**
lets you add recordings later.

## Changes to test

- CGP cars keep their correct colors while the stats window opens, including
  Black Bull. The native rotating-car animation is preserved.
- Neighboring car columns remain visible through the stats and league panels.
- The expanded league/class picker uses the original shaded lettering.
- HD Mode 7 retains its detail on the frozen course behind race results.

This includes the 0.7.3 shared-records, difficulty-control, lap-recap and
renderer-performance fixes. See CHANGELOG.md in the source repository for
earlier changes.

## Packs and music

The original 15 courses remain available. The bundled CGP, Astra Front,
Bower and MAX packs add 15 cups containing 75 course versions. Required
course mechanics load with their packs. Track Pack Loader defaults on;
BS Satellaview Tracks is mutually exclusive with it. Cars and title-screen
choices remain separate. The Community Grand Prix preset applies the full
CGP experience and preserves unrelated pack choices.

Every course ships as an editable project ZIP with a prebuilt cache. Existing
projects start without conversion; changed or newly added projects rebuild
automatically. Restart after installing or removing a pack.

MSU-1 defaults off. Enable it in Settings > Audio. Put course recordings in
that pack's `music` folder, matching the course ZIP's filename:
`mods/packs/cgp/courses/huckmine.zip` uses
`mods/packs/cgp/music/huckmine.pcm`. No source picker is needed. Missing songs
use the course's SNES music. **Mods > Menu and event music** contains the
prefilled CGP menu/event cues and lets you replace individual songs.
See [MODS.md](../MODS.md) for examples and the Huckmine editor workflow.

## Testing notes

Always show Records and Legend difficulty default on; diagnostics default off.
Saved choices are respected. Please check car colors and rotation while the
stats panel opens, both neighboring columns, league/class lettering, and HD
detail as a race enters results. Include the course, car, difficulty, aspect
ratio and HD scale with any report.

The fixes were checked with native-input frame captures across all three CGP
car groups and Dragon Bird in Practice, plus the base-game menu. Lettering
matches the native menu pixels; widescreen panels and HD results were checked.
All 13 automated tests pass, including HD results at stock and wide aspect
ratios at 2x and 4x resolution.

An earlier cropped report showing the Metal Fort skyline with a BEST label
still needs a reproducible transition or capture to identify the remaining
problem. It is separate from the reproduced and fixed lap-recap digit issue.
