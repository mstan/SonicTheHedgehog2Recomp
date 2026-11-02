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

Current intake: **26 reported defect/action items; 12 validated fixes, 14 remaining**. One save-menu
question is answered below. All defect reports still require checking against
the current source/build; do not assume the reporter used the latest version.

| ID | Report and acceptance | Status / evidence |
| --- | --- | --- |
| S2-01 | Launcher sample-rate choice does not persist. Save the chosen rate, restore it on relaunch, and use it for the output device. | **Validated fix**, not released. 0.9.4 already saves on Play. Owner explicitly requested save-on-close too; shared engine `eb52f8c` adds that path. Actual widget clicks, Play/save/relaunch at 44100 Hz, close/save/relaunch at 48000 Hz, and actual device rates pass. Tracking `beads-3vb.32`. |
| S2-02 | Save-select background moves when changing cards. Keep the background stationary while the cards/cursor scroll, including widescreen. | **Validated fix**, not released. Baseline background moves; candidate background stays pixel-identical while cards move at 320x224 and 796x224. |
| S2-03 | Save-select appears over a blank native Options menu without correct fades. Fade the complete save screen in/out on entry, start, cancel and relevant return paths; no exposed underlying screen. | **Validated fix**, not released. Actual complete-frame entry, cancel and game-start fades at 320x224 and 796x224; original native palette timing retained, real title/game destinations reached, no dispatch misses. |
| S2-04 | Native 2P competition HUD assumes Sonic/Tails. Show each actual competitor's identity, life artwork and applicable name colours. | Open. |
| S2-05 | Knuckles has the blue Sonic title card. Use the original Knuckles-in-Sonic-2 green title-card treatment. | **Validated fix**, not released. Original two-tile ink-4 change plus the donor's original $080 green applied to those displayed plane-A cells. Actual 320/796-wide images are green, Sonic's card stays blue, matched intro RAM is byte-identical, and dispatch misses are empty. Native Sonic palette remains available to mixed parties. |
| S2-06 | Knuckles visibly spawns as the title card slides away. Match the original entry/spawn presentation without a late pop-in. | **Validated fix**, not released. Reproduced a late host-only reveal: native Sonic is exposed with the card, Knuckles waited until frame170. Donor actors now use the actual card planes for occlusion and the stage camera before Level_started; feet/body reveal at165..169. All32 native A/B RAM/VRAM samples are identical, Sonic images unchanged, native/wide center matches. Native 2P retains its previous gate. |
| S2-07 | Knuckles glide landing/ground slide lacks the original dust effect. Restore the source-backed dust at the real slide/landing trigger. | **Validated fix**, not released. Source cadence, original atlas and six-pixel offset restored. Real glide/slide A/B checks pass for P1/P2/P3/P6, native and widescreen; no sampled actor-motion change. Current-build P1/P6 save/load restores full WRAM and yields 16 identical continuation images/RAM samples. See dust qualification below. |
| S2-08 | Knuckles HUD still says Sonic and uses Sonic's life icon. Use Knuckles' icon/name and original orange/white name treatment. | **Validated fix**, not released. Exact original lock-on face/name art and palette-0 colours; native/wide/mixed-party images match every original opaque HUD pixel. Native numbers update, other Sonic/Tails images and all matched RAM/VRAM stay unchanged. Select the verified original KiS2 ROM resource under Knuckles; no extra feature toggle. Current-build 1200-frame diagnostic replay passes with that resource restored. |
| S2-09 | Knuckles shield monitor/effect remains blue. Use the original Knuckles-in-Sonic-2 grey monitor and shield. | Open. |
| S2-10 | Knuckles uses a walking pose after a vertical spring (early EHZ1 example). Use the original airborne spring flip animation. | **Validated fix**, not released. Real EHZ1 subtype-03 spring at (2220,568): baseline walking frames 1..7 become source tumble frames $32..$3A. All 24 matched samples have identical position, velocity, status and tumble angle. The fixture positions the player but never sets the spring result. Focused both-facing/wrap tests pass; zero dispatch misses. |
| S2-11 | Knuckles invincibility monitor/sparkles use Sonic 2 art. Use the original Knuckles-in-Sonic-2 grey monitor/sparkles. | Open. |
| S2-12 | Widescreen Knuckles sometimes jitters in midair while gliding. Reproduce and remove jitter without changing glide physics. | Open. |
| S2-13 | Sega logo animation is not extended for widescreen. Extend its intended scene consistently through the margins. | Open. |
| S2-14 | Options background is not extended for widescreen. Fill the extended area with the correct background across navigation/fades. | Open. |
| S2-15 | Native 2P competition is not extended for widescreen. Extend both viewports, camera/object coverage and HUD appropriately while preserving competition mechanics. | Open. |
| S2-16 | Save cards always show Sonic & Tails. Update portraits from the characters currently chosen in Options; keep campaign saves roster-independent. | **Validated fix**, not released. Current Options roster supplies original Sonic/Tails/Knuckles solo portraits and verified Amy/Shadow/Mighty gameplay art. Nine runtime cases cover five distinct available characters, native/wide six-seat parties, the original pair and empty seats; campaign bytes stay unchanged. Focused tests cover Amy bank selection, all six identities, bounded layout and unchanged progress. No separate card selector was added. |
| S2-17 | Delete-mode UI sounds are missing: entering delete mode, scrolling, choosing a file and successful deletion. Match the source-backed events, including silence for ignored actions. | **Validated fix**, not released. Focused event/ignored-action/failing-commit tests and actual native generated-audio A/B pass. Source-backed event order uses native S2 cues: Checkpoint $A1 for enter/confirm, BonusBumper $D8 for delete navigation, TallyEnd $C5 after successful disk commit. S3&K uses Starpost/SmallBumpers/Perfect; this preserves the established S2-native sound policy rather than claiming identical timbres. |
| S2-18 | Delete-mode cancellation differs from S3&K. Start on an empty card or Delete, B anywhere, and choosing No at confirmation should leave deletion mode without starting/deleting a file. | **Validated fix**, not released. All five actual routes pass with disposable saves: empty, No, B at confirmation, Delete card and Yes. Unit cases also preserve read-only data and fail commits without a success sound. |
| S2-19 | Widescreen title cards do not cover the extended viewport properly. Extend the intended card elements and preserve the normal-aspect layout. | Open. Separate from Knuckles' green artwork S2-05. |
| S2-20 | Widescreen title-card slide-out looks glitched. Animate the complete card coherently through its exit without wrapping or edge remnants. | Open. Separate transition acceptance from S2-19 static coverage. |
| S2-21 | Native 2P separator differs during title cards: recomp bar is thicker and briefly flashes blue; reference CNZ bar is thinner and blank. Compare actual interlaced fields/display policies and correct unintended size/colour changes. | Open. User is unsure which differences are original limitations or reference-emulator presentation; do not assume Kega Fusion is a hardware oracle. |
| S2-22 | 2P title-card transition flashes the wrong region. In reference CNZ P1 flashes; recomp flashes P2. In reference EHZ part of the separator flashes; recomp still flashes P2. | Open. Validate CNZ and EHZ independently, including field ordering and the actual source-triggered event. |
| S2-23 | Native 2P corruption below P1 extends across the recomp screen, protruding into P1 and clipping the life icon/count. Reference shows only a small left-edge disturbance. | Open. Preserve the complete intended HUD; compare matching scenes/fields and distinguish rendering defects from original artifacts. |
| S2-24 | Level graphics and characters can draw inside the native 2P separator (particularly EHZ). Keep the intended divider opaque to both viewports. | Open. Test both terrain and actors crossing its boundary, with intact neighbouring HUD rows. |
| S2-25 | Bring Sonic 2 launcher behavior into parity with the other recomps. | **Validated fixes and general audit**, not released. Builds the exact current upstream UI base plus six-seat compatibility/fixes, rather than the stale pin. Play/close/relaunch preserve tested preferences; P3-P6 keyboard/gamepad/reset and pad modes target the selected seat; P5/P6 bindings read back correctly. 18 focused UI tests, 10 game tests and real hidden launcher checks pass. See launcher audit below. |
| S2-26 | Large companion parties make the game's framerate unstable in 4:3. Restore stable normal-speed play with the reported party workload and preserve ordinary native-aspect behavior. | Open. Owner notes a possible forthcoming host-native optimization overhaul; coordinate and measure the exact 4:3 workload against it rather than duplicating shared work or assuming it fixes this report. Distinguish host frame pacing from actual guest-loop cadence and retain matched roster/input/host measurements. |

## Imported 0.9.4 handoff (owner transfer, 2026-11-01)

Imported at the owner's explicit request from
`C:/temp/Codex/sonic-feedback-release-20261101/sonic2/docs/PLAYER_FEEDBACK_HANDOFF_20261101.md`.
Its historical S2-01..04 identifiers are scoped to that handoff; they are
retained below with an H prefix to avoid colliding with the screenshot intake.
**Three delivered outcomes, two explicit deferrals, two queued enhancements.**
Do not count deferrals as fixes or repeat their bounded probes without new logs.

| ID | Work | Imported status and acceptance / evidence |
| --- | --- | --- |
| H-01 | CPZ purple bottom line (handoff S2-02). | Fixed and released in v0.9.4. Engine `b43c9f2` delivers H-int after its completed row; matched native/wide water phases preserve deep-water images and RAM. Reuse accepted evidence. |
| H-02 | Runtime Options menu also controls gameplay (handoff S2-04). | Fixed and released in v0.9.4. Engine `3182e40`/`c4aea97` gates local input while the actual menu is open, including keyboard/virtual pad and held/released state; authoritative remote input preserved. Reuse accepted evidence. |
| H-03 | Sonic & Tails cannot drown in CPZ2 (handoff S2-01). | Deferred by owner for this pass, not reproduced. Sonic air 29→0, drowning/death, lives 3→2 and retry with air 30 passed. Tails' independent drowning remains unqualified. Reopen only with new occurrence/diagnostics. |
| H-04 | Sonic sprite flicker on spring jumps (handoff S2-03). | Deferred by owner for this pass, not reproduced. Real EHZ1 spring launch retained Sonic in all 120 consecutive native/16:9 frames; exact occurrence/other springs remain unverified. Reopen only with new diagnostics. This is separate from Knuckles glide jitter S2-12. |
| H-05 | Default-on replayable diagnostics across all S2 variants. | Released and verified in v0.9.4; retain as an ongoing release contract. Native, six-seat party, fresh campaign and existing campaign recordings replayed all 6,351 executions without SRAM writes. Saved opt-outs remain respected; reports retain every sidecar. No new Linux release was claimed. |
| H-06 | Above-60-FPS native Sonic 2 presentation. | Queued; no native S2 adapter implemented. Reuse the S3 session's shared presenter/API. Independent presentation deadlines must support rates such as 144 Hz without rounding to 120; guest/input/audio/canonical state stay at their normal cadence. Interpolation history is presentation-only, separate from widescreen activation/culling. Qualify the native adapter before release. |
| H-07 | Infinite lives and infinite time in native Sonic 2. | Queued; no implementation started at transfer. Elective, opt-in features; ordinary behavior must remain intact when off. Validate real death/retry, timer expiry and transitions. |

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
