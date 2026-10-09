# HD Mode 7 performance

The [2026-10-09 efficiency assessment](HD_MODE7_EFFICIENCY_ASSESSMENT.md)
examines the remaining single-threaded architecture, measures an isolated
multicore prototype, and describes the affected-machine profiling bundle.
Its experimental renderer is not included in normal builds.

## Forever follow-up (2026-09-30)

Against Forever 0.7.2 (`ac92240`), the HD sampler now precomputes subpixel
offsets and checks the affine row's coordinate bounds once at its endpoints.
Ordinary rows then keep whole texels as integers through course/tile lookup,
avoiding repeated floating-point bounds checks and conversions per sample.
Unusual transforms retain the general sampler. Resolution, affine arithmetic,
interpolation and image quality are unchanged.

Windows Release (`gcc -O3`), Ryzen 7 9800X3D, CGP Huckmine race captures
1780/1781 at alpha 0.5. Both executables use the same flags. Runs alternate
baseline/optimized/optimized/baseline with no headless simulation running;
each entry averages two medians of three batches of 30 presentations.

| Aspect | HD scale | 0.7.2 | Updated | Less render time |
| --- | --- | ---: | ---: | ---: |
| 4:3 | 4x | 5.36 ms | 3.48 ms | 35% |
| 16:9 | 2x | 3.74 ms | 3.13 ms | 16% |
| 16:9 | 4x | 11.06 ms | 5.36 ms | 52% |
| 21:9 | 4x | 14.15 ms | 9.43 ms | 33% |
| 32:9 | 4x | 21.20 ms | 14.07 ms | 34% |

Native and HD output hashes match for all four aspects, native/2x/4x and
alpha 1/0.5 on those consecutive frames. Additional live frames from all
five Zenith courses also match. Private CSVs and captures are under
`captures/feedback-0930/` (`perf-*.csv`, `parity-*.csv`, `performance.json`).

These measure CPU rendering, excluding emulation, texture upload, GPU work
and frame pacing. They do not establish GTX 1060 performance or a comparison
against bsnes on the tester's machine. The tester still needs to check the
updated build at the same HD scale, aspect and presentation rate.

## Earlier optimization

The v1.8.0 implementation did unnecessary CPU work when HD Mode 7 and
Widescreen were enabled together. Widescreen also exposes more track samples,
and Presentation FPS repeats that work at the requested rate. Disabling
`EnhancedRenderer` reduces the width and removes a native composition pass;
it therefore explains the reported workaround, but sacrifices widescreen.

The optimized implementation:

- Produces the native thumbnail/rewind image and HD presentation in one
  renderer traversal, sharing sprites, raster state and native composition.
- Wraps ordinary whole texture coordinates with integer masks, retaining
  safe floating reduction for oversized coordinates.
- Resolves window membership and sprite visibility once per native column.
  Columns without sprites share per-scanline palette/color-math tables;
  sprite columns retain the full main/subscreen composition rules.
- Reuses texture/course lookups when neighboring HD samples hit the same
  texel. Every requested subpixel still uses the original affine coordinate;
  resolution, interpolation and image quality are unchanged.

## Earlier renderer measurements

Windows Release (`gcc -O3`), AMD Ryzen 7 9800X3D, recorded BS Deluxe race
frames 1600/1601, interpolation alpha 0.5. Each entry is the median of three
batches of 30 presentations after warmup. The baseline is v1.8.0 (`fa53884`);
the optimized result is the earlier combined-renderer implementation. Runs are sequential on the same
machine. These are renderer-only milliseconds, excluding emulation, audio,
texture upload, display work, pacing and capture-file I/O.

| Aspect | HD scale | v1.8.0 | Optimized | Speedup |
| --- | --- | ---: | ---: | ---: |
| 16:9 | 2x | 8.08 ms | 3.18 ms | 2.54x |
| 16:9 | 4x | 23.77 ms | 7.63 ms | 3.12x |
| 21:9 | 2x | 11.51 ms | 4.33 ms | 2.66x |
| 21:9 | 4x | 29.75 ms | 9.14 ms | 3.26x |
| 32:9 | 2x | 16.41 ms | 6.53 ms | 2.51x |
| 32:9 | 4x | 46.17 ms | 13.92 ms | 3.32x |

Output hashes match the baseline for the native and HD images, at both
alpha 1 and alpha 0.5, at 4:3, 16:9, 21:9 and 32:9, with HD off, 2x and 4x.
Linux checks also match their baseline output. Renderer regression tests
cover combined output bounds, exact native thumbnails, palette changes,
brightness, window masks, sprites on either screen, transparency, color
math and blank frames. Mode 7 tests cover both sides of integer conversion
limits, huge/non-finite coordinates and the floating-reduction fallback.

A 60 FPS presentation has about 16.7 ms for the entire frame; 120 FPS has
8.3 ms. A 32:9 4x renderer taking 13.9 ms leaves little room for everything
else at 60 FPS on this CPU. These measurements are not a promise that every
computer can sustain an arbitrary resolution and presentation rate. Auto
Presentation FPS follows the monitor: 2x at 32:9 and 240 FPS requests about
43 times the output samples of native 4:3 at 60 FPS.

## Whole-application checks

Sequential baseline/optimized Windows runs on the same CPU, each lasting
2,400 simulation frames (about 40 seconds), with EnhancedRenderer enabled:

| Game | Aspect | HD scale | Presentation FPS | Missed, v1.8.0 | Missed, optimized |
| --- | --- | --- | ---: | ---: | ---: |
| F-Zero | 16:9 | 2x | 120 | 1,143 | 9 |
| BS Deluxe | 32:9 | 2x | 60 | 494 | 0 |
| BS Deluxe | 32:9 | 2x | 120 | 1,572 | 410 |
| BS Deluxe | 32:9 | 4x | 60 | 863 | 233 |

These are missed presentation deadlines reported by the application, not
dropped simulation frames. All runs completed 2,400 simulation frames and
reached active-race state `[2,3,0]`; each baseline/optimized pair had identical
final WRAM. The route includes boot, menus and racing, so its totals are not
an isolated racing FPS measurement. SDL used dummy video/audio and software
presentation: this includes application overhead but does not qualify a
particular GPU or real display driver.

The optimized 32:9 2x run meets 60 FPS on this machine. The remaining misses
at 32:9 2x/120 FPS and 4x/60 FPS show why higher scales and refresh rates
still require more CPU capacity. Keep Widescreen enabled and start with 2x
and Presentation FPS set to 60 when troubleshooting performance.

## Reproduction

Build the `FZeroRendererBenchmark` target with the same Release settings as
the game. It uses private renderer captures, not ROM data bundled in the
repository:

```text
FZeroRendererBenchmark frame-001600.bin 30 frame-001601.bin
```

With two consecutive captures it tests current and interpolated frames;
with one capture it tests current frames only. CSV output includes width,
scale, blend, median render time and separate native/HD hashes. At 4:3 with
HD off, the custom renderer is unused; its reported zero cost excludes the
shared PPU, which still renders the native game.

The measured captures used BS Deluxe, 32:9, `FZERO_CAPTURE_FRAMES=1600,1601`
and the deterministic input route
`240:8,400:256,600:256,800:256,1100-1649:1`. Both captures are in active-race
state `[2,3,0]`. Use `FZERO_CAPTURE_PREFIX` to choose an ignored output path.
Compare baseline and optimized builds with the same input captures, and do
not run them concurrently. Synthetic regression tests need no private ROM.

For the whole-application checks, use isolated configs and save roots, set
`SDL_VIDEODRIVER=dummy`, `SDL_AUDIODRIVER=dummy`,
`SNESRECOMP_AUTOCLOSE_FRAMES=2400`, and
`SNESRECOMP_INPUT_SCRIPT=240:8,400:256,600:256,800:256,1100-2399:1`.
Set `EnhancedRenderer=1`, `HDMode7=1`, and the table's `Aspect`,
`HDMode7Scale`, `PresentationFPS` and `BSDeluxe` values in the file selected
by `FZERO_VIDEO_CONFIG`, with `PresentationEnabled=1`. Set
`SNESRECOMP_WRAM_DUMP` to compare guest state and read the final
`simulation=... presentations=... missed=... target_hz=...` log entry.
