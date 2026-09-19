Sonic 2 v0.6.0 adds optional local party play and Sonic 3-style campaign saves.

- **Sonic 3-style Save Menu:** enable it under Mods > S3&K after selecting your combined Sonic 3 & Knuckles ROM. Eight slots, No Save, animated Delete, zone/act names, Chaos Emeralds and completed-file zone selection. Off by default and independent of Knuckles.
- **Campaign SRAM picker:** lives under the save-menu mod. Load and update a chosen file anywhere, or let the first save create `sonic2-campaign.srm` beside the executable. Paths inside the game folder remain relative; existing `.sav` progress is preserved. Saves use validated external data and a backup; neither ROM is modified.
- **Local party Options:** up to four unique player/CPU character slots, with optional Amy and Knuckles using owner-supplied donor ROMs. Native Sonic/Tails special stages return to the chosen party. The save menu always shows Sonic & Tails; Options determines the actual roster.
- Existing optional adaptive widescreen remains available.

Windows x64: extract the ZIP and run `SonicTheHedgehog2Recomp.exe`. Keep `assets/` beside it. Supply your Sonic 2 (World) Rev A / REV01 ROM; optional donors are selected under Mods. No ROMs, settings, or saves are included.

Party and campaign saves are local-only and cannot be combined with machine quickstates or netplay. Saves retain progress and Emeralds, with fresh lives on load. This release does not claim exhaustive whole-campaign or four-controller certification.

Source: https://github.com/mstan/SonicTheHedgehog2Recomp (tag `v0.6.0`). PolyForm Noncommercial 1.0.0; third-party notices are included.
