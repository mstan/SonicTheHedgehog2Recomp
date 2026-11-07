#pragma once
#include <stddef.h>
#include <stdint.h>

/* Asset-only decoder: the donor never becomes g_rom and no donor entry point
 * executes. Bounds-checked normal gameplay mappings/DPLCs, not title assets. */
enum { S2_DONOR_MAX_FRAMES = 256, S2_DONOR_MAX_PIECES = 64 };
typedef struct S2DonorLayout {
    unsigned frames, mappings, dplc, art, palette, mapping_stride;
    unsigned animations, animation_count;
    unsigned super_palette, super_frames; /* optional three-shade palette cycle */
    unsigned super_animations, super_animation_count;
} S2DonorLayout;
typedef struct S2DonorFrame {
    int16_t x, y;
    uint16_t width, height;
    uint8_t *pixels; /* palette indices; zero transparent; owned by bank */
} S2DonorFrame;
typedef struct S2DonorBank {
    unsigned count;
    uint16_t palette[16];
    unsigned super_frames;
    uint16_t super_palette[10][3];
    S2DonorFrame frames[S2_DONOR_MAX_FRAMES];
    unsigned animation_count;
    uint16_t animation_length[64];
    uint8_t animations[64][256];
    unsigned super_animation_count;
    uint16_t super_animation_length[64];
    uint8_t super_animations[64][256];
    uint16_t water_palette[16], cycle_palette[2][16][4];
    uint16_t water_palette_alt[16]; /* optional second native water zone */
    unsigned water_palette_count;
} S2DonorBank;
typedef struct S2KnucklesArt {
    uint8_t life[12*32],shield_stars[66*32],monitor[8*32],signpost[24*32];
    uint8_t continue_icons[12*32],font_k[4*32];
    uint16_t palette[16],water_palette[2][16]; /* KiS2 CPZ and ARZ */
} S2KnucklesArt;
/* Immutable native competition HUD sources from the verified REV01 ROM. */
typedef struct S2NativeHudArt {
    uint8_t life[3][12*32]; /* Sonic, Miles, Tails */
    uint8_t font[88*32], numbers[10*32];
} S2NativeHudArt;
int s2_native_hud_art_decode(const uint8_t *rom,size_t size,S2NativeHudArt *out);
/* REV01's native 34-tile star atlas, retaining S2 animation/geometry. */
int s2_native_stars_decode(const uint8_t *rom,size_t size,uint8_t out[34*32]);
/* Exact original lock-on artwork; shares the existing bounded Nemesis codec. */
int s2_knuckles_art_decode(const uint8_t *rom,size_t size,S2KnucklesArt *out);
extern const S2DonorLayout s2_amy_171_layout;
extern const S2DonorLayout s2_sk_knuckles_layout;
extern const S2DonorLayout s2_megamix_mighty_layout;
extern const S2DonorLayout s2_megamix_shadow_layout;
int s2_donor_decode(const uint8_t *rom, size_t size, const S2DonorLayout *layout,
                    S2DonorBank *out, char *error, size_t error_size);
void s2_donor_free(S2DonorBank *bank);
