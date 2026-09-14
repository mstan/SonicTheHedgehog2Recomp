SonicTheHedgehog2Recomp v0.5.0 — native static recompilation of Sonic the Hedgehog 2
==============================================================================

A native Windows port produced by statically recompiling the Sega Genesis
68000 code to C. No emulator core: recompiled CPU code, clean-room
VDP/bus/scheduler, ymfm FM synthesis, clean-room SN76489 PSG.

BRING YOUR OWN ROM
------------------
This package contains NO game data. Place your own legally obtained
Sonic the Hedgehog 2 (World) (Rev A / REV01) ROM next to the exe, named:

    sonic2.bin

then run SonicTheHedgehog2Recomp.exe, or select it in the launcher. Keep the
assets folder next to the executable. No ROM or save data is included.

OPTIONAL WIDESCREEN
------------------
Open Mods, enable Widescreen, then choose Adaptive, 16:9, 21:9, or 32:9.
Native rendering remains the default. Adaptive follows the window beyond
32:9, with expanded scenery, object activation, rings and screen-anchored HUD.
The mod gives gameplay extra CPU headroom while retaining normal VBlank,
audio and physics tick rates. Special-stage scenery expands; its half-pipe
projection remains centered. Extreme widths remain limited by host rendering
cost and the original dynamic-object pool. Native 2P competition is unchanged.

CONTROLS
--------
Keyboard and SDL2 gamepads are configurable in the launcher. Default keyboard:
arrow keys = D-pad, Z/X/C = A/B/C, Enter = Start.

LICENSE
-------
This software: PolyForm Noncommercial 1.0.0 — see LICENSE.
Third-party components (ymfm BSD-3-Clause, superzazu Z80 MIT, clowncommon
ISC, SDL2 zlib): see THIRD-PARTY-LICENSES.md.

Sonic the Hedgehog 2 is a trademark of SEGA. This project is not affiliated
with or endorsed by SEGA. No game ROM is distributed.

SOURCE
------
https://github.com/mstan/SonicTheHedgehog2Recomp
