Optional F-Zero 55 title artwork

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

CGP P1/P2/P3 have a different "Community GP" logo. The F-Zero 55 artwork
is offered under CGP for this all-in-one build, disabled by default.
Its dropdown choice applies only while the CGP track pack is enabled.
