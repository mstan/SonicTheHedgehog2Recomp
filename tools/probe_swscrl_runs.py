"""probe_swscrl_runs.py — query rdb for SwScrl_CPZ side-effects.

If SwScrl_CPZ runs, it writes to these addresses (per disasm line refs):
  $FFFFEE14  Camera_BG2_Y_pos  (line 17296)
  $FFFFEE52  Scroll_flags_BG   (cleared at 17308)
  $FFFFEE54  Scroll_flags_BG2  (cleared at 17309)
  $FFFFEE56  Scroll_flags_BG3  (written at 17304)
  $FFFFA800  TempArray_LayerDef (every 8 frames, line 17316)
  $FFFFE000..$E37F  Horiz_Scroll_Buf  (line 17398)

We also check writes by FUNCTION PC matching SwScrl_CPZ's range $00D27C..$00D381.
That tells us if ANY code in SwScrl_CPZ is executing.
"""
import socket, json, sys
from collections import Counter

port = int(sys.argv[1]) if len(sys.argv) > 1 else 4380
s = socket.socket(); s.connect(("127.0.0.1", port)); s.settimeout(20.0)
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

def h(v):
    if isinstance(v, str): return int(v, 16)
    return int(v or 0)

fi = cmd("frame_info"); print(f"frame={fi.get('current_frame')}")

# Pull entries with the broadest filter set already on the ring.
d = cmd("rdb_dump")
log = d.get("log", [])
print(f"rdb log entries returned: {len(log)}")

# Bucket by writing-function PC.
by_func = Counter(h(e.get("func", 0)) for e in log)

# How many writes came from inside SwScrl_CPZ ($00D27C..$00D381)?
swscrl_writes = [e for e in log if 0x00D27C <= h(e.get("func", 0)) <= 0x00D381]
print(f"\nWrites whose FUNC PC is inside SwScrl_CPZ range: {len(swscrl_writes)}")
if swscrl_writes:
    print("First few:")
    for e in swscrl_writes[:5]:
        print(f"  f={e.get('f')} adr=0x{h(e.get('adr',0)):06X} val=0x{h(e.get('val',0)):04X} "
              f"func=0x{h(e.get('func',0)):06X} caller=0x{h(e.get('caller',0)):06X}")

# How many writes from inside DeformBgLayer / DeformBgLayerAfterScrollVert?
deform_writes = [e for e in log if 0x00C3D0 <= h(e.get("func", 0)) <= 0x00C4FF]
print(f"\nWrites whose FUNC PC is inside DeformBgLayer range ($00C3D0..$00C4FF): {len(deform_writes)}")

# Specific addresses that SwScrl_CPZ writes if it executes past the bsrs.
print("\nWrites to specific SwScrl_CPZ output addresses (any function):")
for addr, label in [(0xFFEE14, "Camera_BG2_Y_pos"),
                    (0xFFEE52, "Scroll_flags_BG"),
                    (0xFFEE54, "Scroll_flags_BG2"),
                    (0xFFEE56, "Scroll_flags_BG3"),
                    (0xFFA800, "TempArray_LayerDef"),
                    (0xFFE000, "Horiz_Scroll_Buf[0]")]:
    hits = [e for e in log if h(e.get("adr",0)) == addr]
    funcs = Counter(h(e.get("func", 0)) for e in hits)
    fn_str = ", ".join(f"${pc:06X}×{n}" for pc, n in funcs.most_common(3))
    print(f"  ${addr:06X}  {label:<22}  {len(hits):>5} hits  funcs: {fn_str}")

s.close()
