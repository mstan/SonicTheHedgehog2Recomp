#pragma once
int s2_resources_prepare_stage_images(const unsigned char *rom,unsigned size);
#include "sonic2_donor_assets.h"
#include "sonic2_save_assets.h"
#include "sonic2_stage_layouts.h"
#include "sonic2_knuckles_ending.h"
enum { S2_RESOURCE_AMY, S2_RESOURCE_SK, S2_RESOURCE_MEGAMIX, S2_RESOURCE_KIS2, S2_RESOURCE_COUNT };
/* Banks contain only normalized gameplay art, never a mapped guest ROM. */
void s2_resources_load(const char *settings_path);
int s2_resource_set(unsigned resource, const char *path);
int s2_resource_verified(unsigned resource);
int s2_resource_owner_verified(unsigned resource);
const S2SaveAssets *s2_resource_save_assets(void);
const S2DonorBank *s2_resource_bank(unsigned resource);
const S2DonorBank *s2_resource_shadow_bank(void);
const S2KnucklesArt *s2_resource_knuckles_art(void);
const S2StageLayouts *s2_resource_knuckles_stages(void);
const S2KnucklesEnding *s2_resource_knuckles_ending(void);
const S2SpecialStageArt *s2_resource_amy_special(void);
const S2ItemArt *s2_resource_amy_items(void);
const S2ItemArt *s2_resource_primary_items(void);
const char *s2_resource_error(unsigned resource);
void s2_resources_shutdown(void);
