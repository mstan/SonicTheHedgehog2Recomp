# Shared campaign camera prototype

Experimental build `0.8.2-campaign1`, branch `explore/campaign-viewports`.
Central issue: `beads-5dyp.19`. This branch is separate from the v0.8.1 release.

## Playing

Everyone must use this build. Create/join the usual netplay lobby, select the
party in Options, and start **1 PLAYER** (campaign). That title-menu label
selects the campaign rules; the party can contain up to four characters.
Each participant automatically gets an unsplit view following their assigned
character. Nearby allies remain visible. Rings, progression, checkpoints,
leader death, bosses and stage transitions retain the existing party rules.

No additional mod or widescreen setting is required. With widescreen disabled,
the renderer uses the normal 320x224 canvas and existing display aspect handling.
With it enabled, the session retains the host's selected aspect. Amy and Knuckles
still require each participant's own verified donor assets. No ROMs are included.

Offline local play and native 2P competition keep their existing view paths.
This is an exploration build, not a general release or a full-campaign signoff.

## Implementation

The simulation runs once per peer using the existing synchronized inputs.
There is no additional game instance per character, split-screen render target,
network protocol, or video transmission. A read-only engine query resolves
the local session seat to its logical player, including nonidentity mappings
and spectators. Only presentation uses this query.

The native scene publication holds world sprites and four camera origins.
It no longer throws away campaign sprites outside P1's screen. Terrain comes
from the full resident level layout. The local renderer and donor-character
overlay use the same selected origin. The campaign leader keeps the original
camera; companions currently use a simple position-follow camera clamped to
the shared stage boundaries. Their human controllers no longer trigger the
300-tick recovery merely because they are outside P1's viewport.

Activation uses every party member's window. Visited cells stay active, with
placement records and the area bitmap held in native memory and included in
rollback. Rendering a different view does not mutate this simulation state.
The cold-reset snapshot always includes video simulation state so leaving
netplay or rematching cannot change the snapshot schema.

## Remaining exploration

The native object routines still use their original 112-slot dynamic allocator;
P3/P4 reserve two slots. If retained areas exhaust it, the prototype drops back
to the union of all current player windows, allowing the existing cull routines
to free distant objects. Thus **unlimited permanently active entities are not
implemented**. The map is resident; the entity execution pool has not yet been
replaced with an unrestricted native allocator. No remote player's current
window is deliberately evicted. Extremely dense simultaneous views may still
exhaust the pool, as can priority queues used by the original object routines.

Emerald Hill's horizontal parallax is reprojected from its existing formula.
Other backgrounds use a provisional half-speed horizontal / quarter-speed
vertical adjustment. Water palettes, per-zone scroll effects, dynamic art,
boss boundaries and distant partners at stage transitions need playtesting.
The existing leader-driven progression is intentionally retained.

More than four players is outside this prototype. Automated checks use at
most two participants in the same fixed test installations. The owner also
explicitly requested and tried one visible four-peer desktop session, using
the same established executable path for every process to avoid new Firewall
application registrations.

## Validation

MSVC x64 Release build with netplay/ICE enabled and runtime trace/debug servers
disabled. Sonic 2 unit tests cover the existing party/character/options/save
paths plus four distant activation windows, all four camera projections,
donor overlay origin alignment, render-independent rollback bytes, and cold
world restore after a different local video configuration is selected.

The first real two-peer campaign test ran 2,400 ticks with native video settings.
P1 stayed at x=96 while P2 reached x=1335, then x=1656. P2 remained in routine 2
and normal controller state 6 for a further 400 ticks; the party collected five
shared rings. Both peers captured the same world positions and different
full-screen views, with matching confirmed hashes and zero desyncs/refusals.

A second two-peer session used Knuckles/Amy with Sonic/Tails CPU companions,
the host's 16:9 aspect versus a guest 32:9 preference, two 1,800-tick matches,
and 18 forced rollback episodes per peer per match. Both confirmed timelines
matched, with no desyncs/refusals. Both processes then cold-reset into offline
play and completed another 1,800 ticks. Screenshots show all four characters.
This checks donor rendering and rematch state, not a four-human Internet game.

The subsequent owner-requested four-peer session used Sonic/Tails/Knuckles/Amy
and four independent windows, all in the same local campaign. It ran through
approximately 3,910 confirmed ticks with matching hashes, natural rollback,
zero desyncs and no refusal on every peer. Closing the Tails window ended the
session normally. The owner confirmed the four-player experience and approved
committing and merging this work. Full-campaign and four-human Internet
coverage remain outside this validation.

The reproducible harness is `tools/validate_netplay_launch.py --campaign-views`.
It requires a private ROM, an isolated lobby server and a fixed `--runtime-dir`.
Do not create new executable installation paths per run. Automated testing
stays at two game processes concurrently unless the owner explicitly requests
a larger interactive session.
