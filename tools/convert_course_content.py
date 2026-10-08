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
import lzma
from pathlib import Path, PurePosixPath
import re
import shutil
import stat
import subprocess
import tempfile
import zipfile
import zlib

from audit_track_metadata import audit_metadata, GLYPHS, SONGS, span
from course_tool_paths import ROOT, native_tool
from export_runtime_packs import export_pack
from import_astra_front import SOURCE_SHA256 as ASTRA_SOURCE_SHA256
from inspect_bs_deluxe import STOCK_SHA256
from parse_track_pack import donor, fields, recognize
from reconstruct_fzedit_course import ALPHABET

IMAGE_LIMIT = 16 * 1024 * 1024
INPUT_LIMIT = 32 * 1024 * 1024
ZIP_ARCHIVE_LIMIT = 2 * 1024 * 1024 * 1024
ZIP_TOTAL_LIMIT = 2 * 1024 * 1024 * 1024
ZIP_FILE_LIMIT = 256 * 1024 * 1024
ZIP_MEMBER_LIMIT = 4096
ZIP_DIRECTORY_LIMIT = 8 * 1024 * 1024
ROM_SUFFIXES = ('.sfc', '.smc', '.rom', '.fig')
PATCH_SUFFIXES = ('.ips', '.bps')
AUDIO_SUFFIXES = ('.pcm', '.msu', '.wav', '.flac', '.mp3', '.ogg')
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


def hash_file(path, limit):
    result, count = hashlib.sha256(), 0
    with Path(path).open('rb') as stream:
        while chunk := stream.read(1024 * 1024):
            count += len(chunk)
            if count > limit:
                raise ValueError(f'Input exceeds {limit:,} bytes: {path}')
            result.update(chunk)
    return result.hexdigest(), count


def bounded_zip(source):
    """Bound directory allocation before ZipFile loads its member objects."""
    with Path(source).open('rb') as stream:
        stream.seek(0, 2)
        if stream.tell() > ZIP_ARCHIVE_LIMIT:
            raise ValueError('Compressed ZIP exceeds the 2 GiB input limit')
        # The same stdlib reader used by ZipFile scans at most the 64 KiB tail
        # plus fixed ZIP64 records. Its directory size is checked before open.
        end = zipfile._EndRecData(stream)
        if end is None:
            raise zipfile.BadZipFile('ZIP end-of-directory record is missing')
        if end[zipfile._ECD_SIZE] > ZIP_DIRECTORY_LIMIT:
            raise ValueError('ZIP central directory exceeds the 8 MiB metadata limit')
        if end[zipfile._ECD_ENTRIES_TOTAL] > ZIP_MEMBER_LIMIT:
            raise ValueError('ZIP contains too many members')
    return zipfile.ZipFile(source)


def zip_inventory(archive):
    """Validate metadata without inflating recordings or incidental tools."""
    entries = archive.infolist()
    if len(entries) > ZIP_MEMBER_LIMIT:
        raise ValueError('ZIP contains too many members')
    total, names, audio = 0, {}, []
    for entry in entries:
        raw = entry.orig_filename
        path = PurePosixPath(raw)
        parts = raw.rstrip('/').split('/')
        if (not raw or len(raw.encode('utf-8')) > 240 or '\x00' in raw or path.is_absolute() or '\\' in raw
                or ':' in raw or any(part in ('', '.', '..') for part in parts)
                or any(part.endswith((' ', '.')) for part in parts)
                or stat.S_ISLNK(entry.external_attr >> 16) or entry.external_attr & 0x400):
            raise ValueError('ZIP contains an unsafe member path or link')
        key = raw.rstrip('/').casefold()
        if key in names:
            raise ValueError('ZIP contains duplicate member names')
        names[key] = entry.is_dir()
        total += entry.file_size
        if entry.file_size > ZIP_FILE_LIMIT or total > ZIP_TOTAL_LIMIT:
            raise ValueError('ZIP expanded data exceeds the input limit')
        if entry.flag_bits & 1:
            raise ValueError('Encrypted ZIP members are unsupported')
        extension = path.suffix.lower()
        if not entry.is_dir() and extension in AUDIO_SUFFIXES:
            item = dict(member=entry.filename, bytes=entry.file_size,
                        format=extension[1:], zip_crc32=f'{entry.CRC:08x}', mapping='not-installed')
            if extension == '.pcm':
                header = b''
                try:
                    with archive.open(entry) as stream:
                        header = stream.read(8)
                except (zipfile.BadZipFile, OSError, RuntimeError, NotImplementedError,
                        EOFError, lzma.LZMAError, zlib.error) as error:
                    item['header_error'] = str(error)
                frames = (entry.file_size-8)//4 if entry.file_size >= 8 else 0
                loop = int.from_bytes(header[4:8], 'little')
                item.update(header_valid=(len(header) == 8 and header[:4] == b'MSU1'
                            and entry.file_size >= 8 and (entry.file_size-8) % 4 == 0
                            and (loop < frames or loop == 0)), frames=frames, loop_frame=loop)
            audio.append(item)
    for key in names:
        for parent in PurePosixPath(key).parents:
            if str(parent) in names and not names[str(parent)]:
                raise ValueError('ZIP contains a file/directory path collision')
    return entries, dict(members=len(entries), expanded_bytes=total), dict(
        members=audio, pcm_count=sum(item['format'] == 'pcm' for item in audio),
        msu_count=sum(item['format'] == 'msu' for item in audio),
        bytes=sum(item['bytes'] for item in audio), mapped_pcm_count=0,
        validation='PCM headers checked; full ZIP CRC checked only when a recording is copied',
        original_archive_preserved=True)


def read_zip_member(archive, entry, limit):
    if entry.file_size > limit:
        raise ValueError(f'ZIP donor exceeds {limit:,} bytes: {entry.filename}')
    with archive.open(entry) as stream:
        data = stream.read(limit + 1)
    if len(data) > limit or len(data) != entry.file_size:
        raise ValueError('ZIP donor exceeds the input limit')
    return data


def zip_patches(data, member=None):
    """Compatibility helper for extracted patch callers; disk ZIPs stream below."""
    with zipfile.ZipFile(io.BytesIO(data)) as archive:
        entries, _, _ = zip_inventory(archive)
        patches = [entry for entry in entries if not entry.is_dir()
                   and PurePosixPath(entry.filename).suffix.lower() in PATCH_SUFFIXES]
        if member is not None:
            patches = [entry for entry in patches if entry.filename == member]
        if not patches or (member is not None and len(patches) != 1) or len(patches) > 8:
            raise ValueError('Select exactly one IPS/BPS with --patch-member; '
                             'original FZEdit ZIPs use package_fzedit_course.py')
        return [(read_zip_member(archive, entry, INPUT_LIMIT), entry.filename) for entry in patches]


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


def decode_donor(data, extension, stock):
    if data.startswith((b'PATCH', b'BPS1')):
        if stock is None:
            raise ValueError('IPS/BPS conversion requires --stock with the original USA ROM')
        target = donor(stock, data)
        report = dict(input_kind='bps' if data.startswith(b'BPS1') else 'ips',
                      patch_sha256=digest(data), bps_crcs_verified=data.startswith(b'BPS1'))
    elif extension in ROM_SUFFIXES:
        target, header = strip_header(data)
        report = dict(input_kind='rom', removed_copier_header_bytes=header)
    else:
        raise ValueError('Expected IPS, BPS, a patch ZIP, or a hacked .sfc/.smc ROM')
    if not 0x8000 <= len(target) <= IMAGE_LIMIT:
        raise ValueError('Donor image must be between 32 KiB and 16 MiB')
    report.update(target_sha256=digest(target), target_bytes=len(target))
    return target, report


def read_zip_submission(source, stock, member):
    report = dict(input_name=source.name, warnings=[])
    with bounded_zip(source) as archive:
        entries, report['archive_inventory'], report['audio_inventory'] = zip_inventory(archive)
        candidates = [entry for entry in entries if not entry.is_dir()
                      and PurePosixPath(entry.filename).suffix.lower() in PATCH_SUFFIXES + ROM_SUFFIXES]
        report['ignored_members'] = [entry.filename for entry in entries if not entry.is_dir()
            and PurePosixPath(entry.filename).suffix.lower() not in PATCH_SUFFIXES + ROM_SUFFIXES + AUDIO_SUFFIXES]
        if member is not None:
            candidates = [entry for entry in candidates if entry.filename == member]
            if len(candidates) != 1:
                raise ValueError('--member must identify one exact IPS/BPS/ROM member')
        valid, errors = [], []
        if len(candidates) <= 8:
            for entry in candidates:
                extension = PurePosixPath(entry.filename).suffix.lower()
                limit = IMAGE_LIMIT + 512 if extension in ROM_SUFFIXES else INPUT_LIMIT
                try:
                    target, metadata = decode_donor(read_zip_member(archive, entry, limit), extension, stock)
                    profile = reviewed_profile(target)
                    valid.append(dict(member=entry.filename, profile=fields(profile)['id'][0] if profile else None,
                                      **metadata))
                except ValueError as error:
                    errors.append(dict(member=entry.filename, reason=str(error)))
        report['archive_donor_candidates'] = valid
        report['archive_candidate_errors'] = errors
        if not candidates:
            report['reason'] = ('No IPS/BPS or ROM donor was found. Ready packs and FZEdit projects '
                                'use the native importer; audio-only archives need an existing course pack.')
            selected = None
        elif len(candidates) > 8:
            report['reason'] = 'The archive has more than eight donor candidates; select an exact --member.'
            selected = None
        elif not valid:
            report['reason'] = 'No donor member could be applied or validated against the selected stock ROM.'
            selected = None
        elif len({entry['profile'] or entry['target_sha256'] for entry in valid}) != 1:
            report['reason'] = 'The archive contains different donor revisions; select an exact --member before conversion.'
            selected = None
        else:
            selected = min(valid, key=lambda item: (item['input_kind'] != 'bps', item['input_kind'] == 'rom', item['member']))
        if selected is not None:
            entry = archive.getinfo(selected['member'])
            extension = PurePosixPath(entry.filename).suffix.lower()
            limit = IMAGE_LIMIT + 512 if extension in ROM_SUFFIXES else INPUT_LIMIT
            target, metadata = decode_donor(read_zip_member(archive, entry, limit), extension, stock)
            report.update(metadata, donor_member=entry.filename)
            if extension in PATCH_SUFFIXES:
                report['patch_member'] = entry.filename
            if len(valid) > 1:
                report['archive_patch_targets'] = valid  # Existing report consumers.
            if errors:
                report['warnings'].append('Some donor members could not be validated; their reasons are listed in the report.')
        else:
            target = None
    report['input_sha256'], report['input_bytes'] = hash_file(source, ZIP_ARCHIVE_LIMIT)
    return target, report


def read_submission(source, stock=None, patch_member=None):
    source = Path(source)
    with source.open('rb') as stream:
        signature = stream.read(4)
    if source.suffix.lower() == '.zip' or signature == b'PK\x03\x04':
        target, report = read_zip_submission(source, stock, patch_member)
    else:
        if patch_member is not None:
            raise ValueError('--patch-member requires a ZIP submission')
        raw = read_bounded(source, INPUT_LIMIT)
        target, report = decode_donor(raw, source.suffix.lower(), stock)
        report.update(input_name=source.name, input_sha256=digest(raw), input_bytes=len(raw))
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


def probe_fzedit_metadata(target):
    """Decode recognized data consumers; this does not qualify other donor ASM."""
    result = dict(status='unrecognized-loader', executable_compatibility_verified=False,
                  cup_labels_verified=False, intro_font_verified=False)
    try:
        selector = span(target, 0x00f7e0, 17)
        order = span(target, 0x10824d, 21)
        if (selector[:9] != bytes.fromhex('08 e2 30 c2 10 ae 59 10 bf')
                or selector[12:] != bytes.fromhex('e2 10 ea ea ea')
                or order[:11] != bytes.fromhex('8a c2 30 29 ff 00 8d 5b 10 bb bf')
                or order[14:] != bytes.fromhex('29 ff 00 8d 59 10 20')):
            return result
        music_table = int.from_bytes(selector[9:12], 'little')
        order_table = int.from_bytes(order[11:14], 'little')
        result.update(status='recognized-metadata-locators', music_table=f'{music_table:06x}',
                      order_table=f'{order_table:06x}')
        positions = span(target, 0x10845b, 20)
        minimaps = span(target, 0x10846f, 21)
        names = span(target, 0x10839b, 26)
        if (positions[:7] != bytes.fromhex('ad 59 10 0a 0a aa bf')
                or positions[10:14] != bytes.fromhex('8d d9 0a bf')
                or positions[17:] != bytes.fromhex('8d db 0a')
                or minimaps[:10] != bytes.fromhex('ad 59 10 0a 6d 59 10 aa 8b bf')
                or minimaps[13:17] != bytes.fromhex('a8 e2 20 bf') or minimaps[-1] != 0x48
                or names[:12] != bytes.fromhex('da c2 30 ad 59 10 0a 6d 59 10 aa bf')
                or names[15:18] != bytes.fromhex('85 00 bf')
                or names[21:] != bytes.fromhex('85 01 e2 30 fa')):
            return result
        position_table = int.from_bytes(positions[7:10], 'little')
        minimap_table = int.from_bytes(minimaps[10:13], 'little')
        name_table = int.from_bytes(names[12:15], 'little')
        if (int.from_bytes(positions[14:17], 'little') != position_table+2
                or int.from_bytes(minimaps[17:20], 'little') != minimap_table+2
                or int.from_bytes(names[18:21], 'little') != name_table+1):
            return result
        distance = position_table-minimap_table
        if (position_table >> 16 != minimap_table >> 16 or distance % 3
                or not 1 <= distance//3 <= 128):
            return result
        count = distance//3
        span(target, minimap_table, count*3); span(target, position_table, count*4)
        source_order = list(span(target, order_table, count))
        if len(set(source_order)) != count or any(slot >= count for slot in source_order):
            result['order_note'] = 'Internal resource count does not establish the published GP subset.'
            return result
        music = span(target, music_table, count)
        standard = bytes.fromhex('a5 46 29 07 c9 06 d0 11 a5 53 a6 58 d0 08 '
                                 'a5 90 0a 0a 65 90 65 53 18 69 0a 60')
        msu = span(target, 0x02c26a, len(standard))
        msu_standard = msu[:24] == standard[:24] and msu[25:] == standard[25:]
        result['msu_selector'] = dict(recognized=msu_standard,
            qualification='Other callers and behavior still require review')
        if msu_standard:
            result['msu_selector'].update(base_track=msu[24], formula=f'{msu[24]} + 5 * cup + race')
        alphabet = dict(zip(ALPHABET, 'ABCDEFGHIJKLMNOPQRSTUVWXYZ'))
        alphabet.update(GLYPHS)
        alphabet.update({0x80+i: str(i) for i in range(10)})
        tracks = []
        for position, slot in enumerate(source_order):
            pointer = int.from_bytes(span(target, name_table+slot*3, 3), 'little')
            encoded = span(target, pointer, 128).split(b'\0', 1)[0]
            row = dict(slot=slot, order_table_position=position, spc_selector_byte=music[slot])
            if len(encoded) < 6 or encoded[1:6] != bytes.fromhex('53 01 82 1b ff'):
                row['name_error'] = 'Unrecognized intro-name encoding'
            else:
                try:
                    row['name'] = ''.join(alphabet[code] for code in encoded[6:]).strip()
                except KeyError as error:
                    row['name_error'] = f'Unreviewed intro glyph {error.args[0]:02x}'
            if music[slot] <= 81 and music[slot] % 9 == 0:
                row.update(spc_index=music[slot]//9, stock_spc_theme=SONGS[music[slot]//9])
            if msu_standard:
                row['msu_track'] = msu[24]+position
            tracks.append(row)
        result.update(status='recognized-resource-metadata', internal_resource_count=count,
            count_evidence='Recognized adjacent minimap/position tables, three-byte minimap stride',
            name_table=f'{name_table:06x}', source_order_prefix=source_order, tracks=tracks,
            limitations=['Published cup labels/count and font artwork need review.',
                         'SPC selector indices do not prove unchanged SPC sound data.',
                         'Course resources and other ASM/mechanics have not been qualified.'])
    except ValueError as error:
        result['probe_error'] = str(error)
    return result


def copy_archive_audio(source, report, pack):
    """Map recordings only through existing course/menu metadata, stream copies."""
    inventory = report.get('audio_inventory')
    if inventory is None:
        return
    index = json.loads((pack/'courses.json').read_text(encoding='utf-8'))
    donor_path = PurePosixPath(report['donor_member'])
    prefixes = {item['prefix'].casefold() for item in index.get('soundtracks', [])}
    for item in inventory['members']:
        path = PurePosixPath(item['member'])
        if item['format'] == 'msu' and path.parent == donor_path.parent and path.stem == donor_path.stem:
            prefixes.add(path.stem.casefold())
    recordings = [item for item in inventory['members'] if item['format'] == 'pcm' and item.get('header_valid')]
    plans, blocked = {}, set()
    def assign(destination, candidates):
        if destination in blocked:
            return
        previous = plans.get(destination)
        if len(candidates) > 1 or (candidates and previous is not None
                                  and previous['member'] != candidates[0]['member']):
            blocked.add(destination)
            plans.pop(destination, None)
            for item in candidates + ([previous] if previous is not None else []):
                item['mapping_reason'] = f'Multiple recordings match {destination}; choose one manually'
            return
        if candidates:
            plans[destination] = candidates[0]
    def numbered(item, number):
        match = re.fullmatch(r'(.+)-(\d+)\.pcm', PurePosixPath(item['member']).name, re.IGNORECASE)
        return match is not None and match[1].casefold() in prefixes and int(match[2]) == number
    for course in index['courses']:
        stem = PurePosixPath(course['source']).stem
        music = course.get('music', {})
        matches = [item for item in recordings if PurePosixPath(item['member']).name.casefold() == (stem+'.pcm').casefold()
                   or ('track' in music and numbered(item, music['track']))]
        assign(f'music/{stem}.pcm', matches)
    for destination in sorted(set(index.get('menu_music', {}).values())):
        path = PurePosixPath(destination)
        if (path.is_absolute() or '..' in path.parts or '\\' in destination or ':' in destination
                or not path.parts or path.parts[0] != 'music'):
            raise ValueError('Invalid reviewed menu music destination')
        match = re.fullmatch(r'.+-(\d+)\.pcm', path.name, re.IGNORECASE)
        candidates = [item for item in recordings if PurePosixPath(item['member']).name.casefold() == path.name.casefold()
                      or (match is not None and numbered(item, int(match[1])))]
        assign(destination, candidates)
    if sum(item['bytes'] for item in plans.values()) > ZIP_TOTAL_LIMIT:
        report['warnings'].append('Mapped recordings would exceed the 2 GiB publication limit; '
                                  'courses were imported without recordings. Place music manually.')
        plans.clear()
    if blocked:
        report['warnings'].append(f'{len(blocked)} course/menu music destinations had multiple matching recordings; '
                                  'those recordings were skipped. Choose one manually; see audio_inventory.')
    with bounded_zip(source) as archive:
        for destination, item in plans.items():
            target = pack/Path(destination); target.parent.mkdir(parents=True, exist_ok=True)
            partial = target.with_name('.'+target.name+'.part')
            checksum, count = hashlib.sha256(), 0
            created = False
            try:
                entry = archive.getinfo(item['member'])
                if entry.file_size != item['bytes'] or f'{entry.CRC:08x}' != item['zip_crc32']:
                    raise ValueError('Archive changed during conversion')
                if target.exists():
                    raise ValueError('Music destination already exists')
                with partial.open('xb') as output:
                    created = True
                    with archive.open(entry) as original:
                        while chunk := original.read(1024*1024):
                            count += len(chunk)
                            if count > ZIP_FILE_LIMIT or count > item['bytes']:
                                raise ValueError('Recording exceeds its declared limit')
                            output.write(chunk); checksum.update(chunk)
                if count != item['bytes']:
                    raise ValueError('Truncated recording')
                partial.rename(target)
            except (ValueError, KeyError, zipfile.BadZipFile, OSError, RuntimeError, NotImplementedError,
                    EOFError, lzma.LZMAError, zlib.error) as error:
                item['copy_error'] = str(error)
                report['warnings'].append(f'Recording {item["member"]} could not be installed: {error}. '
                                          'Courses are available; replace or place this recording manually.')
                continue
            finally:
                if created:
                    partial.unlink(missing_ok=True)
            item.update(mapping='installed', sha256=checksum.hexdigest())
            item.setdefault('destinations', []).append(destination)
    inventory['mapped_pcm_count'] = sum(item['mapping'] == 'installed' for item in recordings)
    inventory['unmapped_pcm_count'] = inventory['pcm_count']-inventory['mapped_pcm_count']
    inventory['copied_bytes'] = sum(item['bytes']*len(item.get('destinations', [])) for item in recordings)
    if inventory['unmapped_pcm_count']:
        amount = inventory['unmapped_pcm_count']
        report['warnings'].append(f'{amount} PCM recording'+(' was' if amount == 1 else 's were')+' not installed because '
            'they have no unambiguous reviewed course/menu mapping or a valid header. '
            'They remain in your unchanged ZIP. Place them in the installed pack\'s music folder manually; '
            'see audio_inventory for unresolved filenames and CONVERSION.md for naming.')
    unsupported = sum(item['format'] not in ('pcm', 'msu') for item in inventory['members'])
    if unsupported:
        report['warnings'].append(f'{unsupported} other audio files were not installed; this importer plays MSU PCM recordings. '
                                  'The original files remain in your unchanged ZIP.')


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
    profile = reviewed_profile(target) if target is not None else None
    report.update(format='fzero.content-conversion', version=1, source_rom_exported=False,
                  donor_code_executed=False)
    report.setdefault('warnings', [])
    if profile is None:
        if target is not None:
            probe = report['donor_probe'] = probe_fzedit_metadata(target)
            count = probe.get('internal_resource_count')
            recordings = report.get('audio_inventory', {}).get('pcm_count', 0)
            detected = f'Detected {count} internal course resources' if count else 'The donor needs a reviewed course layout'
            if recordings:
                detected += f' and {recordings} PCM recordings'
            report['reason'] = (detected+'. New course code, layout, cup labels and mechanics still need qualification. '
                                'No courses or recordings were installed; your original files are unchanged.')
        report.update(status='review-required', qualification_document='mods/PARSE_MANIFEST.md')
        if report.get('audio_inventory', {}).get('pcm_count'):
            report['warnings'].append('Recordings were inventoried, not assigned or installed. '
                                      'They remain in your unchanged ZIP; donor MSU behavior needs review.')
        if stock_data is not None and target is not None:
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
    # Native ZIP/source caches add long names beneath this directory. Keep them
    # outside the caller's nested installation stage to stay below MAX_PATH.
    with tempfile.TemporaryDirectory(prefix='fzc-') as temporary:
        temp = Path(temporary)
        pack = temp/'packs'/data['id'][0]
        baseline = temp/'native'; baseline.mkdir()
        extraction = export_pack(stock_data, target, profile.with_suffix(''), pack, exporter,
                                 KNOWN_IDS.index(data['id'][0]), native_resources=baseline)
        copy_archive_audio(source, report, pack)
        report['validation'] = inspect_roundtrip(inspector, temp, pack, baseline, extraction)
        if data['id'][0] == 'cgp':
            report['preserved_author_source'] = dict(course='huckmine',
                donor_record_hash=extraction['extracted_huckmine_hash'],
                selected_record_hash=extraction['record_hashes']['huckmine'],
                note='Original Huckmine FZEdit project retained, matching the shipped pack; differs from donor extraction')
        report['omissions'] = ['donor executable patches, vehicles and global game rules',
                               'original author editing layers/history for reconstructed courses']
        if not report.get('audio_inventory', {}).get('pcm_count'):
            report['omissions'].append('MSU PCM recordings (add separately)')
        report['title_policy'] = ('Reviewed presentation profile; Astra reuses the CGP title '
                                  'supplied by the CGP pack, Bower uses Original')
        (pack/'conversion-report.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
        # The short workspace may be on another volume. Copy a complete,
        # validated pack beside the destination, then publish by local rename.
        with tempfile.TemporaryDirectory(prefix='.fzc-publish-', dir=out.parent) as publication:
            staged = Path(publication)/'pack'
            shutil.copytree(pack, staged)
            if out.exists() or out.is_symlink():
                raise ValueError(f'Refusing to replace existing output: {out}')
            staged.rename(out)
    return report


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('--out', required=True, type=Path, help='new final pack/report directory')
    parser.add_argument('--stock', type=Path, help='private original F-Zero USA ROM')
    parser.add_argument('--exporter', type=Path)
    parser.add_argument('--inspector', type=Path)
    parser.add_argument('--patch-member', '--member', dest='patch_member',
                        help='exact IPS/BPS/ROM member name in an ambiguous ZIP')
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
