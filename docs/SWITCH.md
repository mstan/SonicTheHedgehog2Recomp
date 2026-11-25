# Nintendo Switch preview

Extract the release ZIP into the SD card root, keeping the `switch` folder.
Launch **Sonic the Hedgehog 2** from Homebrew Menu. Select your original Sonic 2
ROM with the launcher's SD browser. ROMs are not included in public downloads.
Keep the NRO, assets and licenses together in `switch/SonicTheHedgehog2Recomp`.

Attached Joy-Cons are the initial hardware target. The launcher lists connected
controllers by name and defaults Player 1 to a controller. Use the left stick
or D-pad to navigate, **A** to select and **B** to return. During gameplay,
**Minus** opens the runtime menu. Settings and native saves stay on the SD card.

This is an experimental preview. Current-build save/load and runtime-menu
navigation have desktop regression coverage; Switch hardware verification for
those two changes remains pending. Create a fresh quickstate for each build.
Other controller types, docked behavior and optional donor imports are not
qualified by this preview. The alternate Clone Driver and netplay are disabled.

The widescreen mod defaults to adaptive Fit to screen. Replayable feedback is
available under **Mods > Diagnostics** and defaults on; its saved choice is
respected. Reports remain local unless you share them.

## Building

Clone this branch with its pinned submodules (`git clone --recursive`, or
`git submodule update --init --recursive`). Install devkitPro devkitA64, libnx,
Switch SDL2, Mesa, glad, CMake and Ninja. A native host C compiler is also
required to build the recompiler. On Windows, run inside devkitPro MSYS2;
the script uses the native CMake and MinGW tools under their standard paths.

```sh
export DEVKITPRO=/opt/devkitpro  # Windows: C:/devkitPro
bash tools/build-switch.sh --rom /path/to/original/sonic2.bin --jobs 2
```

The image is read as the recompilation input and is not changed. Output defaults
to `segagenesisrecomp/release-switch/switch/SonicTheHedgehog2Recomp`. Public
staging requires a fresh output destination to prevent copying private data.
Use `--out /path/to/fresh/staging` for another package. `--with-roms` is only
for a private SD folder containing your own ROM; do not publish that folder.
The shared script also supports explicit host recompiler paths and local JSON
profiles; run it with `--help` for its build options.
