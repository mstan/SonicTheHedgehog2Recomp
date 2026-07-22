# SonicTheHedgehog2Recomp

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
..\recompiler\build\Release\GenesisRecomp.exe sonic2.bin --game game.cfg --reverse-debug
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
