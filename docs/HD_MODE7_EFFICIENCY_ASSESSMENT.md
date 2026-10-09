# HD Mode 7 efficiency assessment (2026-10-09)

The current renderer has useful optimizations, but a significant architectural
limitation remains: it renders the entire HD image on one CPU thread. A private
parallel scanline prototype reduces renderer time by about threefold at 4x on
the test machine, with matching native and HD output. This supports improving
the scheduling before rewriting the graphics backend.

This does **not** identify the bottleneck on the GTX 1060 tester's machine.
Their CPU, settings, presentation rate and driver timings are not yet known.
The new profiling bundle measures the existing renderer on that machine.

## Where the work happens

- `src/fzero_renderer.c` snapshots two source frames, then `render_frame`
  traverses all 224 scanlines serially. HD rows sample an affine transform,
  fetch VRAM or course-map texels, and compose palette, window, sprite and
  color-math results on the CPU.
- The stock/HD combined pass, integer bounded sampler, repeated-texel cache,
  column contexts and palette/color-math caches from the previous performance
  work are still present. This assessment starts after those optimizations.
- `src/sdl_main.c` uploads the completed image and submits it to SDL or
  OpenGL. The OpenGL display shader does not rasterize the Mode 7 scene.
  A GPU model alone therefore cannot explain HD sampling performance.
- Resolution costs grow with scale squared. Wider views add columns, and
  higher Presentation FPS repeats composition more often. Auto FPS can target
  a high-refresh monitor even though game simulation remains about 60 Hz.

The primary [bsnes-hd scanline renderer](https://github.com/DerKoun/bsnes-hd/blob/master/bsnes/sfc/ppu-fast/line.cpp)
uses an OpenMP parallel loop when enough cached lines are ready. Its
[HD sampler](https://github.com/DerKoun/bsnes-hd/blob/master/bsnes/sfc/ppu-fast/mode7hd.cpp)
also performs CPU affine sampling and texture lookup. Its ability to use
multiple cores is a concrete difference from our current serial compositor;
no direct bsnes frame-rate comparison was performed here.

## Controlled prototype

Baseline game: `79f1c041344c` on `f-zero-forever`. Framework:
`8d9d8934de5f`. Windows Release, GCC 15.2.0 `-O3 -DNDEBUG -std=gnu11`.
Machine: Ryzen 7 9800X3D, eight physical cores / sixteen logical cores,
RTX 3080 Ti. GPU work is excluded from these timings.

The isolated prototype makes the renderer's scratch PPU thread-local, moves
row/object scratch inside the scanline loop, and schedules eight-line bands
with OpenMP. Frame snapshots, source memory and derived frame constants stay
immutable; workers write disjoint output rows and join before returning.
Workers use `OMP_WAIT_POLICY=PASSIVE` to avoid spinning during idle periods.
OpenMP is an experimental measurement dependency, not a shipped requirement.

Inputs are consecutive moving CGP Huckmine race frames 1780/1781, scene
`[2,3,0]`, from `captures/feedback-0930/before-class-4/`. Results below use
alpha 0.5. Baseline / four threads / four threads / baseline were run
sequentially, with no other game/build process running. Each run warms up,
then takes the median of three batches of 30 presentations; the table
averages the two corresponding medians.

| Aspect | HD scale | Current serial | Four-thread prototype | Speedup |
| --- | --- | ---: | ---: | ---: |
| 4:3 | 4x | 3.129 ms | 1.013 ms | 3.09x |
| 16:9 | 2x | 2.772 ms | 0.948 ms | 2.92x |
| 16:9 | 4x | 5.010 ms | 1.664 ms | 3.01x |
| 21:9 | 4x | 8.517 ms | 2.834 ms | 3.01x |
| 32:9 | 4x | 11.879 ms | 3.700 ms | 3.21x |

Additional single-run controls at 16:9/4x/alpha 0.5: one OpenMP thread
4.924 ms, two threads 2.753 ms, eight threads 1.241 ms. More workers have
diminishing returns, and those controls are not the repeated A/B measurement.

All 24 configurations match the baseline native and HD hashes in every run:
four aspects, native/2x/4x, current/interpolated presentation. The existing
renderer regression suite passes with the prototype at four threads, covering
bounds, immutable inputs, panorama, HUD/scene transitions and car identity.
The normal serial renderer suite also passes. This is a bounded CPU-renderer
assessment, not whole-game multicore qualification or a GTX 1060 result.

Private source copies, executables and CSVs are under
`captures/mode7-assessment-20261009/`. No ROM-derived capture is committed or
included in the profiler bundle.

## Recommended implementation

1. Add reusable, opt-in rendering workers to snesrecomp, with a synchronous
   fallback. Keep the pool alive between frames, sleep while idle, bound the
   worker count, and leave capacity for audio and other work. F-Zero should
   submit bands of its immutable presentation snapshot. Its course lookup,
   scenery reconstruction and HUD rules remain game-specific.
2. Keep small/native work serial when worker wake-up overhead would dominate.
   Validate frame ownership, shutdown, save-state/rewind transitions and
   full-game pacing before enabling it in a normal release. The private
   OpenMP experiment establishes potential; it is not that integration.
3. Consider GPU rasterization if the affected-machine report still shows CPU
   composition dominating after multicore work. Upload scanline transforms,
   palettes, VRAM/course data and composition masks, then draw the sampled
   scene on the GPU. Preserve hardware-authentic sampling, wrapping, windows,
   sprites and color math, with a CPU fallback. This would also avoid uploading
   the entire CPU-expanded framebuffer, but it is a larger change requiring
   visual parity checks across raster effects and custom courses.

The profiler reports simulation, native PPU, composition, upload, draw-submit,
present/swap and pacing time separately. Large composition time would support
the CPU-rendering diagnosis. Large upload/present time instead needs backend,
shader and driver investigation; those stages can include asynchronous GPU
back-pressure and are not direct GPU execution timings. Compare the same
course at HD off, 2x and 4x with fixed aspect/shader/FPS before drawing a
machine-specific conclusion.
