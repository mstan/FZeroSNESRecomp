# CGP gameplay sources

The author-supplied `CGP asm.zip` contains 29 ASM files. Their original bytes
are preserved here. Source comments and the bundled CGP credits retain the
original attribution, including Fennor Virastar and the CGP contributors;
the MSU source credits Conn, Khilendel and Catador. No additional license is
inferred from receiving the source.

All gameplay options are **off by default** in Mods. They work independently
of the course pack. P1, P2 and P3 are parameter choices, not three copies of
the courses. No music files, replacement vehicle art or ROM are bundled here.

| Mods option | Source files |
| --- | --- |
| CGP vehicle tuning, P1/P2/P3 | `CGP/{1,2,3}/CGP.asm` |
| CGP energy boost, P1/P2/P3 | `CGP/{1,2,3}/CGP_Boost.asm` |
| CGP exhaust placement, P1/P2/P3 | `CGP/{1,2,3}/CGP_Blast_Pipe.asm` |
| Blue Falcon / Golden Fox animation fix | `BF_GF_Animation_Fix.asm` |
| Dash plate facing fix | `Dash_Fix2.asm` |
| Dynamic collision spin | `Dynamic_Spin_Amount.asm` |
| Softer lateral bounce | `Lateral_Bounce_Damper.asm` |
| Stronger CPU lateral spin | `Lateral_Hit_Harder_CPU_Spin.asm` |
| Lateral collision direction fix | `Lateral_Hit_Redirect_Rotation.asm` |
| Legend difficulty | `Legend_Difficulty.asm` |
| Harmless grip magnets | `No_DMag_Damage.asm` |
| Require finish-line checkpoints | `No_Lap_Finish_on_Shortcut.asm` |
| Rainbow Road course rules | `CGP_Illusion.asm` + `Rainbow_Gravity.asm` |
| Red bumper fix | `redbumperfix-v2.asm` |
| Refined collision hitboxes | `Refined_Hitbox.asm` |
| Reverse magnets | `Reverse_DMAG .asm` |
| Smooth fog | `Smooth_Fog.asm` |
| CGP MSU music adapter | `fzedit-msu.asm` |
| CGP ending credits | `CGP_Credits.asm` |

The three top-level `CGP*.asm` tuning/boost/exhaust files duplicate P3.
Illusion and gravity share one switch because they describe the same Rainbow
Road behavior. The other fixes have individual switches.

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
- Deluxe uses per-car metadata instead of the stock column tables. The four
  base cars get the selected tuning profile, while BS stat records remain.
  Shared physics changes still apply to all cars.
- Energy boost extends the four profile entries to eight, repeating them
  for the corresponding BS car slots. It preserves the native HUD position.
- Exhaust coordinates are decoded from the source for all 13 animation
  frames, including P2 Wild Goose's three exhaust sprites. BS cars retain
  their native exhaust positions.
- Dynamic spin reads the Deluxe player's own weight/spin metadata.
- Legend uses five difficulty levels and its source CPU tables. It wins
  over tuning's overlapping CPU tables. Native opponent-frequency data is
  retained before those tables are overwritten. Course resources keep their
  own frequency values. Lives are 7/6/5/4/3 from Beginner through Legend.
- Grip magnets take precedence over turning/strafe lookup only on grounded
  magnet tiles; elsewhere the active stock/BS/tuning tables are used.
- Rainbow Road behavior uses the stable `cgp/rainbow-road` course ID, not
  the donor's absolute cup number. Other courses are unaffected by this mod.
- Optional MSU music maps stable cup/course positions to the author's track
  numbers. Unknown packs and missing audio fall back to SPC. Music still
  requires the user's own folder under Sound.
- Credits use the author's text and trigger on CGP VI at any difficulty;
  other cups retain their usual Expert-or-higher ending requirement.

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
shortcut, terrain and audio transition. Imported Practice courses and a
combined record browser remain separate work; imported courses currently
appear in Grand Prix. Original BS Practice courses are unavailable while
their track mod is off.

Snapshots include the active course catalog, enabled rules, assembled image
digest and adapter version. Changed rules use a separate records/save root,
so experimenting with tuning does not overwrite ordinary records.
