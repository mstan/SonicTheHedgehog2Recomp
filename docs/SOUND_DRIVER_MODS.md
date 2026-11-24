# Alternate sound driver

The **Audio → Clone Driver + Mega PCM** mod replaces Sonic 2's Z80 SMPS
sequencer with Clone Driver v2.8.0.1 on the recompiled 68000 and Mega PCM 2.1
on the standard Z80 backend. It is experimental, disabled by default and
available for local play. Restart after changing the selection. Disable it
and restart before entering netplay.

The game prepares its music, effects, instruments, PSG envelopes, drum samples
and SEGA sample locally from the verified **Sonic the Hedgehog 2 (World)
(Rev A), JUE REV01** ROM. There is no sound-pack download or separate resource
picker. The ROM and converted assets are never included in release archives.
The original sound driver remains available by disabling this mod.

Clone Driver executes a different 68000 workload from the original Z80
sequencer. Loading, animation/input phase and occasional native competition
lag can differ between drivers. Cross-driver cycle or snapshot identity is
not supported. The original driver retains its original behavior.

Sound-driver selection is stored beside the other settings in
`sonic2-sound.ini`. Selection is fixed when the game starts; changing a menu
setting during play takes effect on the next process launch. Current-build
snapshots include the driver identity and reject a different selection.
Native campaign saves remain independent of that identity.

## Building

`SONIC_CLONE_DRIVER_SUPPORT=AUTO` prepares pinned software sources and native
AS/p2bin tools on Windows/Linux x86-64. These inputs are hash checked and the
assembler receives a bounded software-only include tree. No upstream game
music or sample files are fetched. `ON` makes preparation failures fatal;
`OFF` builds without this optional driver. Unsupported build hosts can supply
`SONIC_CLONE_DRIVER_ASSEMBLER` and `SONIC_CLONE_DRIVER_P2BIN`, or use the
normal game build without this optional mod. The fixed-image Z80 recompiler
backend cannot execute Mega PCM and refuses activation.

The adapter reads the original WRAM sound queue at `sndDriverInput`, including
direct queue writes and pause commands. Converted tracks live in cartridge
extension banks; sequencer state uses the unused `$FFF200–$FFF5FF` region,
outside underwater palettes, the old widescreen word and game mode flags.
Missing native targets in the appended driver fail closed.

The software source is pinned to TheBlad768's ASM port at
`9c6e99358c510fa640868ce21b5fd6aea6cd01f5`. See
[CLONE_DRIVER_NOTICES.txt](CLONE_DRIVER_NOTICES.txt) for upstream notices.
