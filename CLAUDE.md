# CLAUDE.md — SonicTheHedgehog2Recomp

This repository owns the Sonic 2 implementation and release. The shared
`segagenesisrecomp` framework is a pinned submodule, optionally replaced for
development by `engine-local` or explicit `GENESIS_RECOMP_ROOT`.

→ **`segagenesisrecomp/CLAUDE.md`** — read this first.
→ **`segagenesisrecomp/PRINCIPLES.md`** — the 25 rules.
→ **`segagenesisrecomp/DEBUG.md`** — always-on ring inventory + TCP commands.

## What's in this repo

- `CMakeLists.txt` — shared runner from the engine, per-game files from `game/`.
- `game/` — all Sonic 2 adapters, mods, campaign/characters/menu/video, ROM
  configuration, annotations and pinned `s2disasm/` source disassembly.
- `tests/`, `docs/` — game-specific validation and feature ledgers.
- `tools/` — Sonic-2-specific probes (game_state, quick_status, ring_filter,
  vbla_breakdown, vint_audit, divergence_diff, check_dispatch_misses,
  _pause_both, _2p_*).
- `_build_native.bat`, `_build_oracle.bat` — build wrappers.
- `regen.bat` — regen shortcut.
- `PLAN-divergence-diff.md` — older planning doc.

## Workspace layout

Game-specific implementation belongs HERE, never in the engine repository.
The shared engine must expose reusable opt-in contracts only. Legacy engine
game directories are not precedent for new game-specific framework code.
See `docs/REPOSITORY_OWNERSHIP.md`. Sonic 2 does not consume Sonic 1 code.
Place the owner ROM at `game/sonic2.bin`; generated C belongs under the build
directory. Never commit ROMs, extracted artwork or local Ghidra databases.

## Bring-up status

Sonic 2 has progressed well past the early bring-up notes that used to live
here (which listed an attract-demo black screen, half-rate Vint, no airborne
height, and a level-1 crash). Those were point-in-time observations and are
**stale** — do not treat them as current. Re-verify any specific symptom
against the live build before acting on it. A `v0.1.0-linux` AppImage now
builds and ships.

## Engine commit order (PRINCIPLES.md #20)

1. Commit + push engine changes in the top-level `segagenesisrecomp/` checkout
   first.
2. Bump this repo's engine submodule pointer only after the engine commit is
   available upstream. Commit Sonic 2 implementation changes in this repo.
