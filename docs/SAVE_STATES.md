# Save states, the slot browser, and rewind

How the two in-game overlays are wired into this host, what is framework and
what is F-Zero's, and how to verify any of it without a person at the
keyboard.

## What belongs to whom

The panels are framework modules in `snesrecomp/runner/src`:

| Module | Owns |
| --- | --- |
| `snes_savestate_menu.c` | the 12 slots, the selection, thumbnails, the panel pixels, and the `RtlSaveSnapshot` / `RtlLoadSnapshot` calls |
| `snes_rewind.c` | the snapshot ring, the filmstrip pixels, and the restore |
| `snes_overlay_draw.c` | the shared 8x8 font, the pad bit names, and the nav repeat timings |

`src/sdl_main.c` supplies only the two things a framework module cannot: SDL
events, and pixels on the screen. There is no copy of a state machine here,
which is the point — this family has ~25 port repos generated from one
scaffold, and a state machine copied 25 times cannot inherit a fix.

## Opening them

| | Keyboard | Gamepad |
| --- | --- | --- |
| Save-state browser | `[KeyMap] SaveStateMenu`, default **F7** | **Select + R** |
| Rewind filmstrip | `[KeyMap] Rewind`, default **F8** | **Select + L** |

Both keys come from `config.ini` next to the executable, which is the file the
launcher's Controls page edits (`game.config_path`). `src/fzero_hotkeys.c`
parses that section: the modifier prefixes, the `None` / `(unbound)`
spellings, and the ini scan. It is deliberately not the framework's
`mmx_config.c` — F-Zero carries its own `Config` and linking that parser would
collide with it — and it is tested without SDL in `tests/test_hotkeys.c`.

Rewind's own three settings live in the same file, as `[Rewind] Enabled`,
`Depth` and `Interval`. New installations start with rewind enabled; an
explicit saved `Enabled=0` remains off. recomp-ui leaves `Settings` persistence to the host
and this host persisted none of it, so without that the launcher's checkbox
came back off on every launch. They are written through the framework's own
surgical writer (`launcher_ini_kv_write`), so `[KeyMap]` above is untouched,
and read back with `FzeroIniReadInt` before the launcher runs — a launch with
a ROM on the command line skips the launcher entirely and must still honour
them. `configure_rewind()` then translates them into the
`SNESRECOMP_REWIND*` environment the ring actually reads, leaving an
explicitly exported value alone so a capture run still overrides the UI.

Hotkeys are matched **before** the F-key quick slots, with an exact modifier
comparison, so `Shift+F7` still saves slot 7 while plain `F7` opens the
browser. The quick slots remain supported; where a hotkey claims a key the
hotkey wins, and rebinding it returns the key.

The pad gestures both take two buttons on purpose. One ordinary button pressed
mid-race is not a gesture, it is a boost.

## Presenting them

F-Zero has two presenters — an SDL renderer and an OpenGL path with a shader
preset — and an overlay has to reach the screen on both. `present_frame()` is
the single path for both, with an optional panel:

- the **browser** is an opaque panel over the game's whole destination rect;
- the **filmstrip** sits across the bottom third of it, annotating the frame
  it describes rather than covering the middle of the screen.

Both draw as their own layer at window resolution rather than being
composited into the 256- (or 684-) pixel game buffer, so their text stays
crisp at 21:9.

**The GL path needs its own VAO.** `GlslShader_Render` binds its own array
buffer and enables its own attribute arrays inside whichever VAO is current,
then disables them again — so the VAO handed to a shader preset comes back
with its attributes off and the next plain `glDrawArrays` through it draws
nothing. The first version of the overlay hit exactly this: the panel was
uploaded, the draw was issued, and the screen was unchanged. The overlay gets
a VAO no shader has ever touched, and the passthrough program rather than the
user's preset, so a CRT curve does not bend the menu's text.

## Freezing and resuming

While a panel is up the modal loop never calls `RtlRunFrame`. That is the only
way "save right here" names a definite point in time, and it is what the
framework and psxrecomp both do. Audio is paused for the duration.

On the way out the host must undo three things:

1. **Buttons still held.** The press that closed a panel must not also act in
   the game. The browser module masks its own; the rewind module does not, so
   `overlay_filter_guest_input()` covers both.
2. **The renderer's interpolation history.** `FzeroRendererReset()` runs from
   `fzero_on_state_loaded`, so a rewind never blends across the jump.
3. **The presentation clock.** `g_reset_presentation_clock` is set after
   either loop, or the frames the freeze consumed are owed back and the first
   seconds after a panel run as catch-up.

## Stock and BS Deluxe never share a state

They are different cartridges. A Deluxe snapshot resumed on the stock ROM
restores registers and RAM for code that is not in the cartridge, which is a
crash rather than a glitch.

Three things keep them apart, in order of how much work they do:

1. **Path.** `FzeroDeluxeSelectSaveRoot()` already put Deluxe under
   `saves/bs-deluxe/` with the `fzero-bs-deluxe` prefix, against `saves/` and
   `fzero`. Nothing crosses by accident.
2. **A tag in the snapshot.** `FzeroRuntimeState.mode` (`FzeroStateMode`)
   records which mode wrote it. It claims a byte of the trailer's existing
   reserved padding rather than appending a field, so the trailer keeps its
   size and order and a 1.5.0 snapshot reads back as `Unknown` and still
   loads.
3. **Two refusal paths**, because the engine has no pre-load veto and applies
   the guest blob before the game's trailer is read:
   - A load **the host makes itself** (the quick-slot keys) calls
     `FzeroStateFileAcceptable()` first, which reads the tag out of the file's
     fixed-size trailer without loading any of it. Nothing is touched.
   - A load **from the browser** goes through the engine directly.
     `FzeroStateGuardArm()` takes one whole-machine snapshot when the browser
     opens — the guest is frozen from then until it closes, so that one
     snapshot is the exact machine every load from the browser would replace —
     and `fzero_on_state_loaded` restores it if the tag does not match.

`tests/test_state_mode.c` covers the tag and the probe, including truncated
files, foreign magic, an out-of-range tag and a malformed layout.

## Verifying it without a person

Three environment hooks, all in `src/sdl_main.c`:

| Variable | Effect |
| --- | --- |
| `FZERO_OVERLAY_SELFTEST=<frame>` | attach a virtual gamepad; open the browser with Select+R, navigate, close with B; 60 frames later do the same for rewind with Select+L; 60 after that open the browser again with the keyboard binding. Prints a verdict and exits non-zero on FAIL. |
| `FZERO_OVERLAY_DUMP=<path>` / `FZERO_REWIND_DUMP=<path>` | read the composited window back at the first present of that panel and write it as a PPM. Reads the **real** output of whichever presenter is in use, so the SDL and GL paths are covered separately. |
| `FZERO_STATE_SAVE_AT=<frame>[:<slot>]` / `FZERO_STATE_LOAD_AT=<frame>[:<slot>]` | drive one slot operation through `perform_state_action`, so a harness can write a state in one run and try to load it in the next — which is the only way to produce the cross-cartridge case the guard defends against. |

A full check, both presenters and both refusal directions:

```bash
# Browser and rewind, SDL renderer path
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
SNESRECOMP_AUTOCLOSE_FRAMES=600 FZERO_OVERLAY_SELFTEST=300 \
SNESRECOMP_REWIND=1 SNESRECOMP_REWIND_INTERVAL=2 \
FZERO_OVERLAY_DUMP=menu.ppm FZERO_REWIND_DUMP=rewind.ppm \
  ./FZeroSNESRecomp fzero.sfc

# The same on the GL path: set FZERO_SHADER to a preset and leave
# SDL_VIDEODRIVER alone, since the shader path needs a real GL context.
```

The overlay dumps are the evidence that a panel reached the screen. Inferring
it from the module reporting itself open is what let the GL bug above ship
past a first round of testing.
