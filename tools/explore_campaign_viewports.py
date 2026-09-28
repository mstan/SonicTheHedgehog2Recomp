#!/usr/bin/env python3
"""Read-only sizing experiment for independent Sonic 2 campaign views.

Reads a private REV01 ROM and a previously captured 64 KiB gameplay RAM image.
Reports static placement candidates for separate view windows versus one large
window joining them. Does not run the game, open sockets, or change ROM/RAM.
This is a horizontal loading estimate, not a renderer or a performance test.
"""
import argparse
import json
from pathlib import Path
import struct
import zlib


def merge_intervals(intervals):
    merged = []
    for lo, hi in sorted(intervals):
        if merged and lo <= merged[-1][1]:
            merged[-1][1] = max(merged[-1][1], hi)
        else:
            merged.append([lo, hi])
    return merged


def inspect(rom, ram, centers, widths):
    if len(rom) != 0x100000 or zlib.crc32(rom) != 0x7B905383:
        raise ValueError('Expected the Sonic 2 REV01 ROM used by sonic2_spec.c')
    if len(ram) != 65536 or ram[0xF600] != 0x0C:
        raise ValueError('Expected a 64 KiB RAM capture from campaign gameplay')
    if struct.unpack_from('>H', ram, 0xFFD8)[0]:
        raise ValueError('Capture is native 2P competition, not campaign')
    zone, act = ram[0xFE10:0xFE12]
    if act > 1:
        raise ValueError('This experiment supports ordinary two-act placement tables')

    # Same layout width and placement index as sonic2_video.c. The RAM image
    # supplies decompressed level layout; the ROM supplies static placements.
    stage_width = max((col + 1 for row in range(16) for col in range(128)
                       if ram[0x8000 + row * 256 + col]), default=1) * 128
    table = 0xE6800
    offset = struct.unpack_from('>h', rom, table + zone * 4 + act * 2)[0]
    base = table + offset
    xs = []
    for index in range(1024):
        at = base + index * 6
        if at < 0 or at + 6 > len(rom):
            raise ValueError('Placement table exceeds ROM')
        x = struct.unpack_from('>H', rom, at)[0]
        if x == 0xFFFF:
            break
        xs.append(x)
    else:
        raise ValueError('Placement table has no terminator within the current loader limit')

    views = []
    for seat, (center, width) in enumerate(zip(centers, widths)):
        if not 0 <= center < stage_width or not 320 <= width <= stage_width:
            raise ValueError('Centers must be inside this stage; widths must be 320..stage width')
        # A planning camera centered on each player, clamped to level edges.
        # This is not Sonic's native dead-zone/vertical camera algorithm.
        left = max(0, min(stage_width - width, center - width // 2))
        lo = (left - 128) & ~127
        hi = ((left + width + 192) & ~127) + 128
        views.append(dict(seat=seat, planned_center_x=center, width=width,
                          view=[left, left + width], activation=[lo, hi]))
    intervals = merge_intervals(v['activation'] for v in views)
    enclosing = [[intervals[0][0], intervals[-1][1]]]

    def count(ranges):
        return sum(any(lo <= x < hi for lo, hi in ranges) for x in xs)

    return dict(zone_id=zone, act=act + 1, stage_width=stage_width,
                total_static_placements=len(xs), views=views,
                current_p1_window_candidates=count([views[0]['activation']]),
                union_intervals=intervals,
                union_width=sum(hi - lo for lo, hi in intervals),
                union_candidates=count(intervals),
                enclosing_width=enclosing[0][1] - enclosing[0][0],
                enclosing_candidates=count(enclosing),
                native_dynamic_slots=(0xD000 - 0xB400) // 0x40,
                extra_actor_slots=max(0, len(centers) - 2),
                caveats=['Hypothetical camera positions in a captured level; no game execution.',
                         'Counts ignore consumed objects, dynamic children/projectiles, and vertical filtering.',
                         'Static candidates are not measured live occupancy, frame cost, or proof of playability.',
                         'More than four views here is a sizing study; the Genesis adapter currently supports four.'])


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--rom', type=Path, required=True)
    ap.add_argument('--ram', type=Path, required=True)
    ap.add_argument('--centers', type=int, nargs='+', required=True)
    ap.add_argument('--widths', type=int, nargs='+', default=[320])
    ap.add_argument('--out', type=Path, help='Optional JSON report; inputs are never modified')
    args = ap.parse_args()
    widths = args.widths * len(args.centers) if len(args.widths) == 1 else args.widths
    if len(widths) != len(args.centers):
        ap.error('Supply one width or one width per center')
    if args.out and args.out.resolve() in (args.rom.resolve(), args.ram.resolve()):
        ap.error('Output must not overwrite an input')
    try:
        report = inspect(args.rom.read_bytes(), args.ram.read_bytes(), args.centers, widths)
    except (OSError, ValueError, struct.error) as error:
        ap.error(str(error))
    text = json.dumps(report, indent=2) + '\n'
    if args.out:
        args.out.write_text(text, encoding='utf-8')
    print(text, end='')


if __name__ == '__main__':
    main()
