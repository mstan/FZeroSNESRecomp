# Native SNES music for imported courses

Tracked by `beads-8wg.5.59`. The author's September 25 feedback identifies ten
CGP/BS course themes and confirms native music for the original three leagues.
Every course now has a native SNES fallback, whether MSU is disabled or a PCM
file is missing. Courses whose intended soundtrack is MSU use the fallback
already selected in the donor, not an invented approximation of its recording.

## Source and fix

The approved P3test donor selects music from `$109729`, indexed by resource
slot. Entries are offsets into the canonical ten-song SPC upload list, not
venue IDs. The former importer did not extract this table. Deluxe therefore
uploaded its native course's music while imported geometry was displayed;
the retail engine inferred music from venue flags instead.

The optional `music` layout field imports and validates the table. A shared
hook at `$00F7E6` supplies the correct offset and resumes the original uploader
at `$00F7F1`. The MSU adapter and PCM numbering remain unchanged. An optional
named `spc` override allows corrections without changing donor code or artwork.
MAX (`$108762`) and Bower (`$1087D7`) declare their tables too. Native retail
and original BS courses still use their original metadata.

The only CGP override is resource 10, Mute City III: the donor says Big Blue;
the author's original-league guidance is interpreted as Mute City. All ten
specifically named CGP/BS mappings already agree with the donor table.

Music metadata is excluded from course/vehicle record keys, so existing times
are preserved. A pre-fix save state can retain its old, already-loaded SPC
song; restarting the race loads the corrected theme.

## Complete CGP mapping

| Cup | Course | SNES theme |
| --- | --- | --- |
| Knight CGP | Mute City I | Mute City |
| Knight CGP | Big Blue | Big Blue |
| Knight CGP | Sand Ocean | Sand Ocean |
| Knight CGP | Death Wind I | Death Wind |
| Knight CGP | Silence | Silence |
| Queen CGP | Mute City II | Mute City |
| Queen CGP | Port Town I | Port Town |
| Queen CGP | Red Canyon I | Red Canyon |
| Queen CGP | White Land I | White Land I |
| Queen CGP | White Land II | White Land II |
| King CGP | Mute City III | Mute City (override) |
| King CGP | Death Wind II | Death Wind |
| King CGP | Port Town II | Port Town |
| King CGP | Red Canyon II | Red Canyon |
| King CGP | Fire Field | Fire Field |
| BS-1 CGP | Forest I | Big Blue |
| BS-1 CGP | Big Blue II | Big Blue |
| BS-1 CGP | Sand Storm I | Big Blue |
| BS-1 CGP | Forest II | Big Blue |
| BS-1 CGP | Silence II | Silence |
| BS-2 CGP | Mute City IV | Mute City |
| BS-2 CGP | Forest III | Big Blue |
| BS-2 CGP | Sand Storm II | Big Blue |
| BS-2 CGP | Metal Fort I | Big Blue |
| BS-2 CGP | Metal Fort II | Big Blue |
| Baron | Marine City I | Big Blue |
| Baron | Big Blue III | Big Blue |
| Baron | Mercury Sea | Silence |
| Baron | Forest IV | White Land I |
| Baron | Sulfur Swamp | Silence |
| Scepter | Mute City V | Mute City |
| Scepter | Cloud Carpet | Sand Ocean |
| Scepter | Lethal Cave | Death Wind |
| Scepter | Red Canyon III | Red Canyon |
| Scepter | Crystal Forest I | Silence |
| Crown | Warp Sector I | White Land I |
| Crown | Sand Ocean II | Sand Ocean |
| Crown | Gold District | Sand Ocean |
| Crown | White Land III | White Land II |
| Crown | Fire Field II | Fire Field |
| Zenith | Huckmine | Port Town |
| Zenith | Big Blue IV | Big Blue |
| Zenith | Empyrean Colony | Sand Ocean |
| Zenith | Silence III | Silence |
| Zenith | Volcania | Fire Field |
| Falcon | Metal Fort III | Port Town |
| Falcon | Death Wind III | Death Wind |
| Falcon | Cloud Carpet II | Sand Ocean |
| Falcon | Port Town III | Port Town |
| Falcon | Lightning | Port Town |
| True | Rainbow Road | White Land II |
| True | Mute City VI | Mute City |
| True | Sunset Drive VR | Port Town |
| True | Sandstorm III | Sand Ocean |
| True | Warp Sector II | White Land II |

## Verification

`tests/validate_spc_music.py` boots real races and inspects SPC RAM `$07FE`,
the destination of the original uploader's song-selection byte. Expectations
are independent of the runtime mapping and extraction manifest. It covers all
55 CGP courses, all ten songs with the retail engine and missing recordings,
MAX/Bower, and original cup controls. `tests/validate_cgp_music.py` uses distinct
synthetic stereo recordings to check PCM playback, Practice and rewind.

The parser tests reject invalid donor values, addresses and override declarations,
check all ten native song IDs including zero, and verify unchanged record hashes.
Private evidence is kept under ignored `captures/spc-*` directories.

September 25 validation: 90 distinct real-SPC cases passed (55 CGP courses,
10 retail-engine themes, 10 missing-PCM themes, 10 MAX/Bower courses, three
native-cup controls, and two Practice cases including rewind). Four synthetic
MSU playback/fallback/Practice/rewind checks and three parser/catalog/storage
CTest targets also passed. The initial sweep hit the runtime's save-path limit
in 24 test folders before boot; short isolated roots fixed the harness, and
all 24 were rerun successfully. Combined evidence:
`captures/spc-mapping-validation.json`; MSU evidence:
`captures/spc-msu-regression-01/validation.json`.
