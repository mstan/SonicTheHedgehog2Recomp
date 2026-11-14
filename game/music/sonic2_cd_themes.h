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

int s2_cd_custom_build(unsigned track,unsigned kind,const uint8_t *data,size_t bytes,
                        unsigned frames,unsigned loop,const uint8_t hash[32],const S3SoundLayout *layout,
                        uint8_t *bank,size_t capacity,uint32_t *used,char *error,size_t error_size);
void s2_cd_theme_audition(unsigned track,unsigned solo,int stopped);
unsigned s2_cd_theme_channel_count(unsigned track);
const char *s2_cd_theme_channel_name(unsigned track,unsigned solo);
