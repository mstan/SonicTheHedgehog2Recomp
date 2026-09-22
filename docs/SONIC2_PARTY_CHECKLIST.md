# Sonic 2 additive party experiment

Tracking: central Beads `beads-5dyp.1` (game) and `beads-3vb.4` (source baseline).
Engine and consumer branch: `experiment/sonic2-local-4p` in separate worktrees.

## M0 — source baseline (primary master)

- [x] Pin stock Sonic 1, Sonic 2, and Sonic 3 & Knuckles disassemblies as submodules.
- [x] Rebuild and byte-verify Sonic 1/2/3, S&K, and combined S3&K.
- [x] Regenerate recompiler symbols and durable Ghidra annotation exports.
- [x] Import and round-trip Ghidra projects without committing databases or ROMs.
- [x] Build all five native targets; 12 framework tests and five 3,600-frame smoke runs pass.
- [x] Commit primary engine master: `7a927e4`, `24a44ff`.
- [x] Publish shared dependencies and pin the tested engine/UI revisions in the consumer.

## M1 — roster and native Options route

- [x] Stable character IDs and registry; availability independent of slot assignment.
- [x] PLAYERS = available slots, 1–4; defaults Sonic, Tails, NONE, NONE.
- [x] D-pad cursor and left/right cycling; unique characters; P1 cannot be NONE.
- [x] Disabled slots clear to NONE; unavailable imports repaired safely on load.
- [x] Persistent roster; native title Options entry retained.
- [x] Automated model tests and real navigation screenshots.

The roster creates the selected native/imported gameplay actors. Eighteen
ROM-independent CTests and 23 serial live cases pass; every live case has
strict native-stack checks and zero dispatch misses. Four-player runs advance
251 world ticks over 251 output frames (including widescreen). Evidence is in
ignored `build/party-native-special-acceptance/`. Human visual/audio/controller acceptance
remains required; tests do not certify full-campaign parity.

Final stock regression: 3,600 frames, zero dispatch misses; all three PNGs and
both full-RAM captures match primary master's annotation smoke byte-for-byte.
The diagnostic frame-60 boot snapshot matches the fresh primary reference at
`build/primary-boot-probe/frame60.json`. The older checked-in boot baseline still
differs from both builds and was not rewritten. Runner-purity audit reports the
same 67 pre-existing comment/compatibility matches as primary master.

## M2 — resource-backed character mods

- [x] Amy enabled by default, never selected automatically; can be disabled.
- [x] Import gameplay art and port behavior from the supplied Rev 1.7.1 hack.
  - [x] SHA-256-verified gameplay art, mapping and palette decoder; all 253 Amy
    and 251 Knuckles frames decode, contact sheets visually inspected.
  - [x] Portable controllers and native host collision adapters.
- [x] S3&K default off; verified user-selected stock donor ROM required.
- [x] Knuckles gameplay: live terrain glide/grab/climb/wall-jump test passes.
- [x] Source/provenance and collision/animation adapters; no patched ROM or generated-C edits.
- [x] Mods provider explains missing/wrong resources and gates selection accordingly.

## M3 — local party simulation

- [x] Four independent logical inputs, bindings, hotplug and script timelines.
  - [x] Virtual SDL P3/P4 button isolation and disconnect tests; shared launcher
    bindings round-trip through the engine's actual settings.ini parser.
- [x] Native Genesis physical ports remain two; no netplay expansion.
- [x] Other games retain two opened controllers by default; four-device discovery
  requires the game's explicit capability. Default/zero/four-device tests pass.
- [x] Independent P3/P4 actor state; tick the world only once.
- [x] Companion CPU fallback when local controller unavailable (native P2 AI).
- [x] P1 owns camera/progression/checkpoint history; companion recovery costs no life.
- [x] Representative terrain/solid/spring/enemy/monitor/EHZ boss interaction fixtures.

## M4 — mode compatibility

- [x] VS remains native two-player; imports tested in either role; rejects NONE.
- [x] Special stages always use native Sonic + Tails, regardless of campaign
  roster or player count (including P2=NONE). Restore chosen characters on return.
- [x] Imported/swapped/solo special-stage captures match the stock roster before
  and after P2 input; remove imported special-stage art and controller adapters.
- [x] Existing netplay guard rejects experimental rosters; no netplay expansion.
- [x] Companion death/catchup, act reload and unavailable-donor roster repair.
- [x] Owner-reported floating companions after checkpoint special-stage return:
  native character init now inherits P1's collision plane and priority before
  movement. Real starpost/SS-return fixtures cover both roster arrangements,
  the reported ghost-floor location, and primary/secondary-plane recovery.
- [x] Ordinary spawn/SS-return and companion recovery do not add hurt blinking;
  actual hurt reuses the native sprite-submission decision and blink cadence.
- [x] Reject machine quickstates that cannot serialize experimental host state.
- [ ] Whole-campaign devices/power-up visuals/water palettes and exhaustive donor parity.

Extra actors have stable native pool addresses and independent solid ownership.
World routines run once; per-actor collision helpers retain native consequences.
Special stages use original Sonic/Tails half-pipe artwork and native gameplay.
Campaign CPU fallback uses native P2 AI, not full pathfinding. See
`SONIC2_PARTY_RECOVERY.md` for recovery and sprite publication fixes.

Shared-engine integration checks: all seven native targets build and their
3,600-frame stock captures match pre-feature master. See
`SONIC2_SHARED_ENGINE_VALIDATION.md` for coverage and the pre-existing Puyo
build-file caveat. This is bounded regression evidence, not whole-game QA.

## M5 — human validation gate

- [x] Runnable Release build, private Amy assets, and consumer `EXPERIMENT.md` instructions.
- [x] Automated evidence and documented limits, with representative playable coverage.
- [ ] Owner explicitly validates and approves four-player work.

## BLOCKED UNTIL M5 APPROVAL — campaign saves / zone selection

Do not implement these during the character spike. Later: opt-in S3&K-backed
Sonic-3-style save-slot and zone UI, Sonic-2 zone/act progression, unconditional
Sonic-and-Tails slot icon, roster still owned by Options, and unlocked zones
after completion. This is distinct from existing emulator quicksave support.
