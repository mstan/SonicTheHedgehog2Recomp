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
