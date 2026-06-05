#!/usr/bin/env python3
"""Measure displayed wall-frames per REAL second by sampling the live counter.

internal_frame/wall_frame = 1.0 proves logic ticks once per displayed frame,
but says nothing about how fast displayed frames arrive in real time. This
samples frame_info across a real-clock interval to get throughput (fps).
~60 = correct NTSC pacing; ~120 = running at 2x real-time.
"""
import socket
import json
import sys
import time

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 4380
SECS = float(sys.argv[2]) if len(sys.argv) > 2 else 4.0

MODE_NAMES = {0x00: "Sega", 0x04: "Title", 0x08: "Demo", 0x0C: "Level",
             0x10: "SpecialStage", 0x14: "Continue"}


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


def cur_mode(s):
    fi = call(s, "frame_info")
    hi = max(fi["oldest_frame"], fi["current_frame"] - 2)
    r = call(s, "frame_timeseries", field="game_mode", **{"from": hi, "to": hi})
    m = r["values"][0]
    return m, MODE_NAMES.get((m or 0) & 0x7F, f"0x{(m or 0):02X}")


def main():
    s = socket.create_connection(("127.0.0.1", PORT), timeout=8)
    m0, name0 = cur_mode(s)
    f0 = call(s, "frame_info")["current_frame"]
    t0 = time.monotonic()
    time.sleep(SECS)
    f1 = call(s, "frame_info")["current_frame"]
    t1 = time.monotonic()
    m1, name1 = cur_mode(s)
    s.close()

    dt = t1 - t0
    df = f1 - f0
    fps = df / dt
    print(f"mode during sample: {name0}" + (f" -> {name1}" if name1 != name0 else ""))
    print(f"wall frames advanced: {df} over {dt:.3f}s real")
    print(f"=> displayed throughput: {fps:.1f} fps")
    print(f"   NTSC target is 59.94 fps. ratio vs target: {fps/59.94:.2f}x")


if __name__ == "__main__":
    main()
