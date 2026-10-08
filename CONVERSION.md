# Bringing courses into F-Zero Forever

Open **Custom Content**, choose **Import**, and select your ZIP, FZM,
IPS/BPS patch or hacked ROM. You can give the imported pack a display name.
The importer detects its contents, checks usable courses and installs them for
the next time you press Play. For an unpacked editor export, choose its FZM
file with its companions beside it, or ZIP the whole export. There is one
Import button for all of these inputs. The list also shows installed mod files.

CGP, Astra Front, Bower and MAX are included packs. This page cannot remove or
replace them. Huckmine belongs to CGP; it is not an additional default pack.

Reviewed patch conversion runs automatically with your selected game ROM.
You do not need Python or a command line. If a revision needs review, the
importer explains why it cannot be installed; the sections below describe how
an author or maintainer can qualify it.

Keep a spare copy of the files you received. Original F-Zero Edit projects are
the best input: they already contain the editable course. A ROM patch can also
be converted when its exact revision has a reviewed import profile.

Conversion adds course data and supported course mechanics. Vehicle changes,
global game rules and other donor ASM need separate adaptation. Recordings are
added separately; missing recordings use the course's selected SNES song.

## IPS/BPS

The converter accepts `.ips`, `.bps`, a ZIP containing patches, or a hacked
`.sfc`/`.smc` ROM. It checks the resulting ROM's SHA-256 rather than trusting a
filename. The reviewed profiles cover Astra Front, Bower League, the current CGP
P3test revision, and MAX League Classic/Modern. Older CGP revisions are not
accepted. The original reviewed Astra Front ROM is also recognized.

For maintainers, run this from the repository root:

```powershell
python tools/convert_course_content.py path/to/course.ips `
  --stock path/to/original-usa-fzero.sfc `
  --out captures/my-converted-pack `
  --exporter build/FZeroExportCourses.exe `
  --inspector build/FZeroInspectPacks.exe
```

The output directory must be new. A successful conversion contains `pack.json`,
`courses.json`, credits, and editable course ZIPs. Copy that folder into
`mods/packs` to try it. Install one copy of a pack: two copies with the same
stable ID conflict. Conversion preserves the reviewed pack, cup and course IDs,
authored names, race order, SPC selection and declared mechanics.

For a ZIP with equivalent IPS/BPS files, selection is automatic and prefers
BPS, whose checksums are verified. Known Classic/Modern variants can share one
reviewed profile. If a ZIP contains distinct packs or unknown differing targets,
select the intended patch with `--patch-member "folder/course.bps"`.

Unrecognized revisions create only `conversion-report.json` and `REVIEW.txt`.
The command returns **2** to distinguish review needed from successful conversion
(**0**) or a malformed input/tool failure (**1**). The report records hashes and
bounded file-difference evidence when a stock ROM is available. No patched ROM
is included in any output.

To qualify an unknown patch, ask its author for the original project first.
Otherwise a person or agent must review its loader, data tables and ASM, establish
the actual cup labels/order, names, SPC music, intro lettering and title layout,
and check every course's gameplay. Follow the concrete examples in
[the import checklist](mods/PARSE_MANIFEST.md). A new mechanic needs an implemented
native adapter; a successful data parse does not prove a full race works. Add
the validated exact revision to the reviewed registry and exporter profile
before unattended conversion. Arbitrary donor code is never run.

The recovered projects include the road, tile art, terrain, colors, sky, minimap,
AI/checkpoints, shortcuts, intro lettering, opponents and supported mechanics.
Keep each `_Reconstruction.json` companion with its editor files. It preserves
native details and existing record hashes while those editor components are
unchanged; edited components are rebuilt. Author layers, original working
filenames, editing history and ASM source cannot be recovered from a ROM.

Every conversion loads the resulting ZIPs through the native pack inspector and
checks all normalized record hashes. Reconstructed courses must also match the
donor's complete serialized resources, including SPC and intro glyph fields.
CGP Huckmine deliberately retains the supplied original F-Zero Edit project,
matching the shipped pack; its source art/compression and record hash differ from
the older ROM extraction. That exception is stated in the report.

CGP and MAX include their reviewed title artwork. Bower uses Original. Astra
shares CGP's artwork and reuses the CGP title choice; it does not add a duplicate
screen. Titles, music and display names do not change course record hashes.

## F-Zero Edit

Keep the `.fzm` file and all its companion files together. The companions contain
the track, computer drivers' route, artwork, sky, colors and minimap. Export or
ZIP the complete set, preserving relative paths and the author's credits.
Select that ZIP or FZM through **Custom Content > Import**. The game supplies the
pack description for an ordinary single-course project.

A bare editor export uses standard F-Zero course rules. FZEdit files do not
declare a hack's ASM changes. If the course relies on special rules, use the
author's pack with those rules declared in its manifest. For example, the
packaged CGP Huckmine example declares its magnet rules; the original bare
`HM.zip` cannot supply that declaration by itself. The importer does not guess
those rules from the course name or artwork.

For an original single-course F-Zero Edit ZIP, the maintainer helper can add the
pack descriptions without changing the original files:

```powershell
python tools/package_fzedit_course.py path/to/original-project.zip `
  path/to/my-course-pack.zip --id my-author-my-course --author "Course Author"
```

Choose a stable lowercase ID that belongs to this course. Add `--credits
path/to/CREDITS.txt` to preserve separate attribution. Compatible course mechanics
can be declared explicitly, for example `--requires grip-magnets up-magnets`;
those names mean the reviewed native behaviors described in
[the pack format](docs/PACK_FORMAT.md), not arbitrary ASM from the source.

The new ZIP keeps the original project members byte-for-byte. Copy it into
`mods/packs`, restart, and find its one-course cup. For a multi-course league,
use the cup/course examples in [the pack format](docs/PACK_FORMAT.md). A source
ZIP from an existing pack already has its league position in the enclosing
`courses.json`; keep that relationship when editing bundled courses.

For optional MSU music, match the course source filename: `courses/coast.zip`
uses `music/coast.pcm` in that pack. The names inside the ZIP do not select the
recording. The `.fzm` source supplies its SPC selection unless the enclosing
pack explicitly overrides it.

## Standalone Windows helper

The file picker can invoke `FZeroConvertContent.exe`, so testers do not need
Python. The helper uses the same arguments and exit codes as the Python command.
Ship `FZeroExportCourses.exe` and `FZeroInspectPacks.exe` beside it, including
their usual runtime dependencies. Pass their paths explicitly when staging an
import. The helper contains only the converter, Pillow and reviewed descriptors,
credits, title patches and the preserved Huckmine example; it contains no ROM
or audio recordings.

Build it with Python, Pillow and PyInstaller installed:

```powershell
./tools/build_convert_course_content.ps1 -Python path/to/python.exe `
  -EngineRoot path/to/populated/snesrecomp
```

The result is `build-content-converter/FZeroConvertContent.exe`. Checkout scripts
also use `SNESRECOMP_ROOT` when the shared-engine submodule is elsewhere. Native
conversion can later call `FzeroCourseLayoutRead`, `FzeroCourseExtract` and
`FzeroCourseFileWrite` directly; the standalone helper currently supplies the
editable-project reconstruction and native round-trip validation.
