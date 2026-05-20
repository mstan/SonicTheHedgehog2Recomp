"""probe_rdb_hscroll.py — check the Tier-1 rdb ring for WRAM writes to
the alleged Horiz_Scroll_Buf range ($FFFFE000..$FFFFE37F).

If SwScrl_CPZ is running and writing to the buffer, the ring should show
recent writes from PC inside SwScrl_CPZ ($00D27C..~$00D38?).
"""
import socket, json, sys

port = int(sys.argv[1]) if len(sys.argv) > 1 else 4380
s = socket.socket(); s.connect(("127.0.0.1", port)); s.settimeout(15.0)
nid = [1]
def cmd(name, **kw):
    msg = {"id": nid[0], "cmd": name}; nid[0]+=1; msg.update(kw)
    s.sendall((json.dumps(msg)+"\n").encode())
    buf = b""
    while b"\n" not in buf:
        ch = s.recv(1<<20)
        if not ch: raise RuntimeError("closed")
        buf += ch
    return json.loads(buf.split(b"\n",1)[0].decode())

fi = cmd("frame_info"); print(f"frame={fi.get('current_frame')}")

cnt = cmd("rdb_count")
print(f"rdb entries available: {cnt}")

# Filter on the runner side to keep the response small.
cmd("rdb_range", lo="0xFFE000", hi="0xFFE380")
d = cmd("rdb_dump")
entries = d.get("log", [])
def parse_h(v):
    if isinstance(v, str): return int(v, 16)
    return int(v or 0)
hits = [e for e in entries if 0xFFE000 <= parse_h(e.get("adr", 0)) <= 0xFFE380]
print(f"\nWrites to $FFFFE000..$FFFFE380 in dump: {len(hits)} hits (showing last 20):")
for e in hits[-20:]:
    a = parse_h(e.get("adr", 0)); v = parse_h(e.get("val", 0))
    fn = parse_h(e.get("func", 0)); ca = parse_h(e.get("caller", 0))
    print(f"  f={e.get('f'):>6}  vint={e.get('vint'):>6}  "
          f"adr=0x{a:06X}  val=0x{v:04X}  func=0x{fn:06X}  caller=0x{ca:06X}")
print(f"\nTotal entries returned: {len(entries)} (of ring total {d.get('total')})")
# Also count by func to see what's writing in that area at all
from collections import Counter
funcs = Counter(parse_h(e.get("func", 0)) for e in hits)
print(f"\nWriter functions (func PC → count) in this range:")
for f, n in funcs.most_common(10):
    print(f"  0x{f:06X}  hits={n}")

s.close()
