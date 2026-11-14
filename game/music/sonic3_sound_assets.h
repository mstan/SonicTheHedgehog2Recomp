/* Reused ROM music decoder from Sonic3AndKnucklesRecomp 70d69e6. */
#ifndef SONIC3_SOUND_ASSETS_H
#define SONIC3_SOUND_ASSETS_H
#include <stddef.h>
#include <stdint.h>
#include "sonic3_sound_themes.h"

/* Verified retail sound data, reconstructed exactly as the game's own loader
 * installs it. No music, instruments, envelopes or samples are stored in C. */
typedef struct S3SoundSource {
    uint8_t z80[0x2000];
    uint32_t rom_size;
    uint16_t music_banks, music_pointers, sfx_pointers;
    uint16_t voice_bank, modulation_pointers, volume_pointers;
    uint16_t music_count, volume_count, modulation_count;
    uint8_t standalone_sonic3, prototype;
} S3SoundSource;

int s3_sound_read_source(const uint8_t *rom, size_t rom_size,
                         S3SoundSource *out, char *error, size_t error_size);
int s3_sound_read_prototype(const uint8_t *rom, size_t rom_size,
                            S3SoundSource *out, char *error, size_t error_size);
/* Zero means unavailable in this cartridge (notably locked-on S3 songs when
 * running the standalone S&K ROM). IDs retain their original queue domains. */
uint32_t s3_sound_music_source(const S3SoundSource *source,
                              const uint8_t *rom, unsigned id);
uint32_t s3_sound_sfx_source(const S3SoundSource *source,
                            const uint8_t *rom, unsigned id);

typedef struct S3SoundLayout {
    uint32_t program_size;
    uint32_t music_index, sound_index, psg_index, modulation_index;
} S3SoundLayout;
typedef struct S3SoundAssetStats {
    uint32_t music, sfx, nodes, bytes;
} S3SoundAssetStats;
typedef struct S3SoundPCMLayout {
    uint32_t sample_table, map_s3, map_sk, sega_id, sample_count;
} S3SoundPCMLayout;
/* Preserve the cartridge's DAC IDs. Descriptor rates come from the driver
 * port; every waveform is decoded from the verified user's cartridge. */
int s3_sound_build_pcm(const uint8_t *rom,size_t rom_size,
                       const S3SoundPCMLayout *layout,uint8_t *bank,size_t capacity,
                       uint32_t *used,char *error,size_t error_size);
/* The bank already contains the compiled software-only program at $400000.
 * This fills its empty pointer tables and appends ROM-derived sequences,
 * instruments and envelopes. PCM sample preparation is a separate step. */
int s3_sound_build_sequences(const uint8_t *rom,size_t rom_size,
                             const S3SoundLayout *layout,uint8_t *bank,size_t capacity,
                             S3SoundAssetStats *stats,char *error,size_t error_size);
/* Append prototype music without replacing the host cartridge's SFX or
 * envelopes. The software program reserves PSG slots 65..103 and modulation
 * slots 9..18 for this donor. Existing PCM remains the retail cartridge's. */
int s3_sound_build_prototype_music(const uint8_t *rom,size_t rom_size,
                                   const S3SoundLayout *layout,uint8_t *bank,size_t capacity,
                                   uint32_t *used,int standalone_s3,
                                   char *error,size_t error_size);
int s3_sound_build_prototype_pcm(const uint8_t *rom,size_t rom_size,
                                 const S3SoundPCMLayout *layout,uint8_t *bank,size_t capacity,
                                 uint32_t *used,char *error,size_t error_size);
/* Append one chosen event and its own envelopes/percussion. The compiled
 * software is read-only; all musical bytes come from the verified donor. */
int s3_sound_build_event(const uint8_t *rom,size_t rom_size,unsigned donor,
                         unsigned theme,unsigned target,
                         const S3SoundLayout *layout,const S3SoundPCMLayout *pcm,
                         const uint8_t *program,uint8_t *bank,size_t capacity,
                         uint32_t *used,unsigned *next_sample,
                         char *error,size_t error_size);
/* One bank build shares validated donor tables and decoded percussion across
 * every selected track. Cache lifetime ends before its input ROMs are freed. */
typedef struct S3SoundMusicCache {void *donors[S3_ROM_DONOR_COUNT];} S3SoundMusicCache;
void s3_sound_music_cache_free(S3SoundMusicCache *cache);
int s3_sound_build_track(const uint8_t *rom,size_t rom_size,unsigned donor,
                         unsigned source_song,unsigned target,
                         const S3SoundLayout *layout,const S3SoundPCMLayout *pcm,
                         const uint8_t *program,uint8_t *bank,size_t capacity,
                         uint32_t *used,unsigned *next_sample,S3SoundMusicCache *cache,
                         char *error,size_t error_size);
#endif
