#pragma once
#include <stddef.h>
#include <stdint.h>

enum { S2_STAGE_LAYOUT_COUNT=34, S2_STAGE_LAYOUT_BYTES=0x10000,
       S2_STAGE_LAYOUT_ROM=0x140000 };
typedef struct S2StageLayouts {
    uint32_t address[S2_STAGE_LAYOUT_COUNT];
    size_t size;
    uint8_t data[S2_STAGE_LAYOUT_BYTES];
} S2StageLayouts;

/* Original KiS2 object lists, including CNZ's lists in the lock-on tail.
 * Terrain, collision, rings and their native loaders remain Sonic 2's. */
int s2_stage_layouts_decode(const uint8_t *rom,size_t size,S2StageLayouts *out);
uint32_t s2_stage_layout_address(const S2StageLayouts *layouts,unsigned zone,unsigned act);
