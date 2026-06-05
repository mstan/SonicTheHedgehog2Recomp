#!/usr/bin/env python3
"""Measure game-logic ticks per wall-frame across the always-on frame_record ring.

The "special stages run at 2x" question reduces to: how many times does the
game advance its internal frame counter (Vint_runcount) per displayed wall
frame? ~1.0 = locked to display. ~2.0 = double-stepping (the suspected bug).

This is a pure ring QUERY (PRINCIPLES #17): we connect, read backward over the
~10s the ring holds, segment by game_mode, and report the tick rate per mode.
If the user passed through a normal level before entering the special stage,
both segments appear in the same window and we get a same-session baseline.

game_mode: 0x0C = normal level, 0x10 = special stage, 0x08 = demo, 0x04 = title.
"""
import socket
import json
import sys
import statistics

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 4380

MODE_NAMES = {
    0x00: "Sega", 0x04: "Title", 0x08: "Demo", 0x0C: "Level",
    0x10: "SpecialStage", 0x14: "Continue", 0x18: "2P-Menu",
    0x1C: "EndingSeq", 0x20: "Credits",
}


def mode_name(m):
    return MODE_NAMES.get(m & 0x7F, f"mode_0x{m:02X}") + (" (loading)" if m & 0x80 else "")


_id = [1]


def call(sock, cmd, **args):
    req = {"id": _id[0], "cmd": cmd}
    req.update(args)
    _id[0] += 1
    sock.sendall((json.dumps(req) + "\n").encode())
    buf = b""
    while not buf.endswith(b"\n"):
        chunk = sock.recv(65536)
        if not chunk:
            break
        buf += chunk
    return json.loads(buf.decode().strip())


def fetch_series(sock, field, lo, hi):
    out = []
    f = lo
    while f <= hi:
        chunk_hi = min(f + 599, hi)
        r = call(sock, "frame_timeseries", field=field, **{"from": f, "to": chunk_hi})
        if not r.get("ok"):
            raise RuntimeError(f"{field}: {r}")
        out.extend(r["values"])
        f = chunk_hi + 1
    return out


def main():
    s = socket.create_connection(("127.0.0.1", PORT), timeout=8)
    fi = call(s, "frame_info")
    lo, hi = fi["oldest_frame"], fi["current_frame"]
    # leave a small margin off the live edge; those frames may be mid-write
    hi = max(lo, hi - 2)
    span = hi - lo + 1
    print(f"ring window: wall frames {lo}..{hi}  ({span} frames, ~{span/59.94:.1f}s)\n")

    modes = fetch_series(s, "game_mode", lo, hi)
    iframe = fetch_series(s, "internal_frame", lo, hi)
    s.close()

    # Segment into runs of constant game_mode.
    segs = []  # (mode, start_idx, end_idx)
    i = 0
    n = len(modes)
    while i < n:
        if modes[i] is None:
            i += 1
            continue
        m = modes[i]
        j = i
        while j + 1 < n and modes[j + 1] == m:
            j += 1
        segs.append((m, i, j))
        i = j + 1

    print(f"{'mode':<22} {'wall frames':<20} {'frames':>6} {'ticks/frame':>12}  note")
    print("-" * 78)
    for m, a, b in segs:
        wf_a, wf_b = lo + a, lo + b
        nframes = b - a + 1
        # consecutive positive deltas of internal_frame within this segment
        deltas = []
        for k in range(a, b):
            x, y = iframe[k], iframe[k + 1]
            if x is None or y is None:
                continue
            d = y - x
            if 0 <= d <= 10:  # drop counter resets / wraps
                deltas.append(d)
        if deltas:
            rate = statistics.median(deltas)
            mean = statistics.fmean(deltas)
            note = f"mean={mean:.2f}"
            if nframes >= 8:
                if rate >= 1.75:
                    note += "  <== DOUBLE-STEPPING (~2x)"
                elif rate <= 0.6:
                    note += "  <== half-rate"
            rate_s = f"{rate:.2f}"
        else:
            rate_s, note = "n/a", "(too short / no ticks)"
        print(f"{mode_name(m):<22} {f'{wf_a}..{wf_b}':<20} {nframes:>6} {rate_s:>12}  {note}")


if __name__ == "__main__":
    main()
