# FZeroSNESRecomp

A native PC build of *F-Zero* for SNES.

You bring your own legally dumped *F-Zero (USA)* ROM. No ROM is included.

<p align="center">
  <img src="docs/screenshots/bs-forest-iii-race-21x9.png" width="96%" alt="BS F-Zero Deluxe Forest III race in 21:9">
  <br>
  <img src="docs/screenshots/widescreen-title.png" width="48%" alt="F-Zero title screen in widescreen">
  <img src="docs/screenshots/bs-blue-thunder.png" width="48%" alt="BS F-Zero Deluxe Blue Thunder machine select">
  <br>
  <img src="docs/screenshots/bs-forest-iii-race.png" width="48%" alt="BS F-Zero Deluxe Forest III race in 16:9">
  <img src="docs/screenshots/bs-forest-iii.png" width="48%" alt="BS F-Zero Deluxe Forest III course select">
</p>

## Features

- Play *F-Zero* as a native app.
- Widescreen modes: 16:9, 21:9, 32:9, and Fit.
- High refresh presentation: 60, 90, 120, 144, 165, 240, or 360 FPS.
- Display shaders: CRT Soft, LCD Grid, Sharp, Warm Composite, or your own GLSL shader.
- Save states with F1-F12.
- Gamepad support through SDL.
- Optional BS F-Zero Deluxe content.

## Download And Play

On Windows:

1. Download the Windows x64 ZIP from the Releases page.
2. Extract the whole ZIP.
3. Run `FZeroSNESRecomp.exe`.
4. Pick your own *F-Zero (USA)* `.sfc` or `.smc` ROM when asked.

On Linux:

1. Download the Linux AppImage from the Releases page.
2. Put your own *F-Zero (USA)* `.sfc` or `.smc` ROM next to it.
3. Make the AppImage executable.
4. Run it.

The app remembers your ROM path after the first launch.

## Settings

Open **Settings > Display** for:

- **Aspect ratio:** 4:3, 16:9, 21:9, 32:9, or Fit.
- **Shader:** None, CRT Soft, LCD Grid, Sharp, Warm Composite, or a custom shader.

Open **Mods** for:

- **Widescreen:** makes races wider.
- **Presentation FPS:** makes motion smoother on high refresh screens.
- **BS Deluxe:** adds the Satellaview machines, leagues, and tracks.
- **DLSS5:** enables neural rendering with Vulkan presentation.

When the Widescreen mod is on, its aspect setting wins over the normal Display aspect setting.

## BS F-Zero Deluxe

BS F-Zero Deluxe is included with permission from its authors:
GuyPerfect, Porthor, and PowerPanda.

The release includes two BS Deluxe files:

- `mods/bs-deluxe.dat` is used by this app.
- `patches/bs-deluxe-usa.ips` is the SNES patch for your own ROM.

No patched ROM is included.

## If The Game Crashes

Send these files from the game folder:

- `crash_report_*.json`
- `crash_minidump_*.dmp`
- `last_run_report.json`

## Build From Source

Clone with submodules:

```bash
git clone --recurse-submodules git@github.com:mstan/FZeroSNESRecomp.git
cd FZeroSNESRecomp
```

Put your own USA ROM at `fzero.sfc`, then generate and build:

```bash
bash tools/regen.sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Local ROMs, generated files, saves, captures, and builds are ignored by git.

## License

Code in this repo is not yet under a declared license.

*F-Zero* belongs to Nintendo. The game ROM is not included.
