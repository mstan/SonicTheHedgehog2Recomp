SonicTheHedgehog2Recomp v0.15.3
===========================================================

A native Windows port produced by statically recompiling the Sega Genesis
68000 code to C. Recompiled CPU code, clean-room
VDP/bus/scheduler, ymfm FM synthesis, clean-room SN76489 PSG.

BRING YOUR OWN ROM
------------------
This package contains NO game data. Place your own legally obtained
Sonic the Hedgehog 2 (World) (Rev A / REV01) ROM next to the exe, named:

    sonic2.bin

then run SonicTheHedgehog2Recomp.exe, or select it in the launcher. Keep the
assets folder next to the executable. No ROM or save data is included.

TITLE OPTIONS: SOUND TEST AND LEVEL SELECT
-----------------------------------------
Open Options from the title screen. Sound Test uses the original hexadecimal
selection and cheat-code handling: Left/Right changes the sound number, A adds
0x10, and B/C plays it. Start saves the roster and returns to the title.
Choose Level Select and press A, C or Start to open the native stage selector.
The original sound-test cheat sequence and title A+Start entry also work.
These controls work with the original driver and the optional Clone Driver.

LOCAL SPLIT-SCREEN CO-OP
----------------------
Enable Local Split-Screen in Mods and assign each player's input device in
Controls. Choose the characters in the title's OPTIONS, then start 1 PLAYER.
Each local player gets their own view of the shared campaign, with up to six
views in a 3-by-2 grid. CPU companions share the party without taking a view.
Imported characters require their verified donor ROMs.

ONLINE PLAY
-----------
Leave Clone Driver + Mega PCM disabled for netplay. This experimental alternate
sound driver is for local play; disable it and restart before joining a room.

Everyone must use this exact package: the lobby checks the executable and ROM.
Use Online in the launcher to host or join the same room. Online rooms support
up to six players; LAN / Direct IP currently supports two.

Enable Amy and/or Knuckles under Mods before creating the room. Each player
must supply the donor ROMs for the host's enabled character features. Leave
Sonic 3-style Save Menu disabled for netplay; it is separate from Knuckles.
The host's initial roster is adopted for the match. After launching, open
Options on the title screen, set PLAYERS up to 6, and choose each character.
Start applies the roster to this session without changing saved local settings.
Each participant controls their assigned seat using their player-1 bindings.
Quickstates and turbo are disabled during netplay.

The enhanced party Options menu is available during netplay, including Amy,
Knuckles, Mighty, Shadow and six-player parties. Menu edits rewind with rollback.

ONLINE NOT WORKING? SEND A DIAGNOSTICS FILE
-------------------------------------------
If online play fails (for example the screen stays black after Play):

  1. Open Mods > Diagnostics and turn on Online diagnostics.
  2. Host or join and press Play again, as before. Wait about a minute if
     the screen stays black.
  3. Close the game.
  4. Send logs\Sonic2-online-diagnostics.txt from the game folder.

If both players can do this, send both files. The file includes network
addresses and lobby/player names, but no passwords. It does not affect
gameplay or who you can play with. Turn it off again whenever you like.

OPTIONAL SOUND DRIVER
---------------------
Mods > Audio > Clone Driver + Mega PCM enables an experimental alternate
sequencer and PCM driver for local play. It is off by default. Music, effects,
instruments and samples are prepared automatically from your verified Sonic 2
ROM; no separate sound pack is needed. Restart after changing drivers.
Current-build save states require the same driver selection. Campaign SRAM
remains independent. See docs/SOUND_DRIVER_MODS.md and
docs/CLONE_DRIVER_NOTICES.txt for setup, source identity and notices.

MUSIC CUSTOMIZATION
-------------------
Mods > Music Customization has Shared Themes, Sonic 2 Campaign and
2-Player Competition.
Each track has its own dropdown and defaults to Stock. Stage rows offer their
matching Simon Wai and August 21, 1992 prototype arrangements. Common themes
have applicable Sonic 1, Sonic 3, Sonic & Knuckles, Sonic 3 prototype and
Sonic CD US choices, including Boss music. Super Sonic and Invincibility are separate.

Choose a replacement, then import its matching ROM or ZIP if needed. The
Windows importer is included; no installed Python is required. Verified imports
are shared across compatible tracks and the Sonic 3 family. CD choices require
your supported US disc with audio tracks. Imports remain private on your PC.

Choosing a ready replacement enables its section and Clone Driver. Disable a
section to restore its stock tracks while retaining its choices. Press Play
and restart to apply changes. Disable these sections and Clone Driver before
netplay. Current-build save states, rewind and replay retain the selected
music configuration and CD playback clock. See docs/EVENT_MUSIC.md.

Re-importing a supported Sonic CD US disc adds Boss music to older imports
while retaining their existing recordings.

Supply your own supported donor ROMs or CD media. May 17 Sonic 3C credits,
September 14 prototype
credits and Emerald Hill 2P are supported. Sources and setup are described in
docs/EVENT_MUSIC.md. Existing explicit selections retain their original songs.
Choose Custom music for a track and import a WAV, Genesis VGM or standalone
Sonic 1-relative SMPS BIN/data-only ASM. See docs/CUSTOM_MUSIC.md for format
limits. Restart after changes. Converted copies stay in your private profile.

MUSIC PLAYER
------------
Options > Music Player plays the configured soundtrack. Up/Down changes tracks,
Left/Right chooses a voice, A/Start plays, C stops and B returns. Synthesizer
tracks allow channel isolation, including individual PSG voices in Genesis
VGM tracks; stereo recordings offer All Channels. Browsing retains the playing
track until you press Play or Stop.
Music Player is for local play. Closing it restores all channels.

CONTROLLER SHORTCUTS AND PRESENTATION
-------------------------------------
Mods > Input > Controller Shortcuts lets Player 1 remap, unbind or disable
controller state shortcuts and choose the full state menu instead of immediate
save/hold-to-load. Keyboard state controls remain available.
Optional presentation preferences select the sound for entering a stage or
backing out of the save menu, act numbers,
character title-card colours and Options fades/slower lettering transitions.
Knuckles landing recovery can optionally accept Down + Jump for a spindash.
These options retain the original behavior by default.

OPTIONAL SAVE MENU AND CAMPAIGN SRAM
-----------------------------------
In Mods > S3&K, supply your combined Sonic 3 & Knuckles ROM and enable
Sonic 3-style Save Menu. This option is off by default. 1 PLAYER opens eight
save slots plus No Save and Delete. Saves keep zone/act, Chaos Emeralds,
completion, lives and continues at campaign checkpoints. Completed files can
replay all 20 acts, including Metropolis Act 3. Numbered zone cards show images
decoded from your Sonic 2 ROM; CLEAR starts with TV static. Menu movement has
Sonic 3-style feedback played through Sonic 2's native sound driver.
Portraits show the current Options roster; campaign progress is independent
of the selected characters.

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
After loading a save state, play can diverge from an uninterrupted run.
This remains under investigation.

Escape > Save states offers slots 1-9. Shift+F1...F9 quick saves.
Hold F1...F6 or F9 for 1.5 seconds to quick load. Hold Ctrl+F7/F8 for slots
7 and 8. With a 3-button controller layout, press LB to save slot 1 and
hold RB to load it. A progress bar fills during the hold. Release early
to cancel.
Quickstates capture the complete local party, campaign session, video and
audio at a safe native gameplay boundary. Gameplay, native pause, VS and
special stages are supported; menus, loading, results and endings are not
save points.
Keep the matching executable and character/mod/video setup with each state.
Older or incompatible machine quickstates are rejected. Campaign SRAM is the
portable progress format.

OPTIONAL PARTY CHARACTERS
-------------------------
Options supports up to six local or online human players.
Players 1 and 2 must be distinct; later slots may repeat a character.
Unoccupied local party slots use AI companions. Configure controllers in the
launcher; each online participant uses their own player-1 bindings.
Amy Rose requires your Amy in Sonic 2 Rev 1.7.1 donor ROM; Knuckles requires
the combined Sonic 3 & Knuckles donor. Stock and supported Sega Classics Steam
images are accepted. Select them under their Mods entries.
The save-menu toggle is independent of Knuckles. Returning from a special stage
restores your chosen 2D party. The campaign save menu is local-only.
Compatible offline quickstates include the full party.
P3/P4 follow closely, with small spacing differences; recovery no longer
requires blocking legacy per-frame logging. Imported companions also render
correctly while the world is frozen during Player 1's death.

SUPER FORMS
-----------
All six characters can go Super as Player 1: collect seven Chaos Emeralds
and at least 50 rings, then hold jump through the apex (Shadow uses B+C in air). Characters keep their
own sprites and abilities; native ring drain and reversion still apply.
Companions and VS players do not gain Super.
Death pits, crushing and drowning remain dangerous.

OPTIONAL WIDESCREEN
------------------
Fresh installs enable Widescreen with Adaptive Fit to screen. Existing saved
choices are preserved. Open Mods to choose Adaptive, 16:9, 21:9, 32:9 or native.
Adaptive follows the window beyond
32:9, with expanded scenery, object activation, rings and screen-anchored HUD.
The mod gives gameplay extra CPU headroom while retaining normal VBlank,
audio and physics tick rates. Special-stage scenery expands; its half-pipe
projection remains centered. Extreme widths remain limited by host rendering
cost and the original dynamic-object pool. Native 2P competition extends both
viewports, with independent cameras, roster-aware HUDs and an opaque divider.

KNUCKLES CAMPAIGN STAGES
------------------------
When PLAYER 1 is Knuckles, the campaign automatically uses the original
Knuckles in Sonic 2 object placements and ending artwork. Supply your original
combined Sonic & Knuckles + Sonic 2 image under the Knuckles mod, in addition
to the Sonic 3 & Knuckles character donor. Terrain, rings and collision data
are shared with Sonic 2 in the original cartridge. Other lead characters keep
the standard Sonic 2 layouts. Companions do not switch the shared campaign.
Native 2-player competition retains its original layouts and needs only the
character donor. A missing campaign donor is reported before starting.

HIGH REFRESH RATE AND PRACTICE
-----------------------------
Mods > Presentation > High refresh rate offers 120 Hz, 144 Hz and Display
refresh. Off by default. Supported camera and actor movement interpolates
between normal simulation ticks, including imported characters and party
overlays. This adds one tick of visual latency. Controls, gameplay, audio,
animation poses and palettes retain their original simulation cadence.
Widescreen can remain disabled. Competition, online campaigns, teleports,
transitions, menus, special stages and unsafe effects retain completed frames.
Higher presentation rates require additional rendering work.

Mods > Practice has separate Infinite lives and Infinite time options, both
off by default. Lives remain after death/retry; the stage timer stops at 9:59.
Drowning, death animations and round countdowns retain their normal rules.
Easy Debug Mode is another independent option, off by default. B enters/exits
native item placement, A changes the object, and C places it.

CONTROLS
--------
Keyboard and SDL2 gamepads are configurable in the launcher. Default keyboard:
arrow keys = D-pad, Z/X/C = A/B/C, Enter = Start.

LICENSE
-------
This software: PolyForm Noncommercial 1.0.0 - see LICENSE.
Third-party components (ymfm BSD-3-Clause, superzazu Z80 MIT, clowncommon
ISC, SDL2 zlib): see THIRD-PARTY-LICENSES.md. VGM playback uses the replaceable
Game_Music_Emu library (LGPL 2.1 or later); its exact source and licenses are
in docs/libgme-source. See docs/CUSTOM_MUSIC.md to rebuild it.

Shadow the Hedgehog (fifth character option, experimental)
Enable Shadow under Megamix Characters in Mods with your verified Sonic
Megamix v3 donor, then select SHADOW EXP in title OPTIONS. This replaces the
retired sprite-sheet prototype with donor animations and source-derived
abilities. B/C jumps; press again in air for double jump/homing. A boosts on
ground for five rings; hold A during homing to chain targets for five rings
each. A curls into a ball in free fall. B+C together in air transforms/reverts
with seven Emeralds and 50 rings. Super ground A starts Chaos Control (hold A;
faster ring drain); Super airborne A with an invincibility-monitor reward
triggers Chaos Blast. Default keyboard: X/C = B/C, Z = A.
Graphics, animations and original mechanics: Team Megamix.
Characters: SEGA / Sonic Team. Credits: assets/megamix/CREDITS.txt.
Special-stage support remains partial. HUD/lives, 1-UP monitors, signposts
and result names use imported character artwork where supported.

Mighty the Armadillo (sixth character option, experimental)
Enable Mighty in Mods with your verified Sonic Megamix v3 donor ROM, then
select MIGHTY in title OPTIONS. Graphics: Team Megamix; characters: SEGA /
Sonic Team. The initial Sonic 2 spike uses Sonic physics; Megamix shield
inventory and elemental abilities are not yet included. No donor art ships. Shadow and Mighty share one verified donor.

Sonic the Hedgehog 2 is a trademark of SEGA. This project is not affiliated
with or endorsed by SEGA. No game ROM is distributed.

SOURCE
------
https://github.com/mstan/SonicTheHedgehog2Recomp

REPLAYABLE FEEDBACK
-------------------
Mods > Diagnostics > Replayable feedback is ON by default. After a bug, quit
normally and send logs/*-feedback.replay with all .sav, .config.ini and .mods.txt
sidecars and your description. Nothing uploads automatically. See
docs/PLAYER_FEEDBACK.md. The bundled tools/replay_diagnostic.py uses the exact
recorded executable, your own ROM and an isolated reproduction directory.

The imported November prototype Competition Menu arrangement restores its opening instrument and detune on each repeat. Its first pass and introductory rest keep their timing.
