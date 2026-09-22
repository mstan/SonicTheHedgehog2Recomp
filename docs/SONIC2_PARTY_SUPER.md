# Player 1 Super forms

Game-owned additive adapter; no ROM patch and no shared-engine implementation.
Issue: `beads-5dyp.10`. Worktree branch: `feature/sonic2-states-thumbnails`.

## Player rules

Sonic, Tails, Amy and Knuckles can transform as Player 1 with all seven Chaos
Emeralds and at least 50 rings. Jump and hold through the apex, as in Sonic 2;
there is no new double-jump transformation command. Retain the selected
character's sprites and abilities. Knuckles still requires the verified,
enabled S3&K donor mod; Amy uses the existing verified Amy donor.

Super uses native invincibility, speed, music, stars, ring consumption and
reversion. Death pits, crushing and drowning are not made safe. The native
countdown resets to 60 then decrements through zero (61 gameplay ticks per
subsequent ring); pausing does not consume rings. Act completion, death/reload
and special-stage return restore normal gameplay. Companions and VS players do
not gain Super. Special stages remain unconditionally stock Sonic and Tails.

Tails' fur and Amy's pink shades glow using character-local derived colors.
Knuckles uses the owner's S3&K Super palette, paced by the Sonic 2 palette clock
(not an exact S3&K transformation-timing port). No Super Flickies, Hyper forms,
new Tails flight ability or donor-global patches are implied.

## Source-backed adapter

Host addresses are byte-matched Sonic 2 REV01 at the pinned `game/s2disasm`.

| Host entry | Responsibility |
|---|---|
| `$1AB38` Sonic_CheckGoSuper | Seven-emerald / 50-ring eligibility, lock, stars, invincibility, sound/music |
| `$1ABA6` Sonic_Super | Native ring countdown and reversion |
| `$213E` PalCycle_SuperSonic | Fade timing and `$81` object-control unlock |
| `$1C6F6` Tails_JumpHeight return | Supply the native jump-apex eligibility check missing from Tails |
| `$1C644` Tails_Jump | Super impulse before the native underwater override |
| `$1BA14` / `$1BA1C` Obj02_Control | Tick Super after display; maintain Tails physics after water/shoe updates |

Previously the party adapter deliberately skipped both Super routines for
imported characters. It now permits native P1 progression, retaining the
companion/VS exclusions. Custom-character eligibility also rejects a stopped
act timer, following the later native revision's stuck-at-act-end fix.

Do not skip the palette routine: it releases transformation control. For a
non-Sonic leader, execute it normally but restore its four blue CRAM shades
and corresponding underwater shades, so companion Sonic stays blue. Clear
the complete palette-frame word on custom fade-out/re-entry: REV01 clears
only its high byte, leaving `$F8` behind. The host overlay applies each custom
P1's glow without changing other actors' palette or native hit response.

S3&K `PalCycle_SuperHyperKnuckles` is at donor `$3AAE` in the verified combined
ROM (ten groups of three colors). Decode it transactionally with the existing
donor bank. Imported animation `$1F` is each donor's own transformation. S2
Tails has no such animation; use his roll pose during the native timed lock.
His host-rendered Super body and separate native Obj05 tails use owner-ROM
mappings/DPLC, prepared at level initialization, not in the render hot path.

Quickstates include the fifth published sprite (detached tails, not a fifth
player). Cold restore rebuilds the immutable native Tails bank. The existing
source fingerprint rejects older incompatible machine quickstates; campaign
SRAM remains portable. Never retag an old quickstate to bypass that check.

Death testing also exposed an older party-rendering defect (`beads-5dyp.13`):
`RunObjectsWhenPlayerIsDead` uses `RunObjectDisplayOnly` for the dynamic pool,
so imported/extra companions could enter native sprite lists outside the
`update_player` scope. Rendering donor frame IDs against Sonic's mappings
produced invalid native dispatches, also reproduced in the pre-Super build
and without Super. Suppress those submissions by actor address regardless of
call scope; retain the native death freeze and host-rendered frozen poses.

## Validation

`tests/runtime/run_sonic2_super.py` runs serial private-ROM fixtures. Seeds are
limited to RAM prerequisites/scene setup; transformation and abilities use
real pad input and native logic. Assertions about shared Super/physics globals
read snapshots at safe native boundaries: arbitrary VBlank RAM dumps can see
the companion adapter's temporary masked globals and are not valid evidence
that P1 lost Super.

Coverage: four identities, exact eligibility and negative cases, depletion and
retransformation, isolated companions/palettes, pause, enemy contact, act end,
death, water transitions, native special stages, unchanged VS restriction,
Amy hammer and Knuckles glide, no-donor Sonic/Tails, and repeat/fresh-process
quickload of both transformation and active Super. Continuation RAM and PNGs
must match; every process checks dispatch misses. Existing campaign and
quickstate suites remain required. Owner visual/gameplay acceptance is a
separate gate, not inferred from fixtures.

2026-09-22 automated checkpoint: 25 CTests and all 60
Super fixtures passed. Both trace-enabled and stripped release builds compile.
Final trace-enabled regressions: 23 quickstate cases, 36 campaign cases and
the before/after native menu-audio comparison passed with no dispatch errors.
The stripped Super evidence is in private `build/super-release-20260922-04/`;
the final save/menu regression evidence uses `build/super-final-*20260922/`.
The pre-update owner executable and saves are preserved together under
`build/states/before-super-20260922/`. No ROM, derived asset, quickstate,
campaign fixture or generated diagnostic is included in source changes.

Owner acceptance 2026-09-22: "all fixed"; explicitly requested commit,
master integration/push, a new build and closeout. v0.7.0 release/integration
is tracked in `beads-5dyp.14`. The shared engine remains unchanged.
