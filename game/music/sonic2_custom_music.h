#pragma once
#include "sonic3_sound_assets.h"
enum { S2_CUSTOM_SMPS=1,S2_CUSTOM_PCM,S2_CUSTOM_VGM };
void s2_custom_load(const char *settings);
int s2_custom_ready(unsigned track);
const char *s2_custom_path(unsigned track);
int s2_custom_use(unsigned track,const char *path,char *error,size_t capacity);
int s2_custom_import(unsigned track,const char *source,char *error,size_t capacity,
                     void (*progress)(void *,float,const char *),void *user);
void s2_custom_identity(unsigned track,uint8_t out[32]);
int s2_custom_build(unsigned track,const S3SoundLayout *layout,uint8_t *bank,
                    size_t capacity,uint32_t *used,char *error,size_t error_size);
int s2_custom_assemble(const char *source,const char *track,const char *settings,
                       char output[1024],char *error,size_t capacity,
                       void (*progress)(void *,float,const char *),void *user);
