"""Report runtime dispatch evidence not already present in game configuration.

The runner writes dispatch_misses.toml as a directly parseable GameConfig
discovery file. Addresses remain evidence, not authority: verify every result
against the disassembly before adding it to game.toml or a discovery file.
"""

from __future__ import annotations

import argparse
import os
import re
import sys
import tomllib


REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def _engine_root() -> str:
    for name in ("engine-local", "segagenesisrecomp"):
        candidate = os.path.join(REPO_ROOT, name)
        if os.path.isdir(candidate):
            return candidate
    return os.path.join(REPO_ROOT, "segagenesisrecomp")


DEFAULT_EVIDENCE = os.path.join(
    REPO_ROOT, "build", "Release", "dispatch_misses.toml"
)
DEFAULT_GAME = os.path.join(_engine_root(), "sonicthehedgehog2", "game.toml")


def _native_path(path: str) -> str:
    """Accept Windows drive paths when invoked from an MSYS Python."""
    if os.name != "nt" and re.match(r"^[A-Za-z]:[\\/]", path):
        drive = path[0].lower()
        tail = path[2:].replace("\\", "/")
        return f"/{drive}{tail}"
    return path


def _load_toml(path: str) -> dict:
    with open(path, "rb") as source:
        return tomllib.load(source)


def read_function_addrs(path: str, visited: set[str] | None = None) -> set[int]:
    """Read [functions].extra recursively through game.discovery_files."""
    resolved = os.path.realpath(_native_path(path))
    if visited is None:
        visited = set()
    if resolved in visited or not os.path.isfile(resolved):
        return set()
    visited.add(resolved)

    data = _load_toml(resolved)
    functions = data.get("functions", {})
    addrs = {int(value) for value in functions.get("extra", [])}

    base = os.path.dirname(resolved)
    for relative in data.get("game", {}).get("discovery_files", []):
        child = relative if os.path.isabs(relative) else os.path.join(base, relative)
        addrs.update(read_function_addrs(child, visited))
    return addrs


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--log",
        default=DEFAULT_EVIDENCE,
        help=f"runtime evidence TOML (default: {DEFAULT_EVIDENCE})",
    )
    parser.add_argument(
        "--cfg",
        default=DEFAULT_GAME,
        help=f"active game.toml (default: {DEFAULT_GAME})",
    )
    args = parser.parse_args()

    args.log = _native_path(args.log)
    args.cfg = _native_path(args.cfg)

    if not os.path.isfile(args.log):
        print(f"[dispatch_misses] evidence not found: {args.log}")
        print("  Run the binary once; it writes dispatch_misses.toml at startup/shutdown.")
        return 0
    if not os.path.isfile(args.cfg):
        print(f"[dispatch_misses] game config not found: {args.cfg}", file=sys.stderr)
        return 2

    evidence_data = _load_toml(args.log)
    if evidence_data.get("format_version") != 1:
        print("[dispatch_misses] unsupported or missing format_version", file=sys.stderr)
        return 2
    if evidence_data.get("evidence_kind") != "dispatch_miss":
        print("[dispatch_misses] evidence_kind is not dispatch_miss", file=sys.stderr)
        return 2

    evidence_addrs = read_function_addrs(args.log)
    configured_addrs = read_function_addrs(args.cfg)
    candidates = sorted(evidence_addrs - configured_addrs)

    print(f"[dispatch_misses] evidence: {args.log}")
    print(f"  evidence addresses : {len(evidence_addrs)}")
    print(f"[dispatch_misses] config  : {args.cfg}")
    print(f"  configured extras  : {len(configured_addrs)}")
    print(f"  new candidates     : {len(candidates)}")

    if not candidates:
        print("[dispatch_misses] no new candidates.")
        return 0

    print("\n# Verify these against disassembly before adding them:")
    print("[functions]")
    print("extra = [")
    for index, address in enumerate(candidates):
        comma = "," if index + 1 < len(candidates) else ""
        print(f"  0x{address:06X}{comma}")
    print("]")
    return 1


if __name__ == "__main__":
    sys.exit(main())
