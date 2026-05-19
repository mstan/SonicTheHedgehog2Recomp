#!/usr/bin/env python
"""Count distinct H_Int handler firings per frame.

Strategy: dump the rdb ring, find every write whose `func` is 0x000F54
(H_Int). Group by frame; assume contiguous H_Int writes within one frame
== one firing. Print the per-frame firing count.
"""
import socket, json, sys

PORT = 4380
def call(cmd, **kw):
    req = {"id":1,"cmd":cmd}; req.update(kw)
    s=socket.socket(); s.settimeout(15); s.connect(("127.0.0.1",PORT))
    s.sendall((json.dumps(req)+"\n").encode())
    buf=b""
    while b"\n" not in buf:
        chunk=s.recv(65536)
        if not chunk: break
        buf+=chunk
    s.close()
    return json.loads(buf.decode("utf-8", errors="replace").strip())

total = call("rdb_count")["count"]
PAGE = 4096
entries = []
start = 0
while start < total:
    n = min(PAGE, total-start)
    r = call("rdb_dump", start=start, count=n)
    entries.extend(r.get("log", []))
    start += n
print(f"[rdb] {len(entries)} entries", file=sys.stderr)

# Per-frame: track H_Int writes (func == 0x000F54) and a separate counter
# for distinct firings (contiguous runs of H_Int writes within a frame).
per_frame_writes = {}
per_frame_firings = {}
prev_was_hint = {}
for e in entries:
    f = e["f"]
    func = e["func"]
    if func != "0x000F54":
        prev_was_hint[f] = False
        continue
    per_frame_writes[f] = per_frame_writes.get(f, 0) + 1
    if not prev_was_hint.get(f, False):
        per_frame_firings[f] = per_frame_firings.get(f, 0) + 1
    prev_was_hint[f] = True

frames = sorted(set(list(per_frame_writes.keys()) + list(per_frame_firings.keys())))
print(f"{'frame':>8s} {'firings':>8s} {'h_writes':>10s}")
for f in frames[-30:]:
    print(f"{f:>8d} {per_frame_firings.get(f,0):>8d} {per_frame_writes.get(f,0):>10d}")
