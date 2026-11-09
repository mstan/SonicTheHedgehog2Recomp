#pragma once
#include "sonic2_state_io.h"
void s2_video_rb_state(S2StateIO *io);
void s2_video_state(S2StateIO *io);
unsigned s2_video_main_cpu_divisor(void);
unsigned s2_video_level_zone(void);
void s2_video_vblank(void);
#include "game_video.h"
extern const GameVideo sonic2_video;
int s2_video_hook(uint32_t pc);
void s2_video_command(int id, const char *json);
void s2_video_actor_origin(const struct GVDP *vdp, int line, int width, int *left, int *top);
int s2_video_runoff_x(unsigned role,unsigned object,int x);
int s2_video_runoff_velocity(unsigned role);
int s2_video_actor_pixel_visible(const struct GVDP *vdp, int wx, int wy, int high, const unsigned char *world);
void s2_video_front_ui(const struct GVDP *vdp,int line,uint32_t *out,int width);
void s2_video_preserve_foreground_row(const uint32_t *out,int width);
int s2_video_intro_pixel_visible(const struct GVDP *vdp,int native_x,int line,int high);
int s2_video_knuckles_hud_pixel(const struct GVDP *vdp,unsigned attr,int x,int y,uint32_t *color);
int s2_video_hud_origin(int width);

void s2_video_prepare_knuckles_items(void);
int s2_video_water_palette(const struct GVDP *v);
uint16_t s2_video_donor_color(uint16_t shade);
void s2_video_donor_rgb_row(uint32_t *pixels,int width);
const struct GVDP *s2_video_slots_overlay(const struct GVDP *v,int line,uint32_t *out,int width);
void s2_video_results_name(const struct GVDP *v,int line,uint32_t *out,int width,int draw);
void s2_video_special_actor(const struct GVDP *v,int line,uint32_t *out,int width);
void s2_video_special_results(const struct GVDP *v,int line,uint32_t *out,int width);

int s2_video_intro_world_visible(void);

int s2_video_competition_divider(const struct GVDP *v,int line,uint32_t *out,int width);

int s2_video_competition_extended(void);
