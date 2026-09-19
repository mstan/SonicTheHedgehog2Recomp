# Sonic 2 local party playtest

## Phase 2: Sonic 3-style campaign saves

Selectable-path follow-up (`beads-5dyp.4`): the save-menu mod now exposes an
optional Campaign SRAM picker. New installs start empty and create/register
`sonic2-campaign.srm` beside the executable on the first save; existing `.sav`
files are preserved in place. Selected files are read and updated directly.
Local paths remain executable-relative; external selections retain full paths.
Clear selection returns to the default without deleting anything. The current
development build passes 20 CTests and 28 campaign cases. The native picker,
Clear selection and boot comparison were also checked. The owner requested
ending validation here. Shared UI companion `beads-0fu.6` uses the existing mod resource API.
The integration evidence below describes the earlier accepted checkpoint.

The separate `feature/sonic2-s3-save-menu` worktrees contain a playable
implementation. Run
`../_wt-sonic2-save-engine/build/save-menu/Release/SonicTheHedgehog2Recomp.exe`.
The earlier four-player build remains available below.

Enable **Sonic 3-style Save Menu** in launcher **Mods -> S3&K** using the same
verified combined donor as Knuckles. The features are independent; source
default is OFF. Native title **1 PLAYER** opens eight cards plus **No Save**
and **Delete**. Left/right selects, A/C/Start enters, B returns. Delete enters
erase mode; choose a file and confirm again, or B to cancel.

Robotnik carries a spinning sign above the selected file in erase mode. On the
YES/NO sign, Left confirms and Right cancels; A/C/Start and B remain aliases.
File captions are centered, and the normal bottom control-text footer is gone.
The striped card shadows match the original donor capture.

Cards always show Sonic & Tails, full Sonic 2 stage names/acts and Emeralds.
Actual characters stay in Options. Saves retain zone/act, Emeralds and
completion, with fresh lives on load. Completed files use up/down to select
zones at Act 1 and keep new Emeralds. `sonic2-campaign.sav` sits beside settings,
with the previous revision in `.sav.bak`. This feature is local-only and guards
machine quickstates. Menu audio currently uses Sonic 2 Options music.

Automated checks: 19 CTests, 24 campaign cases, 23 prior party cases, boot
reference and five stock screenshot/RAM comparisons pass. Owner approved the
layout and confirmed Act 1 saves the Act 2 destination, then accepted the
corrections and requested integration as a default-off mod. Source/evidence
ledger: `segagenesisrecomp/docs/SONIC2_SAVE_MENU.md`. This is bounded milestone
acceptance, not exhaustive whole-campaign certification.

The consumer pins the published engine containing campaign saves; the UI pin
is unchanged. A recursive checkout builds with its pinned dependencies.
The optional `-DGENESIS_RECOMP_ROOT` override remains useful for development.

The integration build `build-save-menu-pinned` was configured without an engine
override against published engine `73b3f789e9e6d1eb107e93b77b543778dddb772e`.
Its 19 CTests and 24 campaign fixtures pass; all three stock PNGs and both RAM
captures match the existing primary reference exactly. Evidence is under that
build's `campaign-acceptance`, `stock-acceptance`, and `stock-comparison.json`.

The character/4P spike is integrated on master with pinned engine and UI
dependencies. The `experiment/sonic2-local-4p` worktrees and local build remain
available for playtesting. This is not a claim of full-campaign or donor-perfect
parity. Megamix/Mighty/Shadow and six-player work were cancelled.

## Run this local build

Executable: `../_wt-sonic2-4p-engine/build/party/Release/SonicTheHedgehog2Recomp.exe`.
Double-click it to use the shared launcher. The stock Rev A Sonic 2 ROM remains
the only guest image; do not choose a donor as the game ROM.

1. In launcher **Mods**, Amy Rose is enabled by default. The supplied verified
   ZIP has already been privately staged for this build. No Amy title replacement
   is installed, and Sonic/Tails remain the default roster.
2. To unlock Knuckles, open **S3&K**, browse to your unheadered combined Sonic 3
   & Knuckles ROM, and enable Knuckles. The exact revision is documented in the
   engine's `docs/SONIC2_DONOR_PORT.md`. S3&K defaults off.
3. Assign devices for P1-P4 in launcher controls. P3/P4 default to their gamepads
   with no keyboard bindings; keyboard mappings can be assigned there. P2 defaults
   unassigned. Unassigned/disconnected companions use basic CPU follow/jump and
   catch-up respawn. Connected idle pads stay under local control.
4. Start the game. Enter skips the title animation. Down twice selects the
   existing **OPTIONS** entry; Enter opens the roster.
5. Set PLAYERS to 4 and choose four unique characters. Defaults are Sonic, Tails,
   NONE, NONE. PLAYERS is slot capacity: increasing it does not auto-pick a
   character. Without S3&K there are only three unique available characters.
6. Up/Down moves the cursor, Left/Right changes values. Start or B saves and
   returns to the title. Choose **1 PLAYER** for the shared-camera party.

P1 keyboard: arrows, Z=A, X=B, C=C, Enter=Start. P2's prepared keyboard set is
I/J/K/L, N=A, M=B, comma=C, right Shift=Start; enable its keyboard device first.
Gamepad primary jump is the bottom face button.

Amy: A hammer; Down+A hammer jump; B/C jump; Down+B/C giant jump; airborne A
hammer, Down+A whirl; Up+B/C charges a dash, releasing Up launches it.
Knuckles: jump, release/repress jump near the apex and hold to glide; glide into
a solid wall to grab it, Up/Down climbs, jump releases away from the wall.

P1 owns camera, lives and checkpoints. Companions respawn/catch up without
spending P1 lives. VS remains native two-player with chosen P1/P2; NONE for P2
is rejected. Special stages always use native Sonic and Tails, even for a solo
campaign roster or P2=NONE. Your chosen characters and companions return afterward.

## Validation and known limits

- 18 CTests and 23 serial live cases pass in the engine's ignored
  `build/party-native-special-acceptance/` directory.
- Coverage: Options/persistence/uniqueness, swapped native roles, both 4P roster
  arrangements, widescreen, imports in both VS roles, P3/P4 input, springs, Amy
  enemy/monitor/EHZ boss hits, Knuckles terrain glide/grab/climb/wall-jump,
  recovery, act reload and special-stage round trips.
- Checkpoint-based special-stage returns now preserve companion terrain
  collision settings. Ordinary spawning no longer blinks; hurt and protected
  recovery retain the native blink behavior.
- Every live case uses strict native-stack checks and has zero dispatch misses.
  Four-player tests verify 251 native world ticks over 251 output frames.
- Fixtures place actors/objects in RAM; they do not certify a complete campaign
  traversed by controller. The stock ROM is never modified.
- Special stages retain original Sonic/Tails art and native gameplay; imported,
  swapped and solo campaign rosters produce identical half-pipe captures.
  Campaign CPU partners are basic followers, not pathfinders.
  All-zone devices, power-up combinations, water palettes and full visual/audio
  fidelity need playtesting. Four physical controllers need your validation.
- Machine quicksave/load is unavailable for experimental rosters: existing
  snapshots cannot capture the additional host controller state.
- Experimental rosters are local-only; vanilla netplay is not expanded.

The owner has chosen phase 2, superseding the earlier scheduling gate.
Whole-campaign four-player QA remains a separate limitation. The engine's
`docs/SONIC2_PARTY_CHECKLIST.md` records phase-1 work.

## Rebuild and reproduce

Use native Visual Studio CMake on this Windows installation:

```powershell
cmake -S . -B ../_wt-sonic2-4p-engine/build/party -G "Visual Studio 17 2022" -A x64 `
  -DGENESIS_RECOMP_ROOT=F:/Projects/segagenesisrecomp/_wt-sonic2-4p-engine `
  -DSONIC_REVERSE_DEBUG=OFF -DGEN_ENABLE_TRACE=OFF
cmake --build ../_wt-sonic2-4p-engine/build/party --config Release --parallel 8
ctest --test-dir ../_wt-sonic2-4p-engine/build/party/tests -C Release --output-on-failure
```

Run the engine's `tests/runtime/run_sonic2_party_suite.py` with `--exe`, `--rom`,
`--amy`, `--s3k`, and a new `--out` directory. `tools/import_sonic2_amy.ps1`
accepts `-Archive` and `-RuntimeDirectory` to stage the verified private Amy
donor for another build. No ROMs or donor art belong in commits/packages.

Roster/mod settings are in `sonic2-party.ini` beside the executable. Consumer
dependency pins reference published engine `7f23d3d` and UI `b1e6f3b`. The UI
adds optional file controls to the tested Genesis-binding revision without
unrelated newer UI changes. The existing local build uses
the explicit engine-worktree override above; a recursive checkout can build
using its pinned dependencies without that override.

The engine's `docs/SONIC2_SHARED_ENGINE_VALIDATION.md` records seven Windows
targets and byte-identical stock regression captures, including the existing
Puyo build-file caveat. Other games retain two-controller discovery by default
and do not enable Sonic 2's character, rendering, or save-state hooks. The
current save-menu work lives in the separate phase-2 worktrees described above.
