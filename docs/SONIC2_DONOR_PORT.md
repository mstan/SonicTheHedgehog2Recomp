# Character donor evidence and host boundary

The stock Sonic 2 image remains the sole guest ROM. Donors supply gameplay
sprite art, mappings, palettes and behavior references. Their titles, zones,
save rules, global patches and executable entry points must not run in the host.

## Verified revisions

| Donor | Size | SHA-256 |
|---|---:|---|
| Supplied Amy in Sonic 2 Rev 1.7.1 | 2,097,152 | `9c028944730128f6b9999fc74babf69694b0edab50e3f42cc6b60a185d0b1457` |
| Stock S3&K combined | 4,194,304 | `fba0677fde9f76df93f3e98d6310d8af68b9847bde16e253d73cd4dd8134ed23` |

Amy ZIP contains one `.bin`, no source. Public source search did not establish
an exact Rev 1.7.1 tree. Do not substitute the different Superstars Amy or
Anniversary Edition controller. Existing donor bytes are available for an
evidence-led behavior port. Credit E-122-Psi and original contributors; source
availability and redistribution rights are separate questions. Keep extracted
owner assets private, outside commits/packages.

## Asset addresses

All numbers below are donor addresses, not Sonic 2 host addresses.

| Data | Amy Rev 1.7.1 | S3&K Knuckles |
|---|---:|---:|
| Normal gameplay mappings | `$08B8C0` (8-byte pieces) | `$14A8D6` (6-byte pieces) |
| Dynamic pattern load cues | `$08D6CE` | `$14BD0A` |
| Uncompressed character art | `$060000` | `$1200E0` |
| Normal character palette | `$0029E2` | `$0A8AFC` |
| Mapping/DPLC frame count | 253 | 251 |
| Animation table / streams | `$01C96E` / 44 | `$017EF4` / 37 |

S3&K evidence: pinned skdisasm `1e1b5aff82c21175c593e42c966a6ff8b1586ff3`,
byte-matched listing. `Map_Knuckles`, `PLC_Knuckles`, `ArtUnc_Knux`,
`Pal_Knuckles`, and `Knuckles_Load_PLC2` establish the table format and art base.

Amy evidence: its sole `movea.l (pc,d0.w),a1; jsr (a1)` object-dispatch site is
`$01631A`, pointing at `Obj_Index=$016352`. First entry `$01ACEC` is the player
object. Its state table `$01AD06` identifies initialization `$01AD12` and
control `$01ADCC`. Initialization loads mappings `$08B8C0`. The control tail
calls DPLC loader `$01CC3E`, which loads cues `$08D6CE`, adds art base `$060000`,
and queues to VRAM `$F000`. Palette pointer at `$00279A` targets `$0029E2`.
The first mapping frame is deliberately empty (offset zero); the following
pointer `$01FA` establishes 253 table entries. Knuckles has 251 source entries.

`sonic2_donor_assets.c` normalizes only those gameplay frames to independent
indexed surfaces. It validates table bounds, piece counts, tile references,
palette use and image extents, builds transactionally and leaves an existing
bank untouched on failure. It also validates bounded animation streams.
Registration separately requires a verified bank and enabled mod.

## Portable behavior port

Amy modes at donor `$01AE68`: ground `$01B018`, air `$01B09A`, roll `$01B0DA`,
jump `$01B104`. `sonic2_character.c` ports specials as independent actor state;
the native host retains movement, terrain and combat consequences.
Knuckles reference routines: `Knuckles_Glide`, `Knuckles_Gliding_HitWall`,
`Knuckles_Fall_From_Glide`, `Knuckles_Sliding`, `Knuckles_Wall_Climb`,
`Knuckles_ClimbUp`, `Knuckles_LetGoOfWall`, `Knuckles_Climb_Ledge`.

Following the Captain Falcon/SMW example, behavior belongs in a portable
character controller; a Sonic 2 adapter owns terrain/solids/combat and native
world consequences. No donor object model or global RAM copy belongs in that
interface. The world must tick once, and each actor must have independent
state. UI registration is gated by both verified assets and implemented
gameplay capabilities.

Amy evidence used by the port:

- `$1B972`: Down+A hammer jump, additional `$250` impulse.
- `$1B9B2`: A hammer swing, `$28->$23` animations, inertia decay by one eighth.
- `$1BA9A`: Down+B/C giant jump, water/speed-shoe variants.
- `$1BD8C/$1BE44`: Up+B/C charged dash, `$800..$C00` release-speed table.
- `$1BFE2/$1C122`: downward whirl / airborne hammer.
- `$1C0D2/$1B922`: giant-roll transition and landing/recovery animations.
- `$41964`: grounded hammer's facing-dependent 34x50 attack rectangle.
- `$41B78/$41C14`: hammer breaks monitors, including upward-moving strikes.
- `$41C60`: attack predicate; ordinary unarmed Amy jumps do not attack.

Knuckles uses the pinned source's lower jump, glide acceleration/turning,
release/fall/slide, wall grab/climb/jump, and ledge offsets. The host supplies
integer sine and signed floor/wall/ceiling distances. Native helpers perform
actual world collisions; donor code never executes.

## Host ownership and extension boundary

`sonic2_runtime.c` separates character identity from player role. P3/P4 reserve
stable native pool addresses, keeping monitor/platform parent links valid.
Solid standing/pushing flags are stored independently per extra actor. Springs
execute their world routine once and collision/launch helpers for each actor.
P1 owns progression, camera and checkpoints; companions have independent input,
physics and death/catch-up respawn. Campaign rings use a shared native pool.

Native VS has only P1/P2 and respects their selected characters. Special stages
always use stock Sonic and Tails, including solo/P2=NONE campaign rosters.
Their native controllers, art, shadows and CPU follow remain unchanged; the
chosen campaign characters and companions return afterward. No imported
special-stage projection remains, and no donor title assets are decoded.

The Options registry uses stable IDs (capacity 16), availability callbacks and
uniqueness checks. New imports add a verified resource, portable controller and
host registration; roster/input code is reusable. S3&K is a package with a
Knuckles feature, leaving room for separately opt-in future features.

This is a representative playable spike, not whole-campaign parity certification.
Live tests cover real-terrain glide/grab/climb/wall-jump, Amy enemy/monitor/boss
hits, springs, recovery, transitions and SS return. All-zone devices, all power-up
visual combinations, water palettes and exhaustive donor timing need further
validation. Machine quickstates reject experimental rosters because their
format does not serialize host controller/solid state. That guard does not
implement the separately gated campaign save/zone-selection feature.

## Knuckles loop animation correction (beads-5dyp.6)

The donor's `Animate_Knuckles` ($17D30), specifically $17E42-$17E82, uses
sector offsets of four frames for walking and two for running. The shared
Sonic-style selector used eight/four instead, selecting unrelated Knuckles
poses on steep slopes and loops (e.g. left-facing angle $60: walking $37
instead of $1F, running $39 instead of $2D). The donor also writes the surface
pose every tick, before checking the gait timer. Reusing a cached frame while
changing its flip flags could mismatch the pose and surface orientation.

Knuckles now uses the donor's stride and timing order. Rolling and the other
character controllers retain their existing behavior. The focused character
check failed on the old selector and passes for both facings, walking/running,
all loop sectors, and angle/speed changes during a held gait timer. Gameplay
was reproduced from the owner's EHZ1 position (6535, 711), using the spring at
(6472, 720); wall and ceiling captures show the corrected, nonrolling poses.
No dispatch misses occurred. Local captures live in the consumer's
`build-party-recovery/qa/knuckles-loop` directory. The owner accepted the
relaunched build and requested a release. The fix landed on engine master
at `837f93a` and consumer master at `f5dd907`.
