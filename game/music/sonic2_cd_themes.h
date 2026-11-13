#pragma once
#include "sonic3_sound_assets.h"
struct S2StateIO;
int s2_cd_donor_verify(const char *manifest,unsigned donor);
void s2_cd_themes_reset(void);
int s2_cd_theme_build(unsigned theme,unsigned donor,const char *manifest,
                      const S3SoundLayout *layout,uint8_t *bank,size_t capacity,
                      uint32_t *used,char *error,size_t error_size);
void s2_cd_theme_request(unsigned native);
void s2_cd_theme_frame(int paused,int oneup);
void s2_cd_theme_tick(uint64_t tick);
void s2_cd_theme_state(struct S2StateIO *io);
void s2_cd_theme_identity(uint8_t out[32]);
void s2_cd_theme_debug(unsigned *active,unsigned *theme,uint64_t *position);
