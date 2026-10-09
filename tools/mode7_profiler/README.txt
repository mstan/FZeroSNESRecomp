F-Zero Mode 7 profiler

1. Extract the entire ZIP into a writable folder.
2. Double-click "Profile Mode 7.cmd". Choose your F-Zero ROM if prompted.
3. Use the same graphics settings as your usual build. In particular, keep
   the same HD Mode 7 scale, aspect ratio, Presentation FPS and shader.
4. Play a course that runs poorly for at least 30 seconds. Quit the game.
5. Send the small mode7-profile-*.zip file from profile-results back to us.
   The folder will open with that report selected when you finish.

The report automatically includes your CPU, RAM, graphics driver, settings,
frame pacing and timings for rendering, texture upload and presentation.
It contains no ROM, music, save files or raw course data. It is never uploaded
automatically. Diagnostics is enabled only for this profiling launch.

For a useful comparison, repeat with HD Mode 7 off, then with it at 2x and 4x.
Keep the same course, aspect, shader and Presentation FPS. Send all three
report ZIPs. Let the window stay focused while measuring; Alt-Tab pauses are
different from gameplay slowdown.

This is an assessment build based on Forever 0.8.0. It uses the existing
renderer. The experimental parallel renderer is not included, so the report
measures the implementation that testers have been using.

If you already have a working 0.8.0 installation, you can first copy config.ini,
fzero-video.ini, keybinds.ini and rom.cfg beside this executable to retain your
settings. You can also copy your own packs/music. These remain local and are
not included in the report. The profiler does not copy them automatically.
