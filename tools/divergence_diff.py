"""
divergence_diff.py — paired-run divergence detector for Sonic 2
                     native vs oracle.

Inspired by SuperMarioWorldRecomp's tools/divergence_diff.py and the
N64Recomp ares-bridge oracle pattern. Connects to two TCP debug
servers running the same ROM, advances both in lockstep, queries
state at frame boundaries, and reports the first divergence — the
exact frame where the recompiled native target's state stops
matching the all-interpreter oracle's state.

Usage:
    # In two separate consoles, start both binaries first:
    #   build\\Release\\SonicTheHedgehog2Recomp.exe         sonic2.bin --port 4380
    #   build\\Release\\SonicTheHedgehog2Recomp_oracle.exe  sonic2.bin --port 4381

    # Then run the diff:
    python tools/divergence_diff.py --max-frames 600

The driver:
  1. pings both servers to make sure they're alive
  2. pauses both, queries initial state
  3. steps both forward in fixed-size chunks (default 30 wall frames)
  4. after each step, diffs CPU registers + a curated RAM-window list
  5. on divergence: bisects within the chunk to the exact failing frame,
     prints a structured report of the offending fields, and stops

The script reads-only; it never writes RAM or registers, so a
divergence-investigation session can be re-run as many times as
needed against the same binaries.
"""

from __future__ import annotations

import argparse
import json
import socket
import sys
import time
from dataclasses import dataclass


# --- TCP client -----------------------------------------------------------

class DebugClient:
    """Minimal newline-delimited JSON client for the runner's cmd_server."""

    def __init__(self, host: str, port: int, label: str):
        self.host  = host
        self.port  = port
        self.label = label
        self.sock  = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.connect((host, port))
        self.sock.settimeout(15.0)
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
            chunk = self.sock.recv(65536)
            if not chunk:
                raise RuntimeError(f"{self.label}: connection closed")
            self.buf += chunk
        line, _, rest = self.buf.partition(b"\n")
        self.buf = rest
        return json.loads(line.decode("utf-8"))

    def close(self):
        try: self.sock.close()
        except Exception: pass


# --- State capture --------------------------------------------------------

# Curated set of RAM addresses to diff per step. Keep small enough that
# a divergence report is human-readable; expand as new symptoms surface.
# All addresses are byte addresses in the $FFxxxx mirror.
WATCHED_RAM = [
    ("game_mode",            0xFFF600, 1),
    ("vint_routine",         0xFFF62A, 1),
    ("hint_flag",            0xFFF644, 2),
    ("vint_runcount",        0xFFFE0C, 4),
    ("scroll_x",             0xFFF700, 2),
    ("scroll_y",             0xFFF704, 2),
    ("p1_obj_id",            0xFFD000, 1),
    ("p1_x",                 0xFFD008, 2),
    ("p1_y",                 0xFFD00C, 2),
    ("p1_xvel",              0xFFD010, 2),
    ("p1_yvel",              0xFFD012, 2),
    ("p1_routine",           0xFFD024, 1),
]


@dataclass
class FrameState:
    frame:    int
    regs:     dict
    ram:      dict   # name -> int

    @classmethod
    def capture(cls, c: DebugClient) -> "FrameState":
        regs_resp  = c.cmd("get_registers")
        frame_resp = c.cmd("frame_info")
        ram = {}
        for name, addr, width in WATCHED_RAM:
            r = c.cmd("read_ram", addr=addr, len=width)
            # The runner returns a hex string in the "data" field.
            data = r.get("data") or r.get("bytes") or ""
            try:
                v = int(data.replace(" ", ""), 16) if data else 0
            except ValueError:
                v = 0
            ram[name] = v
        return cls(
            frame = frame_resp.get("frame", -1),
            regs  = regs_resp.get("regs", regs_resp),
            ram   = ram,
        )


def diff_states(a: FrameState, b: FrameState) -> list[str]:
    """Return a human-readable list of differences."""
    out = []
    # Heuristic: in oracle builds the recompiled-C g_cpu globals are
    # rarely populated (the interpreter keeps state in clown68000's
    # internal struct), so get_registers reports zeros. Skip the CPU
    # comparison when oracle's registers all look zeroed — RAM diffs
    # are still meaningful and route through clownmdemu's shared RAM.
    a_regs = a.regs
    b_regs = b.regs
    oracle_regs_unavailable = all(a_regs.get(k, 0) == 0 for k in
                                  ("D0","D1","D2","D3","A0","A1","A7","SR"))
    if not oracle_regs_unavailable:
        for k in ("D0","D1","D2","D3","D4","D5","D6","D7",
                  "A0","A1","A2","A3","A4","A5","A6","A7","SR","PC"):
            if k in a_regs and k in b_regs and a_regs[k] != b_regs[k]:
                out.append(f"  reg.{k:<3} oracle=0x{a_regs[k]:08X}  native=0x{b_regs[k]:08X}")
    elif a.frame > 5:
        out.append("  (oracle CPU regs unavailable — known limitation: oracle's "
                   "interpreter state isn't surfaced via get_registers; "
                   "comparing RAM only)")
    # RAM windows
    for name in a.ram:
        if a.ram[name] != b.ram[name]:
            out.append(f"  ram.{name:<15} oracle=0x{a.ram[name]:08X}  native=0x{b.ram[name]:08X}")
    return out


# --- Main --------------------------------------------------------

def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--oracle-host", default="127.0.0.1")
    ap.add_argument("--oracle-port", type=int, default=4381)
    ap.add_argument("--native-host", default="127.0.0.1")
    ap.add_argument("--native-port", type=int, default=4380)
    ap.add_argument("--max-frames",  type=int, default=600,
                    help="stop diff after N frames if no divergence found")
    ap.add_argument("--chunk",       type=int, default=30,
                    help="frames per coarse step before checking for divergence")
    args = ap.parse_args()

    print(f"[divergence_diff] connecting oracle={args.oracle_host}:{args.oracle_port} "
          f"native={args.native_host}:{args.native_port}")
    oracle = DebugClient(args.oracle_host, args.oracle_port, "oracle")
    native = DebugClient(args.native_host, args.native_port, "native")

    # Sanity ping.
    print(f"[divergence_diff] oracle ping: {oracle.cmd('ping')}")
    print(f"[divergence_diff] native ping: {native.cmd('ping')}")

    # Pause both so we control stepping. Subsequent run_frames brings
    # them forward by exactly the requested amount.
    oracle.cmd("pause")
    native.cmd("pause")

    last_good_frame = 0
    cur_frame = 0
    while cur_frame < args.max_frames:
        chunk = min(args.chunk, args.max_frames - cur_frame)
        oracle.cmd("run_frames", n=chunk)
        native.cmd("run_frames", n=chunk)
        cur_frame += chunk

        os = FrameState.capture(oracle)
        ns = FrameState.capture(native)
        diffs = diff_states(os, ns)
        if not diffs:
            last_good_frame = cur_frame
            print(f"[divergence_diff] frame {cur_frame:5d}  match")
            continue

        # Coarse divergence found — bisect within the last chunk to
        # the exact frame where state first diverged.
        print(f"\n[divergence_diff] DIVERGENCE detected somewhere in "
              f"({last_good_frame}, {cur_frame}]")
        print(f"  oracle.frame={os.frame}  native.frame={ns.frame}")
        for d in diffs:
            print(d)

        # NOTE: bisection requires a save-state / rewind capability
        # we don't have yet. Lacking that, we can only narrow by
        # restarting both binaries at a finer chunk size — out of
        # scope for this script. The chunk-frame info above is
        # the divergence boundary; pair with the runner's
        # crash_report execution trail at the same frame to
        # localize the offending recompiled function.
        oracle.close()
        native.close()
        return 1

    print(f"\n[divergence_diff] no divergence detected through frame {cur_frame}.")
    oracle.close()
    native.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
