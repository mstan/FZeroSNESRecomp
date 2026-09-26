# Source projects and tester feedback - September 26

Branch: `f-zero-forever`. Next local test build: 0.5.0.

- [x] `beads-8wg.5.70`: Import the original Huckmine FZEdit ZIP. Preserve all
  twelve author files, add manifests/credits, and explain the steps in MODS.md.
- [x] Validate Huckmine cold/warm cache, folder/ZIP parity, original BMP
  bitfields, course start, both engines' cup records and save reload.
- [x] `beads-8wg.5.71` (partial): Imported records now show the actual course
  skyline. Gold District / Dragon Bird checked against the reported screen.
- [ ] `beads-8wg.5.71` (still open): Reported scattered lap/rank digits have
  not reproduced in current rank-one/rank-two CGP finish captures. Existing
  renderer result/fade tests pass. Do not claim this report is fixed.
- [x] Complete True League on retail/expanded engines through private finish
  fixtures; check five-course records, details, isolation and battery reload.
  Return from its records menu to the title without the reported garble.
- [x] `beads-8wg.5.72`: Save stable vehicle identity in new Practice ghosts,
  resolve the playback car and record icons from that identity. Move rival
  state out of the native ghost codec into the reserved $14CE8..$14CEB gap.
- [x] `beads-8wg.5.73`: Menu and event music in Mods, six CGP defaults,
  individual file choices, pack selection, persistence and SNES fallback.
  Stage all ten supplied Astra recordings without modifying audio/loops.
- [x] Package runtime files, credits and user docs with/without audio;
  retain the developer directory. Omit source ROMs and disposable caches.

Evidence stays in ignored `captures/`: `huckmine-source-tests-02`,
`huckmine-gameplay-01`, `huckmine-records-01`, `feedback-record-art-01`,
`feedback-rank2-03`, `true-records-05`, `ghost-regression-02`,
`practice-rival-workspace-05c`, `menu-audio-01`, `menu-audio-02`, and
`menu-music-ui-01`. Finish fixtures seed times/position and let native finish,
record writes and transitions run; they are not full driven-race qualification.

Huckmine's 32-bit BI_BITFIELDS minimap is accepted without changing its bytes.
The example is tracked in `examples/huckmine/huckmine.zip`; the exporter places it in
`mods/packs/cgp/courses/huckmine.zip` instead of the extracted `.fzc`. Its small
source/ROM differences intentionally change Zenith's records namespace;
previous records are retained on disk. Other 74 course hashes are unchanged.
Bare-project autodiscovery and a Practice-only Custom Tracks UI remain future
work. A manifest is still required.

Vehicle snapshot signature advances because the reserved state moved. Old
vehicle save states are refused; battery records remain supported. Previously
saved ghosts without full identity cannot be reliably relabeled; record a new
one. New Dragon Bird/White Cat ghost tests exercise the native Save Ghost
prompt, checksum validation, reboot and playback artwork.

Release artifacts: `release-stage/0.5.0/` (with/without MSU), built from
`2daf59695bf4151c7e141a043f6f09d919a37837`. All 13 CTest checks pass.
Both ZIP CRC checks pass; no ROMs, saves, build source or caches are packaged.
The music archive contains the reviewed 29 CGP + 10 Astra recordings. The
no-audio archive contains zero PCM files. An extracted player ZIP successfully
opens the real launcher, displays prefilled menu cues and generates Huckmine's
cache from huckmine.zip. Developer files remain in `build-shared-packs/`.
