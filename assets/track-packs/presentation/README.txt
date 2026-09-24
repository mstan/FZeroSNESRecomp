Optional title artwork

Community Grand Prix

Source: the bundled track-packs/cgp.ips, supplied by the CGP authors.
Credits: Fennor Virastar, Worthy MF and the CGP contributors.
The artwork reads "Community GP" and is offered under the CGP track pack,
disabled by default. The CGP preset enables it.

cgp.ips is generated with:
  python tools/extract_title_patch.py assets/track-packs/cgp.ips assets/track-packs/presentation/cgp.ips
Output SHA-256:
27d02127016b848caaae7531e84ad5fd0370d196a6ae396ccb4c440bcb9b445f

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
