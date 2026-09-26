# Adaptive renderer — 1.1.0

Implemented in the isolated `feat/fzero-adaptive-renderer` worktree, tracked
by central Beads `beads-wrca` beneath the F-Zero epic `beads-8wg.5`. The owner
accepted the visual checkpoint and authorized integration/release.

## Independent plugins

| Plugin | Settings | Disabled behavior |
|---|---|---|
| Widescreen (`fzero-widescreen`) | 16:9, 21:9, 32:9, Fit | Stock 4:3 |
| Presentation FPS (`fzero-presentation-fps`) | Auto, 60, 90, 120, 144, 165, 240, 360 | Original cadence |

`fzero-video.ini` beside the executable persists `EnhancedRenderer`, `Aspect`,
`PresentationEnabled`, and `PresentationFPS`. Initial combined-checkpoint
settings migrate automatically. `FZERO_VIDEO_CONFIG` overrides the path for
isolated validation. Ctrl+F6 cycles aspect; Ctrl+F7 enables/cycles FPS.

## Rendering and gameplay

`fzero_renderer.c` owns immutable double-buffered frames containing per-line
PPU registers, palette and OAM, plus VRAM, stock pixels and published game RAM.
It samples Mode 7 beyond the stock viewport, composites Mode 1 backgrounds and
sprites, and preserves colour math, windows and fades. It never writes guest
state. The shared PPU's smaller wide buffers remain disabled. Internal widths
are 342, 448 and 682 pixels at 224 lines; scaling preserves stock pixel aspect.
Fit clamps to 4:3–32:9.

The skyline uses overlapping 512-by-56-pixel strips, rather than a single
wrapping background map. Retail `$A60C` selects a strip through vertical scroll
while keeping horizontal scroll in 0–255. BG1 spans 896 panorama pixels with
scroll bases 36/92/148/204; BG2 spans 768 with bases 92/148/204. Padding beyond
the stock-visible overlap caused scenery to disappear or change abruptly in
the wide margins. The compositor now maps margin pixels into the full panorama
and samples the corresponding strip's first 256 columns. This applies only to
the recognized skyline layout and leaves the stock center and guest state
unchanged.

The `fix/widescreen-background-culling` regression checks both panoramas across
all aspect modes and rotation boundaries, including partially filled final
strips. All five CTest suites pass, and the new panorama test fails against the
old renderer. Comparison of 104 saved-frame/aspect pairs (stock races, an
attract route and BS Deluxe Forest) preserved every center and lower-track/HUD
pixel; 50 outputs corrected skyline margins. Before/after inspection at 21:9
confirmed that the abrupt skyline cutoff in the captured turn is gone.
Both Windows hosts built successfully. A bounded SDL dummy-driver desktop run
completed 2,200 simulation frames at the 144 FPS presentation setting (5,270
presentations, zero missed); this is an automated smoke check, not a human
playtest or display-performance measurement.

Race BG3 and HUD reservations anchor to the outer edges. The power meter's
composed fill follows its outline. Title screens extend their live track and
skyline to the selected viewport; flat selection/loading screens extend the
PPU backdrop with its brightness and colour-window effects. Original title/menu
artwork and course-intro text remain centered. Hidden player/effect reservations stay hidden even when their
stale tile data lies within the expanded viewport.

OBJ slot 47 is a player-relative spark, not a HUD reservation. Retail
`$00:BED7..BF5E` writes its coordinates through `$02BC..02BF`, adding the
player's `$0C70/$0C80` position. It stays centered with the car rather than
following the right-edge HUD anchor. The regression covers 16:9, 21:9 and
32:9 while checking that neighboring HUD slot 46 still anchors right.

OBJ slots 48..51 also change owners: `$00:EDB3/$00:EE93` reuse them as
the first pieces of the player's explosion and smoke (continuing through
slot 63). Moving those four pieces to the rank HUD anchor splits the effect.
The compositor only anchors those slots when their published tile numbers
are rank digits, `$180..$189` or `$190..$199`, written by `$00:A8B1`.
This uses the current scanline's artwork, so direct snapshot loads and
transitions between rank, explosion and smoke need no previous-frame state.
Regression coverage checks all aspects, native/2x/4x output and interpolated
presentations, including restoration of rank anchoring after the effect.

HUD anchoring begins when race setup has installed its graphics (`$55=2`,
`$56!=0`), not only when active racing begins (`$55>=3`). Retail `$8ACD`
installs the HUD and `$8B11` advances the setup substate. Waiting for active
racing left the already-visible HUD at 4:3 positions during setup (24 frames
in the captured stock attract transition, longer during a GP start). BG3,
the sprite reservations, and the power-meter fill share this readiness flag.
The earlier intro/setup text remains centered. Its spare-machine icon/count
uses temporary OBJ slots 126/127, written by `$B164` at `$03F8/$03FC`, before
moving to race slots 22/23. Those temporary counters use the same right anchor
from their first visible frame. No previous-frame layout is
latched, so reset and direct snapshot loads use the correct layout immediately.

The `fix/hud-transition-layout` regression passes all five CTest suites and
fails against the old renderer. Across 440 captured frame/aspect comparisons
covering stock attract, GP start and BS Deluxe attract, 105 wide setup outputs
correct their HUD positions; stock-width, menu, intro and active-race outputs
remain identical.

Vehicle identity comes from DMA ordering pointers `$0AC0..$0ACA`, which select
six 32-byte reservations. Used-tile counts at `$11D0` suppress unused opponent
reservation tails. Interpolation follows identity across OAM sorting changes,
rejects changed attributes and large motion, and resets on discontinuities and
loads. Camera interpolation handles periodic coordinates and rejects scene jumps.

GP and Training loss `$54=2,$55=6` both retain the live race HUD during
YOU LOST, including the timer, power fill and counter. The black results screen
comes later, in scene `$54=3`, and reuses temporary counter slots 126/127.
Its top-left BG3 score stays left anchored and its counter stays right anchored;
the lap table and menu stay centered. Do not copy the race power meter or extend
the collapsed colour window there. The original one-column edge residue remains.

Scene 3 phase 5 is shared by the results-menu fade and live race/attract exit.
Use the native result-setup flag (`$5F` bit 7), not the phase alone.
Successful results retain the frozen course (main `$97`); failed results have
no track backgrounds (main `$94`, BG3 + OBJ). A live race retains BG1/BG2
(main `$17`). In particular, END GAME reuses OBJ slots 20..30; interpreting
those as race HUD slots splits its text as soon as the fade starts. Both menu
choices retain the results layout until black. Regression tests cover these
layouts and all 16 brightness values at every supported aspect.

Training's course-selector map also reuses slots 126/127; those are map pieces,
not the GP spare-machine counter, and stay with the other centered map pieces.

The opponent projection routine `$00:DBC4` runs through the interpreter so a
pre-opcode policy at `$00:DCC6` can extend its horizontal interval `[-32,288)`
by the current viewport's extra columns. Original callers own activation flags,
allocation-related distance metrics, graphics selection and disappearance.
Depth/longitudinal limits and pool size retain original behavior. This changes
actual game state, not just drawing; aspect changes can affect gameplay. No ROM
patch or generated-C edit is used.

## Mode 7 draw distance in the margins

PowerPanda reported that widescreen draw distance still followed 4:3: pieces of
the track were missing and appeared only once they reached the middle of the
screen. The cause is retail's own streaming, and it is sized for the stock
256-pixel viewport.

`$03:9243` keeps exactly one 1024-by-1024-unit world square uploaded into the
Mode 7 tilemap. `$00:97C3` takes the camera `$0B70`/`$0B90` minus 512,
`$03:9268` adds the `$0A:ED00` look-ahead for the camera angle `$0BD1` (a
trapezoid clamped to plus or minus 256 units), and `$03:92AA` clamps the anchor
to one 16-unit block per frame; `$03:9254`/`$03:925E` keep it in `$00A8`/`$00AA`
(`$0020`/`$0022` hold the same value but are reused as scratch and do not
survive every frame). `$03:9327` and `$03:9362` then pick one block row and one
block column, `$03:939E` and `$03:9417` build them at `$7F:4A00`/`$7F:4B00`, and
`$00:829B` uploads 256 cells each through DMA channel 0 to `$2118`. The tilemap
is 128 by 128 tiles - 1024 by 1024 pixels - so that square fills it exactly and
the map aliases the 8192-by-4096-unit world every 1024 units.

A sample outside the square therefore does not read empty space: it reads the
tiles some other part of the course left in the same cell. The stock viewport
accepts a little of this in its aliased horizon band; a widened viewport samples
much further to each side of the same scanlines and reads outside it far more
often. This is not a compositor defect - the centre and margins of one frame are
the same sampler over the same VRAM, and rendering one capture at 16:9, 21:9 and
32:9 leaves every shared column identical outside the anchored HUD.

Widening the streamed square is not available: the tilemap is fully occupied by
it. The compositor instead resolves those samples from the course tables that
`$03:939E` itself streams from, all of which live in WRAM bank `$7F` and are
already carried in the frame snapshot. `$03:93BF` walks three indirections:
`($B0),Y` selects a block from a 32-by-16 grid of 256-unit cells (`$00:9F4C`
places the grid), the block id times 32 picks one of sixteen 16-unit sub-rows in
the `$7F:5000` pointer table, and that sub-row lists sixteen pointers to 2-by-2
tile groups whose four bytes read (x0,y0) (x0,y1) (x1,y0) (x1,y1) - the order
`$03:93E5` stores them to `$4A00`/`$4A80`. The grid spans the whole world, so
every position resolves and there is no void case. Samples inside the square
still read the live tilemap, stock-width output is untouched, and no guest state
is written.

The square starts on a 16-unit block boundary - `$03:9346` and `$03:9381`
select the streamed strip with `($14 & $03F0)` and `($12 & $03F0)` - so the
anchor is aligned down before it is used. Taking it raw put the last block row
and column outside the square: 138,481 of 7,323,648 cells measured across three
race captures, and up to 0.106% of 32:9 margin samples kept a stale tile.

Resolving a tile happens once per eight-unit cell rather than once per sample,
and the texel-to-world conversion collapses to one integer offset per scanline,
so the whole feature is free: at 682 pixels wide a draw costs 2.93 ms against
2.92 ms for the same compositor without it, and 2.99 ms against 3.00 ms at a
half blend (400 repetitions, same capture).

The walk was verified against retail two ways over 68 race captures of a stock
Mute City I Grand Prix: it reproduces the game's own `$7F:4A00`/`$7F:4B00`
staging buffers on 34,816 of 34,816 bytes, and it matches the live tilemap on
1,114,112 of 1,114,112 cells inside the square. Both are exact.

`tools/measure_draw_distance.py` reports, per aspect, the share of Mode 7 pixels
sampling outside the streamed square and the share whose live tilemap tile
differs from what the course tables give - the pop-in itself, and the
before/after figure. Over the same 68 captures:

| Aspect | Width | Centre outside | Centre wrong | Margin outside | Margin wrong before | after |
|---|---|---|---|---|---|---|
| 4:3 | 256 | 0.282% | 0.108% | - | - | - |
| 16:9 | 342 | 0.282% | 0.108% | 2.629% | 1.076% | 0.000% |
| 21:9 | 448 | 0.282% | 0.108% | 3.771% | 1.631% | 0.000% |
| 32:9 | 682 | 0.282% | 0.108% | 7.566% | 3.816% | 0.000% |

The stock columns are corrected too, wherever their own samples leave the square
(0.108% of them), so no seam appears at the stock screen edge. Stock 4:3 output
stays byte-identical because the lookup is confined to a widened viewport.

The Mode 7 centre is the camera reduced to the map, but retail writes either
representative: on some frames it is the camera's map position and on others
that plus 1024. A frame's own origin and centre always agree, so a texel minus
its own centre is exact, but `FzeroMode7Interpolate` takes the shortest path
across that seam and an interpolated origin can land in the other
representative. Subtracting the current frame's centre then moved every Mode 7
sample a whole map period and repainted the screen from elsewhere on the
course for one presentation - occasional full-screen flicker, visible only on
the interpolated desktop path. The centre and the camera are blended the same
periodic way as the origin. Over 321 consecutive race frames at 21:9 and a
0.5 blend, seven presentations changed more than 10,000 pixels against 1.4.3,
the worst 63,091 of 100,352; afterwards none do and the worst is 946. Output
at full blend is byte-identical either way.

Opponents are a separate question and are not affected. Their only horizontal
visibility test is `$00:DCC6`, which the viewport policy already widens; over a
2,600-frame race it admitted four projections, all on the starting grid. What
removes an opponent is `$00:DC57`, a longitudinal window accepting depths in
`[-639, +19)`, and `$00:DB85`, which deactivates a car whose projected row
reaches `$C0`. Both are original and identical at every aspect, so cars do not
additionally disappear into the widened margins.

`fzero_video.c` schedules the original 60.098811862 Hz simulation independently
from presentation. Bounded catch-up batches retain simulation debt; only overdue
presentations are skipped. Pause, minimize and load explicitly reset pacing.
Auto follows display refresh with a 60 Hz fallback and 360 Hz cap. Native
interpolation uses previous/current completed snapshots, adding one simulation
interval of latency. Stock 4:3 repeats authentic frames.

## Validation evidence

- Four Release CTest suites pass: video/config/clock/replay, Mode 7, renderer
  bounds/identity/layout, and independent plugin toggles/options/persistence.
- Eleven captured menu, intro and race frames matched stock pixels exactly
  when the native compositor was evaluated at stock width.
- An 1,800-frame GP input route produced identical RAM at 60/240 desktop FPS
  and in the wide headless run.
- A replay through 32:9, 4:3, 21:9 and Fit at two window sizes matched headless
  and desktop at 60/144 FPS. Final RAM SHA-256:
  `dacd0de1393c0ad9a264de5c1fc9891b15ebc8f9e26de441670cdb94167233a6`.
- Snapshot save/load reproduced ten subsequent frames in RAM and master clock;
  soft reset completed a 3,600-frame lifecycle route.
- Wide attract soak, 108,180 frames: `resume=00803c`, `master=38660019778`,
  `logic_changes=108110`, `video_active=107381`, `video_changes=79913`,
  `audio_samples=57767662`, `audio_active=108017`, `audio_peak=17644`,
  `audio_underruns=4`. This is 30 simulated minutes, not a desktop wall-clock
  benchmark, and preceded the final HUD/plugin fixes.
- Owner feedback identified stray hidden sprites and separated title characters.
  Focused regression tests and an inspected intro capture verified the fixes;
  the owner accepted the updated checkpoint.

Coverage is not exhaustive across all courses or complete cups. The owner
preferred a fast human checkpoint over further broad automated checks. Linux,
macOS and SDL2 were not validated for this release. Three pre-existing
scene-transition raster/HDMA discrepancies remain in `beads-8wg.5.2`. An earlier
optional Python-analysis build failed in attract mode; release generation uses
the supported native backend. Selected FPS targets are not performance guarantees.

## Reproduction and packaging

Run `ctest --test-dir <build> --output-on-failure`. Pure video/replay and Mode 7
tests also build with `-DFZERO_BUILD_GAME=OFF`, without a ROM. The private
`tools/run_capture.py` input grammar is `FIRST[-LAST]:MASK`; viewport events use
`FRAME:ASPECT[@WIDTHxHEIGHT]`. `--desktop-fps` uses SDL dummy drivers and
`--lifecycle` exercises disk snapshots and reset. `FZeroRenderCapture` renders
local source captures and compares stock-width output against stock pixels.
Raw captures are compiler-dependent diagnostics, not portable save states.

`VERSION` owns the release number. Regenerate without
`SNESRECOMP_EMIT_AOT_DENY_GATE`, build Release, and run `tools/make_release.py`.
Packages contain launcher assets, runtime DLLs and notices; they exclude ROMs,
generated C, personal settings, saves and captures.
