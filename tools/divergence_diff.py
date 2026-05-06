"""
divergence_diff.py — free-run state-synced divergence detector
                     for Sonic 2 native vs oracle.

This is the "ring query" version. NO pause/run_frames lockstep — both
binaries run continuously while we read their always-on frame ring
buffers. The sync key is `Vint_runcount` (longword at $FFFFFE0C),
which both VBlank handlers increment exactly once per serviced VBlank
in identical recompiled vs interpreted code paths. Frame numbers from
the runner's wall-clock counter diverge within seconds (oracle is
slower); Vint_runcount does not.

Workflow:
  1. User launches both binaries in two consoles:
       SonicTheHedgehog2Recomp.exe        sonic2.bin --port 4380
       SonicTheHedgehog2Recomp_oracle.exe sonic2.bin --port 4381
  2. Run this script. It sleeps `--wait` wall seconds while both
     binaries fill their 600-frame rings, then reads the rings.
  3. For each side, builds a {vint_runcount -> wall_frame} dict by
     querying frame_timeseries field=wram32[FE0C].
  4. Intersects the two dicts to find K values present in both rings.
  5. For each K (oldest-first), fetches the full FrameRecord from each
     side via get_frame include=all, then diffs subsystem-by-subsystem
     (M68K regs, VDP regs+CRAM, Z80 regs, FM/PSG, WRAM byte ranges).
  6. Stops at the first K where the records differ — that K is the
     first state-divergence point. Reports the offending subsystem,
     and for WRAM, the first 16 differing offsets.

Why no pause: per the project's CLAUDE.md ring-buffer rule, "pause +
step + read state" is structurally identical to arm-then-capture —
you decide what window to observe at probe time and miss everything
before. Always-on rings + retroactive query is the model. Pause/step
is only legitimate as an interactive control-plane primitive.

Usage:
    python tools/divergence_diff.py [--wait 12] [--max-k 600]
"""

from __future__ import annotations

import argparse
import json
import socket
import sys
import time


# --- TCP client -----------------------------------------------------------

class DebugClient:
    """Minimal newline-delimited JSON client for the runner's cmd_server."""

    def __init__(self, host: str, port: int, label: str):
        self.host  = host
        self.port  = port
        self.label = label
        self.sock  = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.connect((host, port))
        self.sock.settimeout(30.0)
        self.buf = b""
        self._next_id = 1

    def cmd(self, name: str, **fields) -> dict:
        msg = {"id": self._next_id, "cmd": name}
        msg.update(fields)
        self._next_id += 1
        line = (json.dumps(msg) + "\n").encode("utf-8")
        self.sock.sendall(line)
        return self._recv_one()

    def _recv_one(self) -> dict:
        while b"\n" not in self.buf:
            chunk = self.sock.recv(1 << 20)
            if not chunk:
                raise RuntimeError(f"{self.label}: connection closed")
            self.buf += chunk
        line, _, rest = self.buf.partition(b"\n")
        self.buf = rest
        return json.loads(line.decode("utf-8"))

    def close(self):
        try: self.sock.close()
        except Exception: pass


# --- Ring discovery -------------------------------------------------------

def vint_timeline(c: DebugClient, frame_lo: int, frame_hi: int) -> dict[int, int]:
    """Map vint_runcount -> wall_frame across [frame_lo, frame_hi].

    Issues one frame_timeseries query per 600-frame chunk (the ring cap).
    Returns the mapping; later frames overwrite earlier ones if the same
    Vint_runcount somehow appears twice (shouldn't, but be defensive).
    """
    out: dict[int, int] = {}
    f = frame_lo
    while f <= frame_hi:
        end = min(f + 599, frame_hi)
        r = c.cmd("frame_timeseries", field="wram32[FE0C]", **{"from": f, "to": end})
        if not r.get("ok"):
            raise RuntimeError(f"{c.label}: frame_timeseries failed: {r}")
        vals = r.get("values") or []
        for i, v in enumerate(vals):
            if v is None:
                continue
            out[int(v)] = f + i
        f = end + 1
    return out


def discover_window(c: DebugClient) -> tuple[int, int]:
    """Return (oldest_frame, newest_frame) currently in the ring."""
    info = c.cmd("frame_info")
    cur = info.get("current_frame", 0) or info.get("frame", 0)
    cap = 600  # FRAME_HISTORY_CAP
    oldest = max(0, cur - cap + 1)
    return (oldest, cur)


# --- Per-frame fetch + diff ----------------------------------------------

def fetch_full_frame(c: DebugClient, wall_frame: int) -> dict | None:
    """Fetch a full frame record. Returns None if the frame has slid out
    of the ring (free-running rings advance during the query loop)."""
    r = c.cmd("get_frame", frame=wall_frame, include="all")
    if not r.get("ok"):
        err = (r.get("error") or "").lower()
        if "ring" in err or "not in" in err:
            return None
        raise RuntimeError(f"{c.label}: get_frame {wall_frame} failed: {r}")
    return r


def _hex_byte_diffs(label: str, a: str, b: str, max_show: int) -> list[str]:
    """Find byte offsets where two hex strings differ. Returns formatted lines."""
    out = []
    if not a or not b:
        return out
    n = min(len(a), len(b))
    diffs = []
    i = 0
    while i + 1 < n:
        if a[i:i+2] != b[i:i+2]:
            diffs.append(i // 2)
        i += 2
    if not diffs:
        if len(a) != len(b):
            out.append(f"  {label}: length differs (oracle={len(a)//2} native={len(b)//2})")
        return out
    out.append(f"  {label}: {len(diffs)} differing bytes (showing first {min(len(diffs), max_show)})")
    for off in diffs[:max_show]:
        out.append(f"    [{off:04X}] oracle={a[off*2:off*2+2]} native={b[off*2:off*2+2]}")
    return out


def _diff_int_array(label: str, a, b, max_show: int = 16) -> list[str]:
    """Diff two equal-length int arrays. Report first `max_show` mismatches."""
    out = []
    if a is None or b is None or a == b:
        return out
    n = min(len(a), len(b))
    diffs = [i for i in range(n) if a[i] != b[i]]
    if len(a) != len(b):
        out.append(f"  {label}: length differs (oracle={len(a)} native={len(b)})")
    if not diffs:
        return out
    out.append(f"  {label}: {len(diffs)} differing entries (showing first {min(len(diffs), max_show)})")
    for i in diffs[:max_show]:
        out.append(f"    [{i:02X}] oracle=0x{a[i]:04X} native=0x{b[i]:04X}")
    return out


def diff_full(o: dict, n: dict) -> list[str]:
    """Diff two full-frame records. Returns a list of report lines.

    JSON shape per cmd_server.c json_*: m68k has D[8]/A[8]/USP/PC/SR/flags,
    vdp has flat scalars + cram/vsram (int arrays) + vram (hex string),
    wram is a hex string. fm/psg are {len,raw(hex)}.
    """
    out = []

    # --- M68K. The oracle build's clown68000 interpreter populates
    # g_cpu only via the sync hooks; if those are stale, the oracle's
    # m68k snap may be all-zero. Skip in that case (it would generate
    # noise, not signal).
    om = o.get("m68k") or {}
    nm = n.get("m68k") or {}
    o_d = om.get("D") or [0]*8
    o_a = om.get("A") or [0]*8
    n_d = nm.get("D") or [0]*8
    n_a = nm.get("A") or [0]*8
    oracle_m68k_blank = (not any(o_d) and not any(o_a) and not om.get("SR"))
    if not oracle_m68k_blank:
        for i in range(8):
            if o_d[i] != n_d[i]:
                out.append(f"  m68k.D{i}  oracle=0x{o_d[i]:08X} native=0x{n_d[i]:08X}")
        for i in range(8):
            if o_a[i] != n_a[i]:
                out.append(f"  m68k.A{i}  oracle=0x{o_a[i]:08X} native=0x{n_a[i]:08X}")
        for k in ("USP","PC","SR"):
            if om.get(k) != nm.get(k):
                out.append(f"  m68k.{k:<3} oracle=0x{om.get(k,0):08X} native=0x{nm.get(k,0):08X}")
        of = om.get("flags") or {}
        nf = nm.get("flags") or {}
        for k in sorted(set(of) | set(nf)):
            if of.get(k) != nf.get(k):
                out.append(f"  m68k.flags.{k:<5} oracle={of.get(k)} native={nf.get(k)}")

    # --- VDP scalars. (Skip blob fields; they get their own pass.)
    ov = o.get("vdp") or {}
    nv = n.get("vdp") or {}
    for k in sorted(set(ov) | set(nv)):
        if k in ("vram", "cram", "vsram"): continue
        if ov.get(k) != nv.get(k):
            out.append(f"  vdp.{k:<22} oracle={ov.get(k)!r} native={nv.get(k)!r}")

    # --- VDP CRAM (64 entries, 16-bit each — palette colors).
    out.extend(_diff_int_array("vdp.cram",  ov.get("cram"),  nv.get("cram"),  16))
    # --- VDP VSRAM (64 entries, 16-bit each — column scroll).
    out.extend(_diff_int_array("vdp.vsram", ov.get("vsram"), nv.get("vsram"), 16))

    # --- VRAM (64 KB, hex string).
    out.extend(_hex_byte_diffs("vdp.vram", (ov.get("vram") or "").lower(),
                                            (nv.get("vram") or "").lower(), 16))

    # --- WRAM (64 KB, hex string).
    out.extend(_hex_byte_diffs("wram", (o.get("wram") or "").lower(),
                                        (n.get("wram") or "").lower(), 16))

    # --- FM / PSG raw register caches.
    of_, nf_ = (o.get("fm") or {}), (n.get("fm") or {})
    if of_.get("raw") != nf_.get("raw"):
        out.extend(_hex_byte_diffs("fm.raw",  (of_.get("raw") or "").lower(),
                                                (nf_.get("raw") or "").lower(), 8))
    op, npg = (o.get("psg") or {}), (n.get("psg") or {})
    if op.get("raw") != npg.get("raw"):
        out.extend(_hex_byte_diffs("psg.raw", (op.get("raw") or "").lower(),
                                                (npg.get("raw") or "").lower(), 8))

    # --- Z80 scalar regs (ram intentionally omitted from get_frame
    #     unless the caller asks for it; we pass include="all" so it's
    #     present, but z80 RAM diffs would dwarf the report — query
    #     them separately if z80.PC indicates divergence).
    oz = o.get("z80") or {}
    nz = n.get("z80") or {}
    for k in sorted(set(oz) | set(nz)):
        if k == "ram": continue
        if oz.get(k) != nz.get(k):
            out.append(f"  z80.{k:<14} oracle={oz.get(k)!r} native={nz.get(k)!r}")

    return out


# --- Main ----------------------------------------------------------------

def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--oracle-host", default="127.0.0.1")
    ap.add_argument("--oracle-port", type=int, default=4381)
    ap.add_argument("--native-host", default="127.0.0.1")
    ap.add_argument("--native-port", type=int, default=4380)
    ap.add_argument("--wait", type=float, default=12.0,
                    help="seconds to let both binaries fill their rings before querying")
    ap.add_argument("--max-k", type=int, default=600,
                    help="cap the number of state-synced frames to compare")
    args = ap.parse_args()

    print(f"[divergence_diff] connecting oracle={args.oracle_host}:{args.oracle_port} "
          f"native={args.native_host}:{args.native_port}")
    oracle = DebugClient(args.oracle_host, args.oracle_port, "oracle")
    native = DebugClient(args.native_host, args.native_port, "native")
    try:
        oracle.cmd("ping"); native.cmd("ping")

        if args.wait > 0:
            print(f"[divergence_diff] free-running both for {args.wait}s while rings fill")
            time.sleep(args.wait)

        o_lo, o_hi = discover_window(oracle)
        n_lo, n_hi = discover_window(native)
        print(f"[divergence_diff] oracle ring=[{o_lo}..{o_hi}]  native ring=[{n_lo}..{n_hi}]")

        print("[divergence_diff] querying Vint_runcount timeline (wram32[FE0C])")
        o_vint = vint_timeline(oracle, o_lo, o_hi)
        n_vint = vint_timeline(native, n_lo, n_hi)
        common = sorted(set(o_vint) & set(n_vint))
        print(f"[divergence_diff] oracle has {len(o_vint)} distinct Vint_runcount values, "
              f"native has {len(n_vint)}, intersection={len(common)}")
        if not common:
            print("[divergence_diff] no overlapping Vint_runcount — rings don't share state. "
                  "Did one binary stop progressing? Try a longer --wait.")
            return 2

        # Skip vint=0: many startup frames may share that value.
        common = [k for k in common if k > 0][:args.max_k]
        print(f"[divergence_diff] checking {len(common)} state-synced points "
              f"K=[{common[0]}..{common[-1]}]")

        first_div_k = None
        evicted = 0
        for K in common:
            of = o_vint[K]
            nf = n_vint[K]
            o_rec = fetch_full_frame(oracle, of)
            n_rec = fetch_full_frame(native, nf)
            if o_rec is None or n_rec is None:
                evicted += 1
                continue
            diffs = diff_full(o_rec, n_rec)
            if diffs:
                first_div_k = K
                print(f"\n[divergence_diff] FIRST DIVERGENCE at Vint_runcount={K}")
                print(f"  (oracle wall_frame={of}, native wall_frame={nf})")
                for d in diffs[:80]:
                    print(d)
                if len(diffs) > 80:
                    print(f"  ... and {len(diffs)-80} more lines suppressed")
                break
            else:
                # Brief progress trace; not every K to keep output readable.
                if K % 30 == 0 or K == common[-1]:
                    print(f"  K={K:6d}  match  (o.f={of}, n.f={nf})")

        if evicted:
            print(f"[divergence_diff] {evicted}/{len(common)} sync points were evicted "
                  f"from a ring before we could read them (free-run drift)")
        if first_div_k is None:
            print(f"\n[divergence_diff] no state-divergence across {len(common)} sync points.")
            return 0
        return 1

    finally:
        oracle.close(); native.close()


if __name__ == "__main__":
    sys.exit(main())
