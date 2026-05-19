# CLAUDE.md — SonicTheHedgehog2Recomp

This is the Sonic 2 release repo. The canonical session brief lives in
the shared submodule (checked out under Sonic 1's release repo):

→ **`../SonicTheHedgehogRecomp/segagenesisrecomp/CLAUDE.md`** — read this first.
→ **`../SonicTheHedgehogRecomp/segagenesisrecomp/PRINCIPLES.md`** — the 25 rules.
→ **`../SonicTheHedgehogRecomp/segagenesisrecomp/DEBUG.md`** — always-on ring inventory + TCP commands.

## What's in this repo

- `CMakeLists.txt` — sources runner + per-game files from the submodule
  via `../SonicTheHedgehogRecomp/segagenesisrecomp/`.
- `tools/` — Sonic-2-specific probes (game_state, quick_status, ring_filter,
  vbla_breakdown, vint_audit, divergence_diff, check_dispatch_misses,
  _pause_both, _2p_*).
- `_build_native.bat`, `_build_oracle.bat` — build wrappers.
- `regen.bat` — regen shortcut.
- `PLAN-divergence-diff.md` — older planning doc.

## Sibling-repo dependency

Sonic 1's release repo must be checked out as a sibling so we can reach
the submodule at `../SonicTheHedgehogRecomp/segagenesisrecomp/`. Sonic 2
does not consume any files from Sonic 1 directly — only the submodule.

Sonic 2's per-game files live at
`../SonicTheHedgehogRecomp/segagenesisrecomp/sonicthehedgehog2/`
(`sonic2_spec.c`, `sonic2_hybrid_table.c`).

## Bring-up status

Sonic 2 is mid-bring-up. Visible bugs at the time of this writing:
- Attract-demo black screen (primary).
- Half-rate Vint in level/demo mode (~50% vs oracle's ~88%).
- Jumping in place / no airborne height.
- Fish-region garble + crash in level 1.

These are downstream of waves 0A → 3 of the active improvement plan.
Per PRINCIPLES.md sequencing: do not detour into these bugs until 0A +
0B + 1 are green.

## Submodule commit order (PRINCIPLES.md #20)

1. Commit `../SonicTheHedgehogRecomp/segagenesisrecomp/` (the submodule)
   first.
2. Bump the submodule pointer in `../SonicTheHedgehogRecomp/` second.
3. This repo bumps independently — but if your Sonic 2 work depends on
   submodule changes, ensure they've landed upstream before this repo
   commits.
