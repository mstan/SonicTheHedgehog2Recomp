# CLAUDE.md — SonicTheHedgehog2Recomp

This is the Sonic 2 release repo. The canonical session brief lives in the
shared `segagenesisrecomp` engine — a single checkout at the workspace root,
reached here through a local directory junction (`./segagenesisrecomp ->
../segagenesisrecomp`):

→ **`segagenesisrecomp/CLAUDE.md`** — read this first.
→ **`segagenesisrecomp/PRINCIPLES.md`** — the 25 rules.
→ **`segagenesisrecomp/DEBUG.md`** — always-on ring inventory + TCP commands.

## What's in this repo

- `CMakeLists.txt` — sources runner + per-game files from the engine
  via the local `segagenesisrecomp/` junction.
- `tools/` — Sonic-2-specific probes (game_state, quick_status, ring_filter,
  vbla_breakdown, vint_audit, divergence_diff, check_dispatch_misses,
  _pause_both, _2p_*).
- `_build_native.bat`, `_build_oracle.bat` — build wrappers.
- `regen.bat` — regen shortcut.
- `PLAN-divergence-diff.md` — older planning doc.

## Workspace layout

The `segagenesisrecomp` engine is a single canonical checkout at the workspace
root (`../segagenesisrecomp`), shared by every game repo. This repo reaches it
through a local directory junction (`segagenesisrecomp -> ../segagenesisrecomp`,
gitignored), so no game repo owns the engine and there is no submodule pointer
to keep in sync. Sonic 2 does not consume any files from the Sonic 1 repo.

Sonic 2's per-game files live at `segagenesisrecomp/sonicthehedgehog2/`
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

## Engine commit order (PRINCIPLES.md #20)

1. Commit + push engine changes in the top-level `segagenesisrecomp/` checkout
   first.
2. This repo commits independently — but if your Sonic 2 work depends on engine
   changes, ensure they've landed upstream first (all game repos read the same
   checkout through the junction, so a local engine commit is visible to every
   repo immediately; push it before relying on it elsewhere).
