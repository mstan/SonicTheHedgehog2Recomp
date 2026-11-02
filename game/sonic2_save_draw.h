#pragma once
#include "sonic2_campaign.h"
#include "sonic2_save_assets.h"
#include "sonic2_donor_assets.h"

enum { S2_SAVE_PORTRAIT_WIDTH=80, S2_SAVE_PORTRAIT_HEIGHT=48 };
typedef struct S2SavePortrait {
    unsigned menu_frame; /* original menu art, or a verified gameplay image */
    const S2DonorFrame *image;
    const uint16_t *palette;
} S2SavePortrait;
typedef struct S2SaveView {
    S2CampaignData data;
    unsigned selection, frame; /* 0=No Save, 1..8=files, 9=Delete */
    int cursor, scroll, erase, confirm, read_only;
    int delete_x;
    unsigned delete_frame;
    int replay_selected; /* Completed file: static/CLEAR until a destination is picked. */
    const char *notice;
    const uint32_t *portrait; /* immutable 80x48 host composition; NULL = stock pair */
} S2SaveView;
void s2_save_portraits(const S2SaveAssets *assets,const S2SavePortrait *portraits,
                      unsigned count,uint32_t *out);
/* 320x224 donor composition centered in any output width. Host-only pixels. */
void s2_save_draw_line(const S2SaveAssets *assets,const S2SaveView *view,
                       int line,uint32_t *out,int width);
void s2_save_draw_notice(const S2SaveAssets *assets,const char *message,int line,uint32_t *out,int width);
