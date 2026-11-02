# Sonic 2 player-feedback handoff — 2026-11-01

Ownership transfer is pending the owner's affirmation. Keep this scope assigned
to the current session until that affirmation; then transfer the native Sonic 2
application work and preserve these historical dispositions.

## Published baseline

[Sonic 2 v0.9.4](https://github.com/mstan/SonicTheHedgehog2Recomp/releases/tag/v0.9.4)
is published as a Windows x64 ZIP. The uploaded ZIP/checksum sizes and SHA256
digests, remote tag and `master` were verified.

- Game/tag commit: `8c121ca3b4b91855ef3cc5353affc257155ccbef`.
- Shared engine: `8424dbb7d6d25615e9597c3d8bb938d96c350c27`.
- UI: `c1867f824e7262b678fd2e2f46ba409d8a40c94e`.
- ZIP SHA256: `c6c7ef9f39e18152b9b17f366110a6f85f61c6b47c9e23f8d1f4296b4e0c0126`.
- Production: tracing/reverse debugging off, netplay on; 90 tests passed.
- Actual ZIP: dependency closure and hidden launcher/Mods/controller checks
  passed. Four normal-speed diagnostic captures/playbacks matched all
  6,351 executions: native 1512, six-seat party 1545, fresh campaign 1647,
  existing campaign 1647. Playback wrote no SRAM; external progress was
  withheld during playback and its frozen hash stayed unchanged.
- Replayable feedback is enabled by default, including party/character/campaign
  configurations. Explicit saved opt-outs remain respected. No new Linux
  artifact was published in this pass; its packaging pipeline includes the
  diagnostic helper and guide.

## Four gameplay reports: two fixed, two deferred

| ID | Report | Disposition and evidence |
| --- | --- | --- |
| S2-01 | Sonic & Tails cannot drown in CPZ2 | Deferred this pass, not reproduced. Actual Sonic air 29→0, drowning/death, lives 3→2 and CPZ2 retry with air 30 passed. Companion Tails' drowning is not independently qualified. A separate natural ARZ air-vent gasp restored air 24→30; original CPZ2 has no such vent. |
| S2-02 | Purple line at the bottom of CPZ | Fixed and released. Engine `b43c9f2` delivers the horizontal interrupt after its completed row. Matched native/wide phases remove the below-screen purple row while deep-water images and matched RAM remain unchanged. |
| S2-03 | Sonic flickers, especially during spring jumps | Deferred this pass, not reproduced. A real EHZ1 spring launch retained Sonic's sprite in all 120 consecutive frames at native/16:9. Other springs and the exact owner occurrence remain unverified. |
| S2-04 | Recomp options navigation also controls gameplay | Fixed and released. Engine `3182e40`/`c4aea97` gates local gameplay input while the real runtime menu is open, including held/released keyboard and SDL virtual-pad state, while preserving authoritative remote input. |

The owner instructed that unreproduced reports are closed out for this pass.
Reopen S2-01/S2-03 when a new occurrence supplies diagnostics, rather than
repeating the same bounded probes without new evidence. Deferrals are not fixes.

## Queued native Sonic 2 work

1. Port the requested Anthology-style presentation above 60 FPS where feasible.
   This has not been implemented. The S3 session owns the shared presenter/API;
   this session should own the native S2 adapter, qualification and release.
   Use independent presentation deadlines: the reviewed Anthology scheduler
   rounds 144 Hz to roughly 120. Guest timing, input, audio and canonical
   state must retain their normal cadence. Keep interpolation captures local
   to presentation, separate from expanded object activation/culling.
2. Carry over infinite-lives and infinite-time requests for the native S2
   application. No S2 implementation has started; elective enhancements must
   be opt-in and preserve ordinary behavior when disabled. Exercise real
   death/retry, timer expiry and transitions.
3. Keep default-on diagnostics in every future S2 release and variant. Capture
   and replay party, donor-character and campaign context without modifying
   the owner's original saves. Retain all replay sidecars with a bug report.
4. Coordinate future shared-engine pin changes with the S3 session and validate
   the native S2 consumer before adopting them. Its separately requested audio
   filter option is shared-engine work owned by the S3 session.

The Sonic 2 ROM importer and imported Sonic 2 campaign **inside Sonic 3 &
Knuckles** remain part of the S3 session's campaign work. This handoff covers
the standalone/native Sonic 2 application unless the owner changes that scope.
Drop dash/peel-out remain stopped for an owner check-in before implementation
or new assets.

## Reuse these local sources and evidence

- Worktree: `C:/temp/Codex/sonic-feedback-release-20261101/sonic2`, branch
  `codex/diagnostics-release-20261101`; published runtime source is the tag above.
- Build: `C:/temp/Codex/sonic-feedback-release-20261101/sonic2-windows`.
  External engine override points to the sibling `engine`; version override
  is `SONIC2_GAME_VERSION_OVERRIDE=0.9.4`.
- Package, acceptance, replay results, launcher evidence and remote release
  receipt: `C:/temp/Codex/sonic-feedback-release-20261101/sonic2-v094`.
- Per-report raw observations and limits:
  `C:/temp/Codex/sonic-feedback-release-20261101/sonic2-feedback-20261101/decisions.json`.
- Logs: task-root `sonic2-v094-{configure,build,ctest,package}.log`.
- Full cross-game tracker/chart: the S3 repository's
  `docs/PLAYER_FEEDBACK_BURNDOWN.md` (36 fixed reports, 22 deferred overall).

Original owner worktrees, settings and native saves were preserved. A deliberately
accelerated 1000-FPS diagnostic capture overran the bounded writer; it is retained
as a test limitation. All accepted replay cases ran at normal 60-FPS pacing.
Production TCP tracing is disabled; use the existing input-script and replay
helpers rather than retrying a stubbed debug connection.

Central Beads was unavailable (`beads` database missing on port 3307). Preserve
the existing service; do not initialize a project database or restart another
service. Backfill the owning central game issues when the tracker returns.
