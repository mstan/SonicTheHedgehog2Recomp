"""Verify an owned sound donor and retain one canonical private ROM per identity."""
import hashlib
import os
from pathlib import Path
import tempfile
import zipfile
import struct

# Full-file identities, never names or cartridge headers, select the decoder.
DONORS = {
    's1': (0x80000, '46160baa06362c711c9f1a5017cb7371026444936c8af5e93a78996cf32ff2a6'),
    's2': (0x100000, '193bc4064ce0daf27ea9e908ed246d87ec576cc294833badebb590b6ad8e8f6b'),
    's2-aug': (0x100000, 'cd1f37fb244f200531b5c93378115fcf2f665fa78ba41548a10bff0ee64f0473'),
    's2-wai': (0x100000, 'c2e63635703e4dac7b0a2bb383abe5a001c128ab8ab98d33519c31d75b59cc8d'),
    's2-sep14': (0x100000, 'e84e1342c1caf6d54fa69f3462763007c83974d4b3ae4f6674349556675a701c'),
    's3-proto': (0x200000, 'ace6af3187c1c2da4e71a1c24fb81f2529cb25057f093f5cb5389842a90d4abf'),
    's3': (0x200000, 'd6404843c5ba32486f4c8c744acb7e1932069376960d886b1de37969787d3f9c'),
    'sk': (0x200000, '6e12e6b33c26ebfcd0be433251d21cf6284eafe9f71b027bda3767ae59affec1'),
}

def identify(data):
    for key, (size, digest) in DONORS.items():
        raw = data[512:] if len(data) == size + 512 else data
        if len(raw) == size and hashlib.sha256(raw).hexdigest() == digest:
            return key, raw
    raise ValueError('Unsupported sound donor. Choose an unmodified supported retail or prototype ROM.')

def read_donor(source):
    source = Path(source)
    if source.suffix.lower() != '.zip':
        with source.open('rb') as stream:
            return identify(stream.read(0x200000 + 513))
    if source.stat().st_size > 64*1024*1024:
        raise ValueError('ROM ZIP exceeds the 64 MiB limit')
    with source.open('rb') as stream:
        size=source.stat().st_size;stream.seek(max(0,size-65557));tail=stream.read(65557)
    offset=tail.rfind(b'PK\x05\x06')
    if offset<0 or len(tail)-offset<22:
        raise ValueError('ROM ZIP has no valid directory')
    _,disk,central_disk,local_count,count,directory_size,directory_offset,comment=struct.unpack_from('<4s4H2IH',tail,offset)
    if (disk or central_disk or local_count!=count or count>256 or directory_size>8*1024*1024
            or directory_offset+directory_size>size or len(tail)-offset!=22+comment):
        raise ValueError('ROM ZIP exceeds its directory limits or is multipart/ZIP64')
    found = None
    with zipfile.ZipFile(source) as archive:
        entries = archive.infolist()
        if len(entries) > 256:
            raise ValueError('ROM ZIP contains too many entries')
        for entry in entries:
            if entry.is_dir() or entry.file_size not in (0x80000,0x80200,0x100000,0x100200,0x200000,0x200200):
                continue
            # Member names never become filesystem paths.
            with archive.open(entry) as stream:
                data = stream.read(0x200000 + 513)
            try:
                candidate = identify(data)
            except ValueError:
                continue
            if found is not None and candidate != found:
                raise ValueError('ROM ZIP contains more than one supported donor; select one ROM.')
            found = candidate
    if found is None:
        raise ValueError('ZIP contains no supported sound donor ROM')
    return found

def atomic_write(path, data):
    fd, temporary = tempfile.mkstemp(prefix='.sound-', dir=path.parent)
    try:
        with os.fdopen(fd, 'wb') as stream:
            stream.write(data)
        os.replace(temporary, path)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)

def import_donor(source, output, receipt=None, expected=None):
    if receipt is not None:
        receipt=Path(receipt).resolve()
        if receipt.suffix.lower()!='.ini' or receipt==Path(source).resolve():
            raise ValueError('Receipt must be separate INI metadata')
    if expected and all(key.startswith('cd-') for key in expected):
        from import_cd_sound_donor import import_cd_donor
        return import_cd_donor(source,output,receipt,expected)
    key, raw = read_donor(source)
    if expected is not None and key not in expected:
        raise ValueError('This ROM does not match the selected theme. Expected: ' + ', '.join(expected))
    output = Path(output).resolve()
    output.mkdir(parents=True, exist_ok=True)
    target = output / (key + '.bin')
    if target.resolve() == Path(source).resolve():
        raise ValueError('Import cache must be separate from the source ROM')
    if target.is_symlink():
        raise ValueError('Sound donor cache must not contain symbolic links')
    atomic_write(target, raw)
    if receipt is not None:
        receipt = Path(receipt).resolve()
        if receipt.suffix.lower() != '.ini' or receipt == Path(source).resolve():
            raise ValueError('Receipt must be separate INI metadata')
        if any(c in str(target) for c in '\r\n'):
            raise ValueError('Newline in sound donor path')
        atomic_write(receipt, f'format=1\ncontent={target}\n'.encode())
    return key, target


def main():
    import argparse
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--campaign',choices=['sound'])
    parser.add_argument('--source',required=True)
    parser.add_argument('--expected',required=True)
    parser.add_argument('--out',required=True)
    parser.add_argument('--receipt',required=True)
    args=parser.parse_args()
    try:
        print('S3K_CD_PROGRESS:0.1:Verifying your source',flush=True)
        key,path=import_donor(args.source,args.out,args.receipt,args.expected.split(','))
        print('S3K_CD_PROGRESS:1:Imported '+key,flush=True)
    except (OSError,ValueError,zipfile.BadZipFile) as error:
        print('S3K_CD_ERROR:'+str(error),flush=True)
        return 1
    return 0

if __name__=='__main__':
    raise SystemExit(main())
