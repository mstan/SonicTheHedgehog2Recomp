Sonic 2 v0.6.1 fixes companion behavior and character rendering during local party play.

- Companions use native Player 2 recovery logic to return safely when left behind.
- CPU Players 3 and 4 have small deterministic differences in following distance and timing, so they spread out naturally. Player 2 retains its native behavior.
- Stable character frames and expanded sprite rendering remove party sprite dropouts and missing head/body streaks.
- Knuckles uses the correct standing/running poses and animation timing on slopes and loops.

The optional Sonic 3-style save menu, Campaign SRAM picker, local party options, and adaptive widescreen remain available. Existing campaign saves continue to work.

Windows x64: extract the ZIP and run `SonicTheHedgehog2Recomp.exe`. Keep `assets/` beside it. Select your Sonic 2 (World) Rev A / REV01 ROM in the launcher and optional Amy/S3&K donor ROMs under Mods. No ROMs, settings, or saves are included.

Party and campaign saves remain local-only and cannot be combined with machine quickstates or netplay.

Source: https://github.com/mstan/SonicTheHedgehog2Recomp (tag `v0.6.1`). PolyForm Noncommercial 1.0.0; third-party notices are included.
