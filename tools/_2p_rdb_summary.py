#!/usr/bin/env python
"""Snapshot rdb ring with VDP+H-int filters armed, then summarize.

Prints, in time order:
  - writes to $C00004/5/6/7 (VDP control port) — shows reg writes & VDP commands
  - writes to $FFF644 (Hint_flag)
  - writes to $FFF624/5 (Hint_counter_reserve)
Per entry: frame, vint_runcount, byte addr, value, caller func.
"""
import socket, json, sys

PORT = 4380

def call(cmd, **kw):
    req = {"id": 1, "cmd": cmd}
    req.update(kw)
    s = socket.socket(); s.settimeout(15); s.connect(("127.0.0.1", PORT))
    s.sendall((json.dumps(req) + "\n").encode())
    buf = b""
    while b"\n" not in buf:
        chunk = s.recv(65536)
        if not chunk: break
        buf += chunk
    s.close()
    return json.loads(buf.decode("utf-8", errors="replace").strip())

# Snapshot the ring (begin/end pattern)
total = call("rdb_count")["count"]
print(f"[rdb] total entries: {total}", file=sys.stderr)

PAGE = 4096
entries = []
start = 0
while start < total:
    n = min(PAGE, total - start)
    r = call("rdb_dump", start=start, count=n)
    if not r.get("ok"):
        print(f"[rdb_dump] err: {r}", file=sys.stderr); break
    entries.extend(r.get("log", []))
    start += n

print(f"[rdb] dumped {len(entries)} entries", file=sys.stderr)

# Filter to interesting writes
INTERESTING = [0xC00004, 0xC00005, 0xC00006, 0xC00007,
               0xFFF624, 0xFFF625, 0xFFF644, 0xFFF645]

def lbl(adr):
    if adr in (0xC00004, 0xC00005, 0xC00006, 0xC00007): return f"VDP_ctrl[{adr & 3}]"
    if adr in (0xFFF624, 0xFFF625): return f"Hint_cnt_res[{adr & 1}]"
    if adr in (0xFFF644, 0xFFF645): return f"Hint_flag[{adr & 1}]"
    return f"$0x{adr:06X}"

last_frame = None
for e in entries:
    adr = int(e["adr"], 16)
    if adr not in INTERESTING: continue
    f = e["f"]; v = int(e["val"], 16); func = e["func"]; caller = e["caller"]
    if last_frame != f:
        print(f"---- frame {f}, vint_runcount {e['vint']} ----")
        last_frame = f
    print(f"  {lbl(adr):20s}  = 0x{v:02X}   func={func} caller={caller}")
