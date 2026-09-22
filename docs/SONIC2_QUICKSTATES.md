# Sonic 2 quickstates and stage thumbnails

Tracking: `beads-5dyp.8` and `beads-5dyp.9`; shared opt-in hooks: `beads-3vb.6`.
Sonic 2 only. The trilogy
campaign branch was inspected as a rendering reference, not merged.
Development worktrees: `_wt-sonic2-states-engine` and
`_wt-sonic2-states-game`, branch `feature/sonic2-states-thumbnails`.
Engine baseline `35620e2`, game baseline `21cbaba` (v0.6.1).

Owner acceptance 2026-09-22: in-game hitch/companion behavior and the revised
numbered/static save cards were approved, followed by an explicit request to
commit and integrate this work into master. The earlier pending-acceptance
notes below are historical checkpoints. P1 Super forms for non-Sonic characters
are a new, separately tracked follow-up (`beads-5dyp.10`).

## Controls and compatibility

- Escape -> Save states: choose slot 1–9, Save state or Load state.
- Shift+F1…F9 saves; F1…F9 loads. Controller shoulders retain their existing
  slot-1 shortcuts. All use `native_save_<slot>.bin` beside the executable.
- Saving queues the next completed native gameplay tick. Levels, native VS,
  native pause and the half-pipe gameplay loops have source-grounded boundaries.
  Menus/loading/results do not: an unfulfilled request expires without replacing
  a previous state. Loading is allowed from another scene, including startup.
- States are local-only and private to a compatible source/compiler build.
  Keep the matching executable. Old `GROWNS2` machine-only files are rejected.
  Keep portable campaign SRAM for progress across releases.
- The roster, donor-mod switches, campaign-mod switch and custom-renderer
  mode/aspect must match. A quickstate cannot enable a missing/unverified donor
  or silently change Options. All four character controllers, companion AI,
  solid ownership, sprite publication and expanded object-loader state restore.
- Special stages still always use native Sonic and Tails. The selected party
  returns through normal level initialization after leaving the half-pipe.
- Campaign quickstates also require the same resolved campaign-file path.
  Loading rewinds the active slot in memory, retaining the current file writer,
  protection flags and other seven slots. No campaign disk write happens during
  quickload; ordinary subsequent autosave commits the active slot. External
  edits remain protected by the existing store's optimistic-write checks.

## Implementation

`GameSpec` opts Sonic 2 into the `GRHOST2` envelope and a resumable-boundary
predicate. Other games retain the legacy path. Container validation checks the
build fingerprint, ROM CRC, exact section sizes, payload CRC and FM blob length
before mutating the machine. The decoded body is staged, with a machine rollback
for I/O failure. Save replacement is atomic; failed writes retain the old file.
Immutable ROM art is rebound, never serialized as process pointers.

End-of-display-frame is not a safe host snapshot boundary: tests captured
`inside_player=1` in normal four-player play. The optional scheduler request
therefore holds the next qualifying WaitForVint until that wall frame has
completed VDP/Z80/audio work. Only then does the runner serialize. The default
path has no request/hold. Deferred audio writes are serialized and stale device
playback is discarded on quickload.

REV01 return addresses, confirmed against the byte-matched s2disasm listing:
`$436E` Level_MainLoop, `$5220` half-pipe intro, `$5268` half-pipe main, `$13BC`
Pause_Loop. Existing `$51FC` SS resume is appropriate only before
SpecialStage_Started; `$5250` is the source-backed started-loop entry added to
discovery. Generated C is not edited.

## Stage artwork

Current card composition (owner revision 2026-09-22): single-line `ZONE 01`
through `ZONE 11`, no names/ACT/FILE labels or character-selection arrows.
Completed cards default to native TV static with `CLEAR` in the caption row.
Up/Down cycles CLEAR plus all 20 replay acts; browsing reveals the selected
image. Switching cards discards that temporary choice. Start on CLEAR resumes
the stored destination. Sonic/Tails portraits and seven identity-specific
Emeralds remain; Sonic-only life/continue icons use the original donor's
`Map_DataSelect_Player_LivesContinues` and `DataSelect_LifeContinue_Nums`.
The counters occupy native plane rows 18 and 21, below the portrait.

Campaign SRAM version 2 uses formerly reserved slot bytes 3/4 for P1 lives and
continues. Version 1 remains readable, with occupied slots defaulting to 3/0.
No write or conversion on open; the next changed checkpoint writes v2 using the
existing atomic/backup/external-edit safeguards. Empty slots require zero fields,
future versions remain protected. REV01 source globals are Life_count `$FE12`
and Continue_count `$FE18`. Counters snapshot at the existing zone/act, Emerald
award and completion commits, and restore on campaign launch. No per-frame or
per-life-change disk writes were added. Zero lives restarts with three; No Save
keeps native defaults. This expands the private quickstate layout, so older
build quickstates remain strictly incompatible and their matching EXEs are kept.

The preview fixture previously held Emerald mask `$17` (four collected), not
`$7F`. A cleared campaign can legitimately lack Emeralds. The new separate
all-seven preview keeps the normal campaign file untouched. All seven source
Emerald mappings are visible in `build/states/menu-s3-counters.png` (game tree).

The quickload-only false failure banner is also corrected: deferred dirty
checkpoint data is not a failed save. The gameplay notice now requires an
actual failure notice from the existing flush path.

The earlier artwork/caption implementation record follows for context.

Card/counter follow-up validation: all 22 CTests, 36 campaign runtime fixtures,
23 quickstate fixtures and the frame-60 reference pass. The campaign tests
verify v1 open performs no rewrite, 12 lives / 4 continues persist across a
checkpoint and a fresh launch, and the v1 original becomes the exact backup.
All act destinations remain reachable through the CLEAR sentinel. Private
evidence: engine `build/campaign-card-counters-20260922`,
`build/quickstates-card-counters-20260922`, `build/boot-card-counters-20260922`.
The additional `banner-check.png` confirms a valid quickload has no false
failure strip. All dispatch lists are empty. The visible playtest reopened
at Data Select without a quickload, using the all-seven preview; its previous
four-Emerald file, EXE and F1 are preserved with `.before-card-layout` names.
The normal campaign SRAM remains untouched. Owner acceptance of this revised
composition is still pending; no code commits, merges or pushes this turn.

Every occupied card now has an 80×56 thumbnail for its Sonic 2 act, with zone
and act text below it. All 20 campaign destinations / 11 zones are covered.
Completed-file act browsing changes the image along with the destination.
Sonic-and-Tails portraits, emeralds, No Save and Delete behavior are unchanged.

Thumbnails are derived in memory from the running stock ROM: LevelArtPointers
`$42594`, Off_Level `$45A80`, PalPointers `$2782`, StartLocations `$C1D0`.
The decoder uses bounded Kosinski/Nemesis data, first animation-script frames,
and InitCam background offsets. HTZ includes block `$985A4` and tile `$98AB4`
supplements; WFZ includes `$C7EC4`. ARZ's documented `$320`-block padding is
decoded safely outside native RAM. Sky Chase includes ObjB2's Tornado
(`$3AFF2` maps, `$8CC44` art) and the native Tails pilot frame `$10`.
No extracted ROM graphics are committed or shipped. The existing optional menu
still uses its owner-supplied S3&K donor for the surrounding card composition.

Private visual QA: `sonic2_save_preview donor.bin output.ppm 1 4 gallery sonic2.bin`
renders all 20 previews in campaign order. `normal` renders the actual menu.

## Validation

Use the serial owner-ROM suites in `tests/runtime/`: `run_sonic2_quickstates.py`,
`run_sonic2_campaign.py`, `run_sonic2_party_suite.py`, and
`run_sonic2_boot_check.py` (wraps the existing `tools/boot_smoke.py`).
Private fixtures/screenshots remain in ignored build directories. Check
`dispatch_misses.toml` after every runtime. Automated success is not owner
visual/audio/controller acceptance; both new issues remain open pending that.

Current checkpoint (2026-09-21 local date):

- Release builds; all 21 CTests and 28 campaign cases pass.
- All 23 quickstate fixtures pass, including native/widescreen repeated and
  fresh-process continuations, native pause, split-screen VS, special-stage
  return, native extra-player cache rebinding, malformed-file rejection and
  active-slot rewind retaining another campaign slot. Evidence: engine
  `build/quickstates-06` and `build/states-campaign-01`.
- Boot frame-60 reference passes (`build/states-boot-01`); stock Sonic 2's
  3600-frame run matches all three PNGs and both RAM captures byte-for-byte
  against the existing primary annotation-smoke reference (`build/states-stock-01`).
- The unchanged Sonic 1 consumer also compiles against this engine with the
  new callbacks left unset. Its 3600-frame run matches all five existing stock
  PNG/RAM references byte-for-byte with no dispatch misses. Evidence:
  `build/sonic1-optout` and `build/states-sonic1-stock-01`.
- Twenty party cases pass. Three older recovery/checkpoint assertions fail
  unchanged on released v0.6.1: recovery, checkpoint, and native-extra checkpoint.
  Every screenshot/RAM capture matches the corresponding release baseline
  byte-for-byte. These old assertions expect the former immediate-recovery/hurt
  protection behavior; they have not been weakened to claim a green suite.
  Evidence: `build/states-party-01`, `build/states-party-remaining`, and
  `build/states-*-baseline`. All four half-pipe variants match stock imagery.
- The actual visible runtime menu changed to slot 5, saved and loaded it
  successfully. GUI evidence: game `build/states/ui-qa`, including
  `save-controls.png` and `run.stderr.log`. No dispatch misses in these runs.
- All 20 stage images and the actual card composition were visually inspected:
  game `build/states/gallery2.png` and `build/states/menu-final.png`.

Owner playtest executable:
`F:\Projects\segagenesisrecomp\_wt-sonic2-states-game\build\states\playtest\SonicTheHedgehog2Recomp.exe`.
It has copies of the owner's v0.6.1 settings, roster and campaign SRAM; the
original playtest directory has not been modified. The new directory's relative
campaign path keeps experiments separate. No commits, merges or pushes yet.

Owner layout follow-up: stage captions are now limited to two rows, using an
eight-glyph abbreviated zone name (e.g. `E. HILL`) at y76 and `ACT X` at y86.
The previous third row at y98 overlapped the portrait. The native LEVELSELECT
period glyph is index29 (`sonic3k.macros.asm`). A synthetic-font regression
test checks all 20 acts stay within the two caption rows and card-stem width;
all 22 CTests pass after rebuilding. Visual: game `build/states/menu-two-lines.png`.
On request, the playtest now selects the separate `sonic2-cleared-preview.srm`
and `open-cleared-save.input` boots directly into its menu. The normal
`sonic2-campaign.srm` remains preserved and can be reselected in the launcher.

## Owner act-selection and hitch follow-up

Cleared files now cycle all 20 ordered campaign destinations with Up/Down,
including Act 2 and Metropolis Act 3. Single-act endgame zones appear once.
Left/Right still chooses a file; entering the selected act preserves completion
and Emeralds. `s2_campaign_select_stage` validates the same existing SRAM index;
there is no format change. The older `select_zone` API remains available.

P3/P4 no longer add 7/13 frames of input lag, shift jump retries, coast, or seek
28/72+ pixel gaps. Both use native P2 timing. Only nearby, visible, grounded
followers get a small 24/40-pixel target separation; airborne, distant and
recovering followers keep the native target. P2 and human controllers are
unchanged. Native respawn routines are unchanged.
The initial 12/24 trial left P3 stacked after rejoining: the original AI ignores
horizontal errors below 16 pixels. The final targets stay above that native
deadband without reintroducing input delay, coasting or distant targets.

Hitch investigation (`beads-3vb.7`, party `beads-5dyp.1`): the owner's original
F1 state, matching executable and configuration are preserved privately in
`build/owner-lag-20260921`. Baseline idle replay had zero native tick holds but
host stalls. Bounded retrospective `frame_performance` telemetry narrowed one
stall to 753606us in `glue_service_vblank`: that function's blocking operation
is `glue_log_frame_state`'s per-frame `fflush`. On that same frame, simulation
took 708us and audio-chip work 116us. Legacy frame streaming now requires
`GENESIS_FRAME_LOG=<path>`; normal debugging uses the existing memory ring.
No CPU speed/timing workaround was applied. Timings cover input, simulation,
chips, bookkeeping, device audio, persistence, presentation and VBlank logging.

Diagnostic copies of the private F1 snapshot relocate only its campaign-path
hash (and, for instrumented same-layout builds, its build ID) with a recomputed
CRC. This is explicit test-fixture preparation, not a relaxed production load
gate. The source snapshot remains unchanged; production compatibility guards
remain strict. Evidence: `baseline-idle`, `measured-idle`, `isolated-timings` and
`fixed-idle` subdirectories under that private capture directory.

Final follow-up validation:

- All 22 CTests, 33 campaign runtime cases and 23 quickstate runtime cases
  pass. Evidence: `build/states-all-acts-01` and
  `build/quickstates-final-20260921`.
- Final forced P3/P4 rejoin (`final-rejoin`, 900 frames) returns both flight
  controls to zero and settles P1/P3/P4 at x4186/4168/4146. Maximum measured
  gameplay frame work is 2.051ms, with zero native tick holds in the captured
  window. Moving through the same snapshot (`final-moving`, 900 frames) has
  maximum measured frame work of 2.016ms. That moving run includes native tick
  holds, not long host stalls; no native clock or simulation workaround was
  introduced. These bounded tests do not establish that every possible hitch
  is eliminated.
- Both stock Sonic 1 (trace OFF consumer) and stock Sonic 2 still match all
  three PNG and two RAM reference captures byte-for-byte over 3600 frames.
  Neither creates the legacy frame text stream. Evidence:
  `build/states-sonic1-stock-final-02`, `build/states-sonic2-stock-final-02`.
  Frame-60 boot reference also passes (`build/states-boot-final-20260921`).
- Native cleared-file arrow mappings formerly flashed across the new caption.
  Their x anchor now sits 42 pixels beside the stem. The synthetic drawing
  test verifies flashing arrows leave both caption rows intact for all 20 acts.
  Visual: game `build/states/clear-arrows.png`.
- The visible owner playtest was reopened and paused at the preserved F1
  scene. A checked same-layout private fixture was loaded, then the normal
  save API wrote and successfully reloaded a fresh `native_save_1.bin`.
  Original bytes remain as `native_save_1.before-act-ai-fix.bin` in playtest
  (SHA256 `9fc55c2fef4b20f85c1c08afd81f1d83c900ff1eee033b54fedf86fc0e071a62`)
  and in the capture directory with the matching original executable.
  Visual/log evidence: playtest `owner-f1-ready.png`, `owner-f1.stderr.log`.
  All tested runs have empty dispatch-miss lists. User acceptance is pending;
  engine and game changes remain uncommitted on
  `feature/sonic2-states-thumbnails` worktrees.
