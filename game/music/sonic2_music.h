#pragma once
#include <stddef.h>
#include <stdint.h>
struct RecompLauncherCModProvider;
struct S2StateIO;
enum { S2_MUSIC_SHARED,S2_MUSIC_CAMPAIGN,S2_MUSIC_VS,S2_MUSIC_GROUPS };
enum { S2_DONOR_S1,S2_DONOR_S2,S2_DONOR_AUG,S2_DONOR_WAI,S2_DONOR_PROTO,
       S2_DONOR_S3,S2_DONOR_SK,S2_DONOR_SEP14,S2_ROM_DONOR_COUNT,S2_DONOR_CD_US=S2_ROM_DONOR_COUNT,S2_DONOR_COUNT,S2_DONOR_CUSTOM=S2_DONOR_COUNT };
enum { S2_THEME_LIFE,S2_THEME_OVER,S2_THEME_CONTINUE,S2_THEME_CLEAR,S2_THEME_INVINCIBLE,S2_THEME_COUNT=5,S2_MUSIC_COUNT=31 };
typedef struct S2MusicChoice {const char *id,*label;unsigned donor,song;} S2MusicChoice;
typedef struct S2MusicSelection {int enabled,choice;} S2MusicSelection;
extern S2MusicSelection s2_themes[S2_MUSIC_COUNT];
extern const char *const s2_theme_ids[S2_MUSIC_COUNT];
extern const char *const s2_theme_names[S2_MUSIC_COUNT];
extern const char *const s2_donor_ids[S2_DONOR_COUNT];
extern const char *const s2_donor_names[S2_DONOR_COUNT];
void s2_music_load(const char *settings);
int s2_themes_save(void);
int s2_themes_validate(void);
const char *s2_themes_error(void);
void s2_themes_clear_error(void);
int s2_theme_choices(unsigned track);
const S2MusicChoice *s2_theme_choice(unsigned track,unsigned choice);
unsigned s2_music_group(unsigned track);
const char *s2_music_section(unsigned track);
unsigned s2_music_target(unsigned track);
int s2_music_available(unsigned track);
unsigned s2_music_title_frames(void);
int s2_music_active(unsigned track);
int s2_music_group_enabled(unsigned group);
int s2_music_group_enable(unsigned group,int enabled);
int s2_music_pick(unsigned track,const char *choice);
int s2_themes_enabled(void);
int s2_theme_ready(unsigned track);
int s2_theme_donor(unsigned track);
int s2_music_donor(unsigned track);
int s2_donor_verified(unsigned donor);
const char *s2_donor_path(unsigned donor);
uint8_t *s2_donor_read(unsigned donor,size_t *size);
int s2_music_donor_use(unsigned donor,const char *verified_path);
int s2_theme_import(unsigned track,const char *path,void (*progress)(void *,float,const char *),void *user);
int s2_music_import_file(const char *source,const char *expected,const char *settings,char content[1024],char *error,size_t capacity,void (*progress)(void *,float,const char *),void *user);
int s2_music_prepare(uint8_t *rom,size_t size,uint32_t music,uint32_t sfx,uint32_t psg,uint32_t samples,char *error,size_t error_size);
const struct RecompLauncherCModProvider *s2_music_mods(const struct RecompLauncherCModProvider *base);

void s2_music_config_identity(uint8_t out[32]);
