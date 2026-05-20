"""probe_cpz_scroll.py — query CPZ scroll-related state on a running Sonic 2 native.

What it reads (all WRAM addresses per s2disasm/s2.constants.asm):
  Camera_X_pos      $FFFFEE00 .l   Plane A foreground camera X
  Camera_Y_pos      $FFFFEE04 .l
  Camera_BG_X_pos   $FFFFEE08 .l   Plane B background camera X
  Camera_BG_Y_pos   $FFFFEE0C .l
  Camera_BG2_X_pos  $FFFFEE10 .l   CPZ-specific second BG layer X
  Camera_BG2_Y_pos  $FFFFEE14 .l
  Camera_BG3_X_pos  $FFFFEE18 .l   (unused in CPZ normally)
  Camera_BG3_Y_pos  $FFFFEE1C .l
  Sonic X           $FFFFB008 .l   (Sonic SST + 0x08)
  Sonic Y           $FFFFB00C .l
  Game_Mode         $FFFFF600 .b
  Current_Zone      $FFFFFE10 .b   (CPZ = $05 in 2-byte zone+act packing)
  Current_Act       $FFFFFE11 .b

It also queries vdp_state (hscroll table address, modes).

Usage:  python probe_cpz_scroll.py [port]   (default 4380)
"""
import socket, json, sys

port = int(sys.argv[1]) if len(sys.argv) > 1 else 4380

s = socket.socket()
s.connect(("127.0.0.1", port))
s.settimeout(10.0)
nid = [1]

def cmd(name, **kw):
    msg = {"id": nid[0], "cmd": name}; nid[0] += 1; msg.update(kw)
    s.sendall((json.dumps(msg) + "\n").encode())
    buf = b""
    while b"\n" not in buf:
        ch = s.recv(1 << 20)
        if not ch: raise RuntimeError("connection closed")
        buf += ch
    return json.loads(buf.split(b"\n", 1)[0].decode())

# Pull whole WRAM via a frame snapshot — least-roundtrip + lets us read any addr.
fi = cmd("frame_info")
cur = fi.get("current_frame", 0)
r   = cmd("get_frame", frame=cur, include="all")
wram = r.get("wram", "")

def b(addr):
    off = (addr & 0xFFFF) * 2
    return int(wram[off:off+2], 16) if len(wram) >= off+2 else 0
def w(addr):
    return (b(addr) << 8) | b(addr+1)
def l(addr):
    return (w(addr) << 16) | w(addr+2)
def sl(addr):
    v = l(addr)
    return v - 0x100000000 if v & 0x80000000 else v

print(f"frame={cur}")
print()
print(f"Game_Mode       $FFF600 = 0x{b(0xFFF600):02X}")
print(f"Current_Zone    $FFFE10 = 0x{b(0xFFFE10):02X}  (CPZ = 0x05)")
print(f"Current_Act     $FFFE11 = 0x{b(0xFFFE11):02X}")
print()
print("Camera positions (signed long, .X is pixel position in level):")
print(f"  Camera_X_pos     $EE00 = {sl(0xFFEE00):>10d}   (Plane A fg)")
print(f"  Camera_Y_pos     $EE04 = {sl(0xFFEE04):>10d}")
print(f"  Camera_BG_X_pos  $EE08 = {sl(0xFFEE08):>10d}   (Plane B bg)")
print(f"  Camera_BG_Y_pos  $EE0C = {sl(0xFFEE0C):>10d}")
print(f"  Camera_BG2_X_pos $EE10 = {sl(0xFFEE10):>10d}   (CPZ 2nd bg)")
print(f"  Camera_BG2_Y_pos $EE14 = {sl(0xFFEE14):>10d}")
print(f"  Camera_BG3_X_pos $EE18 = {sl(0xFFEE18):>10d}")
print(f"  Camera_BG3_Y_pos $EE1C = {sl(0xFFEE1C):>10d}")
print()
print("Sonic SST ($FFFFB000):")
sonic_x = sl(0xFFB008)
sonic_y = sl(0xFFB00C)
print(f"  X.l       $B008  = {sonic_x:>10d}")
print(f"  Y.l       $B00C  = {sonic_y:>10d}")
print(f"  X.w (hi)  $B008  = {(sonic_x >> 16) & 0xFFFF:>5d}   (level pixel X)")
print(f"  Y.w (hi)  $B00C  = {(sonic_y >> 16) & 0xFFFF:>5d}")
print()
# Camera_RAM_copy block ($FFFFEE60 onwards) — these are written to the
# scroll-table-source ring each VBlank.
print("Camera *_copy (snapshot used each VBlank to update VRAM HScroll):")
print(f"  Camera_RAM_copy   $EE60 = {sl(0xFFEE60):>10d}, Y={sl(0xFFEE64):>10d}")
print(f"  Camera_BG_copy    $EE68 = {sl(0xFFEE68):>10d}, Y={sl(0xFFEE6C):>10d}")
print(f"  Camera_BG2_copy   $EE70 = {sl(0xFFEE70):>10d}, Y={sl(0xFFEE74):>10d}")
print(f"  Camera_BG3_copy   $EE78 = {sl(0xFFEE78):>10d}, Y={sl(0xFFEE7C):>10d}")
print()
print("Block-crossed flags:")
print(f"  Horiz_BCF        $EE40 = 0x{b(0xFFEE40):02X}")
print(f"  Verti_BCF        $EE41 = 0x{b(0xFFEE41):02X}")
print(f"  Horiz_BCF_BG     $EE42 = 0x{b(0xFFEE42):02X}")
print(f"  Verti_BCF_BG     $EE43 = 0x{b(0xFFEE43):02X}")
print(f"  Horiz_BCF_BG2    $EE44 = 0x{b(0xFFEE44):02X}   (CPZ)")
print(f"  Horiz_BCF_BG3    $EE46 = 0x{b(0xFFEE46):02X}")
print()
print("Scroll flags:")
print(f"  Scroll_flags     $EE50 = 0x{w(0xFFEE50):04X}")
print(f"  Scroll_flags_BG  $EE52 = 0x{w(0xFFEE52):04X}")
print(f"  Scroll_flags_BG2 $EE54 = 0x{w(0xFFEE54):04X}   (CPZ)")
print(f"  Scroll_flags_BG3 $EE56 = 0x{w(0xFFEE56):04X}")
print()

vdp = cmd("vdp_state").get("vdp", {})
# hscroll_mask encodes the HScroll mode:
#   0x00 = full screen, 0x07 = first 8 lines, 0xF8 = per-cell, 0xFF = per-line
mask = vdp.get("hscroll_mask", 0)
mode = {0x00:"full", 0x07:"line8", 0xF8:"cell", 0xFF:"line"}.get(mask, f"mask=0x{mask:02X}")
print("VDP state:")
print(f"  hscroll table addr = 0x{vdp.get('hscroll', 0):04X}   (in VRAM)")
print(f"  hscroll mode       = {mode}    (per-line is CPZ-normal)")
print(f"  vscroll_mode       = {vdp.get('vscroll_mode', '?')}    (0=full, 1=2-cell)")
print(f"  plane_a            = 0x{vdp.get('plane_a', 0):04X}")
print(f"  plane_b            = 0x{vdp.get('plane_b', 0):04X}")

# Read the first 32 lines worth of the VRAM HScroll table (32 lines × 4 bytes = 128).
# Each line is: 2 bytes Plane A H-scroll, 2 bytes Plane B H-scroll.
# Values are NEGATIVE of the camera (VDP scrolls left = negative).
hscroll_addr = vdp.get("hscroll", 0)
if hscroll_addr:
    rv = cmd("read_vram", addr=f"0x{hscroll_addr:04X}", size=128)
    h = rv.get("hex", "")
    print()
    print(f"VRAM HScroll table @ 0x{hscroll_addr:04X} — sampled lines 0/8/16/.../224:")
    print(f"  (HScroll values are NEGATIVE-of-camera; mod 0x400)")
    print(f"  line   PlaneA       PlaneB")
    for line in [0, 8, 16, 32, 48, 64, 96, 128, 160, 192, 224]:
        # we read 128 bytes = 32 lines worth; if line>=32 wrap a fresh read
        pass
    # Read in chunks: we want 224 lines = 896 bytes max. Do one fetch.
    rv2 = cmd("read_vram", addr=f"0x{hscroll_addr:04X}", size=896)
    h2 = rv2.get("hex", "")
    for line in [0, 4, 8, 16, 32, 48, 64, 96, 128, 160, 192, 220]:
        off = line * 8   # 4 bytes/line × 2 hex chars/byte
        if off + 8 > len(h2): break
        pa = int(h2[off:off+4], 16)
        pb = int(h2[off+4:off+8], 16)
        pa_s = pa - 0x10000 if pa & 0x8000 else pa
        pb_s = pb - 0x10000 if pb & 0x8000 else pb
        print(f"  {line:>3}    0x{pa:04X} ({pa_s:>+6d})   0x{pb:04X} ({pb_s:>+6d})")

s.close()
