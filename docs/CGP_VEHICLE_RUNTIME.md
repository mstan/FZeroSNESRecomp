# Expanded CGP vehicle runtime

The selector has stable ship identities, independent of the guest's four main
racing slots. P1 adds four ships, P2 adds two and P3 adds two. All three packs
can coexist. Four separate options rebalance the existing retail identities;
the selector never duplicates them. All new vehicle options default off.
Stock BS mode excludes every CGP vehicle/rebalance option, in both directions.
Conflicting saved settings favor stock BS mode. Legacy tuning/boost/exhaust
switches are retired without silently opting into a new roster.

Grand Prix uses the selected ship's authored four-car group, with the other
three as its main rivals:

| Enabled vehicle pack | Four-car group |
| --- | --- |
| P1 | Moon Shadow, Dragon Bird, Great Star, Death Anchor |
| P2 | Blue Falcon, P. Emerald, Golden Fox, Black Bull |
| P3 | White Cat, Wild Goose, Red Gazelle, Fire Stingray |

Membership depends on the enabled vehicle pack, independently of tuning.
For example, unmodified Blue Falcon still races P. Emerald, Golden Fox and
Black Bull when P2 is enabled. If its pack is disabled, a retail identity
races the original four-car field. Rebalance-only configurations do not add
hidden opponents. Each identity retains its own selected handling, art,
boost and exhaust. Stock BS mode is unchanged. Practice can select any enabled
opponent regardless of these groups. This does not increase the race grid.

The new GP grouping has a snapshot compatibility marker, separate from the
gameplay/record signature: old states with potentially mismatched rival
actors/art are rejected while existing course record keys are retained.

Validation: `tests/validate_gp_rosters.py` passes 44 starting-field cases.
It checks actual spawned CPU slots against the other three donor identities,
each slot's authored art and selected handling, all seven enabled-pack subsets,
and rebalances with/without additions. Sixteen cases also switch groups before
rewind and verify identical resimulation. The complete 41-case Practice matrix
still passes, including unrestricted cross-group opponents and 16 cups / 80
course previews. Private evidence: `gp-groups-qualified` and
`gp-groups-practice` under `captures/feedback-20260923`.

The adapter constructs canonical runtime images from the verified retail/BS
engine, narrowly imported car artwork and the independently assembled ASM.
It never installs a donor executable or course/HUD/title replacement. Cohort
changes update the canonical cartridge before execution, preserving shared
renderer, culling, course and state hooks. The selected stable identity lives
in an unused byte of the native player metadata, so it is captured by ordinary
snapshots and rewind. All prepared images and options contribute to the
catalog signature. Loading a state restores the derived cartridge data before
execution resumes. Course records additionally include the selected identity,
preventing guest-slot reuse from replacing another ship's records.

The original four in the CGP catalog use the verified retail stat and curve
tables. Inspection found two Wild Goose metadata values in the preexisting
Deluxe conversion that differ from retail. The catalog imports those fields
from retail; legacy stock BS mode retains its existing native baseline.

The car selector uses the original BS guest menu, including native typography,
selected sprites, dimmed background previews, adjacent columns, sliding,
confirmation and the information panel. Two native display columns form a
viewport onto the enabled roster. Left/Right moves between roster columns;
Up/Down moves within a column. Select advances through the whole roster.
Partially filled columns leave unused rows empty and cannot select them.
Three bytes at WRAM `$7F:4CE0` retain the page bindings for snapshots/rewind.
Menu-only graphics/palettes occupy reserved cartridge data at file offsets
`$300000..$343BFF`; racing-slot metadata remains separate. The vehicle asset
adapter includes both normal and authored dim palettes.

League selection remains one vertical list. Up/Down scrolls every enabled cup,
including past the five visible rows; Select advances and the list wraps.
Horizontal input does not page the league catalog.

Validation: `tests/validate_native_menus.py` passes 38 cases: all twelve native
selections and actual race entries, seven partial catalogs, both directions of
column wrapping, Select cycling, all fourteen leagues, reverse/wrapped/held
vertical input, and no horizontal league changes. The desktop
`FZERO_MENU_PAD_REPLAY=1` route uses an SDL virtual gamepad through the actual
controller path, selects the third car column, moves back, confirms a car and
scrolls through all fourteen leagues. A separate Windows keyboard injection
attempt aborted when window focus was unavailable; it is not counted as a pass.

The expanded 80-case `tests/validate_vehicles.py` suite passes after the native
menu restoration: authored handling fields, all twelve identities, four retail
rebalances, partial packs, snapshot/reset, actual rewind-ring resimulation,
Practice, retail/BS course providers and all-rule probes. Attract-mode loading
no longer creates a spurious vehicle record namespace. Practice record keys
are independent of the last selected GP cup.

Private evidence: `captures/feedback-20260923/native-menus-qualified`,
`native-vehicles-final` and `desktop-menu-check/pad.log`.

Remaining B03/B12 work includes importing authored numeric information-card
artwork (the native panel currently retains the corresponding retail card),
keeping existing record paths stable when unrelated vehicle packs are added,
full-race/rival qualification and the replacement sharing release. The adapter
currently qualifies these three CGP sources; an unknown vehicle format requires
a reviewed adapter.

## Practice and September 23 playtest corrections

The native car carousel shows clipped neighboring previews on both sides.
The guest still owns selection, sprite animation, palettes, confirmation and
the information panel. Its two backing columns are presented as a continuous
strip: Right always brings the next column in from the right, Left reverses
that motion, and the native cursor always faces right from the ship's left.
At rest the selected column stays at one position. A 56-pixel column pitch
leaves matching 16-pixel preview areas inside the original 104-pixel pane.
Up/Down, Select, partial-column behavior and circular wrapping are unchanged.

The native slide counter supplies motion; direction is stored at WRAM
`$7F:4CE7` for snapshots/rewind. During car-select scanout only, authored BG
tiles, row palettes and selected OBJ positions are arranged into that strip.
The selected car's six sprite tiles are clipped at the pane edge; other OBJ
tiles, including both native text labels, stay visible. Temporary
VRAM changes are restored after capturing/drawing the frame, so guest VRAM,
other menu panels, stock BS mode and racing resources remain intact.

Practice uses the same complete vertical league catalog as Grand Prix. Up/Down
and Select also cross cup boundaries in the native on-track course preview.
With all bundled CGP courses enabled, all 14 leagues / 70 course versions are
reachable in both directions. Native Practice record keys retain their previous
namespace; imported courses use their own course hash and cup identity.

The original Practice rival panel scrolls over all enabled identities in its
native two-row arrangement. Left/Right moves columns; Up/Down moves rows and
the original No Rival/Ghost choices; Select advances through the full roster.
The selected rival is composed into a spare physical racing slot, with its own
art bank, palette, metadata and acceleration curve. Its identity is independent
of the player cohort and survives snapshots/rewind. A custom rival can use the
extended movement range without replacing the player's stat/boost tables.
Reserved runtime cartridge storage extends through `$35821F`; WRAM
`$7F:4CE3..4CE6` stores rival identity, viewport, version and physical slot.

The results-screen red bar was a deferred-scanout window-latch bug, reproduced
with the unmodified BS cartridge. Final HDMA window bounds now survive when
the next scene disables that channel. Old results snapshots receive the native
empty bounds (left 1, right 0), including the owner's reported save slot 1.
CGRAM/OAM restoration remains separate to preserve menu palette ownership.

Additional private-ROM validation:

- `validate_practice_catalog.py`: 41 cases, all twelve rivals, all twelve players
  with mixed rivals and rewind, four retail rebalances, seven partial catalogs,
  No Rival/Ghost navigation, all 14 leagues and all 70 preview entries/wrapping.
  Actual acceleration consumers and composed handling/art data are compared
  with the authored ASM tables and curated source art. Ghost replay creation
  and full-race AI parity are outside this check.
- `validate_results_window.py`: fresh BS results and the original user snapshot,
  stock/wide/HD output plus old-state rewind. Private Snes9x comparison confirms
  the native empty results window; no ROM/state/render artifacts are committed.
- 80 existing vehicle cases, 38 native-menu cases, 20 presentation/BS-isolation
  cases and 12 CTest checks passed during this correction pass.
- A desktop SDL virtual-gamepad replay selected White Cat versus Red Gazelle,
  scrolled to the 14th league, moved backward/forward across a course boundary
  and entered its race with widescreen and HD Mode 7 enabled. Evidence is in
  `desktop-practice-qualified/practice.log` beside the captures below.

Evidence remains under `captures/feedback-20260923`: `practice-catalog-qualified`,
`results-qualified`, `vehicles-practice-expanded`, `native-menus-expanded` and
`presentation-practice-expanded`. The broader B03/B12 limitations above remain
open; these checks are not a claim of complete CGP donor parity.

Follow-up continuity validation: `validate_carousel_motion.py` checks each
intermediate frame of six directional transitions, including both wrap points.
Sprite positions move monotonically in the requested direction, the cursor
keeps its side/orientation, both native captions remain visible, and the text
area remains pixel-identical. Native, widescreen and HD compositor output
matches the PPU through all 24 intermediate transition frames. Full raw
captures match before/after an actual rewind performed in the middle of a
slide. Evidence: `carousel-motion-render-qualified/motion.png` and `validation.json`.
The 38 native-menu cases, twelve mixed Practice player routes and 12 CTest
checks also pass after the continuity correction.

The launcher font/logo/controller regression was an incomplete private
playtest directory: source-tree assets omit the shared UI assets assembled by
CMake. `tools/stage_playtest.py --build build --output <fresh-directory>
--profile <previous-playtest>` now stages the full built asset tree, checks
required font/branding/controller files, verifies their hashes and preserves
the owner's settings/saves. A launcher screenshot confirms restored Lato,
SNES branding and controller art (`continuous-desktop-check/launcher-restored.png`).
Release packaging already uses the complete built assets; profile-based private
playtests must not be distributed as release ZIPs.
