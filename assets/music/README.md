# Community Grand Prix soundtrack

The project owner initially approved bundling CGP music on 2026-09-23, then
requested removing all CosmicTailz and TheBlurCafe contributions. Eight exact
CosmicTailz matches are now excluded. **The remaining attribution is unresolved
and must be completed before release.** See [evidence and required follow-up](CGP_ATTRIBUTION.md).

Project leads are Worthy MF and
Fennor Virastar; see `../track-packs/cgp-credits.txt` for the project credits.
The included MSU adapter source credits Conn, Khilendel and Catador in
`mods/cgp-source/fzedit-msu.asm`. This permission does not change the licenses
or ownership of the music.

The owner supplied `F-Zero CGP P1.zip`, `F-Zero CGP P2.zip` and
`F-Zero CGP P3.zip`. All three contain the same 61 PCM files, verified by
SHA-256. The importer now retains 53 and skips eight confirmed exclusions,
renaming retained files to the common `cgp` prefix. Retained audio bytes and loop
points are unchanged. `cgp.json` records every retained/excluded size and hash.
The retained files are not yet cleared of all work by the two excluded authors.
No ROM or unrelated archive contents are extracted.

Select **Settings > Audio > Enable MSU-1 music**, leaving the source on
**Community Grand Prix**, or apply that preset in Mods. Music starts off on
a fresh installation. Custom music remains available from **Custom...**.
The smaller **without-msu** download omits the soundtrack while retaining MSU
support. Select your own music folder in Settings > Audio; its CGP preset uses
SNES audio until a custom source has been selected.

## Building from source

Large PCM files are not stored in Git. Import any one of the original archives:

```powershell
python tools/import_cgp_music.py 'E:\Downloads\F-Zero CGP P1.zip'
```

This creates the ignored `music/cgp` directory. Reconfigure and build to stage
it beside the executable under `assets/music/cgp`. The release tool validates
the 53 retained files before packaging, rejects excluded or unexpected files,
and warns that attribution is still incomplete. Do not place arbitrary user
music in this directory. For an old import, run
`python tools/import_cgp_music.py --prune-excluded music/cgp` before building.
Previously generated music ZIPs still contain the excluded files and must not
be treated as cleared releases.

The separately supported Conn/Cubear v11 patch and third-party custom packs
are user-supplied and are not part of this soundtrack bundle.
