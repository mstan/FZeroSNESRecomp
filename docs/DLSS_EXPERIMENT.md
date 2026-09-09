# Vulkan and DLSS 5 experiment

This branch adds explicit SDL3 Vulkan presentation and optional live DLSS 5
Neural Rendering to F-Zero. The neural stage uses a separate Windows D3D12
process with CPU frame transfers; this is not native Vulkan DLSS integration.

## Run

Build with SDL3 using the normal CMake targets. Run
`tools/setup_dlss_experiment.ps1 -Build <build-directory>` once to install the
pinned bridge, Ampere runtime, and isolated Python dependencies. This now
requires MSVC x64 and a Windows SDK to rebuild the bridge with the checked-in
`tools/dlss_temporal.patch`. Setup verifies source/archive hashes and records
the patched binary hashes in `dlss-deps/temporal-bridge.json`.

Open the game launcher and enable **Mods > DLSS5**. Its checkbox enables neural
rendering and selects Vulkan presentation. These settings persist in
`fzero-video.ini`. Vulkan alone requires neither Python nor NVIDIA hardware.

Alternatively:

```powershell
./tools/launch_vulkan.ps1 -Build ./build-experiment -Dlss
```

Supply `-Rom <path>` to skip the launcher. Ctrl+F8 toggles neural rendering
during play. Ctrl+F9 toggles a same-frame split: original on the left, neural
output on the right. The title reports Starting, Priming history, Temporal,
Off, or Failed (original output). Existing controls, save slots, audio and original simulation
timing remain available. The OpenGL shader preset is inactive under Vulkan.

Every game build includes the BS Deluxe native module, data and credits.
Configure `FZERO_DELUXE_GEN_DIR` and `FZERO_DELUXE_MODS_DIR` to the existing
private generated/imported directories when building in a separate worktree.
Missing module inputs fail configuration; they are never silently omitted.

Overrides: `FZERO_OUTPUT_METHOD=Vulkan` selects Vulkan, `FZERO_DLSS=0|1`
overrides the saved neural option. `FZERO_DLSS_PYTHON` and `FZERO_DLSS_ROOT`
override the dependencies under the executable's `dlss-deps` directory.
`FZERO_DLSS_HEIGHT=240..960` selects processing height (default 480, width
capped at 1280). `FZERO_DLSS_COMPARE=1` starts in split-comparison mode.

## Implementation and limits

- SDL3 owns the Vulkan device, swapchain, uploads and presentation. An explicit
  Vulkan request fails visibly if unavailable instead of silently using SDL's
  preferred driver. Original SDL and OpenGL paths remain selectable.
- The worker invokes NGX neural feature 18 through ComfyUI-DLSS5-NR v0.3.0's
  native C ABI. ComfyUI, Torch, DLSS Super Resolution and Frame Generation are
  not used. Upstream native bridge source is MIT licensed; the NVIDIA runtime
  is separate and is not committed or bundled in game releases.
- One frame may be in flight. Vulkan displays the newest completed neural
  frame while simulation continues. Worker errors restore original pixels;
  Ctrl+F8 off/on retries. Reset/load/resume and aspect changes invalidate old
  output. A Windows job bounds the worker and its Python child to game lifetime.
- Neural processing is 480 pixels high (width follows aspect, capped at 1280).
  Asynchronous presentation adds latency and repeats images between neural
  completions. The simulation/presentation counter is not the neural frame rate.
- Temporal processing is on by default. The upstream BGRA optical-flow input
  failed at `nvOFExecute` on the tested 3080 Ti. The patch uses advertised R8
  luminance input, retaining the 2x2 hardware flow grid. Measured backward
  vectors are uploaded to NR and history is retained between evaluations.
  Reset/load/resume, dimension changes, and large scene cuts reset history.
  `--no-temporal` selects independent still frames for diagnostic comparisons.
- RGB order is explicit for the pinned SF-v2 DLL, never guessed from a black
  boot frame. Zero strength bypasses NR with exact source pixels and invalidates
  history; the runtime itself returned black at zero despite reporting success.
- Split comparison uses the exact input associated with the completed neural
  frame, with identical dimensions and the same configured texture filtering.
  The full framebuffer still includes the HUD; no invented scene geometry or
  replacement assets are supplied, and altered lettering remains possible.
- The pinned bridge's shutdown can hang on this driver. The disposable CLI
  terminates after flushing results; the host gives its worker two seconds
  before terminating its own isolated process/job. No driver or unrelated
  process is terminated.

## Reproduce validation

Use the Python interpreter at `<build>/dlss-deps/venv/Scripts/python.exe`:

```text
tools/run_vulkan_experiment.py --build <build> --rom <rom> --frames 1800 --dlss --screenshot <capture.bmp> --input-script 180:8,300:8,450:8,600:8,750:8,900:8,1000-1800:1
tools/capture_dlss_sequence.py --build <build> --rom <rom> --output <new-capture-directory>
tools/dlss_worker.py process --root <build>/dlss-deps/bridge/ComfyUI-DLSS5-NR --input <new-capture-directory>/frames --output <new-result-directory>
tools/validate_dlss_temporal.py --root <build>/dlss-deps/bridge/ComfyUI-DLSS5-NR --output <new-validation-directory>
tests/test_dlss_worker.py
```

Omit `--dlss` for the original Vulkan control. Readback captures come from
SDL's Vulkan renderer, not desktop screenshots. Sequence export requires zero
stock-renderer pixel differences. Output reports record runtime hash, GPU,
settings, processing times and channel interpretation. Live evaluations are
logged to `dlss-deps/bridge/ComfyUI-DLSS5-NR/live.jsonl`.

The helper CLI intentionally runs experimental native code in its own process;
automation should apply an external timeout, as the bounded live runner does.

## Dependencies

- Native bridge v0.3.0, source `3745b8ab6c70761e8d9e7daf47948a389833086f`:
  https://github.com/lisitskyaa/ComfyUI-DLSS5-NR
- Ampere runtime `310.8.SF-v2`, DLL version `310.8.SF.0`, unsigned:
  https://github.com/RankFTW/rhi-repo/releases/tag/dlssnr-310.8.SF-v2
- NumPy 2.2.6 and Pillow 11.3.0, isolated from the system Python environment.

Detailed machine-local evidence is in `build-experiment`; copyrighted captures,
ROMs, runtime DLLs and dependency archives remain untracked.

## Temporal validation (2026-09-08)

- An eight-pixel rightward translation produces approximately -8 pixels of
  horizontal backward flow (within 0.02 pixels mean in the tested sequence).
  The first/reset frame has zero flow, subsequent frames supply measured flow.
- The GPU acceptance test verifies history changes output, explicit reset,
  odd-width resize, scene-cut detection, and exact zero-strength bypass.
- 300 consecutive race frames processed without the former second-frame error.
- Live 1,800-frame races passed at 1280x549 and 1120x480 with measured flow and
  `reset=false` between cuts. At 21:9 the former delivered about 16 neural
  updates/second and the latter about 21; 480-high remains the playable default.
  This is not 60 FPS neural output. Simulation remains independent of NR.
- Fresh baseline and temporal runs ended with identical WRAM SHA-256:
  `a96a4188f093eff8c202658848c2740e78fa5193cff45a7baf0a798e162af854`.
- Five CTest targets and four worker regression tests passed. The patched
  installed DLL also passes the GPU acceptance test, not just the source build.
- In-game Ctrl+F8 off/on and Ctrl+F9 split were verified through the window's
  key event path. A 1,200-frame resize run delivered 357 neural frames across
  853x480, 720x480, and 640x480, with exactly three resets and 354 measured-flow
  frames. Missing-runtime fallback also completed normally.
- Initialization after rebuilding can outlast a short smoke test. The title
  remains Starting and original pixels remain visible until a result arrives;
  worker startup has a 60-second timeout rather than claiming NR is active.

## Earlier still-frame results (2026-09-08)

RTX 3080 Ti, driver 32.0.16.1686:

- Explicit Vulkan startup and 1,800-frame scripted race passed. Vulkan readback
  shows the game and HUD; original and neural captures were visually inspected.
- The matched original/neural race runs ended with identical WRAM SHA-256:
  `c971cd06f81ad60015d92d8d7b66cf8049cad9d7d6684367148ed5a85a1cb914`.
- Live neural output updated about 27-29 times/second while simulation remained
  near 60.099 Hz. Pooled warmed 640x480 evaluations: median 15.78 ms,
  p95 17.95 ms. These exclude upload/IPC/presentation and are not total latency.
- A 300-frame stock-aspect race sequence exported with zero differing stock
  pixels and processed through NR. Original, neural and side-by-side MP4s are
  local artifacts. These clips play cached results at the source rate; they
  are not recordings proving 60 FPS live neural rendering.
- Repeating the same 640x480 race frame produced identical output PNG hashes.
  A 1280x960 still also evaluated successfully. The modification is mostly
  shading/color at these settings; it does not reconstruct a modern 3D scene.
- 16:9 -> Fit -> 4:3 resizing passed, including neural resource size changes.
  Missing-runtime fallback completed normally with original Vulkan output.
  No live experiment worker remained after normal exit.
- All five CTest targets passed, including new renderer-option persistence
  coverage. The standalone Windows IPC module passes strict GCC syntax checks.

The optical-flow execution failure is fixed. Temporal quality on SNES pixel
art remains content-dependent: this is a playable experiment, not a claim of
photorealistic reconstruction or a production/performance release.
