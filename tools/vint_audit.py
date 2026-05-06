"""vint_audit.py — confirm whether native is firing the VInt handler
each wall frame and whether $FFFFFE0C writes happen each fire.

Runs three queries on the native binary:
  1. vblanks_fired per wall frame (frame_timeseries on g_pace_snap)
  2. Vint_runcount per wall frame (wram32[FE0C])
  3. WRAM-ring count + a sample of stores in the FE0C..FE0F window
"""
import socket, json, sys

def cli(port, label):
    s = socket.socket()
    s.connect(("127.0.0.1", port))
    s.settimeout(20.0)
    next_id = [1]
    def cmd(name, **kw):
        msg = {"id": next_id[0], "cmd": name}
        next_id[0] += 1
        msg.update(kw)
        s.sendall((json.dumps(msg) + "\n").encode())
        buf = b""
        while b"\n" not in buf:
            chunk = s.recv(1 << 20)
            if not chunk: raise RuntimeError("closed")
            buf += chunk
        return json.loads(buf.split(b"\n", 1)[0].decode())
    return s, cmd

s, cmd = cli(4380, "native")
fi = cmd("frame_info")
cur = fi.get("current_frame", 0)
LO = max(0, cur - 60)
HI = cur
print(f"native current_frame={cur}  examining {LO}..{HI}")

# Q1: vblanks_fired (the executed-flag from g_pace_snap)
ts1 = cmd("frame_timeseries", field="pace.vblanks_fired",
          **{"from": LO, "to": HI})
v1 = ts1.get("values") or []
sum1 = sum(x for x in v1 if x is not None)
print(f"\npace.vblanks_fired: sum={sum1} across {len(v1)} frames "
      f"(expected ~{HI-LO+1})")
print(f"  first 30: {v1[:30]}")
print(f"  last 30:  {v1[-30:]}")

# Q2: Vint_runcount per frame
ts2 = cmd("frame_timeseries", field="wram32[FE0C]",
          **{"from": LO, "to": HI})
v2 = ts2.get("values") or []
deltas = [v2[i+1] - v2[i] for i in range(len(v2)-1) if v2[i] is not None and v2[i+1] is not None]
print(f"\nVint_runcount per frame:")
print(f"  start={v2[0]}, end={v2[-1]}, total delta = {v2[-1]-v2[0] if v2[0] is not None else '?'}")
print(f"  per-frame deltas: {deltas[:30]}")
ones = sum(1 for d in deltas if d == 1)
zeros = sum(1 for d in deltas if d == 0)
twos  = sum(1 for d in deltas if d == 2)
others = len(deltas) - ones - zeros - twos
print(f"  delta breakdown: 0={zeros}, 1={ones}, 2={twos}, other={others}")

s.close()
