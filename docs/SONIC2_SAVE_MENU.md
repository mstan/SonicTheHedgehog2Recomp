# Sonic 2 campaign saves: phase 2

Current follow-up: [Sonic 2 quickstates and stage thumbnails](SONIC2_QUICKSTATES.md)
supersedes this historical implementation record's machine-state exclusion
and text-only card design. The 2026-09-22 numbered/static card layout and
version-2 lives/continues persistence also supersede the original defaults-only
policy below. No trilogy campaign work is included.

Tracking: central Beads `beads-5dyp.3`. Work solo. Owner chose implementation
on 2026-09-19; the older phase-1 approval scheduling text is superseded. This
does not certify whole-campaign four-player QA. Megamix remains cancelled.

## Verified starting point and worktrees

Engine master and existing 4P worktree: `95c0ac79a917789b5b7c415d82f6e8383a79e498`.
Consumer master and existing 4P worktree: `2c148d515a2457a793a07f4477cfa14f0e0a7cd6`.
UI stays at `454b3ca54756f60b23ba77cc84f512ed3bdccd77`.
No preexisting save implementation or branch was found. The uncommitted
`_wt-sonic2-4p-engine/docs/HANDOFF_SONIC2_S3_SAVE_MENU.md` is preserved in place.
Primary disassembly submodules have private untracked listings/ROM output;
those are preserved too. No reset, cleanup, or dependency upgrade occurred.

New engine: `F:/Projects/segagenesisrecomp/_wt-sonic2-save-engine`.
New consumer: `F:/Projects/segagenesisrecomp/_wt-sonic2-save-game`.
Both use `feature/sonic2-s3-save-menu`. Build: engine `build/save-menu`, with
explicit matching engine override, Release, trace/reverse-debug off.

## Accepted behavior

- Owner selected the **combined S3&K eight-slot menu plus No Save**. Reuse the
  existing exact verified combined donor and picker, with a separate default-off
  save-menu feature independent of Knuckles.
- Native title 1 PLAYER opens Data Select when enabled. Options and native
  two-player VS keep their existing routes. B returns to title.
- Left/right selects No Save, one of eight cards, or Delete. Delete enters
  erase mode; choose a card and confirm; B cancels. No Save never writes a slot.
- Every card always depicts Sonic AND Tails. Characters come only from current
  Options. Up/down cannot change characters.
- Owner selected persistence of zone/act, seven Chaos Emerald identities, and
  completion only. Loading starts with native fresh lives (3), zero continues,
  score/rings/timer, no starpost and no mid-frame state. Emerald count is derived
  from the mask; the mask restores native acquired-stage flags too.
- Display full Sonic 2 stage names (two lines when needed) and acts above
  portraits, with earned Emeralds around them. No Save and Delete labels sit
  below their small cards (native plane row 12, y96). Owner approved the donor
  layout and requested these name/label adjustments. Thumbnails remain optional.
- After completion, up/down cycles the eleven legal zones at Act 1. Completion
  remains unlocked during replay; newly acquired Emeralds remain saved.
- Saves are game-local external campaign data, separate from roster INI and
  machine quickstates. Disabling the feature or losing the donor preserves them.
- Native Sonic/Tails special stages and Options party restoration stay intact.
  Never serialize a special-stage destination. Never save attract/debug/VS.
- Local only, including with the default roster. Preserve the experimental-party
  machine-state guard; campaign state must also be guarded from machine rewinds.
- Stock Sonic 2 is the only running ROM. Decode owner assets privately; no donor
  global loop, donor SRAM execution, patches or generated-C edits.
- Initial menu uses Sonic 2's native menu audio. Donor music is not yet ported.

## Source-backed boundaries

Pinned source: s2disasm `65ddcc24250af08ddfdf58e36351374ace998e66`, skdisasm
`1e1b5aff82c21175c593e42c966a6ff8b1586ff3`. Initialized sources and byte-matched
listings are in the primary engine; reuse those without modifying them.

Sonic 2 `s2.asm:28035` LevelOrder supplies ordinary progression. Its sparse
zone table contains unused destinations, so it cannot directly be an unlock list.

| Zone | Native act IDs | Display acts |
| --- | --- | --- |
| Emerald Hill | 0000, 0001 | 1, 2 |
| Chemical Plant | 0D00, 0D01 | 1, 2 |
| Aquatic Ruin | 0F00, 0F01 | 1, 2 |
| Casino Night | 0C00, 0C01 | 1, 2 |
| Hill Top | 0700, 0701 | 1, 2 |
| Mystic Cave | 0B00, 0B01 | 1, 2 |
| Oil Ocean | 0A00, 0A01 | 1, 2 |
| Metropolis | 0400, 0401, **0500** | 1, 2, 3 |
| Sky Chase | 1000 | 1 |
| Wing Fortress | 0600 | 1 |
| Death Egg | 0E00 | 1 |

Transition references: `loc_14270`/`loc_1429C` use LevelOrder, clear starposts,
and set Level_Inactive_flag. `ObjB2_SCZ_Finished` at 3A88E separately advances
to WFZ; `ObjB2_Start_DEZ` at 3AC40 separately advances to DEZ. Completion is
the native final-boss transition to EndingSequence (s2.asm:83006), not the
negative DEZ LevelOrder sentinel, which returns to SegaScreen.
`loc_36172` awards the Emerald by setting Got_Emeralds_array[Current_Special_Stage],
incrementing the stage/count, and setting SS_Check_Rings_flag. Capture that
committed award without saving half-pipe mode. Native `Level` (3EC4) and
`Level_SetPlayerMode` (4450) retain initialization and the current party lifecycle.
Verified instruction hooks: TitleScreen 3998; single-player title reset tail
3CF4; results tail 142AE; SCZ destination commit 3A898; WFZ deactivation tail
3AC54; Emerald award 36198; EndingSequence 9C7C. Native ending mode is **20**,
not 18 (two-player results). Source inspection corrected an initial wrong mode
in the fixture and guard before acceptance.

Combined donor SHA-256 rechecked:
`fba0677fde9f76df93f3e98d6310d8af68b9847bde16e253d73cd4dd8134ed23`, 4194304 bytes.
`SaveScreen` C570 and `ObjDat_SaveScreen` D13E explicitly construct eight slots,
plus selector/header/No Save/Delete. The separate S3 mapping file is not the
chosen variant. `Obj_SaveScreen_Emeralds` supplies seven fixed surrounding
positions; `Load_Level_Icons` supplies completed-slot up/down behavior.

Verified combined listing assets: SaveScreen mappings CE0E; character/emerald
palettes CA78/CA9A; background palette 39D262; background Enigma map 39D2A2;
background Kosinski art 39D4A4; layout Enigma map 3A2020; NEW map 3A20DE;
static map pointers 3A216A; miscellaneous Kosinski art 3A23AA; extra Kosinski
art 15A774; S2-menu Nemesis font CA5E0. Copy only source-grounded assets into
host-owned buffers. Private ROM/art bytes never enter commits or packages.

## Implementation sequence and actual status

1. **S0 planning complete:** repository audit, accepted choices, source sequence,
   separate worktrees and configured build. No phase-1 work restarted.
2. **S2 separable foundation implemented:** pure campaign model and store;
   eight slots, 20 stages, sticky completion and Emerald mask, fixed endian
   version 1 schema, exact REV01 identity, CRC, transactional decode, atomic
   replacement with flushed temp file and previous-revision backup. Invalid or
   future data is protected; a valid backup opens read-only. External edits
   block stale writes. Connected to native campaign events and menu actions.
3. **S1 implemented:** transactional donor decoding, independent mod toggle,
   presentation/navigation and title routing. Owner approved the preview;
   subsequent name/label adjustments are implemented.
4. **S2/S3 implemented:** native event hooks/initialization, session isolation,
   completed-file zone selection, continued Emerald persistence, No Save,
   Delete confirmation/cancel, write-failure notice/retry. Machine states and
   netplay reject the enabled campaign feature.
5. **S4 checks and owner acceptance:** 19 CTests, 24 campaign fixtures, 23 prior
   party cases, boot reference and mod-off screenshot/RAM parity passed. After
   testing Act 1 saving and reviewing the visual corrections, the owner said
   "I think we're good" and requested the opt-in mod be integrated. This is
   milestone acceptance, not a claim of exhaustive whole-game/controller QA.
   Dependency-first publication and consumer integration follow that acceptance.

### Owner playtest corrections (2026-09-19)

Owner confirmed that finishing Act 1 saves the Act 2 destination. The reported
File 1/2/3 problem was clarified as caption alignment only, not a load failure.
Removed the extra eight-pixel caption offset (including CLEAR) and the normal
bottom instruction/stage-name footer. Actual storage failure notices remain.

The native S3&K capture confirms the black striped borders are intentional
donor shadows. Twelve sampled border/shadow regions match the native capture
pixel for pixel (`build/save-menu/border-comparison.json`); the original
character-selection arrow is excluded because character selection is fixed here.
No shadow art was replaced. The selector now moves in native eight-pixel steps,
with the camera following its position, rather than jumping to the destination
and waiting for scrolling to catch up.

Delete now renders both native parts: body frames 13/14 using `byte_D93A` at
six-frame intervals, and sign frames 8..11 using `sub_D94A` at four-frame
intervals. Robotnik follows the selector above the selected card and returns
to Delete after leaving erase mode. The donor confirmation sign is frame 12:
Left confirms YES, Right cancels NO. Existing A/C/Start and B aliases remain.
`loc_D83C`, `loc_D854`/`loc_D884`, `sub_D912` and `sub_D94A` provide the movement
and animation references; the existing verified asset bank already held these
frames. No new donor content or execution was introduced.

Updated Release and trace builds pass 19 CTests, all 24 campaign fixtures
(`campaign-polish-01`, including Left/Right confirmation and preservation on
cancel), and the frame-60 comparison (`boot-polish-01`). All native runs have
empty dispatch-miss lists. Actual updated menu captures are in
`campaign-polish-01/delete-directions`; `robotnik-delete-preview.gif` shows the
sign/body cycle. The original donor comparison used the existing S3&K runner
in `donor-reference-01`, privately and serially. Owner saves/settings were
preserved and the campaign file still validates. The owner subsequently
accepted the result and requested integration as an opt-in mod.

### Independent check of the striped shadows

After the owner questioned the shadows again, verified the unmodified combined
ROM in **Genesis Plus GX v1.7.4 f687c49**, independent of this project's
recompiler and renderer. The private capture is
`build/save-menu/genesis-plus-gx-reference-01/original-menu-2x.png`; its metadata
records the core identity and options. A two-frame Start press at frame 600
opens Data Select, captured at frame 781 without ROM or RAM patches.

The striped right/bottom shadows are present there too. Eleven nonempty sampled
regions (1,849 dark pixels) have identical border/shadow placement, recorded in
`build/save-menu/independent-shadow-comparison.json`. This comparison maps the
dark donor ink between the emulator's RGB (32,32,32) and this renderer's
(49,49,49), with nonzero shadow pixels required in every counted region. The
twelfth earlier sample contains no dark pixels and is excluded. This does not
claim whole-image RGB equality across different palette conversion models.
The earlier recompiler-to-recompiler capture alone was insufficient as an
independent authenticity check. No shadow artwork or game behavior was changed.

## Evidence so far

`ctest --test-dir build/save-menu/tests -C Release --output-on-failure`
passes (19/19). The campaign test covers all destinations, Act 2 persistence, MTZ3 native identity,
independent slots, incomplete-file restrictions, completed replay/new Emeralds,
delete, every byte corrupted, invalid fields with valid CRC, foreign/future
files, failed backup replacement, Windows read-only primary, external edits,
backup recovery and absent primary recovery. The real-donor resource test
also passes with private Amy/S3&K inputs and independent feature toggles.

Runtime evidence under engine `build/save-menu/`:

- `campaign-live-06`: 22 serial cases from `run_sonic2_campaign.py`. New file,
  act transition/relaunch/Act 2 restore, No Save including a transition, Back,
  off/missing donor, Emerald/4P restore, 4P native SS award/return, MTZ3 -> SCZ
  -> WFZ -> DEZ, completion, completed replay/new Emerald/reload, independent
  eighth slot, Delete/cancel, VS and Options, invalid-file No Save.
- `party-regression-01`: all 23 prior party cases, including both real
  starpost/SS returns, native SS image equality, VS and donor moves.
- `stock-smoke-01`: 3600 frames; three PNGs and both RAM dumps exactly match
  primary `build/annotation-smoke/sonic2`. No baseline changed.
- `boot-check-02`: frame 60 matches old 4P WT's fresh-primary reference at
  `build/primary-boot-probe/frame60.json`. First boot attempt omitted the
  required local debug.ini, so TCP never opened; trace-on alone is insufficient.
- `menu-preview-v2.png`: donor preview with requested stage names and labels.
  Actual in-game captures are in `campaign-live-06`.

All successful runtime cases have zero dispatch misses. Fixtures place native
objects or enter native transitions through RAM; they do not certify controller
traversal of a whole campaign, physical controllers, audio parity, Linux/macOS,
or power-loss behavior. Sky Chase's fixture needed native camera bounds and
event phase set with the player position; otherwise native bounds correctly
moved Sonic away from the exit. No gameplay workaround was added for this.

Owner accepted the implementation on 2026-09-19 and asked for the default-off
mod to be integrated. Publish the engine before committing the consumer pin;
then validate the consumer using that published dependency. Beads records the
landed hashes; its separate Dolt sync has the known missing remote data ref.
Preserve previous worktrees and test evidence. Never reset either feature
worktree to phase 1 or recreate the implementation from the older handoff.

The storage audit confirms there is no ROM write or emulated SRAM mapping in
this feature. Only the external 128-byte campaign record and its backup/temp
files are written. File contents carry stage/state/Emerald values, not memory
addresses, pointers or execution state; transactional decode validates them
before the menu loads a legal destination into fixed Sonic 2 RAM fields.
Both the Sonic 2 and combined donor ROM SHA-256 values still match the original
identities after all playtests. ROM donors are opened with read-only `rb` mode.

## Playtest and storage

Build: `build/save-menu/Release/SonicTheHedgehog2Recomp.exe` in the engine WT.
The new local playtest directory copies the owner's prior roster/settings and
enables campaign saves in its separate configuration. Source defaults remain
OFF. No prior settings or saves are replaced.

Launcher Mods -> S3&K -> Sonic 3-style Save Menu. Reuse the existing combined
donor; Knuckles is independent. Title 1 PLAYER opens Data Select. Left/right
selects, A/C/Start loads, B returns. Completed cards use up/down for zones at
Act 1. Select Delete, choose a file, then use the sign's Left=Yes or Right=No;
A/C/Start also confirms and B cancels. Options retains
the actual party. Audio currently uses native Sonic 2 Options music.

The optional **Campaign SRAM** picker is under this mod alongside the donor
picker, not on stock Sonic 2's main screen. A fresh install has no selected
path and creates no save until a campaign write occurs. That first write
creates `sonic2-campaign.srm` beside the executable and records `campaign_path`
in `sonic2-party.ini`. This follows native S3&K's executable-adjacent `.srm`
placement (`runner/main.c`, `runner_sram_init_and_load`), while keeping a
distinct filename for this mod's campaign format. It is not raw S3&K SRAM.

Choosing a file loads and updates that exact file in place; it is not copied
into a default location. Local selections are stored relative to the executable
directory supplied by the settings hook, independent of process working
directory. External selections remain absolute. Relative configured paths
(including `../`) also resolve from that directory. An explicit missing path
is recreated there on the next save; a failed write never falls back elsewhere.
Clear selection returns to the default destination without deleting files.
The first release's `sonic2-campaign.sav`, including protected files/backups,
is detected before the new default and continues in place without conversion.

`<selected-path>.bak` is the previous valid revision. Neither file contains
ROM/art bytes. Invalid/foreign/future files
are preserved. A valid backup can be played read-only if primary data is lost
or damaged; automatic recovery never overwrites original files. Keep both
files when recovering. Failed writes retain progress in memory for the next
event/menu retry and show a notice; quitting before a successful retry loses
that uncommitted progress.

### Selectable-path follow-up (beads-5dyp.4)

Implemented in the existing phase-2 worktrees after the initial menu was
accepted and published. Shared UI companion `beads-0fu.6` gives optional mod
resources a neutral unselected status and Clear selection through the existing
provider API; required donor pickers keep their behavior. No main-screen SRAM
or shared cartridge persistence logic changes. New `sonic2_campaign_file`
coordinates the store, selection and settings; menu save/delete paths both
use it. Rejected selections preserve the current file and selection. If data
is saved but recording its path fails, retry keeps the same destination and
does not increment the data revision again.

Release build and 20 CTests pass, including file selection, lazy creation,
backup writes, legacy preservation, invalid selections, missing destinations
and settings-write failure/retry. `campaign-picker-01` passes 28 runtime cases,
including relative/external destinations, no fallback on write failure and
moving the executable folder. All native dispatch-miss lists are empty.
These are automated checks; owner validation of the new picker is pending.
The actual native file dialog was exercised against an external fixture;
`picker-ui-01/selected-picker.png` shows the validated selection under the mod.
`boot-picker-01` matches the existing frame-60 reference with no dispatch misses.
