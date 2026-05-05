"""
check_dispatch_misses.py — read dispatch_misses.log next to the binary
and propose extra_func lines for game.cfg.

Per PRINCIPLES.md rule 13a + the project's CLAUDE.md "after every run"
workflow, the recompiled binary writes dispatch_misses.log on shutdown
listing every address `call_by_address()` couldn't resolve. Each line
is already in the cfg-compatible form `extra_func 0xADDR`. This script:

  1. Reads the log next to the most recent build (defaults to the
     Release dir).
  2. Compares against the active game.cfg's existing extra_func set.
  3. Prints the new (unseen) addresses, one per line, ready to append.
  4. With --append, writes them directly into game.cfg under a
     dated comment block.

Usage:
    python tools/check_dispatch_misses.py
    python tools/check_dispatch_misses.py --log build/Release/dispatch_misses.log
    python tools/check_dispatch_misses.py --append   # write into game.cfg
"""

from __future__ import annotations

import argparse
import datetime
import os
import re
import sys


REPO_ROOT  = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT_LOG = os.path.join(REPO_ROOT, "build", "Release", "dispatch_misses.log")
DEFAULT_CFG = os.path.normpath(os.path.join(
    REPO_ROOT, "..", "SonicTheHedgehogRecomp", "segagenesisrecomp",
    "sonicthehedgehog2", "game.cfg"))

# Accepts both `extra_func 0x000206` (runner output) and `extra_func 000206`
# (existing game.cfg entries). The trailing comment after the addr is
# allowed.
ADDR_RE = re.compile(r"^extra_func\s+(?:0[xX])?([0-9A-Fa-f]+)\b")
FILE_RE = re.compile(r"^extra_func_file\s+(\S+)")
# disasm_seeds / disasm_subs / disasm_jumptables files: one address per line,
# typically `00xxxx` hex. Lenient parser — strip comments, accept leading
# `0x`, accept the `address  name` two-column form.
SEED_LINE_RE = re.compile(r"^\s*(?:0[xX])?([0-9A-Fa-f]{4,8})\b")


def read_addrs(path: str) -> set[int]:
    """Pull every address from an extra_func / extra_func_file chain."""
    addrs: set[int] = set()
    if not os.path.exists(path):
        return addrs
    base = os.path.dirname(os.path.abspath(path))
    with open(path, "r", encoding="utf-8", errors="ignore") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            m = ADDR_RE.match(line)
            if m:
                addrs.add(int(m.group(1), 16))
                continue
            mf = FILE_RE.match(line)
            if mf:
                rel = mf.group(1)
                seed_path = rel if os.path.isabs(rel) else os.path.join(base, rel)
                addrs |= _read_seed_file(seed_path)
    return addrs


def _read_seed_file(path: str) -> set[int]:
    """Seed/sub/jumptable files are themselves cfg-snippets — same syntax."""
    addrs: set[int] = set()
    if not os.path.exists(path):
        return addrs
    with open(path, "r", encoding="utf-8", errors="ignore") as f:
        for line in f:
            line = line.split("#", 1)[0].strip()
            if not line:
                continue
            m = ADDR_RE.match(line)
            if m:
                addrs.add(int(m.group(1), 16))
                continue
            m = SEED_LINE_RE.match(line)
            if m:
                addrs.add(int(m.group(1), 16))
    return addrs


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--log", default=DEFAULT_LOG,
                    help=f"path to dispatch_misses.log (default: {DEFAULT_LOG})")
    ap.add_argument("--cfg", default=DEFAULT_CFG,
                    help=f"path to game.cfg (default: {DEFAULT_CFG})")
    ap.add_argument("--append", action="store_true",
                    help="append new addresses to game.cfg under a dated block")
    args = ap.parse_args()

    if not os.path.exists(args.log):
        print(f"[dispatch_misses] log not found: {args.log}")
        print("  Run the binary at least once (it writes the log on shutdown).")
        return 0

    log_addrs = read_addrs(args.log)
    cfg_addrs = read_addrs(args.cfg)
    new_addrs = sorted(log_addrs - cfg_addrs)

    print(f"[dispatch_misses] log    : {args.log}")
    print(f"  log addresses     : {len(log_addrs)}")
    print(f"[dispatch_misses] cfg    : {args.cfg}")
    print(f"  cfg extra_func    : {len(cfg_addrs)}")
    print(f"  new (in log only) : {len(new_addrs)}")

    if not new_addrs:
        print("[dispatch_misses] no new addresses — nothing to add.")
        return 0

    print("\n# Add to game.cfg:")
    for a in new_addrs:
        print(f"extra_func 0x{a:06X}")

    if args.append:
        if not os.path.exists(args.cfg):
            print(f"\n[dispatch_misses] --append failed: cfg not found at {args.cfg}")
            return 2
        stamp = datetime.date.today().isoformat()
        with open(args.cfg, "a", encoding="utf-8") as f:
            f.write(f"\n# Added by check_dispatch_misses.py on {stamp} "
                    f"({len(new_addrs)} addresses)\n")
            for a in new_addrs:
                f.write(f"extra_func 0x{a:06X}\n")
        print(f"\n[dispatch_misses] appended {len(new_addrs)} entries to {args.cfg}")
        print("  -> regenerate (regen.bat) and rebuild before next run.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
