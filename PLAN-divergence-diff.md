# Plan: align Sonic 2 debug methodology with the team's applied patterns

Synthesized from surveys of `nesrecomp-release/`, `PokemonStadiumRecomp/`,
and `SuperMarioWorldRecomp{,-oracle}/` against `recomp-template/NES/`.

## What the canon actually does (vs. the template's prescription)

| Concern | Template says | NES applied | SMW applied | PS applied |
|---|---|---|---|---|
| Frame ring | 36k entries | 36k, full state | 6k, full state | Multiple targeted rings (4096 trace / 128 ultra / 4096 RSP) |
| Sync key | "hardware events, not frame" | `ops_count` + `vblank_depth` | **explicit state-sync** ("wait_for_state=K, then step N"), not wall-frame | per-frame counter + VI ticks |
| Tier 1 store ring | 1M entries | shipping | shipping (per-store, byte-addressed) | aspirational/roadmap |
| Tier 2 block trace | 256k | shipping | shipping (`g_recomp_stack[]` snapshot, NOT just `g_last_recomp_func` — that's documented as unreliable) | absent |
| Tier 3 rewind | 64 anchors | shipping | shipping | absent |
| Tier 4 oracle | per-insn trace | partial (Nestopia patch deferred) | shipping (oracle VRAM ring + differ) | Ares bridge thread, no diff harness yet |
| Topology | not specified | one binary, `--native` / `--verify` / `--emulated` | one binary, `ENABLE_ORACLE_BACKEND` CMake flag | one binary, Ares thread |
| Pause+step | "no" (CLAUDE.md ring rule) | `tcp_compare.py` queries rings — no pause | `divergence_diff.py` DOES pause+step (deterministic bisect) + `oracle_block_diff.py` ring-driven (no pause) | Ares step-frame is paused-by-default |

## Lessons from the docs (worth lifting)

- **SMW `GOLDEN_TESTING.md`**: "**cycle-accurate emulator vs recomp frame counters diverge within seconds**" → state-sync, not frame-sync. Recomp advances per-C-call, oracle per-cycle. After ~2s the frame numbers don't mean the same thing.
- **SMW `HANDOFF_VRAM_DIFFER.md`**: "**always-on rings, query backward** — don't arm-then-attach." Both rings record continuously; the differ walks them backward to find first-divergence.
- **SMW `DRY_REFACTOR.md`**: width-correctness mistakes land repeatedly across instructions — DRY them to one module, not at each emit site.
- **NES `reverse_debug.h`**: poll-based watchpoints retired in favor of synchronous Tier-2.5 watches that fire inside the store hook.
- **PS `extras.c`**: a 4096-entry trace ring is overrun by audio in <2s — use non-evicting per-function counters for rare events.
- **PS `main.cpp`**: "loud-abort, never silent stub" — pairs with `recomp-template/NES/PRINCIPLES.md` rule #12.
- **All three**: no printf debugging — extend TCP.

## What we have today (Sonic 2 specifically)

We already have most of this stack, just not used correctly:

- **Frame ring**: `s_frame_history[600]` in `cmd_server.c`, FrameRecord captures `cram[64]`, full WRAM, VDP state, M68K state, OAM, CGRAM. Good — but only 600 frames (10s) and divergence_diff ignores it (queries live state instead).
- **Tier 2 block trace**: shipping via `rdb_on_block(addr)` in generated C, populated when `--reverse-debug` regen + `SONIC_REVERSE_DEBUG=1` build.
- **Tier 1 store ring**: NOT shipping. We have `g_mem_write_trace_fn` + `mem_write_log_callback` (CSV-style, file-backed, address-filtered) — close to NES's pattern but file-only, not ring-buffered + TCP-queryable.
- **Symbol resolution**: `crash_report.c` loads `annotations_from_disasm.csv` at startup (NES/SMW embed names in generated C — either works).
- **`vint_runcount`** at `$FFFE0C` is captured in `SonicGameData` (Sonic 1's per-frame tail). Sonic 2's spec doesn't populate it — that's the broken sync key.

## What we're doing wrong

1. **Pause+step + frame-number sync** in `divergence_diff.py`. SMW says state-sync. Switch to `internal_frame_ctr`-based.
2. **Live-state queries** (`read_cram`, `vdp_state`) in the diff. Should query the FRAME RING (`get_frame N`) which captures CRAM and VDP state at a known frame.
3. **printf-based `[FPACE]` and `--vdp-ctrl-log`**. Both violate "no printf debugging." Should be FrameRecord fields (FPACE counters) and a TCP-queryable ring (vdp control writes).
4. **Sonic 2 has no `fill_frame_record`** — its FrameRecord game_data tail is zeros. So `internal_frame_ctr` isn't recorded, can't be used for sync.
5. **No Tier 1 store ring** for arbitrary RAM. We can only file-log writes to fixed addresses — not query-by-frame retroactively.
6. **`dispatch_misses.log` ignored.** RULE 0a says check after every run. We never have.

## The plan — bring Sonic 2 onto the canon stack

Order is dependency-first. Each step is a small commit.

### Step 1 — Sonic 2 `fill_frame_record` populates the sync key
File: `sonicthehedgehog2/sonic2_spec.c`. Define a `Sonic2GameData` struct (same shape as Sonic 1's `SonicGameData`) with `internal_frame_ctr` = `m68k_read32($FFFE0C)`. Wire to `g_game_spec.fill_frame_record`. Both native and oracle FrameRecord game_data tails now carry the sync key.

### Step 2 — `divergence_diff.py` switch to ring-query + state-sync
- Remove pause/run_frames flow.
- Both binaries free-run.
- Query each side's `frame_range` to find a frame where `vint_runcount = K` matches. That's the sync point.
- `get_frame K` from each, full FrameRecord JSON, byte-by-byte diff (CRAM, VRAM, WRAM, VDP regs, palette buffer at $FFFB00).
- Walk backward through K-1, K-2, … until they match. The first K where they DIFFER is first-divergence.
- Report: which subsystem (DIFF_FIELD_VRAM, DIFF_FIELD_CRAM, etc. — already in frame_record.h enum), addr, expected (oracle), actual (native).

### Step 3 — Promote `[FPACE]` printf into a FrameRecord field
Drop the per-frame fprintf. Add `bus_accesses_delta`, `native_insns_delta`, `audio_cyc` fields to `FrameRecord`. Populate from `s_bus_ring_total`, `g_native_insn_count`, `g_audio_cycle_counter` deltas. Query via existing `frame_timeseries`.

### Step 4 — Upgrade Tier 1 store ring (NES-style, TCP-queryable)
File: a new `runner/store_ring.c`, hooked into `g_mem_write_trace_fn` (already firing on every clownmdemu bus write). Ring entry: `(frame, addr, val, width, vint_runcount, func_addr, caller_pc)`. 1M entries, 8 address-range filters. TCP commands: `rdb_range {lo,hi}`, `rdb_dump {start,max}`, `rdb_count`. Subsumes the file-based `--mem-write-log`.

### Step 5 — Retire `--vdp-ctrl-log` printf
Once Tier 1 ring exists, the vdp control sequence is just `rdb_range $C00000 $C00007`. Delete the printf path; keep the ring.

### Step 6 — `dispatch_misses.log` check in the runner exit path
On `--max-frames` exit (and on watchdog), append the active dispatch_misses to a stable file path next to the binary. Add a one-liner Python helper `tools/check_dispatch_misses.py` that reads it and proposes `extra_func` lines for `game.cfg`. Make this part of the standard "after every run" workflow per CLAUDE.md.

### Step 7 — Apply state-sync workflow to find Sonic 2's white-bg bug
- Run native + oracle to frame ~600 with no input (let attract demo run).
- `divergence_diff.py` finds first `vint_runcount` where CRAM diverges.
- Arm `rdb_range` on the divergent palette buffer addresses.
- Re-run. `rdb_dump` shows the writer.
- The recompiled function whose store produced the wrong value is the bug. Cross-reference disasm. Fix in recompiler / runner / cfg, never in generated C.

### What we explicitly do NOT do

- Add `--start-paused` (violates ring philosophy; SMW pause+step is a separate bisect tool, not the primary path)
- Per-frame printf telemetry (rule #9)
- Random RAM address probing in divergence_diff (let it diff the entire FrameRecord)
- Write fixes into generated/*.c (template rule)

### Effort estimate

Steps 1-3 are mechanical, ~30 min each. Step 4 is medium (~1-2 hr — modeling on existing nesrecomp Tier 1). Step 5 is trivial cleanup. Step 6 is ~30 min. Step 7 is the actual debugging session that uses all of the above; expected to take one focused hour and resolve the white-bg + black-demo bugs.
