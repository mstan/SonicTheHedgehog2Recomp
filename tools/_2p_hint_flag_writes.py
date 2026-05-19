#!/usr/bin/env python
"""Snapshot rdb ring and tabulate writes to Hint_flag ($FFF644) and reg $02
(VDP PNT A) writes via $C00004 with high byte == 0x82.
"""
import socket, json, sys
PORT = 4380
def call(cmd, **kw):
    req = {"id":1,"cmd":cmd}; req.update(kw)
    s=socket.socket(); s.settimeout(15); s.connect(("127.0.0.1",PORT))
    s.sendall((json.dumps(req)+"\n").encode())
    buf=b""
    while b"\n" not in buf:
        c=s.recv(65536)
        if not c: break
        buf+=c
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

last_f = None
prev_hi_byte_82 = False
for e in entries:
    f = e["f"]
    adr = int(e["adr"], 16)
    val = int(e["val"], 16)
    func = e["func"]
    if f != last_f:
        print(f"---- frame {f} (vint {e['vint']}) ----")
        last_f = f
        prev_hi_byte_82 = False
    if adr == 0xFFF644:
        print(f"  Hint_flag[hi]= 0x{val:02X}   func={func}")
    elif adr == 0xFFF645:
        print(f"  Hint_flag[lo]= 0x{val:02X}   func={func}")
    elif adr == 0xC00004:
        # First byte of reg write: $8X high byte = reg X
        if val == 0x82:
            prev_hi_byte_82 = True
        else:
            prev_hi_byte_82 = False
    elif adr == 0xC00005 and prev_hi_byte_82:
        # Second byte of reg $02 write — this is PNT A nametable base.
        plane_a = (val & 0x78) << 10
        print(f"  PNT_A base   = 0x{plane_a:06X} (reg val 0x{val:02X})   func={func}")
        prev_hi_byte_82 = False
