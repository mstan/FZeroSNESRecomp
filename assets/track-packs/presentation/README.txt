Optional title artwork

One Title screen override mod under Presentation selects Original, Community
Grand Prix or MAX League. It works independently of enabled course packs.
Original is the default; disabling the override remembers the selected style.
The CGP preset selects Community Grand Prix. Vanilla/Satellaview use Original.
F-Zero 55 remains hidden.

The reviewed registry is screens.txt: stable-ID|display name|visible or hidden.
Each entry uses <stable-ID>.ips in this directory. Original is built in.
New artwork in the same native sprite/DMA layout needs a reviewed patch and
registry entry; it does not need another mod or a hard-coded dropdown entry.
Different layouts need a reviewed adapter, never arbitrary donor code.

Source comparison: Astra Front's title tiles/palette exactly match CGP.
Bower League and BS Deluxe match the original title resources. They therefore
reuse existing choices instead of adding duplicate screens with different names.
Native animated scenery/menu layout remains supplied by the active engine.

Community Grand Prix

Source: the bundled track-packs/cgp.ips, supplied by the CGP authors.
Credits: Fennor Virastar, Worthy MF and the CGP contributors.
The artwork reads "Community GP". The CGP preset enables it.

cgp.ips is generated with:
  python tools/extract_title_patch.py assets/track-packs/cgp.ips assets/track-packs/presentation/cgp.ips
Output SHA-256:
27d02127016b848caaae7531e84ad5fd0370d196a6ae396ccb4c440bcb9b445f

MAX League

Source: the bundled MAX League Classic IPS. Credits: PowerPanda and Zephyrum25.
Generated with:
  python tools/extract_title_patch.py assets/track-packs/max-league.ips assets/track-packs/presentation/max-league.ips
The title DMA descriptors match stock; the extracted logo and palette were
visually checked in both the retail and BS engines. Full credits remain in
assets/track-packs/MAX-League-credits.txt.

F-Zero 55 (retained, hidden)

Source: FZero55.ips from the owner's FZero55.zip, version 2.
Original IPS SHA-256:
1f6e6bde6c641ad2f7097622818503b9291cc6bd6476f1f89893e8edead00955
The source archive's INSTRUCTIONS.txt does not identify the title artist.
Artwork remains credited to the F-Zero 55 hack's creators.

fzero-55.ips is a presentation-only derivative, generated with:
  python tools/extract_title_patch.py FZero55.ips fzero-55.ips
Output SHA-256:
65ea06f037e598d99901f6252c495286babce89ee4a0b41419ffafff0a106962

Only writes to the original title graphics ($0C:EC00-FFFF) and sprite
palette ($0F:C2E0-C35F) are retained. No engine changes, tracks, vehicle
changes, MSU support or music are present. A user-supplied original USA
ROM supplies the unchanged bytes; no ROM is bundled.

F-Zero 55 remains a separate backend style and artwork patch. Its menu choice
is hidden for now; it is not selected by the CGP preset. It is retained for a
future F-Zero 55 course pack.
