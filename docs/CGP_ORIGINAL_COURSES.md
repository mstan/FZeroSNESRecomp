# CGP revisions of the original leagues

All fifteen original-course slots in the approved CGP donor contain authored
checkpoint, path-coordinate or path-property edits. Ten also change road-map
tile cells. They are not merely the original courses with different global
car stats, and they are not fifteen entirely new course designs.

The library now offers **Knight CGP, Queen CGP and King CGP** in addition to
the untouched native cups. CGP imports all 55 donor course slots into eleven
cups. Including the original fifteen versions gives fourteen cups and seventy
selectable course versions. MAX remains hidden and off.

| Course | Changed map cells | Native / CGP checkpoints |
| --- | ---: | ---: |
| Mute City I | 0 | 59 / 65 |
| Big Blue | 0 | 84 / 87 |
| Sand Ocean | 489 | 108 / 110 |
| Death Wind I | 0 | 71 / 72 |
| Silence | 148 | 79 / 81 |
| Mute City II | 256 | 66 / 69 |
| Port Town I | 7,360 | 70 / 74 |
| Red Canyon I | 195 | 68 / 71 |
| White Land I | 321 | 96 / 96 |
| White Land II | 11,371 | 88 / 88 |
| Mute City III | 0 | 59 / 67 |
| Death Wind II | 0 | 87 / 87 |
| Port Town II | 2,940 | 111 / 112 |
| Red Canyon II | 67 | 100 / 100 |
| Fire Field | 1,688 | 145 / 142 |

Counts compare the actual retail course loader with the approved typed CGP
extraction. The map comparison walks every 8-unit cell in the 8192 by 4096
world, resolving native venue-variant block pointers before comparing tile
IDs. A changed tile ID is not necessarily a changed boundary. Active path
coordinates/properties are compared separately; unused checkpoint-buffer
tails and computed heading arrays are excluded. Even equal checkpoint counts
contain actual coordinate/property edits. Port Town I/II and White Land II
also change tile graphics; sky graphics match for all fifteen.

Native courses retain native loading and records. Added cups use distinct
stable cup/course IDs and course-resource hashes. Existing CGP cup IDs and
track order are retained, so adding these revisions does not rename or
replace their record namespaces. Car tuning remains an independent choice.

Private audit evidence: `captures/feedback-20260923/original-cups/audit.json`,
native WRAM/renderer captures, and approved extracted course resources. No
ROM, decoded course asset or save is committed.
