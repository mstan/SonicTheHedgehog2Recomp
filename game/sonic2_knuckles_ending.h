#pragma once
#include <stddef.h>
#include <stdint.h>
enum { S2_KNUCKLES_ENDING_ROM=0x110000, S2_KNUCKLES_ENDING_BYTES=0x4000 };
typedef struct S2KnucklesEnding {
    uint32_t art,palette,mappings;
    size_t size;
    uint8_t data[S2_KNUCKLES_ENDING_BYTES];
    uint8_t super_palette[66]; /* original ten-frame cycle plus revert */
} S2KnucklesEnding;
int s2_knuckles_ending_decode(const uint8_t *rom,size_t size,S2KnucklesEnding *out);
