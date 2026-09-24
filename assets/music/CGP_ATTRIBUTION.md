# CGP soundtrack attribution and release requirement

**Release requirement: do not publish a bundled soundtrack until the remaining
CosmicTailz and TheBlurCafe contributions have been identified and excluded.**
Tracked in `beads-8wg.5.53`. The owner confirmed that unresolved attribution
should be documented for removal before release.

The P1/P2/P3 source archives contain the same 61 numbered PCM files and setup
instructions, but no per-track composer list. MSU PCM files contain an eight-byte
format/loop header and raw audio, not artist tags. The original
`mods/cgp-source/CGP_Credits.asm` includes both names in its encoded Sound and
Music section. We retain those original source credits as provenance; they are
not a claim that all credited music is included in our bundle.

The author's [CGP release credits](https://romhackplaza.org/romhacks/f-zero-community-grand-prix-cgp-super-nintendo-romhack/)
name Silverreploid CLE, TheBlurCafe, Cobalt Star, CosmicTailz, Facecat,
Cody O'Quinn and Doktor Fill. No track-by-track assignment is given.

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

## Still unresolved

The [TheBlurCafe reference pack](https://www.mediafire.com/file/mgzb3kr6abux795/TheBlurCafe_recreated_Satellaview_PCM.zip/file)
linked by its PCM converter contains six distinct recordings (plus four duplicate
numbered copies). Its archive SHA-256 is
`6b307c014f24acbb21b4edd514616ae713e9ed9f6c2465dbb53c77cb36c50a69`.

None is an exact match to a CGP file. Exploratory waveform and spectral
comparisons also did not establish reliable correspondence. This is **not proof
that CGP contains none of TheBlurCafe's work**: conversions, edits, alternate
recordings or references not in this pack may differ. Likewise, the eight exact
CosmicTailz matches do not establish that no other contributions exist.

The retained 53 files are therefore **not certified free of those contributors**.
Obtain an authoritative composer-to-PCM/course list, or additional attributable
recordings, before clearing this release requirement. Add confirmed removals to
`excluded_tracks`, prune/reimport, and regenerate the bundled download. Do not
remove names from original source credits to imply the audio has been cleared.

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
contain 53 retained PCM files plus the empty descriptor. Exclusions are absent
on disk, not merely hidden by Git. Global PCM/MSU ignore patterns provide a
secondary guard. Future imports never extract known excluded tracks.

The without-msu ZIPs remain available. Original source archives in the owner's
Downloads directory are outside this worktree and were not modified. Historical
extracted preview directories have been pruned and are no longer exact
reproductions of the deleted ZIPs; regenerate a new package from source when
the remaining attribution requirement is resolved.

For an existing source import, first remove only the hash-verified exclusions:

```powershell
python tools/import_cgp_music.py --prune-excluded music/cgp
```

New imports skip the excluded recordings automatically. Existing build output
is pruned during staging. Packaging checks the retained file hashes and emits an
attribution warning until this review is completed. This warning permits private
previews while preserving the explicit requirement before public release.

## Remaining PCM/course map for author review

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
| `10` | Mute City I CGP |
| `11` | Big Blue CGP |
| `12` | Sand Ocean CGP |
| `13` | Death Wind I CGP |
| `14` | Silence CGP |
| `15` | Mute City II CGP |
| `16` | Port Town I CGP |
| `17` | Red Canyon I CGP |
| `18` | White Land I CGP |
| `19` | White Land II CGP |
| `20` | Mute City III CGP |
| `21` | Death Wind II CGP |
| `22` | Port Town II CGP |
| `23` | Red Canyon II CGP |
| `24` | Fire Field CGP |
| `26` | Big Blue II |
| `27` | Sand Storm I |
| `29` | Silence II |
| `30` | Mute City IV |
| `32` | Sand Storm II |
| `35` | Marine City I |
| `36` | Big Blue III |
| `37` | Mercury Sea |
| `38` | Forest IV |
| `39` | Sulfur Swamp |
| `40` | Mute City V |
| `41` | Cloud Carpet |
| `42` | Lethal Cave |
| `43` | Red Canyon III |
| `44` | Crystal Forest I |
| `45` | Warp Sector I |
| `46` | Sand Ocean II |
| `47` | Gold District |
| `48` | White Land III |
| `49` | Fire Field II |
| `50` | Huckmine |
| `51` | Big Blue IV |
| `52` | Empyrean Colony |
| `54` | Volcania |
| `55` | Metal Fort III |
| `57` | Cloud Carpet II |
| `59` | Lightning |
| `60` | Rainbow Road |
| `61` | Mute City VI |
| `62` | Sunset Drive VR |
| `63` | Sandstorm III |
| `64` | Warp Sector II |
