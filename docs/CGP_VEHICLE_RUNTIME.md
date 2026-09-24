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

Authored numeric information-card artwork and colors are resolved in
preview.6, as detailed below. Remaining B03/B12 work includes keeping existing
record paths stable when unrelated vehicle packs are added,
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

## Author-group columns and minimap colors (preview.5)

Enabled P1/P2/P3 packs now occupy full four-car columns in their authored native
row order (physical slots 0, 2, 1, 3):

| Row | P1 | P2 | P3 |
| --- | --- | --- | --- |
| 1 | Moon Shadow | Blue Falcon | White Cat |
| 2 | Great Star | Golden Fox | Red Gazelle |
| 3 | Dragon Bird | P. Emerald | Wild Goose |
| 4 | Death Anchor | Black Bull | Fire Stingray |

Each original identity appears once. If its pack is disabled it joins the
remaining originals in a separate, possibly partial column. Column membership
does not enable a rebalance. Initial selection remains Blue Falcon, now the
middle column when all three packs are enabled. The existing native carousel
presentation, GP cohorts and unrestricted Practice rivals are retained.
A separate snapshot marker rejects older CGP states containing obsolete menu
indices; the gameplay signature and existing course record keys are unchanged.

The owner confirmed that the alternating minimap dot belonged to Moon Shadow.
Sixty-four consecutive captures reproduce pink `$3D3F` and blue `$7E8C`, changing
every four frames. Native routine `$00:C163` alternates exhaust palettes at
WRAM `$0760/$0860` into `$0660`. The shared marker entry inherited another
car's color in one phase. Other physical slots retained retail marker colors,
including Dragon Bird's green Wild Goose dot.

Vehicle imports now retain only the two-byte marker color in each authored
HUD row (`$7CD06 + row * 32`), eight bytes per donor. The runtime resolves marker
colors using the stable identity in each racing slot, including a composed
Practice rival. It updates only those color entries in the current and both
buffered palettes before the native copy routine. Exhaust colors, timing and
other HUD entries remain native. Neither stock nor original BS mode installs
this hook. These marker resources are separate from record signatures.

The unusual CGP Blue Falcon rear/turning shape is present in the author's P2
artwork. Snes9x reference captures of the small artwork donor and full CGP P2
ROM show that body shape; the full donor also matches its central exhaust.
Enabling the other three original-car rebalances does not replace its artwork.
This does not claim complete pixel or full-race equivalence. The information
panel still used original numeric cards and color styling in preview.5.
The later Dragon Bird/Wild Goose card feedback is resolved in preview.6 below.

Private evidence under `captures/feedback-20260923`:

- `authored-columns-final`: 83 native-menu cases. Every displayed row has the
  expected group, full artwork bank, selected palette and dim palette, including
  all partial pack subsets and empty rows. All twelve cars enter races.
- `authored-columns-practice`: 41 cases, including unrestricted mixed rivals,
  all sixteen cups, eighty course previews and rewind.
- `authored-columns-gp`: 44 starting-field cases, including partial packs and
  sixteen group-switch/rewind checks.
- `authored-columns-motion`: six directional transitions, 84 captured frames,
  native/wide/HD composition and identical mid-slide rewind replay.
- `markers-final`: 22 cases. All twelve identities, four untuned originals,
  stock/BS baselines, Moon Shadow rewind and three cross-group Practice pairs.
  Checks both exhaust phases, source colors, native marker pixels, composed
  rival palettes, continuing exhaust animation and wide/HD Moon Shadow output.
- `blue-falcon-feedback`: private reference-emulator and recomp comparisons.
- Core CTest: 12/12 passed after these changes.

These focused checks do not close the broader B03/B10/B12 limitations.

## Authored information cards (preview.6)

The native confirmation panel now resolves its specification artwork and colors
from the selected stable identity, independently of its current display row.
Every CGP car uses its matching authored engine, power, speed, weight and
acceleration graph. Original identities keep the native retail card until their
individual rebalance is enabled. The original Satellaview layout, shared labels,
Yes/No controls and animations remain intact. This changes presentation only.

`tools/vehicle_cards.py` reads the native tilemap stream consumed by `$03:9892`
and converts the three-plane menu atlas consumed by `$03:98EE`. It extracts the
44 variable tiles in the existing Deluxe card layout and maps their white/curve
colors to the shared Deluxe palette. Horizontal/vertical tile flips are honored;
truncated/oversized streams, unsupported colors and curves outside the reviewed
layout are rejected. The three accent colors come from `$03:8901 + slot * 6`.
Extraction requires neither emulator execution nor hand-authored numeric values.
All three supplied artwork donors produce exactly the same card payloads as the
complete CGP P1/P2/P3 ROMs.

`fzero-vehicles-2` IPS inputs append a data-only card section after the 512 KiB
artwork image: eight-byte `FZCARD1` header (including NUL), then four physical-slot
records, each containing `$580` tile bytes and six accent-color bytes. The loader
verifies the whole target SHA-256, exact appendix length and header. Only selected
card tiles enter the reserved menu cartridge area; the appendix is never executed
or installed as donor game code. The original shared-HUD/fog guards remain.
Untuned original cards continue to use the canonical native resources.

The native `$1E:C4DA` hook assigns the two title/frame palette accents after the
original palette routine copies them. It changes no guest registers or shared
text/graph colors. Stock BS mode installs no expanded-catalog hooks. A snapshot-
only version marker prevents loading older CGP states containing inherited retail
card pixels/palettes in VRAM; gameplay signatures and course record keys stay
unchanged. Current card snapshots and actual rewind restore identically.

`tests/validate_vehicle_cards.py` passes 53 cases against independently executed
Snes9x cards: all twelve CGP identities; all four untuned originals; each original
rebalance enabled alone while viewing every original; every car in each standalone
pack; and Practice. Thirteen cases switch the derived vehicle image before actual
rewind and verify identical resimulation. Numeric fields and acceleration curves
match the donor's rendered pixels; frame accents match source colors. Shared
Deluxe label spacing is retained (some donors move their colons by one pixel).
Dragon Bird and Red Gazelle also match through wide and HD composition.

Private evidence: `captures/feedback-20260923/info-cards/{oracles,qualified2}`.
The independent reference core is the same Snes9x core recorded in the identity
audit. The existing 80 vehicle/race/rewind cases and 12 core CTests pass. All
sixteen BS-isolation runs preserve identical RAM and frames for each of the eight
cars with conflicting CGP settings. The broader presentation suite does expose
an existing failure: unrebalanced Blue Falcon inherits the CGP cohort boost OAM
layout. This reproduces in the shipped preview.5 desktop executable, independently
of the card correction. Evidence: `info-cards/{bs-isolation,previous-boost-check}`.
Per-car boost/exhaust isolation, broader full-race parity and record-namespace
work remain open; this is not a passing claim for the whole presentation suite.
