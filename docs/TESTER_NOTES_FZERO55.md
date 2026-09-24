# F-Zero 55 preview.10: tester notes

Extract this ZIP into a **new folder** and run `FZeroSNESRecomp.exe`. Select
your own original F-Zero (USA) ROM. Keep older build folders and saves until
you have finished comparing them. No ROM, personal settings or saves are
included. Choose **with-msu** for the replacement CGP PC-port soundtrack (about 567 MB
of audio after extraction), or **without-msu** to supply your own music.
Both ZIPs retain MSU support and contain the same game content and executable.

## Soundtrack source

Eight CosmicTailz PCM files are now excluded in source imports and builds.
The owner confirms the replacement PC-port archive already reconciles the
attribution requirement; see `assets/music/CGP_ATTRIBUTION.md`. Obsolete preview.7/.8/.9 music
ZIPs have been physically deleted, along with known excluded files from older
captures and staging. The without-msu editions contain no bundled audio.

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
- CGP information cards now show each car's authored engine, power, speed,
  weight, acceleration graph and name/frame colors. Original cars keep their
  original cards until their individual rebalance is enabled.
- Marine City's trampoline and Lightning's railroad landing have targeted
  fixes. Required course features accompany their courses automatically.
- Volcania uses the author's approved replacement venue. The retired artwork
  is no longer an accepted CGP course source.

## New in this preview

- Replace the old soundtrack with the author's complete 29-recording PC-port
  set. Superseded recordings are physically removed from local staging.
- Use CGP's own Community GP title. F-Zero 55 artwork is retained but hidden.
- Restore the CGP lightning symbol and speed underline; stock and stock BS
  configurations keep the POWER artwork.
- Remove the console ending-credits ASM from runtime, presets and Mods.
- Keep completed-race lap/rank text and the placing graphic aligned during
  the fade to the next race in widescreen and HD.
- Fix Metal Forest's flashing by importing MAX's authored palette cycles.

## Also included from preview.9

- **CGP vehicles** is now one checkbox for all three car groups together.
  Existing partial selections migrate to the full twelve-car roster (eight
  additions plus the original four). Selection columns and Grand Prix rivals
  keep their authored groups; original-car rebalances stay separate.

## Also included from preview.8

- Two download sizes: with-msu includes music; without-msu omits audio files.
  Without bundled music, select your own folder in Settings > Audio. The CGP
  preset uses custom music if already configured, or SNES audio until then.
- Remove the unsupported Install .psxmod button from F-Zero's Mods views.
  Additional IPS/BPS track packs still use the `mods/track-packs` directory.
- **Mods > Preset** applies Vanilla, Satellaview or full Community Grand Prix.
  CGP includes all three vehicle groups, four rebalances, every authored rule
  including Legend, courses, Community Grand Prix title and bundled music.
  You can change any individual option afterward.
- Presets preserve MAX, Bower, other packs and personal settings. Vanilla
  turns off the CGP/BS family; it does not silently disable unrelated packs.
- With bundled music, **Settings > Audio** offers CGP and **Custom...**. Music
  remains off on a fresh install until enabled directly or by the CGP preset.
  Custom accepts your `.msu` file; previously selected folders still work.
- Music maps the current cup/course in GP and Practice. Missing tracks and
  packs without a CGP mapping use their SNES soundtrack.
- Save/load and rewind restart the restored song. They do not restore its
  exact playback position.

## Choosing cars and settings

A fresh installation enables **BS Satellaview vehicles**: the original four
cars plus Blue Thunder, Luna Bomber, Green Amazone and Fire Scorpion, retaining
their original individual behavior.

**CGP vehicles** enables all three groups together and disables BS vehicles.
It provides **12 distinct selectable cars**, including the
original four. The four original-car rebalances are separate, default-off
options. A new CGP car's artwork, handling, boost and exhaust travel together.

Grand Prix uses the selected car's enabled CGP group for its three main rivals:

| Group | Cars |
| --- | --- |
| P1 | Moon Shadow, Dragon Bird, Great Star, Death Anchor |
| P2 | Blue Falcon, P. Emerald, Golden Fox, Black Bull |
| P3 | White Cat, Wild Goose, Red Gazelle, Fire Stingray |

These groups do not require enabling the original-car rebalances. Practice
allows any enabled car as the opponent. With CGP vehicles off,
original cars use their original Grand Prix rivals.

Vehicle choices are independent of track packs. The original **BS tracks**
and CGP are mutually exclusive because CGP includes corrected BS courses.

Fresh defaults enable CGP and Bower tracks, giving 15 cups / 75 versions.
MAX, CGP cars, original-car rebalances, optional gameplay rules, Legend
difficulty and the Community Grand Prix title artwork start off. Existing settings are
respected. No soundtrack is enabled by a track pack.

Rewind is on by default: **R** on keyboard. **D / C** are left/right shoulder;
**F7** opens the save-state menu. Bindings remain configurable.

## Known limitations

- MAX Port Canyon background flashing has not reproduced in direct starts
  or native Grand Prix progression. Metal Forest flashing is reproduced
  and fixed; report any remaining MAX issue with the course name.
- With CGP cars enabled, an unrebalanced Blue Falcon can still inherit its
  group's energy-boost indicator layout. This also occurs in preview.5;
  per-car boost/exhaust isolation remains under investigation.
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
Practice catalog cases, 83 native-menu cases, 22 marker cases and 53 new
information-card comparisons, covering all 12 CGP identities
and all 16 cups. Bower's five courses were loaded through both engines and GP
progression was checked through injected finish transitions. Carousel,
results-screen, Practice heading and targeted landing checks were also run.
These are targeted automated checks, not full manual race playthroughs.
