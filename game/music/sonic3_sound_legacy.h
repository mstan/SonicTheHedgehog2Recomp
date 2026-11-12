/* Reused ROM music decoder from Sonic3AndKnucklesRecomp 70d69e6. */
#pragma once
#include <stdint.h>
#include <stddef.h>
/* Decoder workspace contains only the player's ROM and normalized input
 * addresses. All branch targets remain bounded to this verified song. */
typedef struct S3LegacySource {
    const uint8_t *rom;
    size_t size;
    uint8_t z80[0x2000], song[0x2000];
    uint32_t source, bank, low, high, env;
    unsigned zsize, env_count, donor, little, compressed, fast_tempo, force_pal;
} S3LegacySource;
int s3_legacy_source(const uint8_t *rom,size_t size,unsigned donor,unsigned theme,S3LegacySource *out);
int s3_legacy_music_source(const uint8_t *rom,size_t size,unsigned donor,unsigned song,S3LegacySource *out);
int s3_legacy_song(const S3LegacySource *source,uint8_t *out,size_t capacity,
                   unsigned psg_base,const uint8_t dac_map[128],
                   uint8_t drums[128],char *error,size_t error_size);
int s3_legacy_sample(const S3LegacySource *source,unsigned note,uint8_t *out,
                     size_t capacity,unsigned *rate);
int s3_legacy_envelope(const S3LegacySource *source,unsigned index,uint8_t out[256]);
