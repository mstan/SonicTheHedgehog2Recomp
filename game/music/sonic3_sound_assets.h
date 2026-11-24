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

struct S2DonorSound;
struct S2MusicCue;
struct S2SoundWave;
/* Prepare one immutable playback cue. A donor profile is reused by all
 * selected songs from that image, including decoded percussion. */
int s3_sound_prepare_cue(const uint8_t *rom,size_t size,unsigned donor,unsigned song,
                         S3SoundSource *source,struct S2DonorSound *profile,
                         struct S2MusicCue *cue,char *error,size_t error_size);
int s3_sound_decode_drum(const uint8_t *rom,const S3SoundSource *source,unsigned note,
                         struct S2SoundWave *wave);

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
#endif
