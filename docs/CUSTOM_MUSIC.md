# Custom music and Music Player

In Mods → Music Customization, choose **Custom music** for a track, then import
your file in its resource row. Restart to apply it. Disable that section or
choose Stock to restore the original music while keeping the imported file.
Source files are retained. Converted copies live in the game's private profile.

Supported inputs:

- **WAV:** PCM recordings supported by SDL, converted to 44.1 kHz stereo.
  Campaign, boss, invincibility and Super themes repeat the recording. Event
  jingles, title, credits and drowning play once.
- **Genesis VGM:** uncompressed VGM using YM2612, SN76489, DAC data blocks,
  waits and seeks. Campaign loop boundaries are retained; event recordings
  finish after their first complete pass. Other chip commands,
  compressed VGZ and VGM streaming commands are rejected with the prior import
  retained. Files and converted recordings are limited to 64 MiB; VGM duration
  is limited to 30 minutes.
- **SMPS BIN:** standalone Sonic 1 68K SMPS music with relative pointers and
  embedded FM voices, smaller than 32 KiB. PSG envelopes and DAC sample notes
  use Sonic 2's built-in bank. Other SMPS driver formats need conversion first.
  A 1-UP jingle must use `smpsFade` on its final channel to resume the previous
  song; imports without that command are rejected for the 1-UP slot.
- **SMPS ASM:** UTF-8 data-only source using `dc.b`, `dc.w`, global labels,
  integer `equ` definitions and the standard Sonic 1 `smpsHeader*`, sequencing
  and `smpsVc*` music macros. Includes, binary includes, CPU instructions and
  user-defined macros are unsupported. All data must be contiguous at origin
  zero. Source is limited to 1 MiB. See `tools/assemble_custom_smps.py` for the
  supported macro names. Calls are limited to three levels; recursive and
  zero-time cycles are rejected.

Open **Options → Music Player** to hear the configured soundtrack. Up/Down
changes tracks, Left/Right selects a channel, A or Start plays from the start,
C stops, and B returns to Options. Stock and SMPS tracks offer FM1–6, DAC,
PSG1–2 and PSG3/noise isolation. VGM offers its synthesizer's named voices
(Genesis VGM groups PSG voices). Stereo WAV/CD recordings offer All Channels.
Music Player is local-only. Closing it restores all channels and Options music.

VGM playback uses Game_Music_Emu 0.6.5, LGPL 2.1 or later, with its GENS YM2612
backend. The dynamically linked library can be replaced. Exact source commit:
`9e23d10f9fd2a6a2f33b10912dd8dc7153258995`, from
<https://github.com/libgme/game-music-emu>. The package includes the source and
license in `docs/libgme-source`. To rebuild that library:

```sh
cmake -S docs/libgme-source -B gme-build -DGME_YM2612_EMU=GENS -DGME_BUILD_SHARED=ON -DGME_BUILD_STATIC=OFF -DGME_BUILD_TESTING=OFF -DGME_BUILD_EXAMPLES=OFF -DGME_ZLIB=OFF
cmake --build gme-build --config Release
```

The game build enables VGM and disables the other music formats. It pins the
source archive by SHA-256 in `cmake/Sonic2CustomMusic.cmake`.
