SonicTheHedgehog2Recomp v0.8.0-rc1 — rollback netplay playtest
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

ONLINE PLAYTEST
---------------
Everyone must use this exact package: the lobby checks the executable and ROM.
Use Online in the launcher to host or join the same room. Online rooms support
up to four players; LAN / Direct IP currently supports two.

For a party, the host first chooses the roster in the game's offline Options,
then returns to the launcher. The host's roster is adopted for the match.
Leave Sonic 3-style Save Menu disabled for netplay. If the roster uses a
donor-backed character, each player must supply the corresponding owner ROM.
Each participant controls their assigned seat using their player-1 bindings.
Quickstates and turbo are disabled during netplay.

This Windows release candidate passed a build and a brief startup check.
The final internet multiplayer playtest is still pending.

OPTIONAL SAVE MENU AND CAMPAIGN SRAM
-----------------------------------
In Mods > S3&K, supply your combined Sonic 3 & Knuckles ROM and enable
Sonic 3-style Save Menu. This option is off by default. 1 PLAYER opens eight
save slots plus No Save and Delete. Saves keep zone/act, Chaos Emeralds,
completion, lives and continues at campaign checkpoints. Completed files can
replay all 20 acts, including Metropolis Act 3. Numbered zone cards show images
decoded from your Sonic 2 ROM; CLEAR starts with TV static. Menu movement has
Sonic 3-style feedback played through Sonic 2's native sound driver.
The portrait is always Sonic & Tails; Options controls the actual characters.

The Campaign SRAM picker is under this mod. Select an existing Sonic 2 campaign
save anywhere to load and update that exact file. With no selection, the first
save creates sonic2-campaign.srm beside the executable and remembers the path.
Existing sonic2-campaign.sav files from the earlier build continue in place.
Clear selection preserves the file. No Save creates nothing. A .bak beside the
selected file holds its previous valid revision. ROMs are never modified.
Raw Sonic 3 & Knuckles SRAM files are not compatible with this campaign format.
Older version-1 Sonic 2 campaign files remain compatible; their missing lives
and continues start at 3/0. The next changed checkpoint upgrades the file with
a backup. No Save also uses the native 3/0 defaults.

SAVE STATES
-----------
Escape > Save states offers slots 1-9. Shift+F1...F9 saves; F1...F9 loads.
Quickstates capture the complete local party, campaign session, video and
audio at a safe native gameplay boundary. Gameplay, native pause, VS and
special stages are supported; menus/loading/results are not save points.
Keep the matching executable and character/mod/video setup with each state.
Older or incompatible machine quickstates are rejected. Campaign SRAM is the
portable progress format. Keep your prior build with its old quickstates.

OPTIONAL LOCAL PARTY
--------------------
Options supports up to four local player/CPU slots. Players 1 and 2 must be
distinct; players 3 and 4 may repeat a character.
Amy Rose requires your Amy in Sonic 2 Rev 1.7.1 donor ROM; Knuckles requires
the combined Sonic 3 & Knuckles donor. Select them under their Mods entries.
The save-menu toggle is independent of Knuckles. Native Sonic/Tails special
stages return to your chosen party. These party/save features are local-only;
they do not expand netplay. Compatible quickstates include the full party.
P3/P4 follow closely, with small spacing differences; recovery no longer
requires blocking legacy per-frame logging. Imported companions also render
correctly while the world is frozen during Player 1's death.

SUPER FORMS
-----------
All four characters can go Super as Player 1: collect seven Chaos Emeralds
and at least 50 rings, then hold jump through the apex. Characters keep their
own sprites and abilities; native ring drain and reversion still apply.
Companions and VS players do not gain Super. Special stages are always stock
Sonic and Tails. Death pits, crushing and drowning remain dangerous.

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
