# CGP vehicle artwork

These three bundled IPS files contain only the reviewed vehicle graphics,
palettes and preview resources extracted from artwork supplied by the CGP
authors. They require the user's verified retail F-Zero USA ROM. No ROM is
included. Credits: Fennor Virastar, Worthy MF and the CGP contributors.

`tools/import_cgp_vehicles.py` reproduces these deltas from the private author
inputs. Each manifest records source/target hashes, the original author-input
hash, stable ship identities, source slots and whether the entry is an addition
or a rebalance. Title, course, shared HUD, fog and donor executable changes are
excluded. Handling, boost and exhaust come from the separately retained author
ASM in `mods/cgp-source/CGP/<profile>`.

This version of the vehicle adapter qualifies three known CGP cohorts. Adding
an unknown vehicle format needs a reviewed resource/behavior adapter; these
manifests do not authorize arbitrary executable writes or guess a patch's
vehicle identities. The track-pack manifest system remains independent.
