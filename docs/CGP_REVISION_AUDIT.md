# CGP venue revision, 2026-09-23

The bundled course patch now reconstructs the author's `F-Zero CGP P3test.sfc`
from the original USA ROM. It does not append replacement records to the old
patch: the delta is regenerated, so superseded art is not retained as overwritten
IPS records. No ROM is shipped. See `assets/track-packs/CGP-credits.txt` for hashes.

Compared with old P3, exactly 3,011 ROM bytes differ. The four low-bank changes
are the checksum/complement at file offsets `0x7fdc..0x7fdf`, not executable code.
The resource table locations and extraction layout are unchanged.

| Course | Extracted changes |
| --- | --- |
| Volcania (49) | 9 palette bytes, 2,462 sky graphics bytes, 426 rear sky map bytes, 96 front sky map bytes |
| Forest IV (27) | Setting `c0` to `c8` |
| Lethal Cave (30) | Setting `c2` to `c4` |
| Gold District (34) | Setting `c3` to `c2` |
| Sunset Drive VR (46) | Setting `c8` to `c3` |
| Lightning (51) | Setting `c7` to `c3` |
| Rainbow Road (52) | Setting `c1` to `c9` |
| Mercury Sea (53) | Setting `c5` to `c1` |

The seven setting changes preserve the high course-type bits and change the
low venue nibble. The corresponding seven donor music-table entries also change.
All 55 courses retain identical geometry, terrain properties, paths/checkpoints,
opponents, shortcuts, map positions, road tiles, minimaps and palette-cycle data.
All other extracted resources are identical. The canonical loader receives the
revised settings; donor executable code is still excluded from course import.

The revised resource hashes isolate records for affected cups and invalidate
incompatible snapshots. Existing record files are retained on disk. Older
P1/P2/P3 hashes are removed from the bundled manifest, so loose old patches cannot
replace the current built-in course input. Their private source files remain
available for vehicle and behavior research.

Validation evidence is private under `captures/feedback-20260923/venue`.
Extraction/resource comparison and exact IPS reconstruction pass. In-game
rendering and fresh-release verification are recorded in the burndown separately;
these structural checks alone do not establish complete course playability.
