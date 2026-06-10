# SonicTheHedgehog2Recomp

Static recompilation port of Sonic the Hedgehog 2 (Genesis, 1992)
to native C, sharing the recompiler / runner / clownmdemu emulator
core with [SonicTheHedgehogRecomp](https://github.com/mstan/SonicTheHedgehogRecomp).

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

This repository is a **thin sibling** to
`mstan/SonicTheHedgehogRecomp`. The shared infrastructure (recompiler,
runner code, clownmdemu emulator, common build helpers) lives there;
this repo only contains Sonic-2-specific build wiring. Expected
filesystem layout:

```
your-projects/
├── SonicTheHedgehogRecomp/             ← clone of mstan/SonicTheHedgehogRecomp
│   ├── segagenesisrecomp/              ← submodule (shared engine)
│   │   ├── recompiler/
│   │   ├── runner/                     ← shared runner sources (glue.c, ...)
│   │   ├── clownmdemu-core/
│   │   ├── sonicthehedgehog/           ← Sonic 1 game data + sonic1_spec.c
│   │   └── sonicthehedgehog2/          ← Sonic 2 game data lives HERE
│   │       ├── sonic2.bin              ← provide your own ROM (gitignored)
│   │       ├── game.toml
│   │       ├── sonic2_spec.c           ← per-game GameSpec
│   │       ├── sonic2_hybrid_table.c   ← oracle-build override table
│   │       ├── annotations_from_disasm.csv
│   │       └── generated/              ← regen output
│   └── ...
└── SonicTheHedgehog2Recomp/            ← this repo
    ├── CMakeLists.txt                  ← top-level build, refs the sibling
    ├── regen.bat
    └── README.md
```

The sibling layout means a single configure of this repo reaches into
`../SonicTheHedgehogRecomp/segagenesisrecomp/` for both the shared
runner (`runner/`) and the Sonic-2-specific generated/spec code
(`sonicthehedgehog2/`). Sonic 1's release repo serves only as the
checkout location of the submodule.

## Build

> **No prebuilt binaries are distributed — build from source below and supply your own ROM.**

Requirements: Visual Studio 2022 Build Tools, CMake, and
`SonicTheHedgehogRecomp` cloned as a sibling directory.

```cmd
cmake -B build -S . -G "Visual Studio 17 2022"
cmake --build build --config Release
```

The post-build hook copies SDL2.dll, the Sonic 2 ROM, and the
annotations CSV next to the .exe. To run:

```cmd
build\Release\SonicTheHedgehog2Recomp.exe build\Release\sonic2.bin
```

## Regenerate

```cmd
regen.bat
```

Or manually from the segagenesisrecomp tree:

```cmd
cd ..\SonicTheHedgehogRecomp\segagenesisrecomp\sonicthehedgehog2
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

The recompilation framework (recompiler / runner / tooling) inherits
the licenses of the sibling repos. The Sonic the Hedgehog 2 ROM
itself is **not provided** and must be obtained legally.
