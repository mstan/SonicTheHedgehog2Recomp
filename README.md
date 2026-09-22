# SonicTheHedgehog2Recomp

## Save states

Open **Escape -> Save states**, choose slot 1–9, and save or load.
**Shift+F1…F9** saves; **F1…F9** loads the corresponding slot. Files are
`native_save_<slot>.bin` beside the executable. Quicksaves capture the next
completed gameplay tick, including all four players, campaign session and
widescreen objects. Levels, paused gameplay and special stages are supported;
menus/loading/results are not save points. Loads can start from the title.

Keep the matching build and character/mod/video setup. Incompatible or damaged
states are rejected before changing the game. Old machine-only quickstates
are not compatible. Campaign SRAM remains the portable zone/act save format.
Campaign quickstates must use the same selected campaign path; they rewind
only the active slot, without overwriting other slots or immediately writing
SRAM. These features are local-only.

## Optional Sonic 3-style save menu

In **Mods -> S3&K**, enable **Sonic 3-style Save Menu** and select your
original combined Sonic 3 & Knuckles ROM. This is disabled by default and
independent of the Knuckles character option. The donor supplies menu artwork;
Sonic 2 remains the running game.

Choose **1 PLAYER** on the title screen for eight save files, **No Save**, and
**Delete**. Files retain the zone and act, Chaos Emeralds, completion, and P1's
lives/continues at campaign checkpoints. Completed files use Up/Down to choose
any of the 20 acts (including Metropolis Act 3), cycling through CLEAR and
Emerald Hill through Death Egg, and
keep newly earned Emeralds. Portraits always show Sonic & Tails; actual
characters come from Options. Left/Right selects a card, A/C/Start opens it,
and B returns. Delete uses Robotnik's Yes/No sign: Left confirms, Right or B
cancels; A/C/Start also confirms.

Occupied cards use numbered `ZONE 01` through `ZONE 11` captions and thumbnails
decoded from your Sonic 2 ROM. There are no stage-name, act, file-number or
character-arrow labels. A completed card starts with TV static and `CLEAR`;
Up/Down picks a replay act and reveals its image. Cycling past either end returns
to CLEAR; changing cards resets the temporary choice. Opening CLEAR without
choosing resumes the file's stored destination. No extracted artwork is bundled.

The bottom lives/continue icons always depict Sonic, independent of Options.
Counters are saved with act/zone progress, Emerald awards and completion, not
on every frame or life change. Loading restores those checkpoint values; a
zero-life checkpoint restarts with three lives. No Save keeps native 3/0 defaults.
Older version-1 campaign files load with 3 lives and 0 continues. The next changed
checkpoint writes version 2, preserving the previous file as `.bak`; merely
opening an old file does not rewrite it. All seven Emeralds appear only when all
seven were collected: completion and Emerald collection are separate flags.

The mod's **Campaign SRAM** picker selects an existing Sonic 2 campaign save
anywhere on disk. Saves load from and write back to that selected file, with a
previous-revision `.bak` beside it. The picker starts empty on a fresh install;
the first save creates **`sonic2-campaign.srm`** beside the executable and
remembers its path. **No Save** creates no file. Paths inside the game folder
are stored relative to the executable, so moving the folder keeps them working;
external files retain their full paths. **Clear selection** returns to the
default destination and preserves both files. Existing `sonic2-campaign.sav`
files from the first version continue to load and save in place.

It does not patch the ROM or occupy emulated cartridge memory. Loading checks
the file version, game identity, checksum, stage range and Emerald values
before restoring known game fields. Invalid saves are preserved, and a valid
backup can be opened read-only. Disabling the mod keeps your files. Campaign
saves are local-only and cannot be combined with netplay. Compatible quickstates
include the active campaign session, as described above.

## Experimental widescreen mod

In the launcher's **Mods** page, enable **Widescreen** and choose **Adaptive
(fit window)**, **16:9**, **21:9**, or **32:9**. The same controls are available
in the in-game settings overlay. It is disabled by default; selecting an
aspect alone does not enable it. Settings persist between launches.

This uses a Sonic-2-specific custom renderer through the shared framework's
Sonic 1 mod interface. It reconstructs scenery from the real stage data,
expands object spawning/culling and ring visibility, anchors the gameplay HUD
to the screen, and keeps the selected canvas through title cards and fades.
Adaptive follows live window resizing without a 32:9 cap; the practical
limits are texture size, memory, and rendering cost.

While enabled, the mod also gives single-player level simulation extra CPU
headroom so the wider active object range does not inherit the original
hardware's slowdown. Physics still advances once per VBlank; music, DMA,
interrupts and display timing retain their normal clocks. Disabling the mod
restores native CPU timing.

For development, `--widescreen fit`, `--widescreen 32:9`, and
`--widescreen off` override the saved selection. `--widescreen stage` requests
the entire stage width; arbitrary ratios such as `--widescreen 64:9` also work.

First-pass limitations:

- Special-stage scenery expands, but the half-pipe and its HUD retain the
  original centered projection.
- Native two-player competition keeps its original split-screen rendering;
  netplay disables the custom renderer.
- Extreme stage-length views can exhaust the original dynamic-object pool.
  Loading prioritizes objects nearest the player; rendering itself has no
  Genesis sprite-per-line limit.
- This is an experimental port, not a full-game/boss-route certification.
  Collision and stage decompression are unchanged, but expanded activation
  deliberately changes when off-screen enemies and objects begin updating.

Renderer details and input-only validation commands are in
[CUSTOM-VIDEO.md](segagenesisrecomp/sonicthehedgehog2/CUSTOM-VIDEO.md).

## Netplay development

The native target opts into the shared `segagenesisrecomp` delay-sync runtime
and the `recomp-ui` lobby flow. Build the `SonicTheHedgehog2Recomp` target,
then launch two local peers with:

```powershell
.\scripts\launch_netplay_pair.ps1
```

The host controls Sonic/player 1 and the guest controls Tails/player 2 using
each process's player-1 device bindings. Use
`-Headless -Scenario Campaign -Frames 1200` for the primary Sonic & Tails
synchronization test, or `-Scenario Versus` for the native split-screen gate.
Hosted lobbies, LAN rooms, direct connections, and ICE use the same runtime
frame-admission path.

During Sonic 2's native split-screen mode, each peer gets a full-window local
view: the host/slot 0 sees the top (Sonic) viewport and the guest/slot 1 sees
the bottom (Tails) viewport. The crop is presentation-only; deterministic
framebuffer hashes and emulated state still contain the complete split frame.

Static recompilation port of Sonic the Hedgehog 2 (Genesis, 1992)
to native C, sharing the recompiler / runner engine with
[SonicTheHedgehogRecomp](https://github.com/mstan/SonicTheHedgehogRecomp).
The native build is a clean-room implementation (own VDP / bus / Z80
scheduling, ymfm FM, permissively-licensed throughout); the AGPL
clownmdemu core is a development-only conformance oracle and is never
part of the shipped binary.

## Status

Sonic 2 boots through the SEGA logo, reaches the title screen, and
enters the attract demo.

**Two-player versus mode works natively.** Sonic 2's split-screen runs
the VDP in interlace mode 2 (double vertical resolution: 448 lines,
8×16-pixel cells for planes and sprites, double-res sprite coordinates
and vertical scroll). This is a consumed feature of the shared
`segagenesisrecomp` engine: its clean-room VDP renders the full
448-line interlaced frame progressively — every line is a real
rendered line, both fields, no squish hack.

Two presentation modes (engine feature, both correct):

- `interlace_display=tv` (default) — squashes 448→224 exactly like the
  original hardware looked on a TV; each player viewport appears
  vertically compressed, as on a real Genesis.
- `interlace_display=raw` — presents all 448 lines at full height: the
  window extends vertically and both player viewports display in
  proper 4:3 at full vertical detail. Sharper than original hardware
  could show; kept because we can.

Set the mode either via the CLI:

```cmd
SonicTheHedgehog2Recomp.exe --interlace-display=raw
```

or by adding a line to `debug.ini` next to the .exe:

```ini
interlace_display=raw
```

1P gameplay is unaffected (non-interlaced rendering is unchanged).

## Known behavioral differences (not defects)

- **Special stages run ~2× faster than original hardware.** The Sonic 2
  half-pipe special stage was CPU-heavy enough to lag the original 68000,
  so its main loop effectively ran at ~30 Hz. The recomp executes the same
  code as native, never overruns the frame budget, and runs it at a full
  60 Hz — technically *more* faithful to the code's intent, but it changes
  the tuned feel. This is a hardware-timing-fidelity gap (a future
  frame-lag-emulation enhancement), **not** a correctness bug. See
  [`ISSUES.md`](ISSUES.md) (ENH-1) for the measurements and analysis.

## Layout

This repo contains only Sonic-2-specific build wiring. The shared engine
(recompiler, runner) and Sonic 2's handwritten spec code live in
[segagenesisrecomp](https://github.com/mstan/segagenesisrecomp), pulled
in as a git submodule so a recursive clone is self-contained:

```
SonicTheHedgehog2Recomp/                ← this repo
├── CMakeLists.txt                      ← Sonic 2 build wiring
├── scripts/link-engine.{sh,bat}        ← optional shared-engine setup (local dev)
└── segagenesisrecomp/                  ← submodule (shared engine)
    ├── runner/                         ← shared runner sources (glue.c, ...)
    ├── clownmdemu-core/                ← DEV-ONLY oracle (AGPL; never in the native build)
    └── sonicthehedgehog2/              ← Sonic 2 game data
        ├── sonic2_spec.c               ← per-game GameSpec
        ├── sonic2_hybrid_table.c       ← oracle-build override table
        ├── annotations_from_disasm.csv
```

Generated C is ignored build output under `build/generated/sonic2/`, not a
source-tree input.

CMake resolves the engine through the committed `segagenesisrecomp` submodule.
For local dev across Sonic 1/2/3, an optional gitignored `engine-local` symlink
(→ a single shared `../segagenesisrecomp` checkout) takes precedence — see
**Build** below.

## Build

> **Prebuilt binaries are on the
> [Releases](https://github.com/mstan/SonicTheHedgehog2Recomp/releases) page —
> supply your own ROM.** You can also build from source below.

The engine is a git submodule, so a recursive clone is self-contained:

```bash
git clone --recursive https://github.com/mstan/SonicTheHedgehog2Recomp.git
cd SonicTheHedgehog2Recomp
# (cloned without --recursive? run: git submodule update --init --recursive)
```

`Skipping submodule 'clownmdemu-core'` in that output is expected and correct.
That core is AGPL and dev-only; nothing you build needs it, and CMake skips the
dev-only `_oracle` targets automatically when it is absent.

Builds natively on Windows (MSVC), macOS (Apple Silicon & Intel), and Linux.
SDL2 is bundled on Windows; `brew install sdl2` on macOS; `libsdl2-dev` on Linux.

Before configuring, copy your ROM to
`segagenesisrecomp/sonicthehedgehog2/sonic2.bin`. Generated C is not checked
in; CMake builds the current recompiler and regenerates it from that ROM and
the current discovery/configuration inputs before compiling the runner.

**Windows (MSVC):**

```cmd
cmake -B build -S . -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
:: post-build copies SDL2.dll + annotations CSV next to the .exe
build\Release\SonicTheHedgehog2Recomp.exe build\Release\sonic2.bin
```

**macOS / Linux (Ninja + Clang/GCC):**

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C build SonicTheHedgehog2Recomp
./build/SonicTheHedgehog2Recomp "path/to/Sonic the Hedgehog 2.bin"
```

> **Local dev across games:** to share ONE engine checkout instead of a per-repo
> submodule copy, clone `segagenesisrecomp` at the workspace root and run
> `scripts/link-engine.sh` (macOS/Linux) or `scripts\link-engine.bat` (Windows).
> CMake then prefers the gitignored `engine-local` symlink over the submodule.

## Audio & video enhancements (opt-in)

The runner includes an optional **verified-enhancement shadow** layer — QoL
audio/video improvements that run alongside the authentic hardware emulation and
substitute only after continuously proving they still match it (reverting loudly
if they ever stop). **Off by default** (output is byte-identical to raw hardware
emulation); enable per-run via environment variables:

| Variable | Values | Effect |
|----------|--------|--------|
| `GENESIS_SCREEN` | `raw` (default), `crt`, `trinitron`, `composite`, `linear` | Present-time color model — maps the Genesis 9-bit gamut through a CRT/phosphor model (gamma + lifted black). `raw` is bit-identical passthrough. |
| `GENESIS_AUDIO_SHADOW` | `0` (default) / `1` | Arms the YM2612 FM shadow — a parallel `ymfm` chip with a relaxed output low-pass that keeps the cleaner, less-aliased highs. |
| `GENESIS_FM_LADDER` | unset (default) / `off` | With `off`, renders the FM shadow through ymfm's ladder-free `ym3438` (no YM2612 DAC crossover crunch). Needs `GENESIS_AUDIO_SHADOW=1`. |

```bash
GENESIS_AUDIO_SHADOW=1 GENESIS_FM_LADDER=off GENESIS_SCREEN=crt ./SonicTheHedgehog2Recomp sonic2.bin
```

Full design, verifier algorithm, and rationale: `segagenesisrecomp/docs/SHADOW_ENHANCEMENTS.md`.

## Regenerate

```cmd
regen.bat
```

Or manually from the segagenesisrecomp tree:

```cmd
cd segagenesisrecomp\sonicthehedgehog2
..\recompiler\build\Release\GenesisRecomp.exe sonic2.bin --game game.toml --reverse-debug
```

The `--reverse-debug` flag enables `rdb_on_block` / `rdb_on_insn`
hooks in the generated C, which power the `crash_report` execution
trail (extremely useful for diagnosing freezes — points to the
exact recompiled function and block when the watchdog fires).

## Recent regen stats

```
[GenesisRecomp] Game config: 384 jump tables, 5023 extra funcs
[FunctionFinder] 4027 functions found
[FunctionFinder] Jump-table discovery: pc_indexed=349 manual=331 unresolved=22
[Codegen] Final function count after boundary splitting: 4425
```

22 unresolved PC-indexed dispatches remain — likely register-indirect
or rare table shapes the static extractor doesn't recognize.

## License

Releases ship under PolyForm Noncommercial 1.0.0 with permissive
third-party components (ymfm BSD-3, superzazu z80 MIT, clowncommon
ISC, SDL2 zlib) — see `segagenesisrecomp/LICENSING.md` and
`THIRD-PARTY-LICENSES.md`. The AGPL clownmdemu core is used only by
unshipped development/oracle builds. The Sonic the Hedgehog 2 ROM
itself is **not provided** and must be obtained legally.

---

<p align="center">
  <sub><b>R.A.I.D. — Retro AI Development</b> · a Discord for AI-assisted retro reverse-engineering, decomp &amp; recomp</sub>
</p>

<p align="center">
  <a href="https://discord.gg/Ad9BwSzctP"><img src=".github/raid-discord.png" alt="Join the Retro AI Development (R.A.I.D.) Discord" width="200"></a>
</p>
