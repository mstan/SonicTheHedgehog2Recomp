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
enum { S2_SPECIAL_FRAMES=18,S2_SPECIAL_PIECES=6,S2_SPECIAL_TILES=32 };
enum { S2_SS_RESULT_FRAMES=30,S2_SS_RESULT_PIECES=16 };
typedef struct S2SpecialPiece {
    int16_t x;
    int8_t y;
    uint8_t size;
    uint16_t attr;
} S2SpecialPiece;
typedef struct S2SpecialArtFrame {
    uint8_t count;
    S2SpecialPiece pieces[S2_SPECIAL_PIECES];
    uint8_t tiles[S2_SPECIAL_TILES*32];
} S2SpecialArtFrame;
typedef struct S2SpecialResultArtFrame {
    uint8_t count;
    S2SpecialPiece pieces[S2_SS_RESULT_PIECES];
} S2SpecialResultArtFrame;

typedef struct S2SpecialStageArt {
    S2SpecialArtFrame frames[S2_SPECIAL_FRAMES];
    uint16_t palette[16];
    S2SpecialResultArtFrame results[S2_SS_RESULT_FRAMES];
    uint8_t result_name[8*32],font_y[4*32],hud_name[5*32];
} S2SpecialStageArt;
typedef struct S2KnucklesArt {
    uint8_t life[12*32],shield_stars[66*32],monitor[8*32],signpost[24*32];
    uint8_t continue_icons[12*32],font_k[4*32],slot[512];
    uint16_t palette[16],water_palette[2][16]; /* KiS2 CPZ and ARZ */
    S2SpecialArtFrame special[S2_SPECIAL_FRAMES];
    uint16_t special_palette[16];
    S2SpecialResultArtFrame special_results[S2_SS_RESULT_FRAMES];
} S2KnucklesArt;
typedef struct S2ItemArt {
    uint8_t life[4*32],continue_icons[12*32];
} S2ItemArt;
/* Immutable native competition HUD sources from the verified REV01 ROM. */
typedef struct S2NativeHudArt {
    uint8_t life[3][12*32]; /* Sonic, Miles, Tails */
    uint8_t font[88*32], numbers[10*32];
} S2NativeHudArt;
int s2_native_hud_art_decode(const uint8_t *rom,size_t size,S2NativeHudArt *out);
/* REV01's native 34-tile star atlas, retaining S2 animation/geometry. */
int s2_native_stars_decode(const uint8_t *rom,size_t size,uint8_t out[34*32]);
enum { S2_NATIVE_SIGNPOST, S2_NATIVE_CAPSULE, S2_NATIVE_EHZ_CAR, S2_NATIVE_SPRITE_ART_COUNT };
/* Original world art can be visible before the native pattern load finishes.
 * Returns decoded bytes; the guest's VRAM and load queue remain unchanged. */
unsigned s2_native_sprite_art_decode(const uint8_t *rom,size_t size,unsigned asset,
                                    uint8_t *out,size_t capacity);
/* Exact original lock-on artwork; shares the existing bounded Nemesis codec. */
int s2_knuckles_art_decode(const uint8_t *rom,size_t size,S2KnucklesArt *out);
/* Match a displayed native slot window, including partial scrolling rows.
 * Input ROM is immutable; no guest index, RNG or DMA source is changed. */
int s2_knuckles_slot_patterns(const uint8_t *rom,size_t size,const S2KnucklesArt *art,
                              unsigned slot,const uint8_t source[512],uint8_t out[512]);
/* Original Amy 1.7.1 half-pipe mappings and standard ROM DPLCs. */
int s2_amy_special_decode(const uint8_t *rom,size_t size,S2SpecialStageArt *out);
/* Completes an already decoded Amy half-pipe bank transactionally. */
int s2_amy_special_ui_decode(const uint8_t *rom,size_t size,S2SpecialStageArt *out);
int s2_amy_item_art_decode(const uint8_t *rom,size_t size,S2ItemArt *out);
extern const S2DonorLayout s2_amy_171_layout;
extern const S2DonorLayout s2_sk_knuckles_layout;
extern const S2DonorLayout s2_megamix_mighty_layout;
extern const S2DonorLayout s2_megamix_shadow_layout;
int s2_donor_decode(const uint8_t *rom, size_t size, const S2DonorLayout *layout,
                    S2DonorBank *out, char *error, size_t error_size);
void s2_donor_free(S2DonorBank *bank);
