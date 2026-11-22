"""Normalize timed Genesis DAC streams to ordinary VGM register writes.

VGM 1.71: https://github.com/vgmrips/vgmplay-legacy/blob/master/VGMPlay/vgmspec171.txt
Only one NTSC YM2612 and Genesis PSG are supported. Unknown chip commands,
non-Genesis clocks and compressed PCM blocks fail before changing a selection.
"""

import argparse
from fractions import Fraction
import gzip
import io
from pathlib import Path
import re
import struct
import zlib

from import_sound_donor import atomic_write

LIMIT = 64 * 1024 * 1024
MAX_LOOP_STATES = 1024


def convert(data):
    if data[:2] == b"\x1f\x8b":
        try:
            with gzip.GzipFile(fileobj=io.BytesIO(data)) as stream:
                data = stream.read(LIMIT + 1)
        except (EOFError, OSError, zlib.error) as error:
            raise ValueError("The VGZ file is incomplete or corrupt") from error
    if not 64 <= len(data) <= LIMIT or data[:4] != b"Vgm ":
        raise ValueError("Select a Genesis VGM or VGZ file")

    def u32(at):
        return struct.unpack_from("<I", data, at)[0]

    version = u32(8)
    if not 0x100 <= version <= 0x171:
        raise ValueError("Unsupported VGM version")
    end = u32(4) + 4
    if not 64 <= end <= len(data):
        raise ValueError("Truncated VGM file")
    start = 0x34 + u32(0x34) if version >= 0x150 and u32(0x34) else 0x40
    if not 64 <= start < end:
        raise ValueError("Invalid VGM data offset")
    fm = u32(0x2C) if version >= 0x110 else u32(0x10)
    psg = u32(0x0C)
    if fm and abs(fm - 7670454) > 8 or psg and abs(psg - 3579545) > 8:
        raise ValueError("Use one NTSC Genesis YM2612 and PSG")
    if not (fm or psg):
        raise ValueError("No Genesis sound chips in this VGM")
    clocks = (
        0x30,
        0x38,
        0x40,
        0x44,
        0x48,
        0x4C,
        0x50,
        0x54,
        0x58,
        0x5C,
        0x60,
        0x64,
        0x68,
        0x6C,
        0x70,
        0x74,
        0x80,
        0x84,
        0x88,
        0x8C,
        0x90,
        0x98,
        0x9C,
        0xA0,
        0xA4,
        0xA8,
        0xAC,
        0xB0,
        0xB4,
        0xB8,
        0xC0,
        0xC4,
        0xC8,
        0xCC,
        0xD0,
        0xD8,
        0xDC,
        0xE0,
    )
    if any(at + 4 <= start and u32(at) for at in clocks) or (
        version >= 0x110 and u32(0x10)
    ):
        raise ValueError("This VGM requires sound chips beyond the Genesis")
    if (
        version >= 0x110
        and psg
        and (
            struct.unpack_from("<H", data, 0x28)[0] not in (0, 9)
            or data[0x2A] not in (0, 16)
            or data[0x2B]
        )
    ):
        raise ValueError("Unsupported PSG variant")
    loop = 0x1C + u32(0x1C) if u32(0x1C) else 0
    if loop and not start <= loop < end:
        raise ValueError("Invalid VGM loop offset")
    events = []
    banks = {}
    blocks = {}
    streams = {}
    time = Fraction(0)
    pc = start
    seek = 0
    loop_event = 0xFFFFFFFF
    loop_time = 0
    loop_entries = {}
    loop_prefixes = {}
    loop_sample = None
    checking_prefix = replaying = False

    def loop_state():
        state = []
        for ident, stream in sorted(streams.items()):
            setup = tuple(
                stream.get(field)
                for field in ("configured", "bank", "step", "base", "frequency")
            )
            playback = ()
            if stream.get("active"):
                playback = (
                    stream["position"],
                    stream["left"],
                    stream["reverse"],
                    stream["loop"],
                    stream["next"] - time,
                )
                if stream["loop"]:
                    playback += (stream["first"], stream["count"])
            state.append((ident, setup, playback))
        # Blocks are append-only on the first pass and are skipped on replay.
        bank_layout = tuple(
            (kind, len(bank), len(blocks[kind])) for kind, bank in sorted(banks.items())
        )
        return seek, bank_layout, tuple(state)

    def emit(at, kind, a, b=0):
        events.append((int(at), kind, a, b))
        if len(events) > 2000000:
            raise ValueError("VGM has too many register writes")

    def take(n):
        nonlocal pc
        if n > end - pc:
            raise ValueError("Truncated VGM command")
        raw = data[pc : pc + n]
        pc += n
        return raw

    def advance(n):
        nonlocal time
        target = time + n
        while True:
            ready = [
                (s["next"], i, s)
                for i, s in streams.items()
                if s.get("active") and s["next"] < target
            ]
            if not ready:
                break
            at, i, s = min(ready, key=lambda v: (v[0], v[1]))
            bank = banks.get(s["bank"], b"")
            pos = s["position"]
            if not 0 <= pos < len(bank):
                raise ValueError("VGM DAC stream exceeds its data bank")
            emit(at, 1, 0x2A, bank[pos])
            s["left"] -= 1
            if not s["left"]:
                if s["loop"]:
                    s["position"] = s["first"]
                    s["left"] = s["count"]
                else:
                    s["active"] = False
            else:
                s["position"] += s["step"] * (-1 if s["reverse"] else 1)
            s["next"] += Fraction(44100, s["frequency"])
        time = target
        if time > 44100 * 1800:
            raise ValueError("VGM music must be at most 30 minutes per loop")

    def begin(s, offset, count, reverse=False, repeat=False):
        if (
            not s.get("configured")
            or not s.get("step")
            or not 1 <= s.get("frequency", 0) <= 192000
        ):
            raise ValueError("Incomplete VGM DAC stream setup")
        bank = banks.get(s.get("bank"), b"")
        first = offset + s["base"]
        if reverse:
            first += (count - 1) * s["step"]
        last = first + ((1 - count) if reverse else (count - 1)) * s["step"]
        if count <= 0 or min(first, last) < 0 or max(first, last) >= len(bank):
            raise ValueError("Invalid VGM DAC stream extent")
        s.update(
            active=True,
            position=first,
            first=first,
            count=count,
            left=count,
            next=time,
            reverse=reverse,
            loop=repeat,
        )

    while pc < end:
        if pc == loop:
            state = loop_state()
            if state in loop_entries:
                loop_event, loop_time = loop_entries[state]
                break
            if len(loop_entries) >= MAX_LOOP_STATES:
                raise ValueError(
                    "VGM loop playback state does not repeat within importer limits"
                )
            loop_sample = time
            loop_event, loop_time = len(events), int(time)
            loop_entries[state] = (loop_event, loop_time)
            checking_prefix = True
        command_pc = pc
        op = take(1)[0]
        wait = 0
        if op == 0:
            continue
        if op == 0x4F:
            emit(time, 3, take(1)[0])
        elif op == 0x50:
            if not psg:
                raise ValueError("VGM writes an undeclared PSG")
            emit(time, 0, take(1)[0])
        elif op in (0x52, 0x53):
            if not fm:
                raise ValueError("VGM writes an undeclared YM2612")
            a, b = take(2)
            emit(time, op - 0x51, a, b)
            if op == 0x52 and a == 0x27 and b & 0xC0:
                raise ValueError("Timer-driven CSM synthesis is not supported")
        elif op == 0x61:
            wait = int.from_bytes(take(2), "little")
        elif op in (0x62, 0x63):
            wait = 735 if op == 0x62 else 882
        elif 0x70 <= op <= 0x7F:
            wait = (op & 15) + 1
        elif op == 0x67:
            marker, kind = take(2)
            size = int.from_bytes(take(4), "little")
            if marker != 0x66 or kind != 0:
                raise ValueError("Use uncompressed YM2612 PCM data blocks")
            raw = take(size)
            if not replaying:
                bank = banks.setdefault(kind, bytearray())
                blocks.setdefault(kind, []).append((len(bank), size))
                bank.extend(raw)
        elif op == 0xE0:
            seek = int.from_bytes(take(4), "little")
        elif 0x80 <= op <= 0x8F:
            bank = banks.get(0, b"")
            if seek >= len(bank) or not fm:
                raise ValueError("VGM DAC seek exceeds its data bank")
            emit(time, 1, 0x2A, bank[seek])
            seek += 1
            wait = op & 15
        elif 0x90 <= op <= 0x95:
            length = {0x90: 4, 0x91: 4, 0x92: 5, 0x93: 10, 0x94: 1, 0x95: 4}[op]
            raw = take(length)
            ident = raw[0]
            if ident == 255:
                if op == 0x94:
                    for s in streams.values():
                        s["active"] = False
                continue
            s = streams.setdefault(ident, {})
            if op == 0x90:
                if tuple(raw[1:]) != (2, 0, 0x2A) or not fm:
                    raise ValueError("Only Genesis DAC streams are supported")
                s.update(configured=True, active=False)
            elif op == 0x91:
                if s.get("active"):
                    raise ValueError(
                        "Stop a DAC stream before changing its data layout"
                    )
                if raw[1] != 0 or not raw[2]:
                    raise ValueError("Invalid Genesis DAC stream bank or step")
                s.update(bank=raw[1], step=raw[2], base=raw[3])
            elif op == 0x92:
                frequency = int.from_bytes(raw[1:], "little")
                if not 1 <= frequency <= 192000:
                    raise ValueError("Invalid VGM DAC stream frequency")
                s["frequency"] = frequency
            elif op == 0x94:
                s["active"] = False
            elif op == 0x95:
                block = int.from_bytes(raw[1:3], "little")
                flags = raw[3]
                if flags & ~0x11 or block >= len(blocks.get(s.get("bank"), [])):
                    raise ValueError("Invalid VGM DAC block")
                offset, size = blocks[s["bank"]][block]
                count = (size - s.get("base", 0) + s.get("step", 1) - 1) // s.get(
                    "step", 1
                )
                begin(s, offset, count, bool(flags & 16), bool(flags & 1))
            else:
                offset = int.from_bytes(raw[1:5], "little")
                mode = raw[5]
                size = int.from_bytes(raw[6:], "little")
                length = mode & 15
                if mode & 0x60 or length not in (1, 2, 3) or offset == 0xFFFFFFFF:
                    raise ValueError("Unsupported VGM DAC length mode")
                if length == 2:
                    size = size * s.get("frequency", 0) // 1000
                elif length == 3:
                    size = (
                        len(banks.get(s.get("bank"), b""))
                        - offset
                        - s.get("base", 0)
                        + s.get("step", 1)
                        - 1
                    ) // s.get("step", 1)
                begin(s, offset, size, bool(mode & 16), bool(mode & 128))
        elif op == 0x66:
            if not loop:
                break
            if loop_sample is None or time == loop_sample:
                raise ValueError("VGM loop must reach a timed command boundary")
            replaying = True
            pc = loop
        else:
            raise ValueError(f"Unsupported Genesis VGM command ${op:02X}")
        if not wait:
            continue
        if checking_prefix:
            # Starts/stops before the first wait can replace a carried DAC
            # stream without emitting any of its samples. Reuse a pass when
            # both its zero-time writes and its resulting state match.
            checking_prefix = False
            prefixes = loop_prefixes.setdefault((command_pc, loop_state()), [])
            count = len(events) - loop_event
            matching = next(
                (
                    entry
                    for entry in prefixes
                    if entry[2] == count
                    and all(
                        events[entry[0] + i][1:] == events[loop_event + i][1:]
                        for i in range(count)
                    )
                ),
                None,
            )
            if matching is not None:
                del events[loop_event:]
                loop_event, loop_time, _ = matching
                break
            prefixes.append((loop_event, loop_time, count))
        advance(wait)
    else:
        raise ValueError("VGM has no end command")
    total = int(time)
    if not events or not total:
        raise ValueError("VGM has no timed music")
    if loop and (loop_event >= len(events) or loop_time >= total):
        raise ValueError("VGM loop must reach a timed command boundary")

    # VGM waits count 44.1 kHz samples. Keep the original order for writes that
    # fall within one sample and point the loop to its original sample time.
    output = bytearray(0x80)
    output[: min(start, 0x80)] = data[: min(start, 0x80)]
    position = 0
    loop_offset = 0

    def wait_until(target):
        nonlocal position
        while position < target:
            delay = min(65535, target - position)
            if delay <= 16:
                output.append(0x70 + delay - 1)
            else:
                output.extend((0x61, delay & 255, delay >> 8))
            position += delay

    for index, (sample, kind, reg, value) in enumerate(events):
        if index == loop_event:
            wait_until(loop_time)
            loop_offset = len(output) - 0x1C
        wait_until(sample)
        if kind == 0:
            output.extend((0x50, reg))
        elif kind == 3:
            output.extend((0x4F, reg))
        else:
            output.extend((kind + 0x51, reg, value))
    wait_until(total)
    output.append(0x66)
    for offset, value in (
        (4, len(output) - 4),
        (8, max(version, 0x150)),
        (0x10, 0),
        (0x14, 0),
        (0x18, total),
        (0x1C, loop_offset),
        (0x20, total - loop_time if loop else 0),
        (0x2C, fm),
        (0x34, 0x80 - 0x34),
    ):
        struct.pack_into("<I", output, offset, value)
    return output


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--campaign", choices=["custom-vgm"])
    parser.add_argument("--source", required=True, type=Path)
    parser.add_argument("--expected", required=True)
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--receipt", required=True, type=Path)
    args = parser.parse_args()
    try:
        if not re.fullmatch("[a-z0-9-]{1,63}", args.expected):
            raise ValueError("Invalid custom track")
        with args.source.open("rb") as stream:
            data = stream.read(LIMIT + 1)
        if len(data) > LIMIT:
            raise ValueError("The music file exceeds 64 MiB")
        normalized = convert(data)
        output = args.out / ("prepared-" + args.expected + ".vgm")
        if (
            output.is_symlink()
            or len({args.source.resolve(), output.resolve(), args.receipt.resolve()})
            != 3
        ):
            raise ValueError("Import output must be separate from the source")
        args.out.mkdir(parents=True, exist_ok=True)
        atomic_write(output, normalized)
        atomic_write(args.receipt, f"format=1\ncontent={output}\n".encode("utf-8"))
        print("S3K_CD_PROGRESS:0.20:VGM prepared", flush=True)
    except (OSError, ValueError) as error:
        print("S3K_CD_ERROR:" + str(error), flush=True)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
