#pragma once
#include <stdint.h>
#include <stddef.h>
int s2_runtime_state_needs_frame_end(void);
#include "sonic2_party.h"
#include "video/genesis_vdp.h"
typedef struct S2RuntimeSprite {
    int16_t x,y;
    uint16_t art;
    uint8_t frame,flip,visible,wet;
} S2RuntimeSprite;
enum { S2_PRESENTATION_DUST=(0xD000-0xB400)/64 };
/* Immutable presentation snapshot; never part of simulation or rollback. */
typedef struct S2RuntimePresentation {
    S2RuntimeSprite sprites[S2_MAX_PLAYERS+1],dust[S2_PRESENTATION_DUST];
    GVDP vdp;
    uint8_t world[65536];
    int active,vs,left,top,vs_left,vs_top,green_card;
    uint16_t card_color;
} S2RuntimePresentation;
void s2_runtime_presentation_capture(S2RuntimePresentation *out);
int s2_runtime_presentation_matches(const S2RuntimePresentation *a,const S2RuntimePresentation *b);
void s2_runtime_presentation_row(const S2RuntimePresentation *a,const S2RuntimePresentation *b,
                                double phase,const GVDP *v,int line,uint32_t *out,int width);
size_t s2_runtime_state_size(void);
int s2_runtime_state_save(void *data,size_t size);
int s2_runtime_state_load(const void *data,size_t size,int apply);
int s2_runtime_state_at_boundary(void);
int s2_runtime_hook(uint32_t pc);
void s2_runtime_load(void);
void s2_runtime_prepare_stages(void);
uint32_t s2_runtime_stage_layout(unsigned zone,unsigned act);
void s2_runtime_capture(void);
int s2_runtime_campaign_online(void);
/* Read-only world positions, including the dynamically reserved P3/P4 slots. */
int s2_runtime_player_position(unsigned player, int *x, int *y);
const char *s2_runtime_state_unavailable_reason(void);
struct GVDP;
void s2_runtime_overlay(const struct GVDP *vdp, int line, uint32_t *out, int width);
/* Rollback state (GameSpec rb_state_save/load, runner/rb_state.c "game"). */
size_t s2_runtime_rb_save(void *dst, size_t cap);
int s2_runtime_rb_load(const void *src, size_t len);

/* Source-backed item artwork belongs to the actual shield/star parent. */
int s2_runtime_knuckles_item(unsigned object);
int s2_runtime_amy_item(unsigned object);
int s2_runtime_shadow_star(unsigned object);
const uint8_t *s2_runtime_ui_font(void);
const uint8_t *s2_runtime_title_font(void);

uint16_t s2_runtime_title_card_ink(void);
