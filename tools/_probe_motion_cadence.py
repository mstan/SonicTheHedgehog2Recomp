#!/usr/bin/env python3
"""Dump per-frame motion deltas in the current special-stage window.

Distinguishes: does motion advance EVERY frame (60Hz) or every OTHER frame
(30Hz, the "should run at half" case)? And how big are the deltas?
"""
import socket
import json
import sys

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 4380
WINDOW = int(sys.argv[2]) if len(sys.argv) > 2 else 40

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


def series(s, field, lo, hi):
    return call(s, "frame_timeseries", field=field, **{"from": lo, "to": hi})["values"]


def main():
    s = socket.create_connection(("127.0.0.1", PORT), timeout=8)
    fi = call(s, "frame_info")
    hi = max(fi["oldest_frame"], fi["current_frame"] - 2)
    lo = max(fi["oldest_frame"], hi - WINDOW + 1)
    fields = ["game_mode", "sonic.x", "sonic.y", "sonic.xvel", "sonic.yvel",
              "scroll_x", "internal_frame"]
    data = {f: series(s, f, lo, hi) for f in fields}
    s.close()

    print(f"window wall frames {lo}..{hi}\n")
    hdr = f"{'wf':>6} {'mode':>5} {'iframe':>7} {'sx':>6} {'sy':>6} {'xvel':>6} {'yvel':>6} {'scrollx':>8}"
    print(hdr)
    print("-" * len(hdr))
    n = hi - lo + 1
    prev = {}
    for i in range(n):
        wf = lo + i

        def cell(f, signed=False):
            v = data[f][i]
            if v is None:
                return "  .  "
            # show value, and mark change vs prev frame with *
            mark = ""
            if f in prev and prev[f] is not None and v != prev[f]:
                mark = "*"
            return f"{v}{mark}"
        row = (f"{wf:>6} {str(data['game_mode'][i]):>5} "
               f"{cell('internal_frame'):>7} {cell('sonic.x'):>6} {cell('sonic.y'):>6} "
               f"{cell('sonic.xvel'):>6} {cell('sonic.yvel'):>6} {cell('scroll_x'):>8}")
        print(row)
        for f in fields:
            prev[f] = data[f][i]


if __name__ == "__main__":
    main()
