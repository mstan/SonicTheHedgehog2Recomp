# Sonic 2 Megamix characters

Owner playtest branch: `spike/shadow-playable`. Mighty was accepted by the
owner on 2026-10-19. Shadow replaces the earlier sprite-sheet prototype;
that importer, embedded crop bank and shipped sheet are retired. The original
Downloads image and historical spike builds remain intact.

Graphics, animation and original mechanics credit: **Team Megamix**.
Characters: **SEGA / Sonic Team**. Runtime assets come from the owner's exact
Megamix v3 ROM, size 1,283,224 bytes, SHA-256
`85d9e9ba79f0035d5854618666ff198871abe1931558496e6563b1cd90e5cdbe`.
Neither that ROM nor extracted art is distributed in the release ZIP.
`tools/import_sonic2_mighty.py` verifies and stages either raw or zipped donors
for both characters. Mods has one donor picker and independent character flags.

## Exact donor bindings

These are offsets in the verified donor, not addresses to execute in Sonic 2.
Behavior was translated into portable C; the host remains native Sonic 2.

| Data | Mighty | Shadow |
| --- | --- | --- |
| Frame count | 176 | 176 |
| Mappings, five-byte pieces | `2A9D6` | `2B6A4` |
| DPLC, byte counts | `2C3D2` | `2CA1C` |
| Raw art | `A9364` | `BE2E4` |
| Normal palette | `2C60` | `2C80` |
| Normal scripts, 38 entries | `1AA1C` | `1AA1C` |
| Super scripts, 38 entries | native timing retained | `1AB5E` |

Animation offsets use signed `ADDA.W` semantics: Super scripts can refer
backward into the normal bank. Shadow's water palette is palette-directory
entry `1A` at `22D0`; four-shade Super cycles are `11CB7C` and `11CCFC`.
Source directional gait offsets and horizontal/vertical flips update even
between animation advances. Normal idle and rising/falling scripts are used
directly, addressing the earlier inappropriate idle and airborne skating art.

## Shadow controls and source behavior

| Input | Behavior / donor reference |
| --- | --- |
| B/C | Native jump; press again for one double jump, `19644`: vertical `-700`, underwater `-3C0` |
| Airborne B/C near a front target | Homing, `19DD0` / `19ECA`: nearest eligible target within 128 pixels; integer native angle/sine, speed `C00` |
| Hold A during homing | Chain to subsequent targets; first hit free, later targets cost five rings and require more than five |
| Ground A | Five-ring boost, `197EC` / `19B4E`: `C00`, or `1000` with speed shoes; rejects blocking walls/pushing/normal rolling |
| Free-fall A | Curl into a ball, then permit the extra jump |
| Airborne B+C together | Transform or revert through native Sonic 2 Super lifecycle |
| Super ground A | Chaos Control, `197EC` / `11D10C`: `C00` launch, `40` per extra tap, top speed `1100`; hold A to maintain speed floor |
| Chaos Control release | Restore Super top speed `C00`; source maintenance runs after native motion |
| Chaos Control ring drain | Countdown capped at ten, from `11CEE2` |
| Super airborne A with S-monitor reward | Chaos Blast, `198F0` / `117094`: native enemy defeats, bosses retain rounded-up half HP; no added ring charge |

Default keyboard: Z=A, X=B, C=C. Super remains Player 1 only and uses Sonic 2's
seven Emeralds and 50 rings, adapting Megamix's six-Emerald progression.
The invincibility monitor is the Sonic 2 equivalent of the S-power prerequisite.
Sonic 2 normally discards that reward while Super. A narrow monitor hook
retains the actual reward for 20 seconds in explicit character state without
expiring Super's permanent invincibility or replacing its music. A normal-form
reward also survives transformation for its remaining duration.

Targets, damage, scoring, explosion objects, collision, death/retry and stage
transitions use Sonic 2's native world handlers. Donor code, music and extended
effects objects are not executed. Ability sound/flash presentation is incomplete.
HUD/lives, title/results and special-stage art remain stock. Mighty retains
Sonic physics and animation semantics; its shield inventory/elemental powers
remain outside the accepted graphics spike.

## Six-player campaign party

The roster and Options now support six independent campaign actors. The four
extra objects reserve native slots from the pool's high end. Character state,
physics, CPU following, solid ownership, springs, presentation and quickstates
scale with `S2_MAX_PLAYERS`. Larger parties use wider grounded spacing while
airborne/recovering companions retain native history following. Read-only art
banks are shared. Native VS still uses two competitors.

The shared input/session runtime and Genesis launcher support six human seats.
The game advertises six-player capacity, adopts six-character online rosters,
and gives seats 5/6 their own input and campaign camera. Local slots without a
connected/configured human source retain AI following. Native VS remains two
competitors. Engine and launcher pins include the six-player changes.

## Validation evidence, 2026-10-19

Passed:

- All 13 game CTests, including portable ability prerequisites/costs and 1,024
  surface-angle/facing/Super pose cases.
- All 176 Shadow frame geometries and pixel hashes match the independent S3
  Megamix importer using the same donor; Mighty matched separately.
- `run_sonic2_shadow.py`: real jump/double-jump/landing, repeated-jump rejection,
  five-ring boost and insufficient rings, native spin dash, native homing kill,
  paid two-target chain, Super transform/revert, Chaos Control speed/drain and
  real invincibility-monitor reward followed by Chaos Blast native kills.
- Long donor idle, unrolled airborne pose and free-fall curl; P2/P3 abilities,
  repeated companions, native VS boot/render.
- `run_sonic2_six_party.py`: six independently live objects follow the leader;
  saved mid-double-jump state repeats byte-identical RAM continuation both
  within a process and after a cold load; native death spends one life and
  rebuilds all six actors on retry. Screenshots inspected for all six characters.

Evidence is retained under `build-megamix-shadow/`: `qa-final`,
`qa-final-presentation`, `qa-final-party`, `qa-blast-routed`, `qa-six-final`,
`qa-loop-visible`, `qa-six-boundary`, frame CSVs and source pose contact sheet. Every automation
uses dummy audio/video and checks strict JSR stack/dispatch misses.

Fixture corrections are preserved in earlier output directories: a loose-ring
pickup invalidated a post-hit ring assertion, so cost is measured at acquisition;
an insufficient camera wait made the first loop images off screen. A visible
loop fixture waits for camera catch-up and retains actual native surface angles.
The six-actor loop also pauses before seeding entry speed so a mid-update
character write cannot overwrite the fixture; on-screen assertions then pass.
The first Blast attempt exposed the host's discarded monitor reward; the adapter
fix was then validated through a real break/reward and native enemy outcomes.

Unverified: every campaign stage/alternate route and boss, sustained six-actor
stress in all terrain types, six human devices and six-peer netplay. Owner
playtesting is ongoing. Teleports and seeded Super eligibility bypass route and
Emerald collection; they do not establish natural progression coverage.

The private handoff `build-megamix-shadow/playtest-shadow-six` contains the
owner's donors and starts Shadow/Tails/Sonic/Knuckles/Amy/Mighty. It is windowed
with volume 100. The separate ROM-free ZIP carries Team Megamix credits and
excludes the deprecated sheet, CD Speed and Fast Loading packages.

## Owner regression follow-up: bridge and interaction audio

The owner found that EHZ's log bridge supported only P1 and the native Sidekick.
The earlier generic-solid checks did not cover Obj11's separate collision code.
`run_sonic2_bridge.py` reproduces exactly two standing contacts before the fix
and six afterward. It uses real level placement and native contacts; positioning
and invincibility bypass approach traversal and Buzzer damage only. The adapter
extends native `F872` / `F88C` to four extra objects, preserves the Sidekick's
log index, and lets extra-only contacts drive the native sag envelope and
`Obj11_Depress`. Tests cover jumping, crossing, cold restoration, extra-only
sag and relaxation after everyone leaves. Evidence: `bridge-before-protected`,
`bridge-weight`, `bridge-r2` under `build-megamix-shadow`.

Generated audio from two- and six-character running/jumping fixtures is identical
and has no clipping. The existing underrun statistic misses DRC concealment:
recovery loops recent audio and increments `stretch_events` instead. Read-only
samples of a tool-owned process's exact linker-map bridge expose these episodes.
Physical WASAPI probes use volume zero; dummy-device and generated-audio
observations are recorded separately.

An exhaustive correlation search ran on the audio callback while blocking its
producer. At 223,721 Hz it took about 19 ms in the isolated benchmark. The engine
branch `spike/s2-audio-pacing` bounds coarse analysis to 24 kHz, then refines the
candidate lag at full source precision, taking about 0.25 ms. The branch name
reflects an earlier hypothesis; **no limiter, simulation, input or netplay
pacing changes are included**. Five-rate tone/period/silence/conceal/reset tests
pass. Generated PCM before/after the change is byte-identical: 31,065,168 bytes,
SHA-256 `bd1825adf3bbaf554cd72a74b16182c51cb10b884e7398f8b57a0377c0c059b9`.

Instrumented runs under I/O load still show host stalls; they do not establish
that all intermittent buzzing is gone. Low-overhead physical-device runs had
zero concealment episodes before and after. An explicit 250-ms producer-only
stall in each tool-owned child produced two recovery episodes before and one
after, with healthy refill afterward. This isolates recovery work from normal
synthesis and leaves subjective confirmation to the owner. Evidence directories:
`audio-fast`, `audio-physical-six`, `audio-physical-six-fixed`,
`audio-light-before/after`, `audio-stall-before/after`, `drc-probe-build`.

The r2 build uses the isolated engine worktree
`F:/Projects/segagenesisrecomp/_wt-s2-audio-pacing`; the consumed engine checkout
and submodule pin are unchanged. Engine changes remain reviewable there until
integration. Combined game/DRC validation totals 14 passing CTests. The private
`build-megamix-shadow/playtest-shadow-r2` retains six characters, normal audio
and windowed presentation. The preceding owner's game is preserved separately.

The owner still encountered one hitch in r2, less frequently. Its production
logs showed a clean exit at frame 5737 with no dispatch misses, but contained
no timing history. The isolated engine now retains up to 128 host hitch/recovery
events in memory even with tracing disabled. Since R5, printing this history at
session exit requires `GENESIS_HOST_TIMING_DUMP=1`.
This records input/simulation/audio/persistence/present/pacer time, audio lock
wait, callback peak, fill and actual conceal counters. No diagnostic disk I/O
runs during play, and the frame limiter's behavior remains unchanged.

`audio-hitch-capture-stall` exposes another recovery defect: live playback
resumes on a single small push after starvation, then immediately empties it
again. The device-only r3 fix holds the existing conceal loop until the normal
50-ms target cushion is refilled. It does not run extra guest frames or discard
generated samples. A deterministic 250-ms producer pause gives 14 recovery
episodes before the fix at 22,050 Hz, and one afterward at all five tested
source rates, with both 480- and 1,024-frame host callbacks. Evidence is in
`drc-probe-build/recovery-before.log` and `recovery-after.log`.

The host also had substantial CPU, paging and disk activity during this follow-up;
one CPU snapshot reached 92 percent. That is context, not proof of the original
stall's cause. The refill fix addresses repeated buzzing after a stall; remaining
host stalls and subjective audio still require validation. The r3 private
handoff uses normal audio/windowed presentation and timestamps each run's logs.
All 14 combined checks pass in r3, including ten deterministic refill fixtures.
`audio-hitch-recovery-r3` used hidden real rendering, WASAPI volume zero, all
six characters, running/jumping and a 250-ms producer-only pause. Host history
caught simulation-phase stalls up to 789 ms while device-lock waits were at
most 1 microsecond and recorded callback work at most 513 microseconds. This
localizes those wall-time stalls outside the audio lock; it does not distinguish
CPU work from descheduling or paging. No additional host events were recorded
after frame 1413 through the script exit at 2641. The run was under background
load and is not a controlled before/after performance comparison.

## Whole-game freeze tooling and default disk logging

The owner clarified that the complete game stops for one or two seconds,
including once at the SEGA logo. This is tracked as a simulation-progress
problem, beyond audible recovery. `tools/sonic2_stall_observer.py` attaches
read-only to an exact executable/map, checks their identities, and uses the
standalone engine `tools/host-stall-observer` executable to record real frame
gaps, main-thread CPU/cycles, faults, process I/O and system load. Owner attach
does not suspend or write target memory. A 60-second R3 owner capture found no
new freeze during its observation window; it cannot reconstruct an earlier one.

Automated child runs can request resident-stack snapshots. The child resumes
before symbolization/unwinding; pauses and observer sampling gaps are recorded.
Qualification against known 250-ms waits and CPU loops distinguishes the two
and resolves their native caller names. Early prototype failures are retained;
the final resident-stack qualification passes with zero target suspension in
read-only mode. The observer itself never writes a per-sample disk stream.

An unforced six-character run (`stall-natural-six`) caught a 1,589.82-ms gameplay
freeze at frame 1681. Main-thread CPU was zero, probe pause was 30 microseconds,
and the native stack was `ZwWriteFile` through CRT `fprintf` to
`glue_yield_for_vblank`. The engine unconditionally opened `yield_log_native.log`
even with tracing disabled; its buffered per-VBlank writes could therefore
flush synchronously on the game fiber. This is a demonstrated stall source,
independent of audio-callback work. A separate startup capture blocked opening
donor files before frame progress; it does not establish the cause of the
owner's visible SEGA-logo freeze. The prototype's startup snapshot also incurred
217 ms of probe overhead, which is explicitly recorded rather than attributed
to the game; the revised observer skips nonresident stack pages.

R4 removes default yield-file streaming and the unsolicited mid-level automatic
RAM dump. Development builds can explicitly request `GENESIS_YIELD_LOG=<path>`
or `GENESIS_AUTO_RAMDUMP=1`; release builds omit these automatic paths. Manual
RAM captures retain their existing contract. In-memory frame/host diagnostics
remain available. Source and engine pins remain isolated as described above.

Example read-only attach, using the private playtest's preserved matching map:

```powershell
python tools/sonic2_stall_observer.py --pid <PID> `
  --exe build-megamix-shadow/playtest-shadow-r3/SonicTheHedgehog2Recomp.exe `
  --map build-megamix-shadow/playtest-shadow-r3/SonicTheHedgehog2Recomp.map `
  --observer build-megamix-shadow/host-stall-observer/Release/host_stall_observer.exe `
  --out build-megamix-shadow/owner-observer/run --seconds 60
```

Automated `probe_sonic2_party_audio.py --light --observer <exe>
--stack-snapshots` profiles boot, running and jumping on a muted tool-owned
child. It preserves the exact linker map alongside every new capture. Its
optional `--stall-ms` injection must be distinguished from natural stalls.

R4 validation: all 14 game/DRC checks pass, the observer's waiting/CPU/stack
qualification passes, and four real boot cases verify release/default versus
explicit development captures. Manual `DUMP_RAM` still produces 64 KiB in all
four cases. The fresh R4 six-character run reached the level and exercised
running/jumping with no active-gameplay progress gaps above 50 ms and no audio
concealment. Its two recorded events were pre-frame-zero device setup and the
script's exit boundary. Maximum observer gap was 12.146 ms. This is a short
follow-up under different host conditions, not proof that every intermittent
freeze is gone. Evidence: `stall-r4-default`, `host-stall-qualification-delivery`
and `stall-diagnostic-contracts`.

The private `playtest-shadow-r4` preserves its exact executable/map and carries
`host_stall_observer.exe`. Its normal-window/audio-100 start script also starts
a hidden, read-only observer for up to 600 seconds or until the game closes.
Timestamped `observe-*-samples.csv` / `observe-*-events.json` are written when
observation ends. The owner's currently running R3 was preserved and a separate
read-only observation was attached for the same bounded interval; it retains
the old logging behavior until restarted into R4. The ROM-free R4 ZIP excludes
the observer, maps and private donors, and retains Team Megamix credits.

## Responsive exit and clean window titles

R5 fixes the observed close-window hang. An unread diagnostic output pipe with
32 intentionally injected producer pauses reproduced the previous build's
shutdown block: the window remained visible and the process stayed blocked
after one second. The same real `WM_CLOSE` test on R5 hid the window in 62.1 ms
and exited in 149.7 ms. These injected pauses populate the diagnostic history;
they are not evidence of natural gameplay stalls. Reproduce using
`tests/runtime/run_window_close.py`; evidence is retained in
`build-megamix-shadow/close-before-protected` and `close-after`.

Close now stops before another simulation/render tick, hides the window and
stops audio before persistence. SRAM saving and normal cleanup remain intact.
Automatic host-history and routine diagnostic summaries no longer flood the
shutdown output pipe. Explicit `GENESIS_HOST_TIMING_DUMP=1` retains the capture
path; the automated party/audio profiler requests it. The external read-only
observer continues to record progress without an in-game diagnostic dump.

Gameplay and launcher captions use the game name alone, including when a debug
port is supplied. The shared runner and canonical Sonic 1, Sonic 2, Sonic 3 /
Knuckles and Rocket Knight engine copies carry this presentation default.
Versions/backend/port details remain available outside the persistent caption.
The owner requirement is also saved in the global agent instructions.

R5 repeats the 14 passing game/DRC checks. Its muted WASAPI/hidden-renderer
six-character smoke run boots and exercises running/jumping for 2,081 frames.
No resumed gameplay frame gap exceeded 50 ms; the single external observation
was the 92.8-ms script-exit boundary. Maximum observer sampling gap was 9.133 ms.
Six audio conceal episodes still occurred while the other titles were being
built; frame timing peaked at 32.9 ms. This does not prove the audible symptoms
are gone. Evidence: `stall-r5-default`. The private R5 handoff retains the exact
executable/map and observer, and starts with normal audio and a normal window.

## R6: airborne pose persisting during grounded sliding

The owner's sliding screenshot matches donor frame `AA` from falling script
`24`. Native Sonic 2 can keep animation `0` (Walk) or `1` (Run) unchanged
through unrolled airtime and landing. The adapter previously noticed entry into
the donor airborne override, but leaving it did not reset the source script.
Shadow could consequently continue the falling or rising animation on ground.

The selection now recognizes both entering and leaving that override. The donor
art and native movement/contact behavior remain the same. The unit regression
fails before this fix and passes for normal/Super, both facings, both native
movement IDs and both vertical directions, followed by another fall.

`tests/runtime/run_sonic2_shadow_landing.py` boots the actual six-character
party and records 50 successive RAM/image observations for each of a native
terrain landing and a real horizontal spring launch. Positioning and spring
placement are explicit cheats; collision, landing, launch force and animation
outcomes are produced by the game. R5 reproduces 31 and 42 stale grounded poses;
R6 records zero in both cases. All recorded motion/contact fields match between
the builds, with only sprite frames differing. All 13 game checks and the
existing idle/free-air/curl presentation checks pass.

Evidence: `landing-before-paused`, `landing-after`, `landing-comparison.json`
and visually inspected `shadow-landing-comparison.png` under
`build-megamix-shadow`. The initial unpaused fixture let an in-flight native
motion copy overwrite its seed, so it is retained as invalid coverage in
`landing-before`; the corrected fixture positions while paused before native
motion resumes. R6 retains the R5 exit fix, clean caption and private read-only
freeze observer. Owner confirmation of the animation fix remains pending.

## Corkscrew animation limitation

The owner's R6 screenshot `SonicTheHedgehog2Recomp_wh00dS8LYZ.png` shows the
Emerald Hill spiral/corkscrew, whose native Obj06 handler sets player
`flip_angle` (`27`) independently of terrain `angle` (`26`). Native
`SAnim_Tumble` also reads `flip_turned` (`29`) and selects twelve tumbling poses.
The current Shadow adapter reads terrain angle only. The screenshot matches
donor frame `1E`, horizontally flipped, so an ordinary upright skating pose is
being used during the twist.

The verified 176-frame donor provides directional gait/ceiling poses and
front/back airborne art, but no complete matching Sonic 2 corkscrew sequence.
The ordinary-loop pose coverage above does not cover this separate rotation.
A flipped-running approximation could address the inverted portion without
supplying the missing perspective frames. The owner explicitly allowed leaving
this visual limitation when no matching sequence exists; no pose substitution
or runtime changes were made for this audit, and the running R6 was preserved.
The limitation remains part of Shadow's experimental presentation.

## Main integration and six human seats

The owner confirmed buzzing/freezing resolved and authorized main-build
integration. Character work is combined with the current v0.8.3 main coverage
fixes, the shared audio/diagnostic/exit fixes, six host input seats and the
Genesis launcher six-seat profile. The existing two physical controller ports
remain intact; four additional sealed logical inputs feed campaign actors.
Six-player rosters use stable compact IDs in match caps so repeated long names
fit the existing network-library extension ABI. The session config revision
is advanced and strict build/config/digest checks remain enabled.

Validation is deliberately limited to focused seat/input/roster tests and one
muted local-server six-peer lobby/rollback gameplay smoke, per owner request.
Internet/NAT, mixed builds/architectures and six physical gamepads are not
qualified by that smoke. Shadow retains the documented corkscrew limitation.
