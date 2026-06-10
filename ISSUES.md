# SonicTheHedgehog2Recomp — Issues & Known Differences

Tracked issues for the Sonic 2 recomp. Each entry separates what we **KNOW**
(measured / verified) from what we **BELIEVE** (interpretation not yet fully
proven), so a future session can pick up without re-deriving.

---

## BUG-2 — 2-player split-screen is broken (own-backend regression)

**Status:** Open · **Class:** Correctness regression. Noticed 2026-06-05 after
the AGPL-free runtime overhaul (own backend + clownmdemu teardown). 2-player VS
/ split-screen mode on Sonic 2 is broken on the current build.

**BELIEVE:** likely a side effect of the own-backend VDP/scheduler work — split
screen relies on a mid-frame H-int that re-points the VDP (separate scroll +
display per half), and the own-backend H-int delivery / per-scanline VDP state
(or the V-int timing model change) probably doesn't reproduce it correctly.
Not yet root-caused. The clownmdemu build is the reference for correct 2P
behavior; A/B the H-int cadence + VDP register writes per scanline against it.

---

## ENH-1 — Special stages run ~2× too fast (missing frame-lag emulation)

**Status:** Open · **Class:** Feature enhancement / hardware-fidelity gap —
**NOT a correctness defect.** The recompiled logic is correct; the recomp is,
if anything, "more correct than the original hardware." Set down deliberately
(2026-06-05) to focus elsewhere; safe to leave as-is.

**Summary.** The Sonic 2 special stage (half-pipe, `Game_Mode == 0x10`) plays
roughly twice as fast in the recomp as on real hardware. Root cause is that the
recomp does not reproduce the per-frame CPU lag that the original 68000 suffered
in this CPU-heavy mode.

### What we KNOW (measured)

- **Recomp special stage:** display runs at a steady **60.0 fps**, and the
  special-stage movement/state cluster (`$FFFDB4`, `$FFFDC0`, `$FFFDCC`, …)
  updates **~1.0×/displayed-frame** — i.e. the game's main loop completes every
  displayed frame. (`tools/_probe_fps.py`, `tools/recomp_ss_churn.py`.)
- **Reference (Genesis Plus GX v1.7.4, the accuracy oracle):** captured a
  4030-displayed-frame special-stage window; the *same* movement/state cluster
  updates only **~0.5–0.7×/displayed-frame** — its main loop runs at ~30–42 Hz,
  i.e. it is **lagging**. (Captured via `F:\Projects\mdref`; see
  `mdref/analyze_md_trace.py`, `mdref/ref_baseline.py`.)
- That ~1.5–2× gap between recomp (60 Hz loop) and reference (~30 Hz loop) **is**
  the reported "2×". (Spread is just different steering / object counts between
  the two manually-driven runs.)
- The recomp's cooperative V-int model (`runner/glue.c`,
  `glue_handle_interrupt` level 6) only runs the V-int handler **after** the game
  yields at WaitForVBlank. By construction it **cannot drop a frame** — it always
  lets the main loop finish, then fires one V-int. This is the architectural
  reason no lag ever occurs.
- `Vint_runcount` (`$FFFE0C`) is **NOT** a usable speed metric here: in the
  recomp it tracks main-loop completion (so it reads ~1.0/frame), but on real
  hardware / GPGX the V-int fires at 60 Hz regardless of lag. Early readings that
  used it as "timing looks fine" were measuring the wrong thing — the
  movement-variable update rate is the correct lens.
- The engine already has a lever for this: **`--pacing=accurate`**
  (`GLUE_PACING_CYCLE_ACCURATE`) caps the game fiber at the hardware per-frame
  cycle budget (`NTSC_CYCLES_PER_WALL_FRAME = 127856`, `runner/glue.c:82`).
  Enabling it is **NOT a usable fix as-is**: it throttled *everything* to ~half
  pace (normal levels too) and corrupted audio tempo. Confirmed both
  experientially (user) and by the existing code comment at `runner/glue.c:129`
  (cap "halves native's FM-write count (~50% tempo slowdown)"). Notably the
  global slowdown did **not** show up in the fps / Vint_runcount probes — it
  lives in a dimension those don't capture (audio tempo, at minimum).

### What we BELIEVE (interpretation, not fully proven)

- On real hardware the special stage's 3D half-pipe workload exceeds the 68000's
  per-frame cycle budget, so the main loop completes only ~30×/sec (frame lag).
  The recomp runs the identical 68K code as native, far faster than a real
  68000, so it never overruns and completes the loop a full 60×/sec → ~2× the
  hardware's effective motion rate.
- This is a **timing-fidelity gap, not a logic error.** "More correct to the
  code's intent, regression in feel" — the mode was tuned and played at its
  lagged speed.
- A *correct* fix is authentic per-frame lag modeling: cap the loop **only** on
  frames that genuinely overrun the hardware cycle budget, so the special stage
  drops to ~30 Hz while normal play stays 60 Hz. That requires fixing the
  cycle-cost **calibration** that currently makes the accurate-mode cap bite on
  normal frames too (the "bias" the glue.c comment is hunting). Likely coupled to
  the existing "music plays slowly" item (Z80/SMPS cycle accounting).

### NOT yet done (next diagnostic if resumed)

- Measure the **real per-frame 68K cycle cost** (`g_cycle_accumulator` at yield,
  or `g_pace_snap` in the frame ring) for a normal level vs. the special stage.
  Expectation if the theory holds: normal frame **under** ~127,856 cycles, SS
  frame **over** it (ideally ~2×). This both proves the lag theory quantitatively
  and pinpoints where the cycle-cost accounting over-counts.

### Tools / artifacts produced

- `F:\Projects\mdref` — wrapped libretro Genesis core (Genesis Plus GX) used as
  the ground-truth oracle; SDL2 frontend (`frontend.cpp`) dumps full-WRAM
  per-frame changes to `md_trace.jsonl`. **GPGX exposes work RAM word-byte-
  swapped: host offset = 68K offset XOR 1** — `analyze_md_trace.py` corrects for
  this. Mirrors snesrecomp's `F:\Projects\mmxref`.
- `tools/_probe_special_speed.py` (per-mode loop-completion rate),
  `tools/_probe_fps.py` (real-time fps), `tools/_probe_motion_cadence.py`
  (per-frame deltas), `tools/recomp_ss_churn.py` (per-frame WRAM churn vs the
  reference baseline).

---

## BUG-1 — Perpetual slow-walk-right after exiting Special Stage  *(new 2026-06-05, user-reported; not yet investigated)*

**Status:** Open · **Class:** correctness defect (user-confirmed this is a
**Sonic 2** bug, not Sonic 3).

After **exiting the half-pipe Special Stage and returning to 2D**, Sonic is
stuck **slowly walking to the right perpetually** — as if a movement input (or a
velocity / control-lock flag) is latched and never cleared on the
special-stage → 2D transition.

### What we KNOW (user-reported)

- Trigger is the special-stage exit; **possibly only on a win** (got the
  emerald) — confirm whether a loss/timeout also reproduces.
- Symptom is continuous low-speed rightward walk with no controller input.

### What we BELIEVE (hypothesis, unverified)

- Carry-over of the special-stage "auto-run" state into the resumed 2D level:
  a control-lock / forced-movement / input-mirror byte (or Sonic's `x_vel` /
  status bits) set during the half-pipe and not reset by the return-to-level
  init. Look at the special-stage→level re-entry routine for what it fails to
  clear (player object control flags, the `Ctrl_1`/input mirror, or an
  auto-control flag). Likely related conceptually to the special-stage state
  machine already studied for ENH-1.
