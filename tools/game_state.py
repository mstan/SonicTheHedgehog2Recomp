"""game_state.py — quick read of Sonic 2 game state on a running binary."""
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

fi = cmd("frame_info")
cur = fi.get("current_frame", 0)
print(f"current_frame={cur}")

# Get full frame record
r = cmd("get_frame", frame=cur, include="all")
wram = r.get("wram", "")  # hex string, 64 KB
def b(addr):
    off = (addr & 0xFFFF) * 2
    return int(wram[off:off+2], 16) if len(wram) >= off+2 else 0
def w(addr):
    return (b(addr) << 8) | b(addr+1)
def l32(addr):
    return (w(addr) << 16) | w(addr+2)

print(f"\nGame state:")
print(f"  Game_Mode      $FFF600 = 0x{b(0xFFF600):02X}")
print(f"  Vint_routine   $FFF62A = 0x{b(0xFFF62A):02X}")
print(f"  Vint_runcount  $FFFE0C = 0x{l32(0xFFFE0C):08X} ({l32(0xFFFE0C)})")
print(f"  Demo_Time_left $FFF614 = 0x{w(0xFFF614):04X}")
print(f"  Ctrl_1_Held    $FFF604 = 0x{b(0xFFF604):02X}")
print(f"  Ctrl_1_Press   $FFF605 = 0x{b(0xFFF605):02X}")
print(f"  Hint_flag      $FFF644 = 0x{w(0xFFF644):04X}")

# m68k state
m = r.get("m68k", {})
print(f"\nm68k:")
print(f"  PC = 0x{m.get('PC', 0):08X}")
print(f"  SR = 0x{m.get('SR', 0):04X}  (imask = {(m.get('SR',0) >> 8) & 7})")
print(f"  A7 = 0x{m.get('A',[0]*8)[7]:08X}")
A = m.get('A', [0]*8)
print(f"  A0..A7 = " + " ".join(f"{a:08X}" for a in A))
D = m.get('D', [0]*8)
print(f"  D0..D7 = " + " ".join(f"{d:08X}" for d in D))

s.close()
