# Sonic 2: independent campaign views (experimental)

Build: 0.8.2-campaign1, Windows x64.

Use this same build on every participant's PC. Supply your own Sonic 2
(World, Rev A) ROM through the normal launcher. Create/join a netplay lobby,
set the desired party in Options, and choose **1 PLAYER** to start the shared
campaign. The party supports up to four characters despite that title label.

Each participant automatically sees their own unsplit camera. Allies remain
visible when nearby, and everyone shares the existing campaign progression.
No camera mod needs enabling. Widescreen works with the host's selected
aspect; disabling it uses the normal canvas and aspect handling.

Amy and Knuckles still require your own verified donor files on each PC.
Offline local multiplayer and native 2P competition retain their existing views.

This is an exploration build, not the next stable release. The two-player
Emerald Hill check kept the players over 1,500 pixels apart without the old
companion return, with matching shared-state hashes. A separate Knuckles/Amy
session passed rollback, rematch and return-to-offline checks.

Remaining limitations: four-human play and the complete campaign have not
been playtested. Zone-specific parallax, water and stage transitions need
review. Visited areas remain active until object-slot pressure requires
streaming around all players; unlimited permanent entity activation is not
implemented. Shared progression still follows the existing leader rules.

No game or donor ROMs, settings or saves are included. This build uses a
separate compatibility version and cannot join stable v0.8.1 matches.
