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

Validation so far: all twelve identities enter a CGP race; the twenty handling
fields match the actual authored source slot (or retail for unchanged cars).
All four separate rebalances, all seven nonempty pack combinations and real
keyboard navigation through twelve entries pass. All twelve identities pass
Legend-enabled snapshot/resimulation/reset, including a deliberately different
car cohort installed between saving and loading. The native tests cover all
nine valid BS/CGP switch combinations and configuration migration. Selector
previews were inspected on all three pages. Evidence:
`captures/feedback-20260923/vehicle-qualified` and `vehicle-selector`.

Final qualification still needs broader course/Practice/rival coverage, the
reported HUD/garble fixes, release verification and all rendering modes. The
vehicle manifest adapter currently qualifies these three CGP sources; an
unknown vehicle format still requires a reviewed adapter.
