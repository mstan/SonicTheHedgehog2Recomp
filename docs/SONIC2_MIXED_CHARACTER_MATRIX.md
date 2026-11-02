# Sonic 2 mixed-character audit ? 2026-11-02

The audit now exercises 66 routes: 57 native-aspect cells and 9 wide spot
checks. Twelve Amy cases were added after locating the verified donor in an
earlier private playtest. One actual product defect was found and repaired:
when Tails leads, both players wrote the P2 history ring and native stars also
selected that ring for P1. This is S2-28 in the feedback burndown.

These are bounded EHZ input and rendering checks, not complete multiplayer
campaign, physical-controller or netplay certification. Effect flags/objects
are seeded **after ordinary movement**; natural power-up acquisition is
bypassed. Previously accepted real Knuckles pickups are separate evidence.

## Native aspect

P = current attached-art/input/HUD checks pass. R = the Tails-history repair is
qualified on the candidate. I = movement and parent links pass, with colours
checked one owner at a time because the frozen AI actors overlap. I does not
certify simultaneous visibility through an occluding actor. R/I combines these.
Shadow's current native-blue stars are checked as current behavior, **not** as
acceptance of the requested purple treatment.

| P1 | P2 | VS | Local co-op | Solo + AI |
| --- | --- | --- | --- | --- |
| Sonic | Knuckles | P | P | I |
| Sonic | Shadow | P | P | I |
| Tails | Knuckles | R | R | R/I |
| Tails | Shadow | R | R | R/I |
| Knuckles | Sonic | P | P | P |
| Knuckles | Tails | P | P | P |
| Knuckles | Shadow | P | P | P |
| Knuckles | Mighty | P | P | P |
| Shadow | Sonic | P | P | P |
| Shadow | Tails | P | P | P |
| Shadow | Knuckles | P | P | P |
| Shadow | Mighty | P | P | P |
| Mighty | Knuckles | P | P | I |
| Mighty | Shadow | P | P | I |
| Sonic | Tails (control) | P | P | P |
| Amy | Knuckles | P | P | P |
| Knuckles | Amy | P | P | P |
| Amy | Shadow | P | P | P |
| Shadow | Amy | P | P | P |

## Wide spot checks (32:9)

| P1 / P2 | VS | Local co-op | Solo + AI |
| --- | --- | --- | --- |
| Knuckles / Shadow | P | P | I |
| Shadow / Knuckles | P | P | I |
| Sonic / Tails | P | P | P |

Every route reached gameplay with the intended mode and no dispatch misses.
Diagnostic human masks are 3 for VS/co-op and 1 for AI; AI scripts never select
PLAYER 2. Each human seat responds to its own movement segment. Native idle
Tails can follow a co-op leader before manual input; that motion is not input
leakage. Diagnostic I records restart after fixture RAM writes and therefore
are not claimed as a complete input replay of these fixture routes.

Both competition HUDs use their actual identities. Source glyph/colour checks
pass; all sampled divider rows 216..223 remain opaque. Grey attached effects
follow the Knuckles parent in either role. Floating world icons retain the
existing P1-selected theme. Offscreen zero-Y SAT mask entries are excluded from
visible-icon checks; they explained one preliminary false flag.

## Confirmed defect and qualification

In released v0.9.7, separated Tails/Knuckles and Tails/Shadow VS/co-op fixtures
have 32 P1 positions and 32 P2 positions in E600. Some stars with the correct
parent pointer therefore draw around the other player. Tails/Shadow exposes
the same position defect even though both actors' stars use native colours.

The fix preserves the sidekick ring/index around main-player Tails' native
update and selects Obj35 history by actual parent role after native guards.
Animation, expiry/deletion and global Player_mode remain native. In all six
Tails/Knuckles and Tails/Shadow mode cells, each ring contains 64 positions of
its sole owner and every star object's center belongs to its parent. Ordinary
idle/P1-move/P2-move positions are unchanged against the released executable.
Reverse Knuckles/Tails and Sonic/Tails co-op controls pass. Sixteen isolated
AI owner runs resolve the eight ambiguous overlapping-art checks.

Development candidate SHA256:
`0e38cdf553d2fc068e596c83ac0db7c0958252f7125dfd6e9b7f7617db7b4e5a`.
BATCHED renderer, PGO off for this qualification. All 22 focused tests pass.
Current-build Tails/Knuckles save/load restores the exact picture and object
RAM; fresh diagnostic playback matches all 1,500 executions; 87 depth-8
rollback checks pass. This identifies the development qualification; the final release is recorded below.

## Explicit remaining limits

- Duplicate P1/P2 identities are not selectable and are N/A. Amy's previous
  12-cell gap is closed by the follow-up below.
- Purple Shadow sparkles: current port uses native S2 stars. Static inspection
  of the exact Megamix v3 donor establishes grey/white normal-form stars,
  rather than a purple treatment. The desired reference remains a colour-policy
  question (S2-29); no guessed purple recolour was introduced.
- Reporter intends to clarify mixed monitor/effect policy later. Current world
  icons use P1's theme; attached shields/stars use their actual parent.
- Exact title-card field flicker needs identified Sonic 2 REV01 on NTSC-U North
  American Genesis. No confirmed hardware tester was available/contacted.
  The independently qualified separator and masked-IRQ repairs remain.

Both supplied primary video references are recorded:
[FramePlay ? Sonic 2 Competition VS Mode | Hardware Recording](https://youtu.be/VfFZUNzXx4M)
and [arronmunroe ? Sonic the Hedgehog 2 two player race 60fps](https://youtu.be/D81W5nkUqt4).
YouTube oEmbed verifies titles/authors only; actual frames, ROM/console identity,
FPS and capture/field processing were not independently verified.

## Evidence

Task root: `G:/Temp/Codex/sonic2-owner-feedback-20261101`.
Released baseline executable SHA256:
`f6c69c38b443373deb9208e216d4f1a6a32d76bfe23e24b116b86717bf2b965d`.
`mixed-effects-matrix-qa` retains all 54 routes, RAM/VRAM/images and initial
checker flags. Those preliminary flags are not final product dispositions.
`mixed-effects-matrix-fixed-qa` retains six affected cases, two controls and
16 isolated-owner cases, plus `history-results.json`. Its two combined AI
nearest-X colour flags are resolved by the corresponding isolated-owner
observations; they are not hidden as product fixes. `mixed-history-state-qa`
retains current-build state/replay/rollback results. A normal-paced control's
six captures matched uncapped capture byte-for-byte; no FPS gain is claimed.

The first candidate isolated the rings but omitted routing the new native
hook through sonic2_spec.c; it was rejected. Its retained captures are under
`mixed-effects-matrix-candidate-qa`, not accepted release evidence.


## Final release qualification

[Sonic 2 v0.9.8](https://github.com/mstan/SonicTheHedgehog2Recomp/releases/tag/v0.9.8)
delivers S2-28 from tag `cd218d1` with fresh BATCHED/IPO/PGO. Final executable
SHA256 `bbea899235008b6495e1f25b6d78846e2e27c1be8fdd32e3e579846f1aa23dd1`.
The six affected cells, two controls and 16 isolated-owner observations repeat
on this exact executable. Final state picture/object restore, 1,500-execution
playback and 87 rollback checks pass. Extracted-package launcher and bundled
helper replay also pass; the runtime confirms BATCHED. Final evidence is in
`mixed-effects-release-098-qa`, `mixed-history-release-098-qa` and
`package-check-profile-098` beneath the same task root. The 54-cell audit's
unaffected baseline observations are reused; no second exhaustive matrix or
full-campaign claim is made. All explicit remaining limits above still apply.


## Additional work completed after the owner asked whether more was possible

The prior missing-Amy-donor conclusion was incomplete. The existing private
`F:/Projects/segagenesisrecomp/SonicTheHedgehog2Recomp/build-megamix-shadow/playtest-shadow-six/assets/characters/amy-1.7.1.bin`
exists and matches required SHA256
`9c028944730128f6b9999fc74babf69694b0edab50e3f42cc6b60a185d0b1457`.
It was reused read-only, without copying or distributing donor art. All twelve
Amy/Knuckles and Amy/Shadow ordered mode cells pass current input, HUD,
separator and attached-effect parent/tag/source-pixel checks on the exact
released v0.9.8 executable. Resource sidecars explicitly verify enabled Amy.
Evidence: `amy-matrix-qa/{run,matrix}-results.json` and bounded raw captures.
Native coverage is now all 18 ordered pairs involving Knuckles/Shadow among
the six available identities, plus the Sonic/Tails control (57 cells); the
nine wide spot checks are unchanged. No new executable was necessary.

Shadow source work also advanced. In verified Megamix v3, monitor reward
`$EFDE` uploads source `$11C162` to VRAM `$AB80` for `$360` words and creates
Obj38 stars. Obj38 handler `$1B254`, initialization `$1B26C..$1B29E` and
mappings `$1B7B6` select tile `$055C`, palette 0. Its normal star frames 4..7
use indices 2/3/5/6. Shadow normal palette `$2C80` maps those to `$000`, `$222`,
`$666`, `$EEE`: black/grey/white. A decoded source preview and exact instruction
bindings are retained in `shadow-stars-source-check`. This is static normal-
form source evidence, not proof of every runtime palette cycle or another
ROM's purple treatment. A colour-target clarification is pending: matching
this verified donor treatment and matching an as-yet-unprovided purple
reference are distinct acceptance choices. Existing colour behavior remains
unchanged while that choice is unresolved.
