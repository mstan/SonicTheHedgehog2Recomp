#pragma once
#include <stddef.h>
#include <stdint.h>
enum { S2_ENDING_FRAMES=26, S2_ENDING_PIECES=64, S2_ENDING_TILE_BYTES=0x4000,
       S2_ENDING_JUMP_MAP=0xADA2 };
typedef struct S2EndingPiece {
    int16_t x;
    int8_t y;
    uint8_t size;
    uint16_t attr;
} S2EndingPiece;
typedef struct S2EndingFrame {
    unsigned count;
    S2EndingPiece pieces[S2_ENDING_PIECES];
} S2EndingFrame;
typedef struct S2KnucklesEnding {
    size_t tile_bytes;
    uint8_t tiles[S2_ENDING_TILE_BYTES], palette[128];
    uint8_t super_palette[66]; /* original ten-frame cycle plus revert */
    S2EndingFrame frames[S2_ENDING_FRAMES];
} S2KnucklesEnding;
int s2_knuckles_ending_decode(const uint8_t *rom,size_t size,S2KnucklesEnding *out);
unsigned s2_ending_draw_sat(const S2EndingFrame *frame,uint8_t *sat,unsigned count,
                            unsigned attr,unsigned flip,int x,int y);
