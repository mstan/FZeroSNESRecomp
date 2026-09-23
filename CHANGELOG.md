# Changelog

## 1.8.3-fzero-55-preview.5 - tester feedback fixes

- Keep each authored P1/P2/P3 set together in its native selection column,
  including partial installations, without duplicating original cars or
  enabling their optional rebalances. Older CGP menu save states are rejected;
  existing course record keys are retained.
- Fix Moon Shadow's alternating pink/blue minimap dot and other cars retaining
  retail marker colors. Resolve colors by car identity for GP and Practice
  while preserving the native exhaust animation.
- Clarify that CGP adds 40 courses beyond the original 15, plus separately
  selectable revisions of those original courses. Information-card numbers
  and styling remain a documented limitation.

## 1.8.3-fzero-55-preview.4 - tester build

- Add Bower League, restore MAX to Mods (default off), and restore the CGP
  leagues' authored names. All packs together provide 16 cups / 80 versions,
  including untouched originals and separately selectable CGP revisions.
- Add coherent CGP vehicle packs and separate original-car rebalances. Grand
  Prix uses each enabled CGP set's three rivals; Practice allows any enabled
  opponent. Stock BS vehicles remain a separate mode.
- Preserve the native car selector with continuous columns and side previews.
  Extend Practice to every enabled cup/course and fix its doubled heading.
- Fix targeted Marine City trampoline and Lightning railroad landings,
  results-screen rendering, and missing launcher fonts, logo and controller art.
- Replace Volcania with the author's approved venue revision.
- Include TESTER_NOTES.md with defaults, known limitations and testing guidance.

## 1.8.3-fzero-55-preview.3 - local sharing build

- Add a Title screen dropdown to Community Grand Prix: Original (default)
  or F-Zero 55. The optional artwork applies only while CGP is enabled.
  Cars, gameplay rules, cup selection and course records remain independent.
- Save track-pack settings when closing the launcher as well as pressing Play.

## 1.8.3-fzero-55-preview.2 - local sharing build

- Split BS Satellaview vehicles and original BS tracks into independent mods.
  BS tracks appear under Track Packs; the eight-car roster works with any cup.
- Include CGP's ten corrected BS courses alongside its thirty new courses.
  CGP and original BS tracks are mutually exclusive, avoiding duplicate courses.
- Add 19 opt-in CGP gameplay, cosmetic and fix mods, including P1/P2/P3 tuning,
  energy boost and exhaust choices. These work independently of track packs.
  BS cars keep their individual stat tables and exhaust positions; shared
  physics and the adapted energy boost also apply to them.
- Fix BS car-select palette corruption and disappearing previews caused by
  duplicate scanline updates. All eight selections retain their colors.
- Keep MAX hidden and disabled. Music remains optional, off by default and
  absent from the ZIP. Rewind remains enabled with R; shoulders default to D/C.
- Imported courses currently appear in Grand Prix; imported Practice courses
  and a combined records browser remain future work.

## 1.8.3-fzero-55 - local branch build

- Use F-Zero-specific keyboard defaults: R for rewind, D/C for L/R shoulders.
  Launcher Reset to Defaults uses the same bindings; other games are unchanged.
- Enable rewind by default for fresh installations; R opens the filmstrip.
  Existing saved preferences remain respected.

- Bundle CGP's 30 new courses alongside the 15 original and 10 BS courses:
  55 tracks in 11 cups, with the canonical eight-car roster and rendering.
- Add enabled packs directly to the in-game league menu. Bundled packs use
  one checkbox; no master Track Library mod or file picker is needed.
- Preserve MAX League's files and implementation, but hide and disable it.
- Retain additive IPS/BPS discovery, stable cup records and contributor tools.
- Exclude the CGP MSU soundtrack. This Windows build is local and unpublished.

## 1.8.3 - 2026-09-21

- Add a default-off Diagnostics mod for troubleshooting performance. Enable
  it under Mods, reproduce a slowdown, and attach the newest timestamped
  report from the diagnostics folder beside the game or AppImage.
- Reports include hardware and build information, effective HD Mode 7 and
  display settings, and timings for rendering, uploads, presentation and
  frame pacing. Logs stay local and contain no ROM or save data.
- This patch adds diagnostic reporting; it does not claim to fix the
  reported HD Mode 7 performance regression.

## 1.8.2 - 2026-09-21

- Save and honor Skip Launcher. `--launcher` reopens it even with the setting
  enabled; a missing or invalid remembered ROM also falls back to the launcher.
- Allow whole-number HD Mode 7 resolution multipliers from 2x through 10x.
  Keep 2x as the default and show a severe-slowdown warning above 4x. Graphics
  backends that cannot create the requested texture fall back for that session.

## 1.8.1 - 2026-09-21

- Keep vehicle explosions and smoke together in Widescreen. Their first
  four sprite pieces reuse rank-display slots; only actual rank digits now
  follow the HUD anchor. Works with native and HD Mode 7 rendering.

- Reduce HD Mode 7 CPU cost with Widescreen by sharing native/HD composition,
  caching scanline color math and repeated texture lookups, and using integer
  wrapping for ordinary texture coordinates. Rendering remains pixel-identical
  in baseline comparisons; ultrawide views and high Presentation FPS still
  increase the workload. See [performance measurements](docs/HD_MODE7_PERFORMANCE.md).

## 1.8.0 - 2026-09-20

- Add optional HD Mode 7 track rendering at 2x or 4x resolution. Enable it
  under Mods > HD Mode 7; it starts off and works independently of
  Widescreen and Presentation FPS.
- Resample track tiles with finer affine coordinates and interpolate
  compatible camera scanlines. Cars, HUD and menus keep their pixel artwork,
  and game timing and save-state thumbnails retain native behavior.
- Include shared snesrecomp HD Mode 7 support and documentation. Both SDL
  and OpenGL/shader presentation use the larger texture. 4x costs more CPU
  time, especially with wide aspect ratios or high presentation FPS.

## 1.7.1 - 2026-09-20

- Remember ROMs selected with Browse For ROM after both Play and closing the
  launcher (#8). The shared recomp-ui launcher now saves the verified source
  ROM path; cancelling or selecting an invalid file preserves the last valid
  selection. Includes regression coverage for actual file selection and
  relaunch, rather than relying on a pre-created ROM cache.

## 1.7.0 - 2026-09-20

- Keep the live game-over HUD anchored, including the timer and power fill.
  Anchor the results score/counter while keeping the lap table and menu
  centered through both Try Again and End Game fades.
- Shaders remain off by default; use Settings > Display > Shader to enable one.

- Resolve saved music/shader paths and saves from the installation directory,
  including launches from shortcuts or other working directories. Relative
  command-line ROM paths still resolve against the caller's directory.
- Updated the shared engine to include audio-buffer recovery after state loads
  and starvation, legacy DMA save compatibility, and the latest host settings.

- Gameplay now reads the launcher's selected gamepad, per-device bindings and
  deadzone. Disconnecting another device no longer drops the active pad;
  reconnects and launcher/game focus transitions keep input working (#3).
- Keep the player's spark sprite with the car instead of moving it to the
  widescreen HUD's right anchor (#4).
- Enable user-provided Conn/Cubear v11 MSU-1 patches and music for stock and
  BS Deluxe, composed in memory without modifying the user's ROM. Added
  separate MSU save modes and audio restart after state loads. Patched sessions
  currently use interpreter execution. Validated with synthetic tracks and
  a user-supplied JUD6MENT pack in stock and Deluxe playtests (#5).
- Document the existing custom GLSL preset picker, including CRT-Geom. Invalid
  presets now fall back to unfiltered output. Preserve imported shaders during
  rebuilds but exclude them from release packages. CRT-Geom is not bundled
  pending redistribution-license compatibility review (#6).

## 1.6.2 - 2026-09-20

- Remember launcher settings on both Play and Quit, including display, sound
  and rewind options (#2).

## 1.6.1 - 2026-09-18

- Fixed the ROM button doing nothing on the Linux AppImage. The AppImage set a
  library path for itself that it also handed to the desktop file dialog it
  opens, so the dialog could not start — and its failure was read back as "the
  player cancelled", which is why the click looked ignored. Host programs now
  get a host environment, and a file dialog that cannot run falls through to
  the launcher's own built-in file browser instead of doing nothing.

## 1.6.0 - 2026-09-18

- Added the in-game save-state menu. **F7**, or **Select + R** on a gamepad,
  opens a browser over the frozen game: twelve slots, each with a thumbnail of
  the moment it was saved, A to load, X to save, B or Escape to back out. The
  menu itself is the framework's (`snes_savestate_menu.c`), so a fix there
  reaches every port; this host supplies the events and the pixels. It draws as
  its own layer in both presentation paths - the SDL renderer and the OpenGL
  shader path - so it lands at window resolution and stays crisp at 21:9
  instead of being composited into the 256-pixel game buffer.
- Added rewind. **F8**, or **Select + L**, opens a filmstrip of the recent
  past; Left and Right scrub, A or Enter drops back in, B or Escape leaves.
  It is off by default because it keeps whole snapshots of the machine in
  memory (about 330 KB each); the launcher's Settings page turns it on and
  sets the depth (50-200 snapshots) and the interval (1-30 frames), which are
  remembered in `config.ini` next to the executable — recomp-ui leaves
  Settings persistence to the host, and this host persisted none of it, so
  the switch would otherwise have come back off on every launch. A rewind
  or a state load now resets the presentation clock and the renderer's
  interpolation history together, so nothing blends across the jump and the
  seconds after one are not run as catch-up.
- Both keys are rebindable on the launcher's Controls page, as SaveStateMenu
  and Rewind, and are read back from `config.ini` next to the executable.
  Where one of them claims an F-key, it wins over the old quick slot on that
  key; rebinding it hands the key straight back. The quick-slot keys
  (F1-F12 to load, Shift+F1-F12 to save) still work; every slot is also
  reachable from the menu, with a thumbnail.
- Save states now record which cartridge took them, and loading one taken on
  the other is refused cleanly instead of resuming a machine whose ROM does
  not match the RAM being restored. Stock and BS Deluxe already kept their
  slots in separate directories under separate prefixes, so this only fires on
  a file moved by hand - which used to crash. A load the host makes itself is
  checked before the engine is called at all; a load from the browser, which
  goes through the engine directly, is undone from a snapshot taken when the
  browser opened. The tag reuses a padding byte, so 1.5.0 states still load.
- Bumped `snesrecomp` (242 commits) and `recomp-ui` (36). The runner's
  save-state menu and OSD units now pull in SDL, so they are held back from
  the SDL-free headless host and handed to the desktop host explicitly. Stock
  4:3 output is byte-identical across the bump on all 160 recorded captures,
  and the native compositor still matches the PPU's own frame exactly.

## 1.5.0 - 2026-09-18

- BS Deluxe is now compiled into the executable, so every download carries it
  and a missing or damaged `mods/bs-deluxe.dat` can no longer stop the game
  from starting. The embedded bytes are verified exactly as a file was: magic,
  declared sizes, the stock digest they were built against, ordered
  non-overlapping records, and the digest of the patched cartridge. A file
  beside the executable, or `FZERO_DELUXE_DATA`, is still tried first as a
  development override; if anything fails, the game logs and runs stock for
  that session without rewriting the settings file. Release packaging now
  refuses to build without the embedded payload.
- Corrected the streamed-square test to retail's 16-unit block grid. The
  anchor's low bits do not move the square, so 138,481 of 7,323,648 measured
  cells were classified outside it and up to 0.106% of 32:9 margin samples kept
  a stale tile; both are zero now.
- Every mod now ships on by default and the aspect defaults to Fit, which
  follows the window between 4:3 and 32:9. A first run with no
  `fzero-video.ini` therefore starts with Widescreen on at Fit, Presentation
  FPS on at Auto, and BS Deluxe on. `FzeroVideoStock()` is the stock baseline
  and is what the headless host, `FZeroRenderCapture` and the runtime's own
  pre-host viewport use, so captures and tests are unchanged unless
  `FZERO_ASPECT` opts in. A build configured without the BS Deluxe native
  module logs that it is starting stock instead of refusing to launch.

- Fixed the widescreen Mode 7 draw distance, reported by PowerPanda: pieces of
  the track were missing in the margins and appeared only once they reached the
  middle of the screen. Retail streams the tilemap for the stock 256-pixel
  viewport - `$03:9243` keeps one 1024-by-1024-unit world square uploaded, which
  fills the tilemap exactly - so a widened viewport sampled outside it and read
  the tiles another part of the course had left behind. The compositor now
  resolves those samples through retail's own course tables in WRAM bank `$7F`,
  which the frame snapshot already carries. Samples inside the streamed square
  still read the live tilemap, stock 4:3 output is byte-identical, and no guest
  state is written. Non-player cars were measured and are unaffected: their only
  horizontal visibility test is already widened, and what removes them is
  retail's longitudinal window and proximity cull, identical at every aspect.
- Fixed occasional full-screen flicker introduced by that change on the
  interpolated presentation path. Retail writes either representative of the
  camera's map position - some frames the camera's own value, some that plus
  1024 - and an interpolated scanline origin takes the shortest path across
  that seam, so subtracting the current frame's representative moved every
  Mode 7 sample a whole map period for one presentation. The centre and the
  camera are now blended the same periodic way as the origin. Measured over
  321 consecutive race frames at 21:9: seven presentations changed more than
  10,000 pixels against 1.4.3, the worst 63,091 of 100,352 (63% of the frame);
  after the fix none do, and the worst is 946. Output at full blend is
  unchanged.
- Added `tools/measure_draw_distance.py` and a `--sequence[=alpha]` mode for
  `FZeroRenderCapture` that replays a whole race through the compositor in
  order, including the interpolated presentation path.

## 1.4.3 - 2026-09-18

- Updated the bundled BS F-Zero Deluxe mod from upstream USA v1.0 to v1.1
  (upstream April 1, 2025). v1.1 adds the recovered BS F-Zero Grand Prix 2
  Week 1 data (Forest course graphics, Forest I/II track and path, BS-1 League
  race parameters, adjusted Forest III), fixes an upstream course-load CPU
  crash and garbled records graphics, and makes opponent speeds and Exploding
  Bumper spawn rates league-aware.
- Regenerated the namespaced Deluxe native module and the guarded cartridge
  delta from the v1.1 image; the runtime now verifies the v1.1 digest and
  rejects v1.0 data. `patches/bs-deluxe-usa.ips` is now the upstream v1.1 USA
  patch.
- The Deluxe import tools accept both the v1.0 and v1.1 archive layouts and
  read the upstream version from the archive readme.

Validated on the regenerated 1.4.3 build: all five test suites pass, plus the
patch-tool unit tests for both archive layouts. A scripted power-on route
reaches a BS-1 League Forest I Grand Prix race with Deluxe enabled and runs
1,800 frames in the desktop host at 21:9 (144 Hz presentation, SDL dummy
drivers) and in the headless host at 16:9; captured frames show the eight-
machine grid, the BS-1/BS-2 league list, the Forest I course card and the
race. A 600-frame stock boot without Deluxe stays on the stock cartridge and
writes no Deluxe save directory. The v1.1 module keeps the interpreter-floor
scheduler policy from 1.3.0. Full-cup coverage remains a user playtest item.

## 1.4.2 - 2026-09-08

- Extended live title and start-sequence scenery to the selected aspect ratio
  while keeping logos, menu text, and course-selection artwork together.
- Kept the race HUD adaptive from its first setup frame through attract-demo
  exit fades; intro lives counters now retain their right-edge position.
- Fixed the split Training course map and the centered timer after a Training
  crash. Training and Grand Prix loss screens now use their own HUD layouts.
- Fixed an oversized red/gray panel on the Grand Prix loss screen caused by
  extending its collapsed color window into the widescreen margins.

Accepted in interactive playtesting. All five regression suites pass, with
focused coverage for scene transitions, temporary sprite reservations, and
loss-screen color windows. Captured Training, loss, and attract-exit frames
preserve stock-width output.

## 1.4.1 - 2026-09-08

- Fixed distant scenery disappearing or changing abruptly in widescreen,
  especially at 21:9. Both skyline layers now sample the full panorama across
  section boundaries, including the partially filled final section.
- Added 15 clean interpreter fallback observations to the static coverage
  profile and regenerated with the native analyzer. The generated program now
  contains 468 AOT-eligible variants, up from 410 in the previous build.
- Preserved stock-width rendering, HUD placement, sprite visibility safeguards,
  and the interpreter policy used for widescreen opponent projection.

The skyline fix was accepted in an interactive owner playtest. Regression tests
cover both background panoramas across all aspect modes; 104 captured
frame/aspect comparisons preserve the original center and lower track/HUD.
All five test suites pass after regeneration. A 10,800-frame 21:9 stock soak
matches the previous build byte-for-byte in final WRAM and framebuffer, and a
1,800-frame BS Deluxe desktop smoke run completes successfully.

## 1.4.0 - 2026-09-08

- Added launcher Display shader support for F-Zero, including staged presets:
  CRT Soft, LCD Grid, Sharp, and Warm Composite.
- Added an OpenGL GLSL presentation path used when a shader is selected. The
  no-shader path keeps the existing SDL renderer behavior.
- Added Display aspect choices for F-Zero: 4:3, 16:9, 21:9, 32:9, and Fit to
  window. The built-in Widescreen/Presentation mod remains authoritative over
  aspect when enabled.
- Added widescreen and BS Deluxe screenshots to the README.
- Added a 21:9 BS Deluxe race screenshot to the README.
- Added the BS Deluxe USA IPS patch to the repo and release ZIP at
  `patches/bs-deluxe-usa.ips`.
- Added Linux x86_64 AppImage release packaging.
- Updated BS Satellaview README credit/permission wording.

Validated with direct unit test executables, no-shader and CRT shader desktop
smoke runs, and a live CRT window capture after fixing the OpenGL VAO binding
needed by the shared GLSL renderer.

## 1.3.0 â€” 2026-09-07

Private feature release, advancing two minor versions from 1.1.0 as requested.

- Added BS F-Zero Deluxe USA 1.0 as one independent, all-or-nothing mod:
  original and BS courses, eight machines, alternate leagues/layouts, records
  and Practice ghosts. The stock ROM file remains unchanged.
- Retained separate Widescreen and Presentation FPS plugins, adaptive HUD
  anchoring, wider opponent projection and interpolation independent of logic.
- Added a separately namespaced Deluxe module and isolated 32 KiB SRAM saves.
  Deluxe currently runs its main scheduler through the interpreter floor;
  native interrupt helpers and the custom renderer remain active.
- Fixed the car-selection HDMA bus-read crash and the shared PPU's missing
  XOR/AND/XNOR window operations that hid league and difficulty text.
- Added original North American SNES box art to the launcher.

Validated both Blue Thunder on Forest I and Blue Falcon on Mute City I in
visible desktop runs. Five game test suites and the focused framework dispatch
and PPU regressions pass. Full-cup coverage and ghost recording/playback remain
unverified. See [BS Deluxe details](docs/BS_DELUXE_EXPLORATION.md).

The private release can include the locally verified Deluxe delta and upstream
credits. The packaging tool refuses this payload unless the F-Zero repository
is private. No stock/patched ROM or user save is included.

## 1.1.0 â€” 2026-09-07

- Added a native Mode 7 renderer for 16:9, 21:9, 32:9 and Fit to window.
- Preserved stock 4:3 rendering. Race HUD groups anchor to the outer edges;
  menus and course-intro text remain centered.
- Extended opponent projection/activation to the current viewport through
  original game routines. Viewport changes apply at simulation boundaries.
- Added separate Widescreen and Presentation FPS built-in plugins in recomp-ui,
  each with independent toggles and settings.
- Added Auto refresh and 60/90/120/144/165/240/360 FPS presentation. Simulation
  stays at 60.098811862 Hz; native camera/car interpolation never writes game state.
- Added aspect/FPS shortcuts, pause/minimize pacing, soft reset, deterministic
  input/window replays and snapshot capture tools.
- Fixed stale sprites appearing in wide margins and separated course-title text.

Owner playtest accepted the checkpoint for release. See
[validation evidence and limits](docs/ADAPTIVE_RENDERER.md).
This is the first semantic-version tag; the previous WIP binary was labeled 1.0.
