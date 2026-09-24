# CGP music and experience presets

Tracked by `beads-8wg.5.50`, `beads-8wg.5.51` and shared UI `beads-0fu.9`.

## Player behavior

Mods offers **Vanilla**, **Satellaview** and **Community Grand Prix** presets.
These are editable recipes for the CGP/BS family of options. They never reset
MAX, Bower, other track packs, display settings, controls, volume or rewind.
For example, applying Vanilla while MAX is enabled leaves MAX enabled.

| Owned options | Vanilla | Satellaview | Community Grand Prix |
| --- | --- | --- | --- |
| Original four cars | Original behavior | Original behavior | All four rebalanced |
| BS vehicles / original BS tracks | Off / off | On / on | Off / off |
| CGP courses, including revised original and BS versions | Off | Off | On |
| CGP P1/P2/P3 vehicle groups | Off | Off | All on; 12 total identities |
| CGP rules, including Legend and credits | Off | Off | All on |
| F-Zero 55 title | Off | Off | On |
| MSU music | Off | Off | On, bundled CGP source |

Legend adds its difficulty choice; the player still chooses a race difficulty.
Required course mechanics continue to accompany their courses independently.
No preset is automatically applied when starting the launcher. Existing
first-run defaults are retained: BS cars, CGP/Bower tracks, music off.
The label becomes **Custom** when an owned option differs. Unrelated changes
do not change the label. Settings persist on Play and on closing the launcher.

Settings > Audio has a source dropdown: **Community Grand Prix**, then
**Custom...**. CGP is the initial source, with MSU disabled. Custom opens a
`.msu` file picker and retains the previous custom path when switching back
to CGP. Existing folder-based custom settings migrate without being replaced.
Turning music off restores SNES audio without removing any track pack.

## Shared UI contract

The picker is implemented in `recomp-ui`, not F-Zero's renderer. Games opt in
by implementing all four optional provider callbacks: `preset_count`,
`preset_get`, `preset_current` and `preset_apply`. Absent/incomplete callbacks
produce no picker and no default recipes. F-Zero owns these three definitions
and their conflict handling. Reusable API documentation lives in recomp-ui's
`docs/OPTIONAL_PRESETS.md`.

Bundled music choices are separately optional through GameInfo's `msu1_packs`
and `num_msu1_packs`. Hosts that omit them retain the existing MSU folder UI.
The shared UI stores a selected ID; each game resolves it to its own assets.

## Soundtrack and runtime

- P1, P2 and P3 archives each contain the same 61 PCM files, totaling
  1,226,929,940 bytes. The importer retains one copy with pinned hashes.
- CGP's adapter maps stable cup identity plus course ordinal to PCM 10–64.
  Both GP and Practice use this mapping. Original courses use the equivalent
  CGP music; unrelated packs without a mapping use their SNES music.
- Missing PCM files use the adapter's original SPC fallback. CGP playback
  needs no separately selected ASM mod.
- A custom pack containing the supported, digest-pinned Conn/Cubear v11
  `f-zero_msu1.ips` uses that adapter and its standard numbering. Without
  that patch a custom pack uses CGP numbering. Other patch versions are not
  supported. The legacy adapter is composed into each derived car image.
- Save/load and rewind restart the restored song from the beginning. The
  shared core does not serialize an exact PCM cursor. Game-state replay is
  checked independently from audio position.

## Burndown and validation

- [x] Audit all three archives; compare every PCM hash and loop bound.
- [x] Import only approved music and make release packaging verify it.
- [x] Add opt-in shared UI capabilities without game-specific defaults.
- [x] Add three editable F-Zero presets and conflict handling.
- [x] Test all preset/MAX/Bower combinations and unrelated settings retention.
- [x] Test eleven CGP cups, original/BS mappings, missing music and unknown
      pack fallback with distinct synthetic stereo samples.
- [x] Test Practice music, actual rewind resimulation and legacy custom music
      with both stock and the additive car catalog.
- [x] Check real launcher selection and persistence across restart.
- [x] Check actual bundled audio through desktop playback and the core mixer.

Release handoff additionally verifies a fresh ZIP extraction against its full
file manifest, then starts that executable and checks preset/music behavior.

Reproduction: core CTest, recomp-ui's `launcher-presets` and
`launcher-video-pick` CTests, and `tests/validate_cgp_music.py` with a private
stock ROM. Captures stay under ignored `captures/msu-presets`.
