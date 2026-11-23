# Custom music and Music Player

In Mods → Music Customization, choose **Custom music** for a track, then import
your file in its resource row. Restart to apply it. Disable that section or
choose Stock to restore the original music while keeping the imported file.
Source files are retained. Converted copies live in the game's private profile.

Supported inputs:

- **WAV:** PCM recordings supported by SDL, converted to 44.1 kHz stereo.
  Campaign, boss, invincibility and Super themes repeat the recording. Event
  jingles, title, credits and drowning play once.
- **Genesis VGM and VGZ** support YM2612 and Genesis PSG music, including DAC
  data blocks, seeks and timed DAC streams. Campaign tracks retain the start of
  the file loop. Event jingles play once. Invalid files leave the previous
  import in place. Music files and expanded recordings are limited to 64 MiB.
  VGM duration is limited to 30 minutes.
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
changes the selection while the current song continues. Left/Right selects a
channel, A or Start plays the selected song from the start, C stops, and B
returns to Options. Stock and SMPS tracks offer FM1–6, DAC, PSG1–2 and
PSG3/noise isolation. VGM offers the channels used by the imported song,
including individual PSG tones and noise. Stereo WAV/CD recordings offer All Channels.
Music Player is local-only. Closing it restores all channels and Options music.

VGM playback uses the same YM2612 and PSG implementations as the game, with
independent chip state for the imported song. Channel selection and rewind
preserve the song's position without replaying from the start. Music resumes
with the original fade after the 1-UP jingle.

Import validation uses Game_Music_Emu 0.6.5, LGPL 2.1 or later, with its Nuked
OPN2 backend. The dynamically linked library can be replaced. Exact source commit:
`9e23d10f9fd2a6a2f33b10912dd8dc7153258995`, from
<https://github.com/libgme/game-music-emu>. The package includes the source and
license in `docs/libgme-source`. To rebuild that library:

```sh
cmake -S docs/libgme-source -B gme-build -DGME_YM2612_EMU=Nuked -DGME_BUILD_SHARED=ON -DGME_BUILD_STATIC=OFF -DGME_BUILD_TESTING=OFF -DGME_BUILD_EXAMPLES=OFF -DGME_ZLIB=OFF
cmake --build gme-build --config Release
```

The game build enables VGM and pins the upstream archive by SHA-256 in
`cmake/Sonic2CustomMusic.cmake`. The shipped source includes the applied DAC
patch.
