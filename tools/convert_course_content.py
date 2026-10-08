"""Convert a reviewed IPS/BPS or hacked ROM to an editable course pack.

Exact donor digests select reviewed layouts. Unknown donors produce a ROM-free
report and return exit code 2; they never select guessed offsets or execute ASM.
Use package_fzedit_course.py when original FZEdit source is available.
"""
import argparse
from collections import Counter
import hashlib
import io
import json
from pathlib import Path, PurePosixPath
import re
import stat
import subprocess
import tempfile
import zipfile

from audit_track_metadata import audit_metadata
from course_tool_paths import ROOT, native_tool
from export_runtime_packs import export_pack
from import_astra_front import SOURCE_SHA256 as ASTRA_SOURCE_SHA256
from inspect_bs_deluxe import STOCK_SHA256
from parse_track_pack import donor, fields, recognize

IMAGE_LIMIT = 16 * 1024 * 1024
INPUT_LIMIT = 32 * 1024 * 1024
ZIP_TOTAL_LIMIT = 64 * 1024 * 1024
ZIP_MEMBER_LIMIT = 128
KNOWN_IDS = ('astra-front', 'bower-league', 'cgp', 'max-league')
ID_PATTERN = re.compile(r'[a-z0-9][a-z0-9-]{0,46}\Z')

REVIEW = """This revision has not been qualified. No course pack was created.

1. Keep the original submission and author's credits. Confirm its required
   stock revision and record the input/target SHA-256 from conversion-report.json.
2. Ask for the original FZEdit project first. It avoids reverse engineering.
3. Otherwise review the donor's course loader and ASM in the headless Ghidra
   registry. Identify bounded data tables and their consumers; file differences
   alone do not identify courses or prove that mechanics are supported.
4. Review authored cup labels, course names, race order, SPC song selection,
   intro glyphs, title layout, and course-specific behavior. Reuse only reviewed
   grip-magnets/up-magnets/rainbow-road capabilities. New behavior needs a native
   adapter and tests. No donor executable code is installed by this converter.
5. Follow mods/PARSE_MANIFEST.md to qualify a typed layout and exact revision.
   Add its digest, stable IDs, attribution and validation to the reviewed
   registry; extend the exporter profile if it is a new pack, then rerun into
   a new output directory. Arbitrary layouts are not accepted by this command.
6. Test GP, Practice, every cup transition, native SPC fallback, stock/other-pack
   isolation and records/save reload. Structural extraction is not a full race.

See CONVERSION.md and mods/PARSE_MANIFEST.md. Keep patched ROMs private.
"""


def digest(data):
    return hashlib.sha256(data).hexdigest()


def read_bounded(path, limit):
    with Path(path).open('rb') as stream:
        data = stream.read(limit + 1)
    if len(data) > limit:
        raise ValueError(f'Input exceeds {limit:,} bytes: {path}')
    return data


def strip_header(data):
    return (data[512:], 512) if len(data) % 1024 == 512 else (data, 0)


def read_stock(path):
    data, _ = strip_header(read_bounded(path, IMAGE_LIMIT + 512))
    if digest(data) != STOCK_SHA256:
        raise ValueError('Expected the original F-Zero USA stock ROM')
    return data


def zip_patches(data, member=None):
    """Read bounded patch candidates without extracting files or following links."""
    with zipfile.ZipFile(io.BytesIO(data)) as archive:
        entries = archive.infolist()
        if len(entries) > ZIP_MEMBER_LIMIT:
            raise ValueError('ZIP contains too many members')
        total, names, patches = 0, set(), []
        for entry in entries:
            raw = entry.orig_filename
            path = PurePosixPath(raw)
            parts = raw.rstrip('/').split('/')
            if (not raw or '\x00' in raw or path.is_absolute() or '\\' in raw
                    or ':' in raw or any(part in ('', '.', '..') for part in parts)
                    or any(part.endswith((' ', '.')) for part in parts)
                    or stat.S_ISLNK(entry.external_attr >> 16)):
                raise ValueError('ZIP contains an unsafe member path or link')
            key = raw.rstrip('/').casefold()
            if key in names:
                raise ValueError('ZIP contains duplicate member names')
            names.add(key)
            total += entry.file_size
            if entry.file_size > INPUT_LIMIT or total > ZIP_TOTAL_LIMIT:
                raise ValueError('ZIP expanded data exceeds the input limit')
            if entry.flag_bits & 1:
                raise ValueError('Encrypted ZIP members are unsupported')
            if not entry.is_dir() and path.suffix.lower() in ('.ips', '.bps'):
                patches.append(entry)
        if member is not None:
            patches = [entry for entry in patches if entry.filename == member]
        if not patches or (member is not None and len(patches) != 1) or len(patches) > 8:
            raise ValueError('Select exactly one IPS/BPS with --patch-member; '
                             'original FZEdit ZIPs use package_fzedit_course.py')
        result = []
        for entry in patches:
            with archive.open(entry) as stream:
                patch = stream.read(INPUT_LIMIT + 1)
            if len(patch) > INPUT_LIMIT or len(patch) != entry.file_size:
                raise ValueError('ZIP patch exceeds the input limit')
            result.append((patch, entry.filename))
        return result


def select_patch(candidates, stock):
    if len(candidates) == 1:
        return candidates[0], []
    if stock is None:
        raise ValueError('Automatic ZIP patch selection requires --stock')
    valid = []
    for patch, name in candidates:
        try:
            target = donor(stock, patch)
            profile = reviewed_profile(target)
            valid.append(dict(member=name, target_sha256=digest(target),
                              profile=fields(profile)['id'][0] if profile else None))
        except ValueError:
            continue
    reviewed = [entry for entry in valid if entry['profile']]
    choices = reviewed or valid
    identities = {entry['profile'] if reviewed else entry['target_sha256'] for entry in choices}
    if not choices or len(identities) != 1:
        raise ValueError('ZIP has distinct or unusable donor revisions; select --patch-member')
    selected = min(choices, key=lambda entry: (not entry['member'].lower().endswith('.bps'), entry['member']))
    return next(candidate for candidate in candidates if candidate[1] == selected['member']), valid


def read_submission(source, stock=None, patch_member=None):
    source = Path(source)
    raw = read_bounded(source, INPUT_LIMIT)
    report = dict(input_name=source.name, input_sha256=digest(raw), input_bytes=len(raw))
    patch = raw
    if source.suffix.lower() == '.zip' or raw.startswith(b'PK\x03\x04'):
        (patch, member), candidates = select_patch(zip_patches(raw, patch_member), stock)
        report['patch_member'] = member
        if candidates:
            report['archive_patch_targets'] = candidates
    elif patch_member is not None:
        raise ValueError('--patch-member requires a ZIP submission')
    if patch.startswith((b'PATCH', b'BPS1')):
        if stock is None:
            raise ValueError('IPS/BPS conversion requires --stock with the original USA ROM')
        target = donor(stock, patch)
        report.update(input_kind='bps' if patch.startswith(b'BPS1') else 'ips',
                      patch_sha256=digest(patch), bps_crcs_verified=patch.startswith(b'BPS1'))
    elif source.suffix.lower() in ('.sfc', '.smc', '.rom', '.fig') and 'patch_member' not in report:
        target, header = strip_header(raw)
        report.update(input_kind='rom', removed_copier_header_bytes=header)
    else:
        raise ValueError('Expected IPS, BPS, a patch ZIP, or a hacked .sfc/.smc ROM')
    if not 0x8000 <= len(target) <= IMAGE_LIMIT:
        raise ValueError('Donor image must be between 32 KiB and 16 MiB')
    report.update(target_sha256=digest(target), target_bytes=len(target))
    if stock is not None:
        report['stock_sha256'] = digest(stock)
    return target, report


def reviewed_profile(target):
    known = recognize(target)
    if known is None and digest(target) == ASTRA_SOURCE_SHA256:
        known = ROOT/'assets/track-packs/astra-front.ini'
    if known is not None and fields(known)['id'][0] in KNOWN_IDS:
        return known
    return None


def validate_identities(data):
    """Protect output paths and preserve unambiguous registry identities/order."""
    for field in ('format', 'id', 'name', 'author', 'adapter', 'source_sha256'):
        if len(data.get(field, [])) != 1:
            raise ValueError(f'Registry needs exactly one {field}')
    if (data['format'] != ['1'] or data['adapter'] != ['fzero-course-v1']
            or data['source_sha256'] != [STOCK_SHA256]
            or not ID_PATTERN.fullmatch(data['id'][0])):
        raise ValueError('Unsupported registry identity or format')
    cups, tracks, slots = {}, set(), set()
    for row in data.get('cup', []):
        parts = row.split('|')
        if (len(parts) != 3 or not ID_PATTERN.fullmatch(parts[0])
                or parts[0] in cups or not parts[1] or not parts[2].isdecimal()):
            raise ValueError('Invalid or duplicate cup identity')
        cups[parts[0]] = 0
    for row in data.get('track', []):
        parts = row.split('|')
        if (len(parts) != 4 or not ID_PATTERN.fullmatch(parts[0])
                or parts[0] in tracks or not parts[1] or parts[2] not in cups
                or not parts[3].isdecimal() or not 0 <= int(parts[3]) < 128
                or int(parts[3]) in slots):
            raise ValueError('Invalid, duplicate or missing course identity')
        tracks.add(parts[0]); slots.add(int(parts[3])); cups[parts[2]] += 1
    if not 1 <= len(cups) <= 32 or not 1 <= len(tracks) <= 128 or any(not 1 <= n <= 5 for n in cups.values()):
        raise ValueError('A pack needs 1–32 cups with 1–5 courses each')


def changed_summary(stock, target):
    """Bound the report even for an image with alternating changed bytes."""
    ranges, banks, count, range_count, start = [], Counter(), 0, 0, None
    for offset in range(min(len(stock), len(target)) + 1):
        changed = offset < min(len(stock), len(target)) and stock[offset] != target[offset]
        if changed:
            count += 1; banks[offset // 0x8000] += 1
            if start is None:
                start = offset
        elif start is not None:
            range_count += 1
            if len(ranges) < 128:
                ranges.append(dict(file_offset=start, length=offset-start, ownership='unreviewed'))
            start = None
    return dict(changed_stock_bytes=count, size_delta=len(target)-len(stock),
                changed_stock_bytes_by_bank={f'{bank:02x}': n for bank, n in sorted(banks.items())},
                changed_ranges=ranges, changed_range_count=range_count,
                changed_ranges_truncated=range_count > len(ranges))


def inspect_roundtrip(inspector, temp, pack, baseline, extraction):
    dump = temp/'roundtrip'; dump.mkdir()
    run = subprocess.run([str(Path(inspector).resolve()), str(pack.parent), '--dump', str(dump)],
                         cwd=temp, capture_output=True, text=True, timeout=120, check=False,
                         creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
    if run.returncode or run.stderr.strip():
        raise ValueError(f'Native pack validation failed: {run.stdout} {run.stderr}')
    index = json.loads((pack/'courses.json').read_text(encoding='utf-8'))
    ident = index['id']
    expected = {f'{ident}--{course["id"]}.fzc' for course in index['courses']}
    if {p.name for p in dump.glob('*.fzc')} != expected:
        raise ValueError('Native validation did not load every declared course')
    hashes = {}
    for line in run.stdout.splitlines():
        match = re.fullmatch(re.escape(ident)+r'/([a-z0-9-]+) ([0-9a-f]{64})', line)
        if match:
            if match[1] in hashes:
                raise ValueError('Native validation returned a duplicate course')
            hashes[match[1]] = match[2]
    if hashes != extraction['record_hashes']:
        raise ValueError('Editable projects changed normalized course record hashes')
    exact = []
    for course in index['courses']:
        cid = course['id']
        if ident == 'cgp' and cid == 'huckmine':
            continue  # Preserved author source intentionally supersedes older ROM data.
        if (dump/f'{ident}--{cid}.fzc').read_bytes() != (baseline/f'{cid}.fzc').read_bytes():
            raise ValueError(f'Editable round trip changed native course fields: {cid}')
        exact.append(cid)
    title_hashes = {title['id']: digest((pack/title['source']).read_bytes())
                    for title in index.get('titles', [])}
    return dict(native_loader_passed=True, record_hashes=hashes,
                byte_exact_to_donor_courses=exact, validated_title_sha256=title_hashes,
                fields_checked='all serialized native course fields, including SPC and intro glyphs')


def convert(source, out, *, stock=None, exporter=None, inspector=None, patch_member=None):
    """Publish one new directory only after validation; never alter the inputs."""
    out = Path(out)
    if out.exists() or out.is_symlink():
        raise ValueError(f'Refusing to replace existing output: {out}')
    out = out.resolve()
    stock_data = read_stock(stock) if stock is not None else None
    target, report = read_submission(source, stock_data, patch_member)
    profile = reviewed_profile(target)
    report.update(format='fzero.content-conversion', version=1, source_rom_exported=False,
                  donor_code_executed=False)
    if profile is None:
        report.update(status='review-required', reason='Target SHA-256 has no reviewed export profile',
                      qualification_document='mods/PARSE_MANIFEST.md')
        if stock_data is not None:
            report['file_difference_evidence'] = changed_summary(stock_data, target)
        out.mkdir(parents=True, exist_ok=False)
        (out/'conversion-report.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
        (out/'REVIEW.txt').write_text(REVIEW, encoding='utf-8')
        return report
    if stock_data is None:
        raise ValueError('Known ROM export requires --stock for reviewed title resources')
    exporter = Path(exporter or native_tool('FZeroExportCourses.exe')).resolve()
    inspector = Path(inspector or native_tool('FZeroInspectPacks.exe')).resolve()
    if not exporter.is_file() or not inspector.is_file():
        raise ValueError('Build FZeroExportCourses/FZeroInspectPacks or supply --exporter and --inspector')
    data = fields(profile); validate_identities(data)
    metadata = audit_metadata(target, fields(profile.with_suffix('.layout')), data)
    report.update(status='converted', pack_id=data['id'][0], profile=profile.name,
                  profile_sha256=digest(profile.read_bytes()),
                  layout_sha256=digest(profile.with_suffix('.layout').read_bytes()),
                  metadata=metadata, cup_labels='previously reviewed registry labels; not redecoded here',
                  gameplay_qualification='Existing profile qualification; this run checks data only')
    out.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.fzero-conversion-', dir=out.parent) as temporary:
        temp = Path(temporary)
        pack = temp/'packs'/data['id'][0]
        baseline = temp/'native'; baseline.mkdir()
        extraction = export_pack(stock_data, target, profile.with_suffix(''), pack, exporter,
                                 KNOWN_IDS.index(data['id'][0]), native_resources=baseline)
        report['validation'] = inspect_roundtrip(inspector, temp, pack, baseline, extraction)
        if data['id'][0] == 'cgp':
            report['preserved_author_source'] = dict(course='huckmine',
                donor_record_hash=extraction['extracted_huckmine_hash'],
                selected_record_hash=extraction['record_hashes']['huckmine'],
                note='Original Huckmine FZEdit project retained, matching the shipped pack; differs from donor extraction')
        report['omissions'] = ['donor executable patches, vehicles and global game rules',
                               'original author editing layers/history for reconstructed courses',
                               'MSU PCM recordings (add separately)']
        report['title_policy'] = ('Reviewed presentation profile; Astra reuses the CGP title '
                                  'supplied by the CGP pack, Bower uses Original')
        (pack/'conversion-report.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
        if out.exists():
            raise ValueError(f'Refusing to replace existing output: {out}')
        pack.rename(out)
    return report


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('--out', required=True, type=Path, help='new final pack/report directory')
    parser.add_argument('--stock', type=Path, help='private original F-Zero USA ROM')
    parser.add_argument('--exporter', type=Path)
    parser.add_argument('--inspector', type=Path)
    parser.add_argument('--patch-member', help='exact IPS/BPS member name in a ZIP with multiple patches')
    args = parser.parse_args(argv)
    try:
        report = convert(args.source, args.out, stock=args.stock, exporter=args.exporter,
                         inspector=args.inspector, patch_member=args.patch_member)
    except (ValueError, OSError, zipfile.BadZipFile, RuntimeError, subprocess.SubprocessError) as error:
        parser.exit(1, f'{error}\n')
    if report['status'] == 'review-required':
        print(f'Unreviewed revision: {args.out}/conversion-report.json; see CONVERSION.md')
        return 2
    print(f'{report["pack_id"]}: validated editable course pack in {args.out}')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
