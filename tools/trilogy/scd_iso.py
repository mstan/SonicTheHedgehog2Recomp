"""S3K content adapter over the shared upstream Sega CD media/ISO readers.

Accepts the same owned CUE/BIN or SGCD image as SonicCDRecomp. The original
S3K content identity is the ISO user-data hash, independent of pregaps and
container headers; stage conversion and audio qualification remain game-owned.
"""
from dataclasses import dataclass
import os
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
_engine = Path(os.environ['GENESIS_RECOMP_ROOT']) if os.environ.get('GENESIS_RECOMP_ROOT') else None
_candidates = ([ROOT / 'tools' / 'segacd']
               + ([_engine / 'tools' / 'segacd'] if _engine else [])
               + [ROOT / 'engine-local' / 'tools' / 'segacd',
                  ROOT / 'segagenesisrecomp' / 'tools' / 'segacd'])
_shared = next((p for p in _candidates if (p / 'owned_media.py').is_file()), None)
if _shared is None:
    raise ImportError('shared Sega CD media tools are missing; initialize the engine submodule or use the complete release tools')
sys.path.insert(0, str(_shared))
from owned_media import OwnedMedia
from sgcd_owned_extract import ISO9660

RAW, USER = 2352, 2048
MAX_READ = 128 * 1024 * 1024


@dataclass(frozen=True)
class Track:
    number: int
    kind: str
    path: Path
    start: int
    end: int
    pregap: int = 0

    @property
    def size(self):
        return self.end - self.start


class Disc:
    def __init__(self, source):
        self.cue = Path(source)
        self.media = OwnedMedia(source)
        rows = self.media.tracks
        if not rows[0]['data'] or any(t['data'] for t in rows[1:]):
            raise ValueError('expected one MODE1/2352 data track followed by AUDIO tracks')
        self.iso = ISO9660(self.media)
        self.input_paths = self.media.input_paths
        self.track_info = [Track(t['number'], f'MODE1/{t.get("sector_size", RAW)}' if t['data'] else 'AUDIO',
                                 self.media.path, t['start'] * t.get('sector_size', RAW),
                                 t['end'] * t.get('sector_size', RAW),
                                 (t['start'] - t['index00']) * t.get('sector_size', RAW)) for t in rows]
        self.tracks = [(t.number, t.kind, t.path, t.start) for t in self.track_info]
        self._sectors = self.iso.volume_sectors
        self.volume_id = self.iso.volume_id
        self.files = self.iso.files

    def sector(self, lba, count=1):
        if (not isinstance(lba, int) or not isinstance(count, int) or lba < 0 or count < 0
                or lba > self._sectors or count > self._sectors - lba or count > MAX_READ // USER):
            raise ValueError('sector read outside data track/volume or allocation limit')
        return self.iso.read_sectors(lba, count)

    def read(self, name):
        return self.iso.read_file(name)

    def system_area(self):
        return self.sector(0, 16)

    def audio(self, number):
        track = next((t for t in self.media.tracks if t['number'] == number and not t['data']), None)
        if track is None:
            raise KeyError(number)
        count = track['end'] - track['start']
        if count * RAW > MAX_READ:
            raise ValueError('audio track exceeds allocation limit')
        return b''.join(self.media.read_raw_sectors(n, min(256, track['end'] - n), track)
                        for n in range(track['start'], track['end'], 256))


if __name__ == '__main__':
    import argparse
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('disc')
    args = ap.parse_args()
    disc = Disc(args.disc)
    for name, (lba, size) in sorted(disc.files.items(), key=lambda kv: kv[1][0]):
        print(f'{name:24} lba {lba:6} {size:8}')
