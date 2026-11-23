#ifndef SONIC2_SOUND_ASSETS_H
#define SONIC2_SOUND_ASSETS_H
#include <stddef.h>
#include <stdint.h>

/* Guest addresses reserved for locally converted assets, outside stock ROM. */
enum {
    S2_SOUND_MUSIC_BASE=0x180000, S2_SOUND_MUSIC_STRIDE=0x4000,
    S2_SOUND_SFX_BASE=0x200000, S2_SOUND_SFX_STRIDE=0x400,
    S2_SOUND_PSG_BASE=0x220000, S2_SOUND_PCM_BASE=0x230000,
    S2_SOUND_SEGA_BASE=0x270000, S2_SOUND_ASSETS_END=0x278000
};
/* Decoded sound data lives in an owned host allocation. All program
 * offsets are relative to an individual cue. Sample slots refer to a PCM
 * bank by number, never to a cartridge or guest bus address. */
enum {
    S2_MUSIC_CUES=31, S2_SFX_CUES=81, S2_PSG_ENVELOPES=13,
    S2_PCM_BANKS=7, S2_PCM_SLOTS=17, S2_PCM_BYTES=0x4000,
    S2_SEGA_PCM_BYTES=0x6174, S2_FM_PITCHES=128, S2_PSG_PITCHES=128
};
typedef struct S2SoundSample {
    uint16_t bytes, rate;
    uint8_t bank;
} S2SoundSample;
typedef struct S2StockSound {
    uint16_t music_bytes[S2_MUSIC_CUES], sfx_bytes[S2_SFX_CUES];
    uint8_t music_tempo[S2_MUSIC_CUES], music_pal[S2_MUSIC_CUES];
    uint8_t sfx_priority[S2_SFX_CUES];
    uint8_t music[S2_MUSIC_CUES][S2_SOUND_MUSIC_STRIDE];
    uint8_t sfx[S2_SFX_CUES][S2_SOUND_SFX_STRIDE];
    uint8_t envelope[S2_PSG_ENVELOPES][256];
    uint8_t pcm[S2_PCM_BANKS][S2_PCM_BYTES], sega[S2_SEGA_PCM_BYTES];
    S2SoundSample samples[S2_PCM_SLOTS];
    uint16_t fm_pitch[S2_FM_PITCHES], psg_pitch[S2_PSG_PITCHES], sega_rate;
    uint16_t save_warp_bytes;
    uint8_t save_warp[S2_SOUND_SFX_STRIDE];
} S2StockSound;

/* The caller owns the returned bank and may release the original image after
 * decoding. Failure returns NULL and leaves the source untouched. */
S2StockSound *s2_sound_decode_stock(const uint8_t *rom,size_t rom_size,char *error,size_t error_size);

typedef struct S2SoundAssetLayout {
    uint32_t music_index, sfx_index, special_index, psg_index, sample_table;
} S2SoundAssetLayout;

/* Pure local transformation of verified JUE REV01 bytes. No I/O, network,
 * toolchain, game execution or source assets. Writes only extension banks. */
int s2_sound_build_assets(const uint8_t *rom, size_t rom_size,
                          uint8_t *memory, size_t memory_size,
                          const S2SoundAssetLayout *layout,
                          char *error, size_t error_size);
#endif
