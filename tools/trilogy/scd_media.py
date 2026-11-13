"""Resolve an owned CD image/ZIP in private bounded staging, never beside inputs."""
from contextlib import contextmanager
from pathlib import Path, PurePosixPath
import os
import re
import stat
import struct
import tempfile
import zipfile

from .scd_iso import Disc

MAX_ARCHIVE = 1024 * 1024 * 1024
MAX_EXPANDED = 1536 * 1024 * 1024
MAX_ENTRIES = 512
MEDIA = {'.cue', '.bin', '.iso', '.img', '.sgcd', '.wav'}
FILE = re.compile(r'^(\s*FILE\s+)"([^"]+)"(\s+\S+\s*)$', re.I)


def member_path(name):
    name = name.replace('\\', '/')
    path = PurePosixPath(name)
    if (not name or name.startswith('/') or any(ord(c) < 32 for c in name)
            or any(p in ('', '.', '..') or any(c in p for c in '<>:"|?*')
                   or p.rstrip(' .') != p or p.split('.')[0].upper() in
                   {'CON', 'PRN', 'AUX', 'NUL', *(f'COM{i}' for i in range(1, 10)),
                    *(f'LPT{i}' for i in range(1, 10))} for p in name.rstrip('/').split('/'))):
        raise ValueError('CD ZIP contains an unsafe or nonportable member path')
    return path


def cue_files(path):
    if path.stat().st_size > 256 * 1024:
        raise ValueError('CUE sheet exceeds the metadata limit')
    lines = path.read_text(encoding='utf-8-sig').splitlines()
    return lines, [(i, FILE.fullmatch(line)) for i, line in enumerate(lines)
                   if line.lstrip().upper().startswith('FILE ')]


def matching_cue(path):
    matches = []
    cues = [p for p in path.parent.glob('*.[cC][uU][eE]') if p.is_file()]
    if len(cues) > 32:
        raise ValueError('Too many adjacent CUE sheets; select the disc CUE directly')
    for cue in cues:
        _, files = cue_files(cue)
        if any(m and (cue.parent / m[2].replace('\\', '/')).resolve() == path.resolve() for _, m in files):
            matches.append(cue)
    if len(matches) > 1:
        raise ValueError('Multiple CUE sheets reference this image; select its CUE directly')
    return matches[0] if matches else path


def data_image(path):
    with path.open('rb') as stream:
        header = stream.read(16)
        if header[:12] == b'\0' + b'\xff' * 10 + b'\0' and header[15:16] == b'\1':
            return True
        stream.seek(16 * 2048 + 1)
        return stream.read(5) == b'CD001'


def extract_disc(archive_path, root):
    size = archive_path.stat().st_size
    if size > MAX_ARCHIVE:
        raise ValueError('CD ZIP exceeds the archive-size limit')
    # Bound central-directory parsing before ZipFile allocates its member list.
    # A supported disc fits below 2 GiB, so ZIP64/multipart containers are not
    # necessary. Ordinary ZIP entries may still use ZIP64 local headers.
    with archive_path.open('rb') as stream:
        stream.seek(max(0, size - 65557));tail = stream.read(65557)
    offset = tail.rfind(b'PK\5\6')
    if offset < 0 or len(tail) - offset < 22:
        raise ValueError('CD ZIP has no valid end-of-directory record')
    _, disk, directory_disk, local_count, count, directory_size, directory_offset, comment = struct.unpack_from('<4s4H2IH', tail, offset)
    if (disk or directory_disk or local_count != count or count > MAX_ENTRIES
            or directory_size > 8 * 1024 * 1024 or directory_offset + directory_size > size
            or len(tail) - offset != 22 + comment):
        raise ValueError('CD ZIP is multipart, ZIP64 or exceeds its directory limits')
    with zipfile.ZipFile(archive_path) as archive:
        entries = archive.infolist()
        if archive_path.stat().st_size > MAX_ARCHIVE or len(entries) > MAX_ENTRIES:
            raise ValueError('CD ZIP exceeds the archive size/member limit')
        names, selected, expanded = {}, [], 0
        for entry in entries:
            relative = member_path(entry.filename)
            key = str(relative).casefold()
            mode = entry.external_attr >> 16
            if (key in names or entry.flag_bits & 1 or stat.S_ISLNK(mode)
                    or stat.S_IFMT(mode) not in (0, stat.S_IFREG, stat.S_IFDIR)):
                raise ValueError('CD ZIP has duplicate, encrypted or non-regular members')
            names[key] = relative
            if entry.is_dir():
                continue
            expanded += entry.file_size
            if entry.file_size < 0 or expanded > MAX_EXPANDED:
                raise ValueError('CD ZIP exceeds the expanded-size limit')
            if relative.suffix.lower() in MEDIA:
                selected.append((entry, relative))
        paths = {}
        for entry, relative in selected:
            target = root / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            remaining = entry.file_size
            with archive.open(entry) as source, target.open('xb') as output:
                while remaining:
                    chunk = source.read(min(1024 * 1024, remaining))
                    if not chunk:
                        raise ValueError('Truncated CD ZIP member')
                    output.write(chunk)
                    remaining -= len(chunk)
                if source.read(1):
                    raise ValueError('CD ZIP member exceeds its declared size')
            paths[str(relative).casefold()] = target
    cues = [p for p in paths.values() if p.suffix.lower() == '.cue']
    if len(cues) > 1:
        raise ValueError('CD ZIP contains multiple CUE sheets; use a single-disc archive')
    if cues:
        cue = cues[0]
        lines, files = cue_files(cue)
        for i, match in files:
            if not match:
                raise ValueError('Unsupported CUE FILE declaration')
            referenced = match[2].replace('\\', '/')
            if referenced.startswith('/') or ':' in referenced:
                raise ValueError('Archive CUE references media outside the archive')
            target = (cue.parent / referenced).resolve()
            if not target.is_relative_to(root):
                raise ValueError('Archive CUE references media outside the archive')
            key = target.relative_to(root).as_posix().casefold()
            if key not in paths:
                raise ValueError('CD ZIP is missing a CUE-referenced track: ' + referenced)
            relative = os.path.relpath(paths[key], cue.parent).replace('\\', '/')
            lines[i] = f'{match[1]}"{relative}"{match[3]}'
        # Only this private extracted copy is normalized, including case and
        # Windows separators, so the same archive works on Windows and Linux.
        cue.write_text('\n'.join(lines) + '\n', encoding='utf-8')
        return cue
    for suffixes in ({'.sgcd'}, {'.iso'}, {'.bin', '.img'}):
        candidates = [p for p in paths.values() if p.suffix.lower() in suffixes
                      and (p.suffix.lower() == '.sgcd' or data_image(p))]
        if len(candidates) > 1:
            raise ValueError('CD ZIP contains multiple data images; use a single-disc archive')
        if candidates:
            return candidates[0]
    raise ValueError('CD ZIP contains no supported CUE, SGCD, ISO or MODE1 data image')


@contextmanager
def open_disc(path, scratch=None):
    original = Path(path).resolve(strict=True)
    if original.suffix.lower() == '.zip':
        with tempfile.TemporaryDirectory(prefix='.media-', dir=scratch) as temporary:
            root = Path(temporary).resolve()
            image = extract_disc(original, root)
            disc = Disc(image)
            if not all(p.is_relative_to(root) for p in disc.input_paths):
                raise ValueError('Archive CUE references media outside private staging')
            disc.source_path = original
            disc.input_paths.add(original)
            yield disc
    else:
        image = matching_cue(original) if original.suffix.lower() in ('.iso', '.bin', '.img') else original
        disc = Disc(image)
        disc.source_path = original
        disc.input_paths.add(original)
        yield disc
