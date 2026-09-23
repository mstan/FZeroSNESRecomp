# F-Zero 55 preview.5: tester notes

Extract this ZIP into a **new folder** and run `FZeroSNESRecomp.exe`. Select
your own original F-Zero (USA) ROM. Keep older build folders and saves until
you have finished comparing them. No ROM, personal settings, saves or MSU
music are included.

## What is included

- The original three leagues remain available unchanged.
- CGP adds 40 courses beyond the original 15: 30 CGP courses and 10 corrected
  BS courses. It also offers revisions of the original 15 as separate cups.
  Its eleven cups are revised Knight/Queen/King, corrected BS-1/BS-2, and
  Baron, Scepter, Crown, Zenith, Falcon and True.
- Bower adds five courses. MAX is visible again and defaults off. With all
  three packs enabled, there are **16 cups / 80 selectable course versions**.
- Grand Prix and Practice use the vertical league list. Practice can preview
  courses across cups and select any enabled rival car.
- Car selection keeps the native Satellaview style, with continuous movement
  between columns and neighboring cars visible on both sides. Each enabled
  P1/P2/P3 set now stays together in its own four-car column. Original cars
  appear once; their optional rebalances remain independent.
- Car minimap markers use their authored colors. Moon Shadow no longer
  alternates pink/blue with its exhaust animation; Dragon Bird no longer
  inherits Wild Goose's green marker.
- Marine City's trampoline and Lightning's railroad landing have targeted
  fixes. Required course features accompany their courses automatically.
- Volcania uses the author's approved replacement venue. The retired artwork
  is no longer an accepted CGP course source.

## Choosing cars and settings

A fresh installation enables **BS Satellaview vehicles**: the original four
cars plus Blue Thunder, Luna Bomber, Green Amazone and Fire Scorpion, retaining
their original individual behavior.

CGP's three vehicle packs are opt-in. Enabling one disables BS vehicles;
enabling all three provides **12 distinct selectable cars**, including the
original four. The four original-car rebalances are separate, default-off
options. A new CGP car's artwork, handling, boost and exhaust travel together.

Grand Prix uses the selected car's enabled CGP group for its three main rivals:

| Group | Cars |
| --- | --- |
| P1 | Moon Shadow, Dragon Bird, Great Star, Death Anchor |
| P2 | Blue Falcon, P. Emerald, Golden Fox, Black Bull |
| P3 | White Cat, Wild Goose, Red Gazelle, Fire Stingray |

These groups do not require enabling the original-car rebalances. Practice
allows any enabled car as the opponent. With a corresponding CGP pack off,
original cars use their original Grand Prix rivals.

Vehicle choices are independent of track packs. The original **BS tracks**
and CGP are mutually exclusive because CGP includes corrected BS courses.

Fresh defaults enable CGP and Bower tracks, giving 15 cups / 75 versions.
MAX, CGP cars, original-car rebalances, optional gameplay rules, Legend
difficulty and the F-Zero 55 title artwork start off. Existing settings are
respected. No soundtrack is enabled by a track pack.

Rewind is on by default: **R** on keyboard. **D / C** are left/right shoulder;
**F7** opens the save-state menu. Bindings remain configurable.

## Known limitations

- Car information cards still retain original-car numbers and color styling
  (for example, Dragon Bird can show Wild Goose's green card). Those displayed
  numbers are not a reliable description of CGP handling. Driving behavior
  uses the separately checked per-car data.
- Changing enabled vehicle packs or gameplay rules can select a different
  record namespace, making earlier times appear absent. Existing files are
  retained. Keep the same configuration when comparing times.
- Save states from older previews or another mod configuration may be
  incompatible. Begin a new race for this preview; keep older installations
  if you need their states.
- Full-race AI and Legend parity, ghost replays, every hazard on every course,
  and complete cup playthroughs are not exhaustively qualified. Focused checks
  passed; this preview is not a claim of complete parity with every CGP ROM.

## Helpful testing and reports

Please include the build version, enabled mods (especially vehicle packs,
rebalances and Legend), GP/Practice, league/course, car/rival and difficulty.
Describe the steps, expected result and actual result; a screenshot or short
video helps. Mention whether a save state or rewind was involved.

Useful next tests include complete Marine City and Lightning races, up/grip
magnets, CPU starts and later laps with Legend on/off, every car in the GP
groups, Practice rivals from different groups, and completing entire cups.
For performance reports, include aspect ratio, HD scale and presentation FPS;
optional local performance logs are described in
[the diagnostics guide](docs/PERFORMANCE_DIAGNOSTICS.md).

The source qualification includes 12 core checks, 44 GP roster cases, 41
Practice catalog cases, 83 native-menu cases and 22 marker cases, covering all 12 CGP identities
and all 16 cups. Bower's five courses were loaded through both engines and GP
progression was checked through injected finish transitions. Carousel,
results-screen, Practice heading and targeted landing checks were also run.
These are targeted automated checks, not full manual race playthroughs.
