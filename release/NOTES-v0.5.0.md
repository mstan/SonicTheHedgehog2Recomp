# Sonic 2 v0.5.0 — Opt-in adaptive widescreen

- Enable Widescreen under **Mods**: Adaptive, 16:9, 21:9, or 32:9. It is off by default.
- Custom stage/background rendering, expanded object spawning/culling and rings, and a screen-anchored gameplay HUD.
- Full-width title scenery, fades and transitions. Adaptive can extend beyond 32:9.
- Stable sprite publication prevents ring/HUD flicker. Opt-in CPU headroom avoids the original hardware budget slowing expanded gameplay; normal VBlank and sound clocks remain intact.
- Verified one gameplay update per VBlank on Emerald Hill, Metropolis and Casino Night routes, plus real-time Adaptive at 2676x374. Water, special-stage scenery, live resizing and native regression checks passed.

Experimental limits: the special-stage half-pipe remains centered; native two-player competition stays unchanged. Whole-stage views can exhaust the original object pool and require more host rendering time. This is not full-game/boss-route certification.

Windows x64: extract the zip and run `SonicTheHedgehog2Recomp.exe`. Keep `assets/` beside it. Supply your own Sonic 2 (World) Rev A / REV01 ROM; none is included. Settings and saves are not bundled.

Source: https://github.com/mstan/SonicTheHedgehog2Recomp (tag `v0.5.0`). PolyForm Noncommercial 1.0.0; permissive third-party notices are included.
