# CGP soundtrack attribution and source clearance

**Resolved on 2026-09-24:** the owner confirms that the replacement
`F-Zero CGP P1 MSU PCPORT.zip` already reconciles the CosmicTailz/TheBlurCafe
attribution requirement. Its 29 digest-pinned recordings are cleared for this
project's bundle. This resolves `beads-8wg.5.53`; the earlier archives are not
the approved soundtrack source. Original credits and historical exclusion
evidence remain below, without inventing a per-track composer assignment.

The P1/P2/P3 source archives contain the same 61 numbered PCM files and setup
instructions, but no per-track composer list. MSU PCM files contain an eight-byte
format/loop header and raw audio, not artist tags. The original
`mods/cgp-source/CGP_Credits.asm` includes both names in its encoded Sound and
Music section. We retain those original source credits as provenance; they are
not a claim that all credited music is included in our bundle.

The author's [CGP release credits](https://romhackplaza.org/romhacks/f-zero-community-grand-prix-cgp-super-nintendo-romhack/)
name Silverreploid CLE, TheBlurCafe, Cobalt Star, CosmicTailz, Facecat,
Cody O'Quinn and Doktor Fill. No track-by-track assignment is given.

## Replacement PC-port source

On 2026-09-24 the owner replaced the three old soundtrack inputs with
`F-Zero CGP P1 MSU PCPORT.zip`. It contains 29 recordings (566,847,024 audio
bytes) and an empty descriptor. None of the eight confirmed exclusions is
present. The retained files match their previous hashes. The other 24
previously retained recordings are now superseded, not newly attributed to
either excluded author. Their hashes remain in `superseded_tracks` solely
so staging can physically delete old copies. This archive is the complete
soundtrack source, not an additive update.

Owner-confirmed source archive SHA-256:
`6917ab1887094e4dd84328e7044c8a9a3f76a1b1b24c04bbecca9dcebe0590aa`

## Confirmed exclusions

The [BS Deluxe MSU release thread](https://www.zeldix.net/t2768-bs-f-zero-deluxe-msu-1)
identifies the CosmicTailz pack converted by Facecat and links the reference
archive below. Eight CGP recordings exactly match reference files, including
their loop headers and all audio bytes. Attribution here is based on SHA-256,
not similarities between course names or subjective listening.

Reference: [BS F-Zero Deluxe - CosmicTailz MSU1.zip](https://drive.google.com/file/d/1ex5kCsi10KRF7FJbYMkDJJCWvrKGdypD/view)

Reference archive SHA-256:
`ab1065d4d346942280d7f2fc8af82f6def60d7d9c127723327a6ae02bdab891f`

| Excluded CGP PCM | Used for | Exact reference file |
| --- | --- | --- |
| `cgp-25.pcm` | Forest I | `f-zero_msu1-22.pcm` |
| `cgp-28.pcm` | Forest II | `f-zero_msu1-25.pcm` |
| `cgp-31.pcm` | Forest III | `f-zero_msu1-28.pcm` |
| `cgp-33.pcm` | Metal Fort I | `f-zero_msu1-30.pcm` |
| `cgp-34.pcm` | Metal Fort II | `f-zero_msu1-31.pcm` |
| `cgp-53.pcm` | Silence III | `f-zero_msu1-26.pcm` |
| `cgp-56.pcm` | Death Wind III | `f-zero_msu1-20.pcm` |
| `cgp-58.pcm` | Port Town III | `f-zero_msu1-18.pcm` |

Each excluded file's complete digest, size, loop point and evidence are retained
in `cgp.json` under `excluded_tracks`. The importer skips those recordings, the
build staging helper removes old matching copies, and package verification
rejects a bundle if excluded files are present. The runtime's existing missing
PCM handling uses the course's SNES music; courses and car groups are unchanged.

## Historical reference comparison

The [TheBlurCafe reference pack](https://www.mediafire.com/file/mgzb3kr6abux795/TheBlurCafe_recreated_Satellaview_PCM.zip/file)
linked by its PCM converter contains six distinct recordings (plus four duplicate
numbered copies). Its archive SHA-256 is
`6b307c014f24acbb21b4edd514616ae713e9ed9f6c2465dbb53c77cb36c50a69`.

None is an exact match to a CGP file. Exploratory waveform and spectral
comparisons also did not establish reliable correspondence. This is **not proof
that CGP contains none of TheBlurCafe's work**: conversions, edits, alternate
recordings or references not in this pack may differ. Likewise, the eight exact
CosmicTailz matches do not establish that no other contributions exist.

That comparison left the original archive review unresolved. The owner has
since resolved the requirement by confirming the replacement PC-port archive
was already reconciled. Clearance comes from that confirmation, not from
the inconclusive reference comparison. Original source credits remain intact.

The reference audio ZIPs were physically deleted after attribution. Private
text evidence remains in `captures/msu-attribution`: waveform/spectral results,
file hashes and the cleanup audit. No reference music is retained in this
worktree. The recorded URLs and archive hashes allow reacquisition if needed.

## Existing downloads and build workflow

At the owner's request, the obsolete preview.7, preview.8-with-msu and
preview.9-with-msu ZIPs were physically deleted, along with their checksum
sidecars. The two downloaded reference music ZIPs were also deleted. Another
56 excluded PCM copies were removed from seven capture/staging directories.
A fresh audit found no known excluded recording in any remaining PCM file
or ZIP in this worktree; no PCM/MSU/ZIP files are tracked in Git.

The canonical extracted source set, `music/cgp`, and active build staging each
contain 29 retained PCM files plus the empty descriptor. Exclusions are absent
on disk, not merely hidden by Git. Global PCM/MSU ignore patterns provide a
secondary guard. Future imports never extract known excluded tracks.

The without-msu ZIPs remain available. Original source archives in the owner's
Downloads directory are outside this worktree and were not modified. Historical
extracted preview directories have been pruned and are no longer exact
reproductions of the deleted ZIPs. Current packages use the complete cleared
PC-port source; the old soundtrack is not restored.

For an existing source import, remove the hash-verified exclusions and superseded files:

```powershell
python tools/import_cgp_music.py --prune-retired music/cgp
```

New imports skip the excluded recordings automatically. Existing build output
is pruned during staging. Packaging checks the retained file hashes and rejects
unexpected files. The manifest records the completed review and its exact
source archive; the former incomplete-attribution warning no longer applies.

## Bundled PCM/course map

This is a usage map, **not an attribution table**. Numbers follow CGP playback
mapping; an original course and its CGP revision share the same PCM number.

| PCM | Used for |
| --- | --- |
| `1` | Start jingle |
| `2` | Zoom jingle |
| `3` | Lost life |
| `4` | Title |
| `5` | Selection / records |
| `7` | Ending |
| `35` | Marine City I |
| `37` | Mercury Sea |
| `38` | Forest IV |
| `39` | Sulfur Swamp |
| `41` | Cloud Carpet |
| `42` | Lethal Cave |
| `44` | Crystal Forest I |
| `45` | Warp Sector I |
| `46` | Sand Ocean II |
| `47` | Gold District |
| `48` | White Land III |
| `49` | Fire Field II |
| `50` | Huckmine |
| `51` | Big Blue IV |
| `52` | Empyrean Colony |
| `55` | Metal Fort III |
| `57` | Cloud Carpet II |
| `59` | Lightning |
| `60` | Rainbow Road |
| `61` | Mute City VI |
| `62` | Sunset Drive VR |
| `63` | Sandstorm III |
| `64` | Warp Sector II |
