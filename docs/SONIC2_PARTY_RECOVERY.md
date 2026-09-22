# Party follower and sprite recovery (beads-5dyp.5)

The v0.6.0 live EHZ1 report reproduced at leader (1969, 758). A read-only
five-second capture showed Amy dying/reinitializing roughly every 64 frames
and Tails snapping back roughly every 184 frames. The host reset whole actors
at offsets behind P1, sometimes inside terrain, and restarted 120 hurt frames.

Companions now execute the original `TailsCPU_Control` ($1BAD4), with independent
$F702..$F70F state per actor. This preserves the native 17-frame leader history,
300-frame offscreen timeout, safe spawning checks, noncolliding catch-up flight,
and collision-plane handoff. Connected controllers retain ownership. CPU jump
buttons are translated to an ordinary jump for the selected character. A Tails
leader also publishes the history consumed by this AI. Companion deaths use
`Obj02_CheckGameOver` ($1CC6C), preserving P1's lives and camera. Character objects
are retained during recovery; no artificial hurt timer is added.

Host-rendered characters previously read live actor state on every scanline.
A tick could change their pose or sprite-submission flag partway through a head
or body. BuildSprites now publishes completed actor poses, and the overlay holds
one pose and foreground/art snapshot across the entire rendered frame. Party
play also opts out of VDP per-line sprite-count and pixel budgets. Intentional
sprite masks, ordering, foreground priority, and actual hurt blinking remain.

Bounded validation (2026-09-19):

- Scripted EHZ1 reproduction: Amy recovered and stayed beside the leader;
  Sonic followed the recorded position; no repeated recovery hurt timer.
- Forced offscreen Tails while P1 was airborne: waited offscreen, then returned
  after P1 landed. Final Tails position (1969, 762), normal routine/control,
  zero hurt timer; P1 retained three lives.
- Crowded-sprite count/pixel-budget fixtures and the existing Sonic 2 video
  checks passed. Intentional X=0 masks still worked.
- Both game runs ended normally with no dispatch misses. Returned/jumping
  screenshots inspected. Local evidence: consumer `build-party-recovery/qa`.

Owner subsequently confirmed that the rendering is correct. To avoid identical
companions piling up, CPU-controlled P3/P4 now have small deterministic variations
only during normal following: seven/thirteen extra history frames, different jump
retry phases, grounded trailing targets of 28-35 / 72-79 pixels, and brief coasting
on one/two of each 32 frames. The target variation changes on a fixed frame schedule
without consuming game RNG. The native history and clock are restored immediately
after the AI call. P2, connected controllers, and waiting/flying recovery retain
their existing behavior; coasting never removes braking or airborne steering.
A short EHZ run/jump/stop check showed distinct positions and jump phases, including
resting x positions 96/97/52/18, with no dispatch misses.

This is native P2 AI, not obstacle pathfinding: a follower can still encounter
springs or remain on a different ledge. The owner tested and accepted the P3/P4
variation and requested integration to master. Rendering and follower behavior
are accepted; no whole-campaign claim is made.
