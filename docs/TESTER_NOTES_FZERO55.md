# F-Zero Forever 0.7.3: tester notes

Extract the whole ZIP into a new folder and run `FZeroSNESRecomp.exe`. Select
your own original F-Zero (USA) ROM. Keep older installations and saves for
comparison. Both downloads contain the same executable, courses and prebuilt
caches. **With MSU** includes the approved CGP and Astra music; **without MSU**
lets you add recordings later.

## Changes to test

- Grand Prix and Practice share records for the same course and car. Better
  Practice times from earlier builds are retained. There is no mode selector.
- The end-of-league lap/rank recap keeps its digits together in widescreen,
  including HD Mode 7.
- Up/Down stop at the first and last difficulty; Select cycles. Confirming a
  difficulty keeps that choice visible during the fade.
- Imported car groups use their own course-intro text colors from the first
  frame, without briefly showing the original group's colors.
- HD Mode 7 does less CPU work while preserving its pixels, resolution and
  interpolation. The reusable sampling work now lives in snesrecomp.

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
Saved choices are respected. Please check GP finishes, Practice records,
Records navigation in your chosen aspect ratio, difficulty selection and
performance at your usual HD setting. Include the course, car, difficulty,
aspect ratio and HD scale with any report.

The fixes were checked with native input and controlled race finishes on
stock and imported courses, including saving, restarting and browsing Records
in widescreen. Renderer comparisons retain identical output across 24
aspect/scale/interpolation combinations. Performance figures from the developer
CPU are not a guarantee for other machines.

An earlier cropped report showing the Metal Fort skyline with a BEST label
still needs a reproducible transition or capture to identify the remaining
problem. It is separate from the reproduced and fixed lap-recap digit issue.
