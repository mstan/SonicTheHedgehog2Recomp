"""probe_hscroll_buf.py — dump WRAM Horiz_Scroll_Buf and check VBlank DMA path.

If SwScrl_CPZ is running correctly, $FFFFE000..$FFFFE37F should hold the
per-line HScroll table that gets DMA'd to VRAM $FC00 each VBlank. Each line
is 4 bytes: 2 PlaneA + 2 PlaneB. Values are NEGATIVE-of-camera.
"""
import socket, json, sys

port = int(sys.argv[1]) if len(sys.argv) > 1 else 4380
s = socket.socket(); s.connect(("127.0.0.1", port)); s.settimeout(10.0)
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

fi  = cmd("frame_info")
cur = fi.get("current_frame", 0)
r   = cmd("get_frame", frame=cur, include="all")
wram = r.get("wram", "")

def b(addr): return int(wram[(addr&0xFFFF)*2:(addr&0xFFFF)*2+2], 16)
def w(addr): return (b(addr)<<8)|b(addr+1)

print(f"frame={cur}, Camera_X={(w(0xFFEE00)):d}, Camera_BG_X={(w(0xFFEE08)):d}, Camera_BG2_X={(w(0xFFEE10)):d}")
print()
print("WRAM $FFFFE000+ (alleged Horiz_Scroll_Buf, first 32 lines × 4 bytes):")
print(f"  line   PlaneA       PlaneB")
for line in range(0, 32):
    a = 0xFFE000 + line*4
    pa = w(a); pb = w(a+2)
    pa_s = pa - 0x10000 if pa & 0x8000 else pa
    pb_s = pb - 0x10000 if pb & 0x8000 else pb
    print(f"  {line:>3}    0x{pa:04X} ({pa_s:>+6d})   0x{pb:04X} ({pb_s:>+6d})")

# Sample further into the buffer (224 lines × 4 = 896 bytes total).
print()
print("Sampled later lines (40, 60, 80, 100, 140, 180, 220):")
for line in [40, 60, 80, 100, 140, 180, 220]:
    a = 0xFFE000 + line*4
    pa = w(a); pb = w(a+2)
    pa_s = pa - 0x10000 if pa & 0x8000 else pa
    pb_s = pb - 0x10000 if pb & 0x8000 else pb
    print(f"  {line:>3}    0x{pa:04X} ({pa_s:>+6d})   0x{pb:04X} ({pb_s:>+6d})")

# Also check $FFFFA800 (TempArray_LayerDef) — SwScrl_CPZ reads/writes it.
print()
print(f"$FFFFA800 (TempArray_LayerDef[0..3]): "
      f"0x{w(0xFFA800):04X} 0x{w(0xFFA802):04X} 0x{w(0xFFA804):04X} 0x{w(0xFFA806):04X}")

s.close()
