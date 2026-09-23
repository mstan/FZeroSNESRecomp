# Bower import, MAX visibility and CGP league names

Local branch: `fzero-55`. Tracking: `beads-8wg.5.47` (2026-09-23).

MAX is visible in Mods again after the owner reported PowerPanda's approval.
The included Classic patch, stable IDs and resources are unchanged. A shipped
`max-league.disabled=1` supplies the previous off default; a saved player choice
takes precedence. `max-league.hidden=0` explicitly overwrites the parked marker
when installing over an earlier build. No file picker is exposed.

CGP and Bower default on, giving 15 cups / 75 selectable course versions with
the native originals. Enabling MAX gives 16 / 80. Course imports retain the
canonical engine's cars, HUD and rendering hooks. No MSU audio is bundled.

## Bower provenance and resource layout

The owner's `F-Zero Bower League.zip` contains one IPS and its original readme.
Both are bundled unchanged, with the readme at
`assets/track-packs/Bower-League-credits.txt`. PowerPanda coordinated the
community challenge; the supplied readme credits individual course authors.

| Input | SHA-256 |
| --- | --- |
| Original USA ROM | `bf16c3c867c58e2ab061c70de9295b6930d63f29f81cc986f5ecae03e0ad18d2` |
| Bower IPS | `c4fbcc2c385230611be41e3c21fd158705fb6d80661733a295b6f88919116f7f` |
| Patched 1 MiB donor | `8def08ded7d1d526bacdc5bf0e7435c3f2732df7bece51596a16c73d79b3ba47` |

The loader at CPU `$10:822D` resolves GP race order through `$10:8612`.
There are seven resource entries, but that five-byte order is `6,4,5,3,2`.
Only those five published courses are exposed; internal slots 0/1 are unused.
The typed layout follows the actual loader's tables, including its bounded
palette-cycle lists at `$10:8786`. No additional executable donor code is used.

| Race | Resource slot | Course | Author(s) |
| --- | --- | --- | --- |
| 1 | 6 | Mute City V | Zephyrum, Erik64 |
| 2 | 4 | Sand Storm III | Moshikomi |
| 3 | 5 | Silence III | Vulduv |
| 4 | 3 | Red Canyon III | Zephyrum |
| 5 | 2 | Sand Ocean II | Fennor |

Names overlap with CGP, but none of these five extracted pool/block/grid
resources equals any of CGP's 55 or MAX's five. The corresponding CGP courses
also differ in grid sizes and checkpoint counts. They remain independently
selectable versions with separate pack/cup/course record identities.

## Authored CGP labels

The approved P3test donor's eleven menu strings are addressed by the pointer16
table at `$10:8747`; strings occupy `$10:875D..887A`, each thirteen tile words.
They use native small-font tile IDs, not ASCII. The race-order table at
`$10:8957` independently confirms the cup-to-resource mapping.

| Stable cup ID | Authored label | Resource slots in race order |
| --- | --- | --- |
| `cgp-1` | Baron | 25, 26, 53, 27, 50 |
| `cgp-2` | Scepter | 28, 29, 30, 31, 32 |
| `cgp-3` | Crown | 33, 54, 34, 35, 36 |
| `cgp-4` | Zenith | 37, 48, 38, 39, 49 |
| `cgp-5` | Falcon | 40, 41, 42, 43, 51 |
| `cgp-6` | True | 52, 44, 46, 47, 45 |

Only display labels changed. Existing cup/track IDs, resources, race order and
record hashes remain intact. Knight/Queen/King CGP and BS-1/BS-2 CGP retain
their provenance suffix to distinguish them from the native versions.

## Validation

- `validate_bower.py`: 11 cases pass. All five courses load matching pool,
  block and grid bytes in retail and Deluxe engines. All five injected GP
  results advance in order and finish the cup. Donor menu names and race-order
  bytes match the manifests; the stock ROM is unchanged.
- `validate_bundled_tracks.py`: 12 cases pass, including fresh defaults,
  individual pack toggles, MAX opt-in, combined catalogs, duplicate IPS copies,
  native stock/BS operation and race entry for all three packs.
- `validate_native_menus.py --all-track-packs`: 38 cases pass, including the
  complete 16-cup vertical list and all twelve car selections/race entries.
- `validate_practice_catalog.py --all-track-packs`: forward/reverse navigation
  reaches all 16 leagues and 80 course previews, including wraparound.
- CTest: all 12 checks pass, including a default-off/explicit-enable/unhide
  regression and bundled checkbox/no-picker checks.

Private evidence is under `captures/feedback-20260923/bower-resources-qualified`,
`bower-bundled`, `bower-native-menus`, `bower-practice`, and
`bower-practice-leagues`. Course checks establish extraction and transitions;
they do not claim every Bower course was manually driven through full laps.
