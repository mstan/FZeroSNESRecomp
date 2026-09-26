# CGP gameplay sources

The author-supplied `CGP asm.zip` contains 29 ASM files. Their original bytes
are preserved here. Source comments and the bundled CGP credits retain the
original attribution, including Fennor Virastar and the CGP contributors;
the MSU source credits Conn, Khilendel and Catador. No additional license is
inferred from receiving the source.

Legend is **enabled by default** for fresh settings. Other general gameplay
options remain editable. General options work independently
of the course pack; required terrain mechanics follow the declaring course.
The CGP preset enables the available options, including Legend. P1, P2 and P3 are coherent vehicle sets, enabled together through one
CGP vehicles mod plus one original-car rebalance option.
Artwork-only IPS deltas live in `assets/vehicle-packs`; no ROM is bundled.
The CGP soundtrack and source clearance are documented in `assets/music/README.md`.

| Feature | Source files |
| --- | --- |
| Per-identity CGP handling | `CGP/{1,2,3}/CGP.asm` |
| Per-identity CGP energy boost | `CGP/{1,2,3}/CGP_Boost.asm` |
| Per-identity CGP exhaust | `CGP/{1,2,3}/CGP_Blast_Pipe.asm` |
| Blue Falcon / Golden Fox animation fix | `BF_GF_Animation_Fix.asm` |
| Dash plate facing fix | `Dash_Fix2.asm` |
| Dynamic collision spin | `Dynamic_Spin_Amount.asm` |
| Softer lateral bounce | `Lateral_Bounce_Damper.asm` |
| Stronger CPU lateral spin | `Lateral_Hit_Harder_CPU_Spin.asm` |
| Lateral collision direction fix | `Lateral_Hit_Redirect_Rotation.asm` |
| Legend difficulty | `Legend_Difficulty.asm` |
| CGP grip magnets | `No_DMag_Damage.asm` |
| Require finish-line checkpoints | `No_Lap_Finish_on_Shortcut.asm` |
| Rainbow Road course rules | `CGP_Illusion.asm` + `Rainbow_Gravity.asm` |
| Red bumper fix | `redbumperfix-v2.asm` |
| Refined collision hitboxes | `Refined_Hitbox.asm` |
| Reverse magnets | `Reverse_DMAG .asm` |
| Smooth fog | `Smooth_Fog.asm` |
| CGP MSU music adapter | `fzedit-msu.asm` |
| Retired ending patch (source only) | `CGP_Credits.asm` |

`CGP_Credits.asm` is retained for attribution but is not assembled or installed.
Its saved rule bit is reserved; older configurations cannot reactivate it.

The 29 source files break down into 11 general gameplay rules/fixes, 9 vehicle
sources (handling, boost and exhaust for each of three sets), 4 required terrain
sources, 1 MSU adapter, 1 retired credits patch, and 3 duplicate vehicle sources.
The three top-level `CGP*.asm` tuning/boost/exhaust files duplicate P3.
All 11 remaining general rule/fix checkboxes originate in these sources.
The four terrain sources are exposed only as three required pack capabilities:
grip magnets, up magnets, and combined Rainbow illusion/gravity. They have no
Mods entries. Vehicle settings and music have their own controls.

## Adaptation and build

The runtime never executes the CGP donor cartridge. The course importer
extracts typed resources; gameplay mods separately apply the supplied,
reviewed ASM to the canonical stock/BS engine. This preserves the common
renderer, HUD, visibility, input and state hooks.

`tools/build_cgp_mods.py` uses Asar 1.91 at build time. It assembles each source
against two differently filled blank images, retaining only authored writes.
The checked-in `src/fzero_gameplay_patches.inc` therefore needs neither a ROM
nor an assembler at runtime. To regenerate:

```powershell
python tools/build_cgp_mods.py --asar path/to/asar.exe
```

Adaptations are explicit in the generator and `src/fzero_gameplay.c`:

- Each source gets its own free bank. No source is applied on top of a
  previously patched donor; options have a fixed application order.
- Deluxe uses per-car metadata instead of stock column tables. The expandable
  catalog binds each identity to its own source slot and parameters. Original
  identities retain retail stats unless their explicit rebalance is selected.
  Stock BS vehicle mode excludes the CGP roster and rebalances entirely.
- Energy boost belongs to CGP identities. The historical eight-slot profile
  extension remains only in the private legacy-profile test path; it is not a
  user option and does not tune stock BS cars.
- Exhaust coordinates are decoded from the source for all 13 animation
  frames, including P2 P. Emerald's three exhaust sprites. BS cars retain
  their native exhaust positions.
- Dynamic spin reads the Deluxe player's own weight/spin metadata.
- Legend uses five difficulty levels and its source CPU tables. It wins
  over tuning's overlapping CPU tables. Per-vehicle launch acceleration lasts
  until the source's Straightaway handoff. Native opponent-frequency data is
  retained before those tables are overwritten. Course resources keep their
  own frequency values. Lives are 7/6/5/4/3 from Beginner through Legend.
- Required magnet and Rainbow capabilities are declared by course layouts and
  have no global mod switches. CGP magnet semantics stay scoped: the runtime uses shared hooks,
  never global copies of the two magnet patches. Downpull gives grip; the
  separate MAGNET property controls damage. Grip magnets take precedence over turning/strafe lookup only on grounded
  magnet tiles; elsewhere the active stock/BS/tuning tables are used.
- Rainbow Road behavior follows the course's required mechanics module,
  never the donor cup number or a global switch. Other courses are unaffected.
- Optional MSU music maps stable cup/course positions to the author's track
  numbers. Unknown packs and missing audio fall back to SPC. Music still
  uses the cleared bundle or a custom folder under Audio settings.
- The credits ASM is source-only and cannot be activated by saved settings.

Player-facing differences are maintained in [MODS.md](../../MODS.md).

The only differing overlapping source writes are tuning with Legend's CPU
tables and tuning with the grip-magnet turning/strafe hooks. They are composed
deliberately as above, without excluding either option.

## Validation

`tests/validate_cgp_rules.py` runs each option on stock and BS engines, combined
profiles, Legend selection, four/eight-car combinations, corrected/original
BS courses, and snapshot resimulation/reset. Its headless-only
`tests/probe_cgp_rules.c` executes guest instruction fragments to check
checkpoint gating, dash direction/airborne behavior, collision spin, magnet
grip, course gravity, class lives, native Practice availability and exhaust
sprite equivalence across engines. Captures and ROM-derived files stay private.

These checks do not replace driving full laps through every collision,
shortcut, terrain and audio transition. Imported courses are available in
Grand Prix, Practice and the records browser. Original BS Practice courses
are unavailable while their track mod is off.

Snapshots include the active course catalog, enabled rules, assembled image
digest and adapter version. Changed rules use a separate records/save root,
so experimenting with tuning does not overwrite ordinary records.
