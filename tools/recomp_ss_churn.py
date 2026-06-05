#!/usr/bin/env python3
"""Recomp-side special-stage churn, comparable to mdref's ref_baseline.py.

Pulls the Tier-1 reverse-debug ring (every WRAM write, frame-stamped), finds
the special-stage window via the Game_Mode ($FFF600 -> 0x10) write, and reports
distinct WRAM bytes changed per frame. Compare median to the Genesis Plus GX
reference (~98 bytes/frame): ~98 => one update/frame (authentic); ~196 =>
the main loop is running twice per displayed frame (the 2x bug).

Recomp WRAM is NOT byte-swapped (direct big-endian), unlike the GPGX view.
"""
import socket, json, sys, collections

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 4380
GAME_MODE = 0xFFF600
SS_MODE = 0x10
# reference SS-cluster addresses + their per-frame change-rate in GPGX
REF_RATE = {0xFFFDB4: 0.63, 0xFFFDC0: 0.68, 0xFFFDCC: 0.70,
            0xFFFE0E: 1.00, 0xFFDB0C: 0.53}

def cli(port):
    s = socket.socket(); s.connect(("127.0.0.1", port)); s.settimeout(60.0)
    nid = [1]
    def cmd(name, **kw):
        m = {"id": nid[0], "cmd": name}; nid[0]+=1; m.update(kw)
        s.sendall((json.dumps(m)+"\n").encode())
        buf=b""
        while b"\n" not in buf:
            ch=s.recv(1<<20)
            if not ch: raise RuntimeError("closed")
            buf+=ch
        return json.loads(buf.split(b"\n",1)[0].decode())
    return s, cmd

def hx(v): return int(v,16) if isinstance(v,str) else int(v)

s, cmd = cli(PORT)
total = cmd("rdb_count").get("count", 0)
print(f"rdb ring entries: {total}")

# pull whole ring
entries = []
start = 0
while start < total:
    r = cmd("rdb_dump", start=start, count=min(100000, total-start))
    log = r.get("log", [])
    if not log: break
    entries.extend(log)
    start += len(log)
    if r.get("done"): break
print(f"pulled {len(entries)} entries")

# find SS window via Game_Mode writes
mode_w = [(e.get("f",0), hx(e.get("val",0))) for e in entries
          if hx(e.get("adr",0)) == GAME_MODE]
print(f"Game_Mode writes seen: {len(mode_w)}")
for f,v in mode_w[-10:]:
    tag = " <== SPECIAL" if (v & 0x7F)==SS_MODE else ""
    print(f"  f{f}: 0x{v:02X}{tag}")
ss_entries = [f for f,v in mode_w if (v & 0x7F)==SS_MODE]
if not ss_entries:
    print("\nNo Game_Mode==0x10 in ring — are you in the special stage? "
          "Enter it, stay ~10s, then re-run.")
    s.close(); sys.exit(0)
ss_lo = min(ss_entries)
later = [f for f,v in mode_w if f>ss_lo and (v&0x7F)!=SS_MODE]
ss_hi = (min(later)-1) if later else max(e.get("f",0) for e in entries)
print(f"\nspecial-stage window: f{ss_lo}..f{ss_hi} ({ss_hi-ss_lo+1} frames)")

# distinct bytes changed per frame (expand write width to bytes)
per_frame = collections.defaultdict(set)
addr_frames = collections.defaultdict(set)  # for REF_RATE comparison
for e in entries:
    f = e.get("f",0)
    if not (ss_lo <= f <= ss_hi): continue
    a = hx(e.get("adr",0)); w = int(e.get("w",1) or 1)
    for b in range(w):
        per_frame[f].add(a+b)
    if a in REF_RATE:
        addr_frames[a].add(f)

frames = sorted(per_frame)
counts = [len(per_frame[f]) for f in frames]
nspan = ss_hi - ss_lo + 1
print(f"frames with writes: {len(frames)}/{nspan}")
if counts:
    med = sorted(counts)[len(counts)//2]
    print(f"distinct WRAM bytes changed per frame: min={min(counts)} "
          f"median={med} mean={sum(counts)/len(counts):.1f} max={max(counts)}")
    print(f"\n  RECOMP median {med} vs REFERENCE median 98  ->  ratio {med/98:.2f}x")
    print("  (~1.0x = authentic one-update/frame; ~2.0x = double-stepping)")

print("\nper-address change-rate vs reference (SS cluster):")
print(f"{'addr':>9} {'recomp/f':>9} {'ref/f':>7} {'ratio':>6}")
for a in sorted(REF_RATE):
    rr = len(addr_frames.get(a,()))/nspan
    print(f"  ${a:06X} {rr:>9.2f} {REF_RATE[a]:>7.2f} {rr/REF_RATE[a]:>5.2f}x")
s.close()
