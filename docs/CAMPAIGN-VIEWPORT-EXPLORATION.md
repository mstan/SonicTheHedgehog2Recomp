# Independent campaign views: exploration

2026-09-28. Branch `explore/campaign-viewports`, based on Sonic 2 master
`e37fdcad0d4774cfb8947e87af6b3aaad531d635` (v0.8.1). Tracked in central
Beads `beads-5dyp.18`. This is a source investigation and an offline sizing
tool. This document records the original investigation. The subsequent
prototype and its validation are described in [CAMPAIGN-VIEWPORT-PROTOTYPE.md](CAMPAIGN-VIEWPORT-PROTOTYPE.md)
under `beads-5dyp.19`; statements below describe the baseline unless noted.

## Conclusion

Independent full-screen views for four campaign players look technically
feasible. Every peer would simulate the same campaign and all four actors,
then render one view following its assigned character. There is no need to
render a four-way split, run four game instances per machine, or transmit video.

The existing party runtime and world renderer provide substantial foundations.
The work goes beyond changing the camera: campaign object lifetime, companion
recovery, camera-dependent stage rules, and presentation need to be separated.
Start with an opt-in Emerald Hill Act 1 experiment before generalizing to the
whole campaign. Keep the native two-player competition mode separate.

## What the current implementation actually does

The inspected engine pin is `dcf77ffb3b5f185990a164077eb85668cf3460e2`, with
recomp-net `588059cbd7bac1e157539fd0ca2b62380be012a4`. Disassembly was checked
out at the game's pinned `65ddcc24250af08ddfdf58e36351374ace998e66`.

| Area | Evidence | Consequence |
| --- | --- | --- |
| Existing peer view | Engine `runner/main.c`, `netplay_peer_view_active()` and SDL presentation block | Only active for an even framebuffer height above 240. It crops one half of the existing double-height frame. Slot 1 gets the bottom; other slots get the top. It creates no cameras. |
| Campaign world drawing | [sonic2_video.c](../game/sonic2_video.c), `world_attr()`, `scanline()` | Already reads world layout/chunks/blocks beyond the native streamed screen. Camera and background scroll still originate from the native P1 view. |
| Object loading | Same file, `spawn_scene()`, `activation_bounds()` and cull hooks | One horizontal interval around `0xEE00`, with placements prioritized by distance to P1. The ordinary dynamic pool has 112 slots; P3/P4 occupy two of them. |
| Sprite publication | Same file, `capture_objects()`, `add_mapping()`, `publish_sprites()` | Sprites are clipped against the current horizontal and vertical view before publication. Publication also writes guest on-screen flags. A later camera offset cannot recover omitted sprites, and local visibility cannot safely control those flags. |
| Companion lifetime | [sonic2_runtime.c](../game/sonic2_runtime.c), `companion_control()` | Human companions keep input ownership but still call native Tails CPU recovery. Their visibility is measured against P1's 320-pixel camera. |
| Native recovery | `game/s2disasm/s2.asm`, `TailsCPU_CheckDespawn` / `TailsCPU_TickRespawnTimer` | An off-screen timer reaches `0x12C` (300) ticks and enters despawn/rejoin. A different displayed camera alone does not prevent this. |
| Boundaries and events | Same assembly, `Sonic_LevelBound`, `Tails_LevelBound`, `RunDynamicLevelEvents` | Global camera limits also constrain movement, bottom deaths and stage events. They cannot be replaced by a different local camera on each peer. |
| Standard Sonic/Tails | Runtime hook at `Level_SetPlayerMode` (`0x4450`) | The vanilla two-character roster bypasses the enhanced party runtime. An independent-view mode must handle that path too. |
| Aspect ratio | Engine `custom_video_prepare()`, `capture_local_session_config()` | Width affects simulation today. The host's aspect is sealed for the session; Adaptive becomes a fixed ratio. Different local simulation widths would desync. The older `CUSTOM-VIDEO.md` statement that custom video is disabled online is stale. |
| Player capacity | `sonic2_party.h`, engine `sim_step.h`, recomp-net `config.h` | Party and Genesis simulation support four; recomp-net supports eight. Extra views are not the main obstacle to more players: actor arrays, inputs, collision bookkeeping and state formats also need extension. |

Native VS has its own second camera and two-player rules, and the party
runtime explicitly does not reserve P3/P4 there. Turning on the VS flag would
not turn campaign into four-player independent-view co-op.

## Small offline experiment

[explore_campaign_viewports.py](../tools/explore_campaign_viewports.py) reads
the owner's REV01 ROM and an existing 64 KiB campaign RAM capture. It applies
the current loader's 128-pixel-rounded horizontal activation margins around
hypothetical per-player views, merges overlaps, and counts placement entries.
It never starts the game, opens sockets, or modifies either input.

The existing Emerald Hill Act 1 capture has an 11,264-pixel layout and 135
static placement entries. For planned player X positions 512, 2048, 4096 and
6144:

| Loading policy | Horizontal coverage including activation margins | Static placement candidates |
| --- | ---: | ---: |
| Current P1 window, 320 pixels | 768 px | 3 |
| Union of four 320-pixel views | 3,072 px | 30 |
| One enclosing window spanning those four views | 6,400 px | 82 |

Mixed widths of 320/398/320/796 produced 37 candidates in the separate regions
versus 84 in the enclosing region. An eight-view sizing example produced 66
versus 135; it does **not** mean eight-player gameplay is supported.

These are static candidate counts, not live occupancy or timing measurements.
Consumed objects, children, projectiles, vertical filtering and transient
allocation affect actual pressure. The result supports using separate active
regions instead of activating everything between the leftmost and rightmost
player. It does not establish a safe maximum player separation or framerate.

Example, using private files outside the repository:

```powershell
python tools/explore_campaign_viewports.py --rom C:/ROMs/sonic2.bin --ram C:/captures/gameplay.ram --centers 512 2048 4096 6144 --widths 320 --out C:/captures/view-sizing.json
```

## Proposed architecture

1. **One shared campaign simulation.** Keep original campaign mode and a
   canonical stage camera/event owner initially. Compute a small camera record
   for every active player from synchronized actor state at a defined tick
   boundary. Any camera state used by loading, recovery or boundaries belongs
   in rollback snapshots. Never put the local seat's camera into shared RAM.

2. **Shared active regions around every human player.** Generalize placement
   loading, culling, ring search and simulation visibility to the union of
   those regions. Every peer computes the same union and allocation order.
   Deduplicate overlapping placements; preserve the existing consumed-state
   semantics. Define stable priorities and tie breakers under pool pressure,
   with headroom for children and party actors. Do not silently drop essential
   platforms or give P1 permanent allocation priority.

3. **One common scene, then local view selection.** Publish world-coordinate
   sprites before per-view clipping, with shared simulation visibility handled
   separately. Draw terrain, native actors, donor actors and HUD against an
   explicit view origin. Render only the local player's view. Camera smoothing
   or spectator view selection that is purely visual stays outside game hashes.
   Use the session's seat-to-controller mapping; `input_player` is the local
   input device index and is usually zero on every peer, not the followed actor.
   A generic read-only mapped-player query may be needed in the engine.

4. **Human companions can remain away from P1.** Separate human off-screen
   persistence from CPU follow/rejoin. Retain real collision, death and
   respawn behavior. Audit global side/bottom limits and object interaction
   state rather than just forcing the on-screen flag or disabling all deaths.
   Keep default shared-camera and disconnected CPU behavior available.

5. **Presentation must work away from the native camera in both axes.** The
   foreground layout reader is reusable, but the current sprite capture loses
   vertically separated objects. Background parallax, water-line palettes,
   HBlank effects and camera-triggered art changes need per-view treatment.
   Current `pattern_pixel()` reads live VRAM, so far-away art residency also
   needs an audit. Existing donor art caches help actors; they do not solve
   every world's art/palette change. Do not execute the full game or native
   scrolling routines again just to render another camera.

6. **Negotiate world-loading coverage separately from display size.** A first
   spike can keep today's agreed aspect ratio and vary only origins. To allow
   different aspects per peer later, agree on per-player activation widths or
   a common maximum at match start; smaller local views may render within that
   coverage. A local resize must never independently alter spawn/cull behavior.
   Local texture limits and allocation failures remain presentation concerns.

Camera state, spatial loading and Sonic-specific stage rules belong in this
game repository. Shared engine changes should be small, generic contracts.
No new transport or video streaming feature is needed in recomp-net for the
four-player version.

## Suggested first playable milestone

Implement an opt-in Emerald Hill Act 1 mode with four view records and the
existing four-character roster. Exercise horizontal **and vertical** separation,
moving platforms, ring consumption and overlap/rejoin. Keep P1-led campaign
progress initially; use a coordinated group transition for bosses, act exits,
deaths that restart the stage and special stages. The exact regroup/respawn
policy is a gameplay decision to settle before extending the whole campaign.

That milestone should prove: each seat follows its own actor; actors can remain
outside P1's view past the native 300-tick recovery threshold; distant platforms
and enemies still exist; overlapping players share the same objects/rings;
rollback restores cameras and object ownership; rendering a different seat or
resizing a local view never changes the simulation digest. Special stages
currently force stock Sonic/Tails and need an explicit shared-view fallback.

Avoid coupling an eight-player extension or a redesigned campaign save system
to this first experiment. Independent views and unrestricted whole-campaign
roaming are separate milestones.

## Work completed here

- Created the branch from current remote master and inspected the pinned game,
  engine, net library and disassembly.
- Ran the placement-sizing tool against the existing Emerald Hill capture.
  Checked the first interval by hand and checked overlap deduplication,
  touching intervals and invariance under player-order changes.
- No gameplay implementation, full game build, game process or multiplayer
  session was started. Future runtime checks should remain sequential and reuse
  at most two fixed executable paths, per the owner's test preference.
