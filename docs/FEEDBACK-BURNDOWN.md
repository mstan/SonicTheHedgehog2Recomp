# Sonic 2 owner feedback burndown

Owner goal: document and resolve all addressable Sonic 2 items, then cut a new
release on the repository (explicit release authorization, 2026-11-01).

Started 2026-11-01. Keep adding incoming reports here, deduplicate repeated
reports, and complete fixes with targeted built-runtime evidence. A source edit
or successful build alone is not a validated fix. Preserve native saves and
owner settings. Work solo; reuse existing builds, source assets and QA tools.

Tracking: `beads-5dyp.22` under game epic `beads-5dyp`, claimed in progress.
Central access works with process-local `BEADS_DOLT_SERVER_PORT=30835`;
the legacy 3307 endpoint does not contain this database. No shared server or
configuration was changed.

## Sonic 2 work queue

Original intake: **26 reports; 23 validated resolutions (22 fixes and 1 current-HUD verification), 2 owner deferrals and 1 hardware-fidelity question**. Follow-up S2-27 audits mixed pairs, S2-28 repairs a newly confirmed shared-history defect, and S2-29 records the unported Shadow sparkle treatment. One save-menu
question is answered below. Open reports require checking against the current source/build; do not assume
the reporter used the latest version.

| ID | Report and acceptance | Status / evidence |
| --- | --- | --- |
| S2-01 | Launcher sample-rate choice does not persist. Save the chosen rate, restore it on relaunch, and use it for the output device. | **Released in v0.9.7**. 0.9.4 already saves on Play. Owner explicitly requested save-on-close too; shared engine `eb52f8c` adds that path. Actual widget clicks, Play/save/relaunch at 44100 Hz, close/save/relaunch at 48000 Hz, and actual device rates pass. Tracking `beads-3vb.32`. |
| S2-02 | Save-select background moves when changing cards. Keep the background stationary while the cards/cursor scroll, including widescreen. | **Released in v0.9.7**. Baseline background moves; candidate background stays pixel-identical while cards move at 320x224 and 796x224. |
| S2-03 | Save-select appears over a blank native Options menu without correct fades. Fade the complete save screen in/out on entry, start, cancel and relevant return paths; no exposed underlying screen. | **Released in v0.9.7**. Actual complete-frame entry, cancel and game-start fades at 320x224 and 796x224; original native palette timing retained, real title/game destinations reached, no dispatch misses. |
| S2-04 | Native 2P competition HUD assumes Sonic/Tails. Show each actual competitor's identity, life artwork and applicable name colours. | **Released in v0.9.7**. Both native viewports use the actual roster and independent life counts. Reversed Sonic/Tails, original Knuckles in either role, Shadow/Mighty and no-lock-on-art Knuckles fallback pass. Default Sonic/Tails and all paired RAM/VRAM stay unchanged; 5,804 original opaque glyph/colour pixels pass. See competition HUD qualification below. |
| S2-05 | Knuckles has the blue Sonic title card. Use the original Knuckles-in-Sonic-2 green title-card treatment. | **Released in v0.9.7**. Original two-tile ink-4 change plus the donor's original $080 green applied to those displayed plane-A cells. Actual 320/796-wide images are green, Sonic's card stays blue, matched intro RAM is byte-identical, and dispatch misses are empty. Native Sonic palette remains available to mixed parties. |
| S2-06 | Knuckles visibly spawns as the title card slides away. Match the original entry/spawn presentation without a late pop-in. | **Released in v0.9.7**. Reproduced a late host-only reveal: native Sonic is exposed with the card, Knuckles waited until frame170. Donor actors now use the actual card planes for occlusion and the stage camera before Level_started; feet/body reveal at165..169. All32 native A/B RAM/VRAM samples are identical, Sonic images unchanged, native/wide center matches. Native 2P retains its previous gate. |
| S2-07 | Knuckles glide landing/ground slide lacks the original dust effect. Restore the source-backed dust at the real slide/landing trigger. | **Released in v0.9.7**. Source cadence, original atlas and six-pixel offset restored. Real glide/slide A/B checks pass for P1/P2/P3/P6, native and widescreen; no sampled actor-motion change. Current-build P1/P6 save/load restores full WRAM and yields 16 identical continuation images/RAM samples. See dust qualification below. |
| S2-08 | Knuckles HUD still says Sonic and uses Sonic's life icon. Use Knuckles' icon/name and original orange/white name treatment. | **Released in v0.9.7**. Exact original lock-on face/name art and palette-0 colours; native/wide/mixed-party images match every original opaque HUD pixel. Native numbers update, other Sonic/Tails images and all matched RAM/VRAM stay unchanged. Select the verified original KiS2 ROM resource under Knuckles; no extra feature toggle. Current-build 1200-frame diagnostic replay passes with that resource restored. |
| S2-09 | Knuckles shield monitor/effect remains blue. Use the original Knuckles-in-Sonic-2 grey monitor and shield. | **Released in v0.9.7**. Exact original grey monitor/shield tiles with native live colours. Source pixels and cold restoration pass in native/wide single-player, mixed P1/P2 parties and both native competition viewports. Original resource is selected under Knuckles; no new feature toggle. See grey-art qualification below. |
| S2-10 | Knuckles uses a walking pose after a vertical spring (early EHZ1 example). Use the original airborne spring flip animation. | **Released in v0.9.7**. Real EHZ1 subtype-03 spring at (2220,568): baseline walking frames 1..7 become source tumble frames $32..$3A. All 24 matched samples have identical position, velocity, status and tumble angle. The fixture positions the player but never sets the spring result. Focused both-facing/wrap tests pass; zero dispatch misses. |
| S2-11 | Knuckles invincibility monitor/sparkles use Sonic 2 art. Use the original Knuckles-in-Sonic-2 grey monitor/sparkles. | **Released in v0.9.7**. Original grey invincibility monitor/stars follow the actual Knuckles parent; Tails stays blue. Native/wide/mixed P2 and both competition viewports pass original-pixel/cold-state checks. Real party monitor landing and stale P2 star-history defects found during qualification are repaired. |
| S2-12 | Widescreen Knuckles sometimes jitters in midair while gliding. Reproduce and remove jitter without changing glide physics. | **Deferred by owner (2026-11-02)**. No reproducing recording/setup supplied; bounded samples did not establish a fix, and the completed pose-history experiment was rejected as pixel/state-identical. Reopen on new reproducing evidence; not counted resolved. |
| S2-13 | Sega logo animation is not extended for widescreen. Extend its intended scene consistently through the margins. | **Released in v0.9.7**. Sonic and the moving streak span the selected viewport; the logo/TM keep their native pixel size and centered position. Original background edge pixels extend the logo stripe band through its palette animation. Native/16:9/32:9 real boot routes pass, with unchanged RAM/VRAM and native images. |
| S2-14 | Options background is not extended for widescreen. Fill the extended area with the correct background across navigation/fades. | **Released in v0.9.7**. Repeat the populated 40-column background instead of the unused portion of the 64-column plane. Real entry/navigation/exit routes at 320/398/796 pixels: all 426 paired RAM/VRAM samples identical; all 142 native images identical; only Options widescreen images change. 43,200/106,880 checked margin pixels match the native pattern. |
| S2-15 | Native 2P competition is not extended for widescreen. Extend both viewports, camera/object coverage and HUD appropriately while preserving competition mechanics. | **Released in v0.9.7**. Both IM2 views extend terrain, sprites/rings, source title cards and HUDs through the selected width, with independent cameras and shared bounded object coverage. EHZ/CNZ/MCZ movement,16:9/32:9, Knuckles P2, both-player bridge/signpost/results mechanics, state roundtrip and diagnostic replay pass. Native control retains162 exact RAM/VRAM/PNG samples. See competition widescreen qualification below. |
| S2-16 | Save cards always show Sonic & Tails. Update portraits from the characters currently chosen in Options; keep campaign saves roster-independent. | **Released in v0.9.7**. Current Options roster supplies original Sonic/Tails/Knuckles solo portraits and verified Amy/Shadow/Mighty gameplay art. Nine runtime cases cover five distinct available characters, native/wide six-seat parties, the original pair and empty seats; campaign bytes stay unchanged. Focused tests cover Amy bank selection, all six identities, bounded layout and unchanged progress. No separate card selector was added. |
| S2-17 | Delete-mode UI sounds are missing: entering delete mode, scrolling, choosing a file and successful deletion. Match the source-backed events, including silence for ignored actions. | **Released in v0.9.7**. Focused event/ignored-action/failing-commit tests and actual native generated-audio A/B pass. Source-backed event order uses native S2 cues: Checkpoint $A1 for enter/confirm, BonusBumper $D8 for delete navigation, TallyEnd $C5 after successful disk commit. S3&K uses Starpost/SmallBumpers/Perfect; this preserves the established S2-native sound policy rather than claiming identical timbres. |
| S2-18 | Delete-mode cancellation differs from S3&K. Start on an empty card or Delete, B anywhere, and choosing No at confirmation should leave deletion mode without starting/deleting a file. | **Released in v0.9.7**. All five actual routes pass with disposable saves: empty, No, B at confirmation, Delete card and Yes. Unit cases also preserve read-only data and fail commits without a success sound. |
| S2-19 | Widescreen title cards do not cover the extended viewport properly. Extend the intended card elements and preserve the normal-aspect layout. | **Released in v0.9.7**. Live card-plane geometry fills the viewport; the serrated red edge follows its new boundary and text keeps its native glyph size/settled position. Real Sonic 16:9/32:9 and green Knuckles 32:9 intros pass; native images remain identical. Separate from S2-05. |
| S2-20 | Widescreen title-card slide-out looks glitched. Animate the complete card coherently through its exit without wrapping or edge remnants. | **Released in v0.9.7**. Card fills use the displayed plane cells without wrapping; revealed terrain/donor actors share the final widescreen origin. Text travel scales around its settled anchor so it exits before native deletion. Complete 320-frame card routes pass, with exact post-exit images and unchanged paired RAM/VRAM. |
| S2-21 | Native 2P separator differs during title cards: recomp bar is thicker and briefly flashes blue; reference CNZ bar is thinner and blank. Compare actual interlaced fields/display policies and correct unintended size/colour changes. | **Released in v0.9.7**. Eight output rows216..223 (four displayed TV rows) form an opaque black gap. P1 HUD ends215; P2 viewport begins224 and its HUD at232 is preserved. The original larger P1 sprite mask is replaced by H-int and is not the permanent divider. EHZ/CNZ complete title/game routes pass. No reference-emulator flicker was invented. |
| S2-22 | 2P title-card transition flashes the wrong region. In reference CNZ P1 flashes; recomp flashes P2. In reference EHZ part of the separator flashes; recomp still flashes P2. | **IRQ repair released in v0.9.7; exact hardware fidelity unverified.** Reporter now questions Kega as the reference and considers the separator leak the only confirmed rendering defect. Do not treat absent/emulator-specific flicker as a required change.  Fixed dropped masked IRQ4 requests in shared engine024db9b: the erroneous full-P2 flash is gone in both zones. Final native source transient appears on EHZ P1 and at the CNZ seam, differing from the reported emulator's zone pattern. No artificial flicker relocation is added. Both-zone rollback, current save/load and diagnostic replay pass; see qualification below. |
| S2-23 | Native 2P corruption below P1 extends across the recomp screen, protruding into P1 and clipping the life icon/count. Reference shows only a small left-edge disturbance. | **Validated current-HUD resolution (v0.9.7)**, not a newly repaired glyph claim. The unintended full-width divider strip is covered by S2-21/24. Current baseline and candidate both match all476 original opaque P1 life/name/count pixels in each of six EHZ and six CNZ samples; no clipping was reproduced. All pixels above216 and from224 onward remain identical. Original small flicker artifacts are not reintroduced. |
| S2-24 | Level graphics and characters can draw inside the native 2P separator (particularly EHZ). Keep the intended divider opaque to both viewports. | **Released in v0.9.7**. Both planes and native/host actors are composited behind the eight-row gap. EHZ/CNZ transition/terrain checks and six positioned EHZ actor observations pass; actual P1/P2 SAT pieces overlap the gap while it stays black. Adjacent HUD/view rows are unchanged. |
| S2-25 | Bring Sonic 2 launcher behavior into parity with the other recomps. | **Released in v0.9.7; general audit complete**. Builds the exact current upstream UI base plus six-seat compatibility/fixes, rather than the stale pin. Play/close/relaunch preserve tested preferences; P3-P6 keyboard/gamepad/reset and pad modes target the selected seat; P5/P6 bindings read back correctly. 18 focused UI tests, 10 game tests and real hidden launcher checks pass. See launcher audit below. |
| S2-26 | Large companion parties make the game's framerate unstable in 4:3. Restore stable normal-speed play with the reported party workload and preserve ordinary native-aspect behavior. | **Deferred by owner (2026-11-02)**. No reproducing recording/setup supplied. Existing four-profile normal-aspect/party samples did not reproduce unstable guest cadence; no optimization is claimed. Reopen with new reproducing evidence and coordinate the owner's host-native overhaul. Not counted resolved. |

| S2-27 | Audit ordered character pairs involving Knuckles/Shadow in native 2P VS, two-human local co-op and solo+AI; check independent controls, HUDs, separator and shield/star ownership without leaking another actor's artwork/palette. | **Audit complete for available donors**, 45 native + 9 wide cells exercised. Found S2-28; targeted fixed-build qualification passes. Eight overlapping AI fixtures require isolated-owner observations (16 runs); simultaneous occluded pixels remain unverified. Amy's 12 cells remain unverified. See [matrix and limits](SONIC2_MIXED_CHARACTER_MATRIX.md). |
| S2-28 | With Tails leading, native invincibility children use P2's movement history; both actors also write the same ring. Keep P1/P2 histories separate and stars attached to their actual parent. | **Fixed and runtime-qualified; patch release pending.** Four separated VS/co-op baseline cases expose alternating 32/32-owner E600 entries and misplaced stars. Candidate has 64/64 correct entries per ring and every star centered on its own parent in all six Tails/Knuckles or Tails/Shadow mode cells. Ordinary sampled movement unchanged. 22 focused tests, exact state picture/objects, 1,500-execution playback and 87 rollback checks pass. |
| S2-29 | Shadow is expected to have purple invincibility sparkles. Establish the original donor treatment and apply it per actual actor without affecting Knuckles or native characters. | **Open, source treatment unverified.** Current source and captured SAT/pixels use native S2 star art/palette for Shadow; this is separate from Knuckles colour leakage. Megamix extended effects were outside the accepted character port. No invented purple palette is substituted. Mixed world-monitor policy also awaits the reporter's promised clarification; existing P1 world art / parent-owned attached effects remains explicit. |

## Imported 0.9.4 handoff (owner transfer, 2026-11-01)

Imported at the owner's explicit request from
`C:/temp/Codex/sonic-feedback-release-20261101/sonic2/docs/PLAYER_FEEDBACK_HANDOFF_20261101.md`.
Its historical S2-01..04 identifiers are scoped to that handoff; they are
retained below with an H prefix to avoid colliding with the screenshot intake.
**Three prior delivered outcomes, two explicit deferrals, and two enhancements released in v0.9.7.**
Do not count deferrals as fixes or repeat their bounded probes without new logs.

| ID | Work | Imported status and acceptance / evidence |
| --- | --- | --- |
| H-01 | CPZ purple bottom line (handoff S2-02). | Fixed and released in v0.9.4. Engine `b43c9f2` delivers H-int after its completed row; matched native/wide water phases preserve deep-water images and RAM. Reuse accepted evidence. |
| H-02 | Runtime Options menu also controls gameplay (handoff S2-04). | Fixed and released in v0.9.4. Engine `3182e40`/`c4aea97` gates local input while the actual menu is open, including keyboard/virtual pad and held/released state; authoritative remote input preserved. Reuse accepted evidence. |
| H-03 | Sonic & Tails cannot drown in CPZ2 (handoff S2-01). | Deferred by owner for this pass, not reproduced. Sonic air 29→0, drowning/death, lives 3→2 and retry with air 30 passed. Tails' independent drowning remains unqualified. Reopen only with new occurrence/diagnostics. |
| H-04 | Sonic sprite flicker on spring jumps (handoff S2-03). | Deferred by owner for this pass, not reproduced. Real EHZ1 spring launch retained Sonic in all 120 consecutive native/16:9 frames; exact occurrence/other springs remain unverified. Reopen only with new diagnostics. This is separate from Knuckles glide jitter S2-12. |
| H-05 | Default-on replayable diagnostics across all S2 variants. | Released and verified in v0.9.4; retain as an ongoing release contract. Native, six-seat party, fresh campaign and existing campaign recordings replayed all 6,351 executions without SRAM writes. Saved opt-outs remain respected; reports retain every sidecar. No new Linux release was claimed. |
| H-06 | Above-60-FPS native Sonic 2 presentation. | **Released enhancement in v0.9.7**. Opt-in120/144/display presentation interpolates regular Sonic/Tails camera/actors with native or wide aspect. Two/three submissions per game tick at144, unchanged guest/input/audio/canonical state, source-identity fallback, current save/load, diagnostic replay and rollback pass. Donor parties, transitions and split-screen retain ordinary completed images. See high-refresh qualification below. |
| H-07 | Infinite lives and infinite time in native Sonic 2. | **Released enhancement in v0.9.7**. Separate default-off Practice features preserve life count after native death/retry and cap the stage clock at9:59. Real Sonic/Tails/Knuckles, last-life retry, current save/load, both native competition timers/P2 respawn, launcher Play/relaunch and diagnostic playback pass. Ordinary disabled behavior matches the pre-feature WRAM baseline. Round countdowns and drowning retain their separate rules. |

Published handoff baseline: game/tag `8c121ca`, engine `8424dbb`,
UI `c1867f8`; Windows ZIP SHA256
`c6c7ef9f39e18152b9b17f366110a6f85f61c6b47c9e23f8d1f4296b4e0c0126`.
The prior session recorded 90 production tests plus actual-package
launcher/dependency/replay validation. Evidence stays in its existing task root,
especially `sonic2-v094` and `sonic2-feedback-20261101/decisions.json`.

This transfer covers the standalone/native Sonic 2 application. Sonic 2
campaigns imported inside Sonic 3 & Knuckles remain with that session. Shared
presenter/API and the separately requested audio-filter option remain shared
engine work owned by the S3 session; coordinate pin changes rather than
duplicating them. Drop dash/peel-out remain stopped for the owner's check-in
before implementation or asset work.

## Questions and fidelity notes

- **Save slots and characters:** campaign saves remain independent of the
  selected roster; gameplay characters come from Options. The earlier fixed
  Sonic & Tails portrait contract answered the initial question. The owner's
  later Part 3 feedback now requests roster-aware portraits (S2-16), superseding
  that visual choice while retaining universal progress and Options ownership.
- **EHZ bottom-edge graphics:** the reporter describes an original Sonic 2
  quirk for Sonic, Tails and Sonic & Tails in 4:3, and says Knuckles lock-on
  corrected it. Widescreen reportedly already corrects it. The final sentence
  says it "shouldn't be fixed" for other characters/widescreen, contradicting
  the surrounding comparison. Preserve this ambiguity and check the original
  source before changing fidelity policy; do not introduce corruption as a fix.
- **Knuckles glide sounds:** absence is described as matching the original,
  and acceptable for accuracy. No request to invent an ability sound.
- Experimental labels for Shadow, Mighty and widescreen are context, not
  defects by themselves.
- **2P reference left-edge flicker:** the reporter describes original flashing
  pixels that protrude around the left edge of both viewports during and after
  the title card, absent in the recomp. Treat this as a fidelity review linked
  to S2-21/23, not an instruction to introduce corruption. The reporter explicitly
  questions original technical limitations and emulator differences. Reference
  screenshots use Kega Fusion; establish the field/deinterlace policy and
  source behavior before deciding which original artifacts to preserve.

## Sonic 1 reports preserved separately

Screenshot 1 explicitly describes Sonic 1. These are retained for intake and
shared-cause checks; they are not silently counted as Sonic 2 defects.

| ID | Report | Status |
| --- | --- | --- |
| S1-01 | Launcher sample-rate option does not save. | Shared candidate S2-01. |
| S1-02 | Launcher behavior differs inconsistently from other recomp launchers. | Open; specific differences not supplied. |
| S1-03 | Level title-card slide-in lags. | Reported, unverified. |
| S1-04 | Minor lag near the start/end of some levels. | Reported, unverified; locations unspecified. |
| S1-05 | Spring Yard Act 3 boss floor tiles flicker. | Reported, unverified. |
| S1-06 | Graphical corruption in Labyrinth and Scrap Brain Act 3. | Reported, unverified. |
| S1-07 | Widescreen title-screen water glitches. | Reported, unverified. |
| S1-08 | Widescreen fade transitions mishandle the extended edges. | Reported, unverified. |
| S1-09 | Marble/Spring Yard title demos do not play correctly in widescreen. | Reported, unverified. |
| S1-10 | Widescreen background scrolling breaks at level left boundaries. | Reported, unverified. |
| S1-11 | Widescreen background scrolling breaks at Final Zone's end. | Reported, unverified. |
| S1-12 | Ending cutscene does not render correctly at widescreen edges. | Reported, unverified. |
| S1-13 | Egg Mobile abruptly appears at Final Zone's end. Also seen in 4:3, possibly original behavior. | Reported; compare original before deciding. |
| S1-14 | Three rings near Green Hill Act 1's start jitter when approached in widescreen. | Reported, unverified. |

## Intake sources

All four images were supplied by the owner on 2026-11-01. Repeated Discord
messages have been combined without discarding caveats.

- `C:/Users/Matthew/Documents/ShareX/Screenshots/2026-11/explorer_k7CUC95GwC.png`: Sonic 1 report; a picture of `logs.zip` is visible, but the ZIP itself was not attached.
- `C:/Users/Matthew/Documents/ShareX/Screenshots/2026-11/Discord_5cBrJeeFoT.png`: Sonic 2 first observations, save question, spawn presentation and fidelity caveats.
- `C:/Users/Matthew/Documents/ShareX/Screenshots/2026-11/explorer_2lToKyCgHX.png`: consolidated Sonic 2 list, first half.
- `C:/Users/Matthew/Documents/ShareX/Screenshots/2026-11/Discord_7Ecxtp5yX0.png`: consolidated Sonic 2 list, second half.
- `C:/Users/Matthew/Documents/ShareX/Screenshots/2026-11/Discord_BEUsYyATNB.png`: Part 3, five new reports S2-16..20. Reporter says the message is unfinished and will be edited; continue accepting updates rather than treating it as final.
- `C:/Users/Matthew/Documents/ShareX/Screenshots/2026-11/Discord_JKrz1QE8cV.png`: detailed native 2P CNZ/EHZ separator, transition and clipping differences (S2-21..24), plus the explicit fidelity caveats.
- `C:/Users/Matthew/Documents/ShareX/Screenshots/2026-11/Discord_kDy2LPmoXH.png`: EHZ transition clarification and still references. Reporter identifies the recomp as the images with left/right borders and Kega Fusion as those without, with window options visible. Also adds launcher parity S2-25.
- `C:/Users/Matthew/Documents/ShareX/Screenshots/2026-11/Discord_iVMtvOhlvG.png`: revised Part 3 with launcher parity; S2-16..20 are duplicates and remain single items.
- Owner text, Part 4: many characters cause unstable framerate in 4:3; possible upcoming host-native overhaul is a lead, not a validated resolution (S2-26).

## Current execution

Frozen-engine qualified build for the two menu fixes and Knuckles tumble:
native Windows x64 Release (netplay on; tracing off), SHA256
`e93391aebd410f5b69d5350a4f59200f08800c95449d4b6da5163309fa647694`.
Six relevant CTests pass. Launcher widget/Play/close/relaunch and actual device
rate qualification is retained in `launcher-qa`; spring A/B samples/images and
the actual collision fixture are in `spring-qa` under the same task root.

Green-card qualification: `title-qa`, executable SHA256
`9fb1bb93d15dbcdecb7338aec8f728d8765066adbc235b24bee94711c2c30e49`.
Visual QA rejected a tile-only change that remained blue with Sonic's palette.
Retained VRAM located the actual plane-A fill ($85DA at x200/y16); the final
presentation change follows those cells and their scroll, using the existing
verified donor's green. It does not recolour other party members. Native and
widescreen Knuckles plus unchanged native Sonic passes are recorded, with full
intro WRAM identity. Widescreen card coverage/slide geometry remain S2-19/20.

Shared sample-rate fix was committed and pushed first, as required:
`eb52f8cae5c9f5bb8f289e65fbee7fd14c714866`, branch
`fix/launcher-sample-rate-close-20261101`. The isolated game index now pins
that upstream-available commit. Neither default branch nor any release changed.

Original KiS2 reference is already present in the cached s2disasm repository:
branch `origin/knuckles-in-sonic-2`, commit
`c336fede00092017f48c9d7851e0d3728c4fae2b`. It documents the green-card patch
and separate life-counter, shield/stars and monitor artwork. Owner-directed share lookup found `//server/games/Genesis/Sonic and Knuckles & Sonic 2 (JUE) [!].bin` (3,407,872 bytes, SHA256 `3da827d319a60b6180e327c37e1fe5d448746c1644cce01ada1f2788d1e182f4`). Its first 2 MiB exactly match the current S3&K donor. The extra 256 KiB at $300000 has SHA256 `e184f5982a99c6ebf7799383f16a8624221755786056a3573944598283a4b8cf`. Original source compressed life-counter, grey shield/stars and monitor-patch payloads match at $33AC46/$33AD40/$33B15E; those exact payloads are absent from the S3&K donor. This unblocks original artwork work; no new art fix is counted yet.

Menu A/B evidence: `G:/Temp/Codex/sonic2-owner-feedback-20261101/menu-qa`,
`results.json` plus complete entry/cancel/start frame sequences, title/game
images, input scripts, WRAM and empty dispatch-miss files. Native baseline
and corrected-build final samples differ by 32 gameplay ticks due to a QA
route adjustment; do not claim byte identity for those unaligned samples.
The 796x224 baseline/candidate routes are aligned and final full WRAM is
byte-identical. The first candidate exposed a missing fade-hook registration;
it was fixed in `game.toml`, rebuilt, and the failed route was rerun successfully.

A brief owner-authorized independent focus audit advised finishing this batch
with the existing full-frame routes before adding candidates. Applied: both
menu defects now have built-runtime decisions; no new framework was created.

- Existing checkout is `master` at `177c8ca`, with an already changed engine
  submodule; preserve that work. Local `origin/master` is `8c121ca` (0.9.4).
- Isolated implementation: `G:/Temp/Codex/sonic2-owner-feedback-20261101/game`,
  branch `fix/owner-feedback-20261101`, freshly fetched base `d620e2e`.
- Shared engine is now isolated at
  `G:/Temp/Codex/sonic2-owner-feedback-20261101/engine`; its four dependencies
  use the exact published pins. This avoids consuming parallel S3 worktree edits.
  Its task changes are the published launcher persistence fixes listed below.
- First coherent batch: stationary save background and complete save-menu
  fades; inspect shared sample-rate persistence in parallel with those reads.
- Task-created temporary data: isolated source worktree and one private Sonic 2
  ROM copy in its ignored `game/sonic2.bin`; build/evidence stays under the
  same `G:/Temp/Codex/sonic2-owner-feedback-20261101` root. Clean disposable
  products after qualification; retain useful fix evidence and source.

Delete-menu qualification: `delete-qa/results.json`; all five runtime routes pass. Cancel paths preserve existing file bytes, the empty-card route creates only the chosen new slot on a subsequent normal Start, and Yes clears the selected occupied slot. Unit cases verify sound queue IDs and silence on ignored/read-only/failed actions. Normally closed native baseline/candidate WAV captures both contain 4,553,040 stereo samples at 223,721 Hz, with identical initial audio and changed erase-menu output. The earlier scripted EXIT capture has an unfinished WAV header and is retained only as raw data, not the accepted capture. Candidate SHA256 `9fb1bb93d15dbcdecb7338aec8f728d8765066adbc235b24bee94711c2c30e49`.

Roster-portrait qualification: `portrait-qa/results.json`; nine actual menu entries preserve campaign bytes with no dispatch misses. Five native/donor solo identities, pair, empty companion seats and six-seat native/796-wide party pass; portrait crops match across widths. Native pair art is retained. Focused tests additionally cover Amy bank selection and six distinct colours/identities within the card window. Exe SHA256 `7627dbb8fca8e66cea0910bf6779131f0a5d3318ae0ff534e057fc8960b6c50c`. No Amy ROM was located during the bounded lookup, so its exact real-art image was not visually qualified; the common image renderer and Amy resource-selection branch are tested.

Late-spawn qualification: `spawn-qa/results.json`, exe SHA256 `dbef3f1bef3c2cb5c9745f55fceef8a87476c8e032f07844c6b4492889924681`. The original Sonic reference reveals feet at165, body at166/167; baseline Knuckles remains absent until170. Corrected Knuckles follows the same card edge, with no changed pixels above that edge and no changes after170. All32 per-character A/B RAM and VRAM samples match; Sonic images are identical; native/wide corrected centers match throughout the reveal. Seven focused CTests pass, including displayed-plane scroll/priority occlusion. Initial QA expectation incorrectly excluded the legitimate frame165 feet reveal and was corrected against the original Sonic capture; no runtime re-run was needed.

Original KiS2 item-art comparison: decoded shield+stars have the same2112-byte layout as stock S2; monitor patch is256 bytes at powerup tiles44..51. Some pixel changes are not a single ink substitution, so a global palette remap was rejected as an exact-art implementation. Use the verified original donor for the remaining art fixes.

Knuckles HUD qualification: `knuckles-hud-qa/results.json`, ten actual baseline/candidate runs covering native/wide Knuckles, mixed Knuckles+Tails and unchanged Sonic/Tails. Every original face/name/counter opaque pixel matches the verified source asset and DAC palette; native counter updates from3 to7 through its own updater (fixture sets the input counter and requests refresh, not an extra-life reward). Changes stay within x16..63/y200..215. All paired full RAM/VRAM dumps match. Exe SHA256 `fbbce5cc365bc16be3cf8b5a6b30b664d510f82dcd695e5b8656e13a5019d7f7`. Ten focused CTests pass; real donor/Mods-resource set/get/failed replacement/clear checks pass. A clear-selection return-value defect found by that check was fixed before qualification; replay startup also validates termination of the new resource path.

Current-build recording/replay with the new optional resource: `hud-replay-qa`, 1200 frames, all recorded executions match, no true dispatch misses. This bounded positive run used temporary C storage and was copied with file hashes to G, then the owned C staging was removed. Earlier G image-QA diagnostic streams hit the existing bounded writer queue under host disk/CPU load; those incomplete streams are not accepted replay evidence. Their screenshots and RAM/VRAM comparisons remain valid. No shared writer changes or general benchmark campaign were added. The exact original shield/star/monitor payloads are now verified/decoded, but S2-09/11 remain unimplemented.

Glide-dust qualification: `dust-qa/results.json`, twelve baseline/candidate runs. Actual input establishes glide before positioning above flat terrain; the fixture does not set the slide/dust result. Native P1/P2/P3/P6 and wide P1/P6 pass. Candidate emits 9-10 clouds against zero baseline, retains original four-update cadence/six-pixel offset, and leaves every sampled actor motion/pose byte unchanged. P3/P6 use original short-lived Obj08 children with private original art, avoiding other actors' spindash VRAM. Visible grey dust is verified in the captures, including P6. Runtime fixture executable SHA256 `10891707727909d9694b393fed4479facef03e4dcca70704ccba7524639e9206`. Both emission age and published effects are in current native and rollback state. `dust-state-qa/results.json` qualifies real mid-slide P1/P6 saves with exact full-WRAM restoration and two identical 16-frame RAM/image continuations, executable SHA256 `4530ccb2bcd5e70b41c37eab5ff6eb50912527a44ce4c0e440b5342261934fab`. Ten focused CTests pass. No old-state migration is needed.

Launcher audit S2-25: owner explicitly requested a general audit. Build cache resolves to this game's pinned `recomp-ui`, not a global UI directory. Its old `c1867f8` pin is 82 upstream commits behind current canonical master `a95615751b413e043983944441ed8912066c65ce`; it also carries a six-seat change absent upstream. Updated UI is based on that exact master plus preserved six-seat support. Actual audit found and fixed discarded non-sample preferences on close, P3-P6 edits limited to P2, and missing P5/P6 INI readback. The exact published UI pin is `761f76e740ed483e98befdc5d272f20a5588d521`, based on canonical master plus the six-seat change and controller fixes; it is reachable from the canonical repository branch `fix/sonic2-launcher-audit-20261101`. No floating branch fetch was added to builds. The original submodule URL remains correct: GitHub redirects the old mstan remote to the canonical repository.

Audit fixes: shared engine `d08cc6b` (parent `0d94eb0`, original sample-close fix `eb52f8c`) uses the same preference-save path on Play and Quit, reloading first to retain UI-written bindings. Existing combined keyboard/gamepad device selections are preserved when their displayed source is unchanged. UI fixes remove two-seat limits from Genesis keyboard/gamepad/reset writes and custom pad-mode selection/button counts, and teach INI readback to recognize every supported seat. Acceptance is backed by `launcher-audit-qa/results.json` and actual view captures: close/relaunch retains volume 5, 44100 Hz, filtering, rewind selection, P6 six-button mode and V binding; Play retains volume 10/48000 Hz and opens that output device. P1's binding and P2's combined input remain unchanged. P6's V label is visually verified after a fresh process. The exact tested executable SHA256 is recorded in that results file. `ui-ctest.log` passes all18 selected existing/new tests, including fresh-file P3-P6 keyboard/gamepad/reset/mode isolation; `ctest.log` passes ten game tests. Dashboard, Settings, Mods/resource controls and six controller tabs render normally. This audit does not claim live online/LAN peer qualification; the existing transport-contract issue `beads-3vb.9.3` remains independently owned. Shared fix tracking: `beads-3vb.32` and `beads-0fu.35`.

Additional populated-effect replay qualification: `dust-replay-qa` records and replays 1425 executions with P6 Knuckles using actual jump/glide input and no RAM writes. Sampled native object RAM contains27 P6 dust observations; every recorded execution matches on playback, exercising the newly serialized effect fields. Exe SHA256 `283fd0e222cb2c52b915f988980d58d342b0b6d5c51bc0ba0762a7be85bb0075`. The initial64-frame sampling window was still airborne; extending the observation to256 frames reached the actual unpositioned landing. All57 retained files were hash-verified after copying from bounded C scratch to G; the owned C scratch was removed. This adds evidence to S2-07 without adding another completed item.

Grey item-art qualification (S2-09/11): `grey-items-qa` has actual native/wide Knuckles monitor acquisitions plus unchanged Sonic/Tails A/B images. The initial art-only primary-player comparison changes only the intended SAT size-bit presentation markers in sampled WRAM/VRAM. **That claim does not apply to the final P2 repair**, which intentionally permits party/human P2 rolling monitor hits and publishes non-Tails P2 positions through the native Tails history routine. The baseline P2 fixture stands on the intact monitor; the corrected fixture breaks it and receives both real rewards. Stock CPU Tails and native VS collision branches are retained.

`grey-items-still-qa/results.json` proves original payload pixels and exact cold restoration at paused native/wide P1 and mixed P2 snapshots: changed shield/star pixel matches are635/166,188/144 and495/138 for Knuckles, with no mismatch. In a Knuckles+Tails party, the primary character's monitor theme changes while Tails' visible shield/stars remain native blue; its changed monitor pixels also match the original payload. Every snapshot is acquired by real native monitor collision before native Start pause; cold references use the same build/state with the optional original-art resource absent or present. The oracle honors first-opaque SAT ordering, including overlapping stars. The P2 Tails viewport is framed after its real reward so the bouncing effect is visible; no reward/effect result is fabricated. Early moving-frame comparisons were rejected because end-frame VRAM could describe an adjacent pose. Ten focused CTests pass, including the actual shared VDP private-bank implementation. Qualified executable SHA256 `ae041882cb5c1de10617d6da71bb0f5b57b6f6ede9e73b537eef41e1d21d9f70`, retained as `build/Release/Sonic2-grey-items-1p-qualified.exe`. Native competition uses separate sprite builders; its completed qualification is recorded below.

Task-only owner-ROM fixture: one3.25 MiB copy at `G:/Temp/Codex/sonic2-owner-feedback-20261101/owner-rom-fixtures/kis2-owner.bin`, SHA256 `3da827d319a60b6180e327c37e1fe5d448746c1644cce01ada1f2788d1e182f4`. A .NET exact-path copy avoids inconsistent Python UNC opens. It stays outside the repository/release and needs cleanup after final qualification. The source share is unchanged. Native/wide natural-glide captures for S2-12 are ready in `glide-jitter-qa`; they are evidence to assess, not an accepted jitter fix.

Native competition item-art qualification: `two-player-art-qa/results.json`, executable SHA256 `f1af4ecfc99c7f0b63409e193c721c74e6bd4a4b7b385a368a71c1c6026d0d45`. Both split viewports match612 changed shield/icon pixels (306 each) and234 changed star/icon pixels (117 each) from the independently decoded original payload. Knuckles has8 grey shield pieces and16 grey star pieces across the two views; Tails retains8/16 native blue pieces. Both original grey floating icons appear in both views. Warm images and same-build cold restores are exact. This is a **graphics-state fixture**, with native powerup conditions seeded; it does not claim competition pickup/random-item mechanics. Real native single-player/party acquisitions were independently qualified above.

Competition source/ROM verification located DrawSprite2P piece DBFs at $16DEE/$16E40/$16E9C/$16F00 and the second SAT buffer at $DD00 (LEA at $16A80). The initial candidate only tagged the top viewport; the final code tags both $F800 and $DD00 buffers and accounts for8x16 cells. The lower buffer is uploaded over the same VRAM SAT by H-int, so the pixel oracle reads that viewport's captured RAM SAT rather than assuming the final VRAM dump contains both lists. Ten focused CTests pass, including all eight ordinary/interlace piece sites, 2P tile addressing, both buffers and exclusion of the 2P buffer outside competition. S2-09 and S2-11 are now counted complete and unreleased; S2-04/15/21..24 remain open on their own acceptance criteria.

## 2026-11-02 continuation decisions

- S2-14 accepted on built executable SHA256
  `ea8ed3af90ad41e1a506c42a9fe3ad425bb8ee4db49f110dde0010c93669b517`.
  Ten focused production tests pass. `wide-scenes-qa/options-result.json`
  retains 142 samples per aspect, exact identities and original/candidate routes.
  No guest code, VRAM, physics or normal-aspect pixels changed.
- S2-12 experiment rejected: binding actors to the existing completed-SAT
  history produced identical pixels, RAM and VRAM in all 128 native and 128
  widescreen samples. Restored the candidate rather than retain unused machinery.
  The one-pixel reversal also occurs in native camera tracking; that observation
  alone does not resolve the reported widescreen glide defect. Existing captures
  remain under `glide-jitter-scroll-qa`; `candidate-result.json` records rejection.
- Bounded independent focus check reaffirmed honest counts, rejected further
  comparison machinery/glide sampling without a new hypothesis, and directed
  one runnable title-card coverage/exit candidate using existing scene routes.

- S2-19/S2-20 accepted on executable SHA256
  `332e13db291cdbb6b09c76ce5852ad12a2d5cac662f31f3ce1a426b30c6c60d5`.
  `wide-scenes-qa/card-result.json` retains three matched 169-sample routes:
  Sonic at398/796 pixels and Knuckles at796. All507 paired RAM/VRAM samples
  match. Only37 title-card images per route change; Sega/Options and every
  post-exit image from card-frame228 onward remain exact. The142 native control
  images/RAM/VRAM samples are exact. Static coverage preserves the original
  serrated edge's three-pixel black outline rather than removing original art.
  Ten focused tests pass, including original ink3/Knuckles ink4 detection,
  palette-1 yellow selection, full-width fill and native-width preservation.
  The bounded first run exposed a centered terrain reveal; aligning that reveal
  and donor origin removes the jump at Level_started. Text motion now reaches
  the actual extended edge before source cleanup. Green Knuckles remains hidden
  by the displayed card and is revealed into the same wide world.

- S2-13 accepted on executable SHA256
  `8babdb60d007a9f2d7090ac83d0df9af1b74227fa4922ea2975d4e04486be405`.
  `wide-scenes-qa/sega-result.json` holds211 samples per aspect, including
  per-frame observations through Sonic's first crossing and return. All633
  paired RAM/VRAM samples match; all211 native images are exact. Only47 Sega
  images per wide route change. Options/card/gameplay and every final-logo
  image from Sega-frame208 onward remain exact. Sonic's red-shoe bounds reach
  both added margins; 5,460/33,320 stripe-band margin pixels match the native
  edge pattern. Ten focused production tests pass. The first candidate exposed
  the remaining logo-band width; the final change extends those edge pixels
  while preserving the actual logo and trademark.

## Native competition divider qualification

Executable SHA256 `af56a50c3ddb820767f5e88af01377b8c9d1d0f8466d8470633cfdbbaa46cc87`.
Ten focused tests pass. `vs-divider-qa/results.json`:162 paired samples each in
EHZ and CNZ,148 interlaced images per route,131 changed divider images per route.
RAM/VRAM remain exact; every pixel outside rows216..223 remains exact, including
P1 life rows200..215 and P2 timer rows232..247. The initial32-row candidate was
rejected because it covered the lower HUD. Source inspection establishes that
the larger P1 SAT mask retires at the H-int viewport switch, unlike a permanent
screen divider. Final geometry preserves the actual lower-view origin224.

`actor-results.json` adds six positioned EHZ observations with unchanged paired
RAM/VRAM. P1 native pieces at y196/h32 and y212/h16, plus P2 pieces at y216/h16
and y216/h32, intersect the divider; it stays completely black. The fixture
positions actors and sets airborne state, and does not claim natural traversal
or altered competition mechanics. `hud-results.json` independently checks the
original static P1 life/name and native count tiles:476 opaque pixels per sample,
2,856 per zone/build,11,424 total. Baseline already passes this artwork check;
S2-23 is a verified current-behavior decision, not an invented new glyph fix.
S2-22's separate wrong-region title flash remains open.

S2-26 bounded baseline measurement (no optimization adopted):
`party-cadence-qa/baseline-result.json` covers original two seats, six with extra
Sonic/Knuckles, widescreen six, and native six with Knuckles/Shadow/Mighty donors.
All760 sampled frame-to-frame intervals advance one level tick and one VBlank;
all six actor objects are populated in the large-party cases. File-write timing
proxies have about16.6ms medians and17.6..18.1ms 95th percentiles; these are not
presentation timestamps or proof of an optimization win. Current short idle/
movement samples do not reproduce the report, so do not raise guest timing
budgets or duplicate the host-native overhaul speculatively. S2-26 stays open.
Central issue is related to optimization campaign `beads-2wy8.1`, with its label.

## Native Practice options (H-07)

The launcher exposes Infinite Lives and Infinite Time under Gameplay, in the
Practice package. Each defaults off, needs no extra ROM, persists with the
existing Mods Play commit and is restored on relaunch. Normal death/retry still
runs; the life decrement is compensated before its original native instruction.
The stage clock advances normally until9:59:59, then remains there. Drowning,
competition loser-round countdowns, checkpoints and stage transitions retain
native handling. Native SRAM/campaign data is not rewritten by these options.

Source/ROM-verified hooks: life decrements `$1B248` (`$FE12`) and `$1CCA6`
(`$FEC6`); timer comparisons `$40DEA/$40F22/$40F62/$40F9A`, including the native
P2 clock at`$FED2`. No ROM patches. The selected values are sealed as`c=XY` in
the existing session config; malformed values fail adoption and old configs
mean both off. An active online session refuses changes to these values.
No live online transport qualification is claimed here. Current-build native
states include the selected practice profile and use runtime version4; old
build snapshots need no migration under owner policy.

Evidence: `practice-qa/results.json` covers old-off/new-off, lives-only,
time-only, both, native Tails, donor Knuckles and last-life1. Real boundary death
enters the native death path and actual retry; life3 becomes2 with the option
off, stays3 on, and last-life1 remains1 through retry. Both timer states are
checked at the real expiry boundary. Last-life current-build save/load returns
the same paused image at the cap. Disabled WRAM matches the pre-feature build.
`practice-vs-qa/results.json` proves actual P2 Tails death/respawn (3->2 off,
3->3 on), unchanged P1 lives and both stage clocks. The native P2 respawn uses
routineA before routine2; it does not use P1's routine8 retry.

`practice-launcher-qa` retains actual checkbox clicks, Play persistence and
relaunch images/settings, with the gameplay handoff hidden/muted. Like existing
Mods, closing the pre-game launcher without Play cancels uncommitted feature
choices; the separately requested sample-rate save-on-close remains in place.
`practice-replay-qa` records natural input with both features selected and plays
back all2,082 executions into a fresh profile. It verifies startup/configuration
and normal-play replay; the forced death/time boundaries are the separate
fixtures above, not claimed as part of that recording.

Core runtime/UI/replay executable SHA256
`e94f27202358399b149229a44fc022f54acb4b9f78c2d5550dd41e63cf6bfd07`.
The final active-online mutation guard is qualified by all11 focused tests,
including provider immutability, independent flags, settings persistence and
session adoption. Final executable SHA256
`1e515646a5550121b23990abfec2176367342360e50d7c8ef198be011312b467`
also requalifies disabled behavior and last-life death/retry/time/save-load.
No remaining H-07 acceptance work is queued. H-06 remains the one imported
queued enhancement; the five open reports in the original26 stay separate.


Competition HUD qualification (S2-04, 2026-11-02): `competition-hud-qa`
retains six real title-menu-to-native-competition cases, each at initial lives3
and after the original HUD updater displays independently assigned P1=7/P2=12.
The fixture changes counters to check their display; it does not claim an
extra-life reward. All12 paired WRAM/VRAM observations match the frozen H-07
baseline `1e515646a5550121b23990abfec2176367342360e50d7c8ef198be011312b467`.
Changes are restricted to the replaced life HUD areas; default Sonic/Tails
images remain exact. Correct IM2 ten-bit Y coordinates select each live SAT.
A private VDP removes the old shapes before donor actors, then source-backed
high-priority glyphs are drawn afterward; guest memory is never changed.

Sonic/Tails use their original REV01 life art, including the Japanese Miles
name selection. Knuckles uses the verified original lock-on face/name and
orange/white palette when supplied. Without that optional resource, the
existing verified S3&K gameplay head and a correctly coloured full name are
used. Shadow/Mighty and the available Amy path use their verified gameplay
heads and the original native menu font, with each player's native life digits.
No Amy donor is available for an actual Amy visual check; the shared identity
path is present, not claimed as asset-qualified. No ROM/art payload is added.

The independent original-art oracle passes5,804 opaque glyph/colour pixels,
including both life values and the fallback name. Actual cropped HUDs were
visually reviewed; the final generic head crop retains its complete width.
All11 focused tests pass. Complete native/reversed/original-Knuckles A/B build:
`53cff0d168b1dbdbab5ffdf0e62601b7b98461f1c3a778fdcc714fc4736a55cd`.
Final custom-name/head/fallback subset build:
`acb43c6748839726b3ae5cbd68f1c07393cafc7d47dc834436f6f7708b650e7b`.
Earlier candidates with premature ROM decode or incorrect raw-font origin were
rejected and corrected; none is treated as accepted evidence.

A bounded independent focus check recommends S2-22 next, before the larger
S2-15 viewport work or H-06 presenter adapter. Stop further HUD matrix expansion
now that its affected final paths pass. S2-12/S2-26 retain their existing
unreproduced/rejected evidence and await a new occurrence or concrete hypothesis.
Shared S3 presenter dependencies (30083ac/42fd35f/3dfcc2c/471d51f/bbe5fa9)
are prepared on the isolated engine branch but are not yet adopted by the game
pin or counted as a validated H-06 result. The engine remains at d08cc6b during
matched S2-04 qualification.

Owner clarification (2026-11-02): "if recordings don't reproduce it (or if I didn't provide any), defer" applies to S2-12 and S2-26. No further sampling or speculative changes are queued for those two reports. The original26 now contain22 validated resolutions,2 explicit deferrals and2 active reports (S2-15/S2-22); H-06 remains a separately imported queued enhancement.


Masked H-int repair (partial S2-22, 2026-11-02): engine `024db9b` on
`fix/sonic2-competition-hint-20261102`, tracked separately as `beads-3vb.33`,
is now adopted by the game dependency pin. It retains masked IRQ4 until the
CPU accepts it, coalesces repeated requests and preserves V-int priority/IE1.
The pending latch participates in the existing file, rollback and state-hash
ownership; the older raw own-backend file container is rejected as GROWNS2
rather than misread under the new GROWNS3 layout. No donor or guest data changes.
The pending/acknowledgement/enable behavior was cross-checked against the
[Genesis Plus GX VDP implementation](https://raw.githubusercontent.com/ekeeke/Genesis-Plus-GX/master/core/vdp_ctrl.c);
this repair is independently authored and copies no code from that source.

`vs-divider-qa/trace-{ehz,cnz}` localizes the bug. During level-loading interrupt
masking, EHZ's split-plane switch happens at output row432 instead of216; the
sampled CNZ frame loses the switch entirely. The accepted candidate services the
pending event as soon as permitted. The EHZ wrong-P2 change drops36,525?0 pixels;
CNZ drops41,686?229 pixels restricted to115x2 rows immediately below the divider.
The source-driven residual transient is on EHZ P1 and at the CNZ seam. This
**does not establish the reporter's opposite zone-specific Kega reference**;
S2-22 therefore remains partially resolved, not counted as another completed
report. The small independent focus check explicitly preserves that caveat.

`flash-result.json` and `final-results.json` in the same evidence directory
retain actual observations. Final native EHZ/CNZ real-menu routes pass71/72
separate eight-tick rollback checks with zero divergences. A separate CNZ
file-state run restores an identical paused picture; fresh diagnostic playback
including SAVE/LOAD matches all1,271 recorded executions. All11 focused tests
pass. Qualified executable SHA256
`99824ef4ec2c9012f876ae7d086baec133c82369495df5b6b232fff0ad375c91`.
Its compiled engine label is d08cc6b-dirty; the exact qualified source changes
are committed as024db9b. The temporary raster trace was removed before this
final build. An earlier fixture mixed external file operations into the
rollback-only probe and was rejected; another copied profile disabled replay
recording and was corrected. Neither is a passing validation claim.

S2-15 now proceeds from this stable corrected dependency. H-06 presenter work
remains separate and unadopted; no more artificial flash adjustments are queued.

S2-15 work in progress (2026-11-02): current uncommitted candidate reuses the existing completed SceneFrame and bounded placement pool rather than introducing a second scene/loader framework. Native2P mapping uses its original alternate pattern word and8x16 cell shape, cross-checked against the actual VDP in a focused test. The current renderer uses live per-field VSRAM so the source's even-row plane scroll is preserved independently of odd actor-camera coordinates. Existing native EHZ control matches all162 captured WRAM/VRAM/PNG samples exactly. First32:9 EHZ prototype shows five active world objects versus one in the native view. Those are preparation/observations, not an accepted S2-15 result. Current qualification is `competition-wide-qa` via `qualify_wide_competition.py`; source remains uncommitted until its acceptance checks pass. H-06 is still unimplemented.


Competition widescreen qualification (S2-15, 2026-11-02): `competition-wide-qa`
retains real-menu EHZ16:9/32:9, CNZ32:9, MCZ32:9 and Knuckles-P2 routes. Both
viewports render original alternate2P mappings and8x16 cells from the current
world; the renderer preserves source per-field VSRAM independently of odd
actor-camera coordinates. The existing completed SceneFrame supplies sprites;
no second scene framework was added. The existing bounded placement pool now
covers both camera windows, including the original CNZ2P object layouts.
Focused tests cover native-object adoption, preserving a moving root inside
an active view, clean retirement/respawn and retaining loader ownership when
widescreen is turned off. The extended simulation retains its VBlank wait;
loading, special stages and the disabled mode retain their previous CPU budget.

All five movement cases retain native competition mode, separate cameras and
independent lives. Source clock samples advance32 ticks for the first32-frame
input setup,31 per30-frame wait plus capture boundary, and11 for the final
10-frame wait plus boundary. This is guest-cadence evidence, not a physical
monitor/FPS benchmark. The independent source-art oracle verifies12,856 opaque
HUD pixels, including the original Knuckles face/name/colours, and both black
dividers. The earlier native-aspect control remains byte-identical across all
162 paired WRAM/VRAM/PNG observations (`vs-divider-qa/native-proto-ehz`).

The final EHZ mechanics fixture positions both characters at the original
bridge and near the original goal; it bypasses travel, never writes standing,
finish or result flags. Both characters land through normal physics and refer
to an actual Obj11 bridge. They then cross the signpost through normal input;
their independent timers stop and the real game enters2P results. The results
background now repeats its populated40-column motif across the whole canvas.
11,424 margin pixels match the source motif; all six matched WRAM snapshots and
the native results center remain unchanged by that final pattern correction.
No complete-stage traversal or exhaustive object-type certification is claimed.

All11 focused tests pass. Core movement/visual/state/replay build SHA256:
`520ff34774fa47f8b8f435266b4df97290d0e37a804c9c6f4199d361a42dd6b7`.
It passes83 eight-tick rollback checks, an identical-picture/current-object
state roundtrip, and all1,467 fresh diagnostic playback executions. Final
moving-root guard/results build SHA256:
`f2eeab2a363f36354b900da2aaaac0467b95e6c3d866a0b40d1a9bdcbf0bc120`
requalifies all11 focused tests, the actual both-player bridge/finish/results
fixture and all83 rollback checks. `results.json`, `pixel-results.json`,
`state-results-core.json`, `state-results.json` and `mechanics/result.json`
retain individual evidence and exact identities.

Next H-06 uses the prepared shared presenter API. Its native-aspect scene
capture must be read-only and use separate presentation history: enabling
interpolation must not mutate the canonical SceneFrame history, object flags,
loader activation, width, input or audio. Fractional draws must use owned
VRAM/world/palette/scene snapshots and restore the ordinary drawing context.
Native vs widened activation stays separate; unsupported scenes retain their
ordinary images. S2-22 remains partially resolved with its existing reference
fidelity caveat, and S2-12/S2-26 remain explicitly deferred by the owner.


High-refresh qualification (H-06, 2026-11-02): game `cf2807b` reuses the
shared presenter API instead of adding another pacing loop. Native-aspect
read-only scene capture has separate histories/identities and never changes
the serialized scene, guest sprite flags, loader ownership, requested width
or CPU divisor. Fractional draws use owned VRAM, world, per-row palettes/scroll
and sprite records. Unsupported scenes, changing sprite identities/poses and
teleports retain the ordinary completed image. Configuration/reset/load discard
presentation history. This remains default-off and adds one tick of visual
latency. Regular Sonic/Tails parties of up to two slots are supported; donor
parties, transitions, menus, special stages and split-screen retain ordinary
completed images. No broader interpolation coverage is claimed.

`high-refresh-qa` contains six matched native/32:9 runs (frozen S15 baseline,
new rate-off, new144). Each of four baseline comparisons retains all1,600
execution hashes/input records,64 full WRAM/VRAM/ordinary-PNG pairs, and the
complete23,884,844-byte WAV identically. Native144 produces3,839 submissions,
876 interpolated, with237 ticks showing distinct fractional pictures;32:9
produces3,838/685 and277 such ticks. Both include hundreds of three-submission
ticks, rather than rounding144 to120. These are SDL submission/owned-image
checks, not a physical-monitor certification. Both fractional samples were
visually inspected. A native paused save/load restores the exact picture and
objects; fresh playback matches all1,500 executions including SAVE/LOAD;87
eight-tick rollback checks pass. All15 focused game/shared tests pass, including
actual native VDP sprite interpolation endpoints/midpoint and canonical-state
isolation. Qualified executable SHA256:
`3fbf71ebea6ca38a2d8318bd6b475a84724751571bd6872b3234d5b351937696`.

Actual hidden launcher clicks found144 displayed but0 saved on normal close.
Engine `83ebcbf` preserves shared Mods edits while importing controller bindings;
the regression test covers rate, audio filter bypass, widescreen, diagnostics
and the saved binding together. Actual select144/close/relaunch now passes
(`high-refresh-qa/launcher`), with all15 tests on executable
`87e9bed702038610a242a0dd78de4f5bfd02c3f60fc6fdac159e3c0008a64b9`.
Tracking `beads-3vb.34`. An initial replay test stopped at its frame limit before
reading the recording's terminal marker; the corrected higher-limit playback
passes. That early stop is not a divergence or a passing replay result.

Before release, current remote master `25291b5`/v0.9.6 was discovered to contain
the newer qualified engine optimization work. Game merge `836da28` and engine
merge `678989b` preserve that released base, the IRQ4 repair and presenter/API.
This integration is prepared, not yet release-qualified. Reuse the existing
native IPO/fresh-normal-PGO workflow and target representative runtime checks;
do not silently replace the current release with its older engine base.

S2-22 bounded original-program comparison: `vs-divider-qa/interp-ehz` and
`interp-cnz` explicitly enable and assert `GENESIS_FORCE_INTERP=1`. The same
REV01 ROM/menu routes execute loading and interrupts directly through the ROM
interpreter. Both show a brief P1 transient (EHZ frames126/128, CNZ130/132);
this also differs from the reporter's zone-specific Kega pattern. Native EHZ
has the same39,085-pixel P1 transient two sampled frames earlier; native CNZ
has its previously documented seam transient. This establishes a remaining
source-scheduling/reference-fidelity difference, not exact emulation parity.
The erroneous whole-P2 flash stays repaired. Exact Kega ROM revision, region
and field/deinterlacing setup are unavailable; do not invent or relocate
flicker to match those screenshots. The residual remains explicitly partial,
separate from the two owner-deferred intermittent reports.


Current-base integration gate (2026-11-02): engine `678989b` plus fixture-only
compatibility repairs (committed `ec523d5`) passes all20 focused tests, including
audio worker/original parity, native row ownership, packed/indexed pixels and
HD-state pointer restoration. Fixtures now supply the new IRQ service stub and
keep paired audio-output symbols separate from the frozen reference. No frozen
oracle body was edited. The merged executable
`b8b79330db002177ea1a3878f697510cb8a60ffb34478031008dfd1e2e5c0865`
passes four matched native/32:9 off/144 runtime cases: all64 WRAM/VRAM/ordinary
PNG samples and the full WAV per case match the pre-merge qualified build.
Within each aspect, all1,600 execution/input records match at off and144.
Native144 emits3,840 pictures,876 interpolated; wide144 emits3,839,678
interpolated. Distinct fractional-motion counts remain237/277. Results:
`high-refresh-qa/integration-equivalence-results.json` and
`integration-run-results.json`. This accepts current-base integration; fresh
normal PGO and the final package remain the next gates. The bounded focus
review recommends stopping additional presenter/flicker experiments and moving
directly through these delivery gates.


## Delivered release: v0.9.7 (2026-11-02)

Published [Sonic the Hedgehog 2 v0.9.7](https://github.com/mstan/SonicTheHedgehog2Recomp/releases/tag/v0.9.7),
normal/latest Windows x64 release, from game `f4cc96f`, engine `ec523d5`, UI
`761f76e`. All currently addressable fixes and both imported enhancements are
delivered. Counts remain **23/26 original reports resolved**, two owner-deferred
(S2-12/26), and one partially repaired reference-fidelity difference (S2-22).
No additional presenter or artificial-flicker work is queued for this release.
The Beads issue remains open for these three residual reports; none is counted
as fixed. Resume implementation with new reproducing/reference evidence.

The final configuration explicitly retains the released engine defaults:
**BATCHED** VDP, faithful FM batching, fast rollback digest, qualified workers,
native IPO and freshly trained normal PGO. An inherited SCALAR cache setting
was discovered after the initial publication. That preliminary archive was
returned to draft and replaced only after the corrected build passed the
runtime/package gates. Its old archive/executable hashes (`1efa2818...` /
`feb98f4a...`) are **superseded**, not current delivery identities. The source
tag was not changed. The corrected packaged runtime itself reports BATCHED.

Fresh normal PGO uses one completed 1,700-frame Sonic/Tails training run
(128.187 seconds under instrumentation), with zero overflow drops. The earlier
optional SCALAR competition training exceeded its 180-second tool limit and
was excluded. It is not a completed training or correctness result. Normal USE
reports zero invalid/new callsites and profile data for 12,692/13,014 functions
(97.5%). The 100% instruction figure refers only to training-observed dynamic
instructions, not unseen gameplay. PG0188 searches for additional PGCs beside
the immutable SHA-named owned PGD; actual training data was explicitly merged.
Final PGD SHA256:
`a39410ae4fecc863457b01ec757c802b16ea576e73ff35611e4983b0c4663977`.
No new FPS or universal performance gain is claimed from this refresh.

**Final executable SHA256:**
`f6c69c38b443373deb9208e216d4f1a6a32d76bfe23e24b116b86717bf2b965d`.
All **22 focused tests** pass, including both renderer variants' sprite tests.
Native and 32:9 presentation at 144 Hz retain all 128 paired full WRAM/VRAM/
ordinary-PNG samples, both complete WAVs, and all 3,200 execution/input records
exactly against the accepted integration control. Each emits 3,840 submissions;
native/wide include 876/685 interpolated submissions and 237/277 ticks with
distinct fractional motion. These are submission/image checks, not physical
monitor certification. Final save/load restores the exact picture/objects,
fresh playback matches all 1,500 executions including SAVE/LOAD, and 87
eight-tick rollback checks pass. Both competition views with rate144 selected
retain all ten accepted S2-15 WRAM/VRAM/PNG observations while using ordinary
split-screen presentation.

**Current ZIP:** `SonicTheHedgehog2Recomp-windows-x64-v0.9.7.zip`, 8,018,542 bytes.
**SHA256:** `8da9795e0bf78811ad35d7cbbb68d0fb7c1727c280ab375c14b30fe64462a8aa`.
GitHub's uploaded asset digest matches. Archive allowlist/dependency closure,
extracted launcher144 select/close/relaunch with a minimal system PATH, and the
bundled replay helper against all 1,500 executions pass. The helper's runtime
also confirms BATCHED. The ZIP contains app-local SDL/MSVC DLLs, assets,
notices and feedback tooling; no ROM/donor artwork, settings/saves, profiler
data, development outputs or forbidden loading mods. Native owner saves and
settings were not changed.

Evidence stays under `G:/Temp/Codex/sonic2-owner-feedback-20261101`, especially
`high-refresh-qa/release-*`, `high-refresh-release-qa`,
`competition-wide-qa/release-results.json`, `pgo-batched-training/results.json`,
`package-check-profile-097/package-results.json`, and
`package-batched-replay-097.runtime.log`. The source, final archive, profile
inputs, relevant frozen controls and bounded raw evidence are retained for the
residual reports. Generated build scratch can be removed after that follow-up.
Disposable package extraction/reproduction copies and the superseded scalar
ZIP are cleaned after retaining their verification logs and identities.


## Post-release character-matrix intake (2026-11-02)

The reporter requests mixed-character checks across VS, local co-op and solo
with AI, especially Knuckles' grey items and Shadow's purple invincibility
sparkles. They intend to clarify the desired mixed-party policy later; do not
invent a new monitor-theme policy in advance. Current implementation separates
P1-selected world-monitor artwork from parent-owned shields/stars.

They now question the earlier Kega comparison, and identify the separator
leaking actors/terrain as the confirmed issue. Preserve the independently
qualified divider and masked-IRQ repair. Exact field/flicker fidelity remains
unverified until an identified Sonic 2 REV01 ROM on an NTSC-U North American
Genesis can be compared. No confirmed hardware tester is available in this
session; no one was contacted. Request console/ROM identity, capture-chain and
field-handling details rather than accepting a video's title alone.

Both reporter links are now supplied. YouTube's oEmbed metadata identifies
[VfFZUNzXx4M](https://youtu.be/VfFZUNzXx4M) as **Sonic 2 Competition VS Mode |
Hardware Recording**, by FramePlay, and
[D81W5nkUqt4](https://youtu.be/D81W5nkUqt4) as **Sonic the Hedgehog 2 two player
race 60fps**, by arronmunroe. This verifies the titles/authors only: actual
frames, hardware provenance, ROM revision/region and field handling have not
been verified. A re-encoded 30/60-fps clip
is not sufficient by itself to distinguish native field quirks from capture
processing. This is a sampling/reference limitation, not evidence of a new
recomp defect.

S2-27 uses existing native fixtures and released executable SHA256
`f6c69c38b443373deb9208e216d4f1a6a32d76bfe23e24b116b86717bf2b965d`.
The bounded matrix covers 14 eligible ordered pairs involving Knuckles/Shadow
among Sonic, Tails, Knuckles, Shadow and Mighty, plus a Sonic/Tails control,
across VS/co-op/AI (45 native-aspect cells). Wide coverage samples both
Knuckles/Shadow directions and the control in all three modes (9 cells).
Four Amy-related ordered pairs across three modes are unverified without a
verified Amy donor. P1/P2 cannot select duplicate identities by design.

Gameplay/input checks precede the graphics fixture. The latter freezes actor
movement, seeds source shield/star objects and parent pointers, and checks
rendering/ownership only. This explicitly bypasses obtaining power-ups and
must not be reported as natural acquisition. Real Knuckles pickup coverage
from S2-09/11 is retained separately. Raw evidence:
`G:/Temp/Codex/sonic2-owner-feedback-20261101/mixed-effects-matrix-qa`.
