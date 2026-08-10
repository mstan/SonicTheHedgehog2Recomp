# PLAN — Widescreen: stale Plane-B margins on 16:9 arm (CPZ "diagonal corruption")

> **OUTCOME (2026-07-27):** implemented, with Phase 0 revising the recipe.
> Phase 0 confirmed the stale-margin mechanism empirically (plane-B dumps,
> hscroll bands, mid-level arm via new `ws_set` TCP command) and REJECTED
> option A's `call_after`→`DrawInitialBG` target: $E300 fills all rows from
> Camera_BG only, so a mid-level call would repaint diverged CPZ strips at the
> wrong offset. Shipped instead: (B) arm-from-load as a pure game.toml change
> (drop `level_started_addr`, add 0x88/0x8C to `eligible_modes`), plus eight
> new sites widening the BG vertical-scroll row draws (the actual source of
> "random missing sections" during play). No recompiler changes were needed.
> Mid-level overlay toggles keep the documented one-scroll heal. Details in
> `segagenesisrecomp/WIDESCREEN_ISSUES.md`.

Target bug: `segagenesisrecomp/WIDESCREEN_ISSUES.md` → "Sonic 2 — CPZ diagonal
corruption (random missing sections), minor". Constraint throughout: at
`g_ws_margin == 0` everything must stay byte-identical (authentic 4:3, boot-smoke
baseline unchanged — PRINCIPLES #23).

## What the code actually does (verified 2026-07-27 against the generated C + disasm labels)

Addresses are ROM addresses; names from `sonic2.disasm_labels.toml`.

- **Arm trigger (engine)**: `runner/main.c` ~line 520 — on the frame extra_px goes
  0→N it writes 1 to `Screen_redraw_flag` ($FFF72C). Eligibility gate at ~line 479
  requires `Level_started_flag` ($FFF711) != 0, so the margin is forced 0 during
  the whole level-load/title-card sequence.
- **Sole flag reader**: $DABE inside `LoadTilesAsYouMove` ($DA5C) → falls into
  `Draw_All` ($DACE): draws full-width FG rows via
  `CalculateVRAMAddressOfBlockForPlayer1` ($E286) + `DrawBlockRow_CustomWidth`
  ($DF8A), with d6 widened by the existing `call_widen` site @ $DADE. It then
  clears the scroll-flag bytes (incl. the BG ones, ~$DAEE, $FFEEA0 area) and
  returns — **Plane B is never drawn**, and any pending BG column draws are
  silently dropped.
- **Initial BG fill**: `Level_TtlCard` ($4114) at $414A → `JmpTo` trampoline
  $4F5E → `DrawInitialBG` ($E300). Zone dispatch inside:
  - zone $C (CNZ) → alt entry $E338 (d4 = 0 instead of -16);
  - zone $B (MCZ) → `DrawInitialBG_LoadWholeBackground_512x512` ($E396);
  - default — **including CPZ ($D)** — draws 16 rows × 32 blocks (d6 = $1F) =
    the **full 512px plane width**, rows from y = -16, relative to Camera_BG
    ($FFEE08).

## Corrected root cause

`WIDESCREEN_ISSUES.md` attributes the bug to the BG margins never being filled
("fill only as you scroll"). That's not quite it: `DrawInitialBG` already fills
the *entire* 512px-wide plane at load, margins included, even at margin 0.

The real gap is **sectional maintenance**. CPZ's BG is horizontal strips with
independent scroll rates (`SwScrl_CPZ` $D27C, extra BG cameras — $FFEE0C/$FFEE18
family), maintained per-strip by `Draw_BG2` ($DC92) / `Draw_BG3` ($DD82) /
`Draw_BG3_CPZ` ($DE12) via `DrawBlockColumn_Advanced` ($DE86). Once the strip
cameras diverge from the load-time uniform fill, each strip is only re-patched
across the *4:3* window (margin was 0 pre-arm). The margins keep the uniform
load-time content, which is at the wrong offset per strip → the "diagonal"
shear, prominent in CPZ because its strip parallax is prominent. Nothing
re-syncs the strips when 16:9 arms; the FG-only `Draw_All` doesn't touch them.

This also predicts the doc's observation that scrolling heals it: the widened
per-strip column draws (existing sites) repaint margins strip-correctly as each
seam advances.

## Phase 0 — empirical confirmation (no code changes)

1. Build with trace on (default `GEN_ENABLE_TRACE=ON`), boot into CPZ 1.
2. Toggle 16:9 at the arm moment and dump Plane B's name table + the BG camera
   family ($FFEE08–$FFEE1C) via the TCP probe (see `DEBUG.md` ring inventory;
   add a small plane-dump command if none exists yet).
3. Confirm: stale margin cells sit in rows managed by the secondary BG cameras,
   and their content matches the load-time uniform fill offset, not the strip
   offset. Also check MCZ ($E396 variant) and CNZ (alt entry) for the same class
   of issue at their margins.
4. Empirically answer the one open design question: does re-running
   `DrawInitialBG` mid-level produce *correct* CPZ strip content (i.e., do the
   strip fills derive from current strip cameras), or uniform content that the
   seam logic then has to heal? Easiest test: force a call from the debugger
   path / a throwaway hack build, observe the plane dump.

## Phase 1 — the fix

Two complementary pieces; Phase 0 picks the exact recipe for (A).

### A. Arm-time BG catch-up (covers mid-level toggles from the runtime overlay)

Add a recompiler injection on the `Draw_All` path — a new `[[widescreen_site]]`
kind (working name `call_after`): after the FG rows, when `g_ws_margin > 0`,
also run the Plane-B refill (target chosen per Phase 0: `DrawInitialBG` $E300
and/or a forced pass of the `Draw_BG2/3` strip logic). Emitted code must
compile to nothing observable at margin 0.

Files:
- `recompiler/src/game_config.{h,c}` — parse the new kind (mirror `call_widen`).
- `recompiler/src/code_generator.{h,c}` — emit (mirror `ws_site_for_kind`
  handling; the call machinery exists — `recomp_call_func` + JSR push/pop).
- `segagenesisrecomp/sonicthehedgehog2/game.toml` — the site entry.
- `WIDESCREEN_ISSUES.md` — status update; site-vocabulary section gains the new
  kind.

Known risk: if Phase 0 shows `DrawInitialBG` mid-level paints uniform (not
per-strip) content, the naive call would transiently corrupt the *visible* BG
for a few frames. Then either replicate the per-strip fill (set all BG scroll
flags across the widened window before the `Draw_BG*` calls), or restrict A to
non-strip zones and rely on B for CPZ.

### B. Arm from level load (fixes the common case at the source)

Relax the engine's `level_started` gate (`runner/main.c` ~479) so the margin is
already nonzero while `Level_TtlCard` runs `DrawInitialBG` and the strip
maintenance starts. Then the margins are strip-correct from frame 0 and stay
maintained; there is no arm transition mid-level at all for the
enable-in-launcher case. First check `git log/blame` for why the gate exists
(likely title-card cull/visual protection) and re-verify title cards, special
stage entry, and 2P lockout ($FFFFD8 gate stays).

Recommendation: do B for the level-start path; keep A (or a documented
one-scroll heal) for live toggles from the runtime overlay.

### Bonus (same area, cheap)

`Draw_All` drops pending BG scroll flags (clears them without drawing). On the
arm frame this can eat one queued BG column. If A lands, its BG pass subsumes
this; otherwise note it in WIDESCREEN_ISSUES.md.

## Phase 2 — verification

- 4:3 regression: boot-smoke baseline byte-identical; a divergence-diff run
  (`tools/divergence_diff`) against a pre-change build at margin 0.
- Widescreen visual pass (user-confirmed per PRINCIPLES #25): CPZ1/CPZ2 arm at
  level start; arm mid-level via runtime overlay; scroll left/right/up/down
  across strip seams; MCZ (whole-bg variant), CNZ (alt entry), EHZ/HTZ/ARZ spot
  checks; 2P mode unaffected (widescreen stays gated off).
- `dispatch_misses.toml` has an empty `functions.extra` array after each run.
- Engine-first commit order (PRINCIPLES #20): recompiler/engine changes land in
  `segagenesisrecomp` before the game repos consume them; Sonic 1 / Sonic 3
  rebuild + smoke (shared code path — `Draw_All` analogs differ per game, so
  the new site kind must stay data-driven via game.toml, no shared-runner
  literals — #21).

## Effort estimate

- Phase 0: half a day (mostly probe wiring if no plane-dump command exists).
- B alone: small — one gate change + regression pass.
- A: the recompiler site kind is the real work (parse + emit + docs), a day-ish
  including Sonic 1/3 regression builds; the CPZ recipe on top depends on
  Phase 0's answer.
