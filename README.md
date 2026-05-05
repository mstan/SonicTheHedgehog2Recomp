# SonicTheHedgehog2Recomp

Static recompilation port of Sonic the Hedgehog 2 (Genesis, 1992)
to native C, sharing the recompiler / runner / clownmdemu emulator
core with [SonicTheHedgehogRecomp](https://github.com/mstan/SonicTheHedgehogRecomp).

## Status

Sonic 2 boots through the SEGA logo (with the iconic Sonic-running
intro animation), reaches the title screen with Sonic + Tails + the
"1 PLAYER / 2 PLAYER VS" menu, and enters the attract demo. **Not
fully functional yet** — three known issues:

1. Title-screen background is white (palette/CRAM not uploaded
   correctly — looks like the recompiled VDP-DMA setup is racing
   with our cooperative-fiber model).
2. Attract demo screen is black (no rendering — same shape).
3. Music plays slowly (Z80 cycle accounting throws off SMPS tempo).

All three look like cycle-pacing artifacts in the runner's bus
accessor / yield logic, not codegen issues. Investigation deferred.

## Layout

This repository is a **thin sibling** to
`mstan/SonicTheHedgehogRecomp`. The shared infrastructure (recompiler,
runner code, clownmdemu emulator, common build helpers) lives there;
this repo only contains Sonic-2-specific build wiring. Expected
filesystem layout:

```
your-projects/
├── SonicTheHedgehogRecomp/             ← clone of mstan/SonicTheHedgehogRecomp
│   ├── runner/                         ← shared runner sources
│   ├── segagenesisrecomp/              ← submodule, recompiler + tools
│   │   ├── recompiler/
│   │   ├── runner/include/
│   │   ├── clownmdemu-core/
│   │   └── sonicthehedgehog2/          ← Sonic 2 game data lives HERE
│   │       ├── sonic2.bin              ← provide your own ROM (gitignored)
│   │       ├── game.cfg
│   │       ├── sonic2_spec.c           ← per-game GameSpec
│   │       ├── annotations_from_disasm.csv
│   │       └── generated/              ← regen output
│   └── ...
└── SonicTheHedgehog2Recomp/            ← this repo
    ├── CMakeLists.txt                  ← top-level build, refs the sibling
    ├── regen.bat
    └── README.md
```

The sibling layout means a single configure of this repo
references `../SonicTheHedgehogRecomp/runner/` (game-agnostic runner
sources) and `../SonicTheHedgehogRecomp/segagenesisrecomp/sonicthehedgehog2/`
(Sonic-2-specific game data, including the regenerated C output).

## Build

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
