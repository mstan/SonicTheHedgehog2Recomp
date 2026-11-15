#pragma once
#include <stddef.h>
#include <stdint.h>

enum { S2_STAGE_LAYOUT_COUNT=34, S2_STAGE_LAYOUT_BYTES=0x10000,
       S2_STAGE_LAYOUT_ROM=0x140000 };
typedef struct S2StageLayouts {
    uint32_t address[S2_STAGE_LAYOUT_COUNT];
    size_t size;
    uint8_t data[S2_STAGE_LAYOUT_BYTES];
    uint8_t special_ring_requirements[28];
    int special_ring_requirements_ready;
} S2StageLayouts;

/* Original KiS2 object lists, including CNZ's lists in the lock-on tail.
 * Terrain, collision and ring placements retain Sonic 2's native loaders.
 * The original solo half-pipe requirements are decoded with the campaign. */
int s2_stage_layouts_decode(const uint8_t *rom,size_t size,S2StageLayouts *out);
uint32_t s2_stage_layout_address(const S2StageLayouts *layouts,unsigned zone,unsigned act);
