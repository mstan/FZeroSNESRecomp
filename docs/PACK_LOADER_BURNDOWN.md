# F-Zero pack loader burndown

Baseline: `002c6d4` (0.2.0), branch `f-zero-forever`.
Owning issue: `beads-8wg.5.65`; shared PCM hook: `beads-8wg.2.76`.

The approved design is one all-or-nothing Track Pack Loader. Installed folders
and ZIPs use `pack.json`; courses can use raw FZEdit exports. No old-config
compatibility layer. BS tracks exclude the loader; vehicles remain independent.
The existing course runtime is the parity oracle, not a replacement engine.

- [x] Identify committed pre-Astra-MSU baseline.
- [x] Inspect FZEdit 1.2.0 bundled documentation and export reader/writer formats.
- [x] Define versioned JSON manifests and portable course resource encoding.
- [x] Implement bounded folder/ZIP discovery and diagnostics.
- [x] Decode raw FZM/AIP/TMX/TSX/indexed image exports without external tools.
- [x] Convert CGP, Astra, MAX, Bower with full decoded-resource comparisons.
- [x] Replace track mod entries with loader; update presets and BS exclusion.
- [x] Move course mechanics and title selection data into pack metadata.
- [x] Namespace SPC/MSU mappings and support separate audio packs.
- [x] Validate records, GP/Practice, reload/rewind and synthetic MSU routing.
- [ ] Package and inspect local 0.3.0 test builds with/without CGP music.

## Findings

FZM is a property file referencing component files. AIP explicitly stores main
and branch origins, checkpoint coordinates, and path/main/green/purple flags.
FZEdit map music is an SPC selection independent of venue. Raw imports must
preserve these values and reject unknown mechanics rather than run donor ASM.
Private inspection material stays under ignored `captures/fzedit-source`.
No Astra recordings were provided; only synthetic audio can be tested today.
