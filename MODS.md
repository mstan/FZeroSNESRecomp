# Mods and differences from the source games

Track packs add courses to the original 15. Their required terrain mechanics
follow the **course you are racing**. Vehicle rebalances, difficulty and other
general gameplay options follow your **Mods settings**, including when you
race a different pack. A preset selects options once; you can edit them afterward.
Presets leave unrelated choices, such as MAX and Bower, as you set them.

## Course behavior

| Content | Behavior in this port |
| --- | --- |
| Original Knight, Queen and King | Original courses remain available. CGP's revisions are additional cups. |
| CGP courses | Use their authored terrain properties, including required grip/up magnets. These mechanics work even with the optional magnet switches off. |
| MAX and Bower | Use their own imported course data and native magnet rules. Enabling the CGP preset does not change the meaning of their magnet tiles. |
| Astra Front | Adds ten courses in Astra and Front. Uses the same author's CGP grip/up-magnet mechanics, scoped to these courses. Unused CGP resources and global car/rule changes in its donor are excluded. See [the import audit](docs/ASTRA_FRONT_IMPORT.md). |
| Original BS tracks / CGP BS revisions | Alternative providers: enabling one disables the other. CGP supplies its corrected versions. |
| Marine City I / Lightning | CGP trampoline behavior belongs to Marine City I; the railroad landing behavior belongs to Lightning. |
| Rainbow Road | Its required gravity and illusion collision rules follow this CGP course. |

**CGP magnets:** `DOWNPULL_MAGNET` identifies the ground magnet/grip surface.
It does not, by itself, make the surface damaging in CGP. The separate
`MAGNET` property must be enabled for magnet damage. Red Canyon III's tile
`$A6` has both properties and **does damage**. Tiles with downpull only retain
their grip/pull without that damage. Ordinary native downpull damage remains
unchanged outside courses declaring CGP's magnet mechanics.

CGP up magnets additionally depend on the author's tile IDs (`$B6`, `$CCâ€“$CF`)
and airborne/tilt logic. Those IDs are not treated as up magnets in unrelated
packs. The retained magnet switches cannot force CGP tile semantics onto
stock, original BS, MAX or Bower. New packs must explicitly declare compatible
mechanics in their layout. See the [technical audit](docs/CGP_COURSE_CAPABILITIES.md).

## Music, cars and presentation

| Feature | Scope and intentional differences |
| --- | --- |
| SNES music | Imported courses use the donor's original song selection, independently of their scenery or display order. MAX and Bower retain their patched soundtrack mappings. |
| CGP music | With MSU-1 off or a recording absent, the course uses its SNES mapping. The one explicit donor override is Mute City III CGP: Mute City replaces the donor's Big Blue selection, interpreting the author's original-league fallback guidance. All mappings are [listed here](docs/CGP_SNES_MUSIC.md). |
| MSU-1 | Optional. The music bundle uses the cleared PC-port archive; the smaller download still supports custom music. Unmapped packs fall back to their SNES music. See [soundtrack notes](assets/music/README.md). |
| CGP vehicles | One mod enables all three authored groups together. Each car retains its own artwork and parameters. In GP, its other three group members are its rivals; Practice permits any enabled rival. |
| Original-car rebalances | One default-off opt-in changes all four original identities together on any course. Adding CGP courses alone does not enable them. |
| Stock BS vehicles | Preserve their native behavior; this mode excludes CGP vehicles and original-car rebalances. Track choice is independent. |
| Legend and general gameplay fixes | Opt-in globally; the CGP preset enables them. Legend's CPU changes apply after vehicle tuning. They can affect races outside CGP. |
| Title screen override | One Presentation mod offers Original, Community Grand Prix and MAX League independently of enabled track packs. Astra uses the same title artwork as CGP; Bower and BS use Original. F-Zero 55 remains retained but hidden. The CGP preset selects its title; Vanilla and Satellaview restore Original. |
| Ending credits ASM | Retained as attributed source only; the PC port does not install it. |
| Engine and menus | Course imports use the shared engine, renderer, HUD/culling fixes and expanded native-style menus. They do not replace the game with the donor ROM or import every donor code patch. |

Same-named courses in different packs can be different revisions. They retain
separate identities and records; names alone are not used to deduplicate them.
The records browser supports enabled cups and individual vehicle records.

## Keeping these notes accurate

For every imported pack, record its source revision, authored course and cup
names, race order, SNES music, required mechanics, intentional deviations and
remaining limitations. Preserve attribution. Do not infer music from venue
art, assume tile IDs have universal meanings, or copy course-specific ASM
over the whole game.

The [import checklist](mods/PARSE_MANIFEST.md) covers machine checks and manual
review. The metadata audit records donor versus selected songs, original
course names and omitted courses in partial packs. Cup labels still require
review against the donor menu or author's documentation. An unknown loader
needs a reviewed adapter, and a structural pass alone does not establish
full-race playability.

Imported intro lettering must be checked against the donor's actual font lookup,
not just decoded name bytes. Astra remaps its font; its authored R and Z graphics
are imported as scoped letter resources. Course and record IDs are unchanged.
