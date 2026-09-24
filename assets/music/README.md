# Community Grand Prix soundtrack

The **with-msu** Windows preview includes the CGP soundtrack with the author's permission,
confirmed by the project owner on 2026-09-23. Project leads are Worthy MF and
Fennor Virastar; see `../track-packs/cgp-credits.txt` for the project credits.
The included MSU adapter source credits Conn, Khilendel and Catador in
`mods/cgp-source/fzedit-msu.asm`. This permission does not change the licenses
or ownership of the music.

The owner supplied `F-Zero CGP P1.zip`, `F-Zero CGP P2.zip` and
`F-Zero CGP P3.zip`. All three contain the same 61 PCM files, verified by
SHA-256. Only one copy ships, renamed to the common `cgp` prefix. The original
PCM bytes and loop points are unchanged. `cgp.json` pins every size and hash.
No ROM or unrelated archive contents are extracted.

Select **Settings > Audio > Enable MSU-1 music**, leaving the source on
**Community Grand Prix**, or apply that preset in Mods. Music starts off on
a fresh installation. Custom music remains available from **Custom...**.
The smaller **without-msu** download omits the soundtrack while retaining MSU
support. Select your own music folder in Settings > Audio; its CGP preset uses
SNES audio until a custom source has been selected.

## Building from source

Large PCM files are not stored in Git. Import any one of the approved archives:

```powershell
python tools/import_cgp_music.py 'E:\Downloads\F-Zero CGP P1.zip'
```

This creates the ignored `music/cgp` directory. Reconfigure and build to stage
it beside the executable under `assets/music/cgp`. The release tool validates
all 61 files before packaging, and refuses unexpected content. Do not place
arbitrary user music in this directory.

The separately supported Conn/Cubear v11 patch and third-party custom packs
are user-supplied and are not part of this soundtrack bundle.
