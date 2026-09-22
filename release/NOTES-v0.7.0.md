Sonic 2 v0.7.0 adds complete local-party quickstates, visual campaign cards and Super forms for every Player 1 character.

- Save/load slots 1–9 through Escape > Save states, Shift+F1…F9 and F1…F9. Restore party, campaign session, video and audio at safe native boundaries, including pause and special stages.
- Campaign cards show owner-ROM-derived images for all 20 acts, numbered zones, CLEAR/static presentation, all collected Emeralds, and checkpoint lives/continues. Version-1 campaign saves remain compatible.
- Sonic, Tails, Amy and Knuckles can go Super as Player 1 with seven Emeralds and 50+ rings: hold jump through the apex. Keep each character's identity and abilities, native ring drain and reversion. Knuckles' Super palette comes from the supplied S3&K ROM.
- Add Sonic 3-style save-menu navigation feedback through Sonic 2's native sound driver.
- Keep P3/P4 closer to the leader, remove blocking legacy frame logging from normal play, and fix imported-companion rendering during Player 1 death.
- Game-specific implementation, configuration, disassembly and tests now live in the Sonic 2 repository. This follow-up changes no shared-engine code.

Party/campaign/quickstate features remain local-only. Special stages always use stock Sonic and Tails; native VS respects selected characters but does not gain Super. No Hyper forms or Super Flickies are added.

Windows x64: extract the ZIP and run `SonicTheHedgehog2Recomp.exe`, keeping `assets/` beside it. Supply your own Sonic 2 (World) Rev A / REV01 ROM, and optional Amy Rev 1.7.1 / combined S3&K donor ROMs under Mods. **No ROMs, extracted donor assets, settings or saves are included.**

Campaign SRAM is portable. Machine quickstates require the matching build and mod/character/video setup: keep older states with their old executable; do not overwrite your previous installation without a backup.

Owner accepted gameplay and menu changes. Pre-release validation: 25 CTests, 60 Super fixtures, 36 campaign cases, 23 quickstate cases, and native before/after menu-audio comparison, with no dispatch errors. This is not exhaustive whole-campaign or four-controller certification.

Source: https://github.com/mstan/SonicTheHedgehog2Recomp (tag `v0.7.0`). PolyForm Noncommercial 1.0.0; third-party notices included.
