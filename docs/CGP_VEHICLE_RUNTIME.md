# Expanded CGP vehicle runtime

The selector has stable ship identities, independent of the guest's four main
racing slots. P1 adds four ships, P2 adds two and P3 adds two. All three packs
can coexist. Four separate options rebalance the existing retail identities;
the selector never duplicates them. All new vehicle options default off.
Stock BS mode excludes every CGP vehicle/rebalance option, in both directions.
Conflicting saved settings favor stock BS mode. Legacy tuning/boost/exhaust
switches are retired without silently opting into a new roster.

Each selected ship retains its donor cohort of three main rivals, subject to
enabled packs and retail-rebalance choices. Disabled additions fall back to
the corresponding original identity. A retail rebalance uses its own donor
data even when appearing in another cohort. This does not increase the number
of simultaneous main racers.

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
