# CGP vehicle identity audit

Names below were read from each supplied graphics donor's running car information
screen, using Snes9x through the private snesref capture tool. Menu order differs
from internal table order: the menu displays slots **0, 2, 1, 3**. Treating menu
row 1 as table slot 1 assigns the wrong stats and exhaust to two cars.

| Pack | Table slot | Menu row | Identity | Relationship to retail | Boost drain parameter |
| --- | --- | --- | --- | --- | --- |
| P1 | 0 | 0 | Moon Shadow | Added | 23 |
| P1 | 1 | 2 | Dragon Bird | Added | 21 |
| P1 | 2 | 1 | Great Star | Added | 19 |
| P1 | 3 | 3 | Death Anchor | Added | 23 |
| P2 | 0 | 0 | Blue Falcon | Rebalance and new art | 22 |
| P2 | 1 | 2 | P. Emerald | Added | 21 |
| P2 | 2 | 1 | Golden Fox | Rebalance and new art | 23 |
| P2 | 3 | 3 | Black Bull | Added | 20 |
| P3 | 0 | 0 | White Cat | Added | 22 |
| P3 | 1 | 2 | Wild Goose | Rebalance and new art | 17 |
| P3 | 2 | 1 | Red Gazelle | Added | 22 |
| P3 | 3 | 3 | Fire Stingray | Rebalance and new art | 21 |

The displayed abbreviation **P. Emerald** is retained without inventing an
expansion. All boost duration parameters are 150; drain parameters above are
the authored values before the source routine's scaling. Exhaust geometry is
the corresponding pack/slot's complete 13-frame source table, including the
three-pipe P2 slot 1 (P. Emerald, despite stale Wild Goose comments).

There are **12 identities across CGP**, comprising eight additions and four
existing retail identities. None is one of the four stock BS additions:
Blue Thunder, Luna Bomber, Green Amazone or Fire Scorpion. The supplied vehicle
graphics also differ from each BS vehicle's graphics bank. The requested
whole-pack BS-versus-CGP conflict policy still applies regardless of overlap.

Enable all three additions packs to obtain the original four plus eight new
ships. Expose CGP versions of existing retail cars as separate opt-in rebalances
of those identities. Enabling an additions pack alone must not silently replace
an original car's handling. A rebalance must switch its matching presentation,
stats, boost and exhaust together. BS mode retains all eight baseline cars.

## Source association and limits

- Unsuffixed `F-Zero (USA).sfc` supplies P1 art; `(1)` supplies P2 and `(2)` P3.
  SHA-256 values are recorded in `CGP_FEEDBACK_BURNDOWN.md`.
- Each small donor retains stock code/stat banks 00/01/02. Its menu's advertised
  specs are artwork; its actual runtime handling is not CGP balance evidence.
- All 20 main stat fields, 30 turning samples and 29 acceleration samples for
  each of the 12 slots match the corresponding independently assembled
  `mods/cgp-source/CGP/{1,2,3}/CGP.asm` and the complete CGP donor. Boost and
  exhaust use that same numbered set, never an independent global profile.
- White Cat is P3 slot 0. Moon Shadow is P1 slot 0. They are distinct identities,
  so the author's correction does not rename Moon Shadow globally.
- Race art, menu preview, palette, information labels and HUD/life icons must
  be extracted with their consumers under B03. Do not copy whole graphics banks
  blindly: the last bank also contains shared HUD data, and small/full donors
  differ in those shared resources.

Private evidence: `captures/feedback-20260923/roster-oracle/names.png` (16 running
information screens), corresponding WRAM/traces, and `roster-audit.json`
(per-slot stat/art hashes and comparisons). Reference core SHA-256:
`31d196105d9ea27bbc48737baa4b5ffe8556133e3006c7507fbd9de022560d56`.
These establish identities and source mapping; expanded-roster integration,
per-resource extraction and donor physics parity are validated by B03/B04/B12.
