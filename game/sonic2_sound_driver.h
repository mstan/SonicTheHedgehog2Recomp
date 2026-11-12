#ifndef SONIC2_SOUND_DRIVER_H
#define SONIC2_SOUND_DRIVER_H
#include <stddef.h>
#include <stdint.h>
struct RecompLauncherCModProvider;
#ifdef SONIC2_MUSIC_DRIVER
enum { S2_SOUND_STATE_BYTES = 72 };
#else
enum { S2_SOUND_STATE_BYTES = 40 };
#endif
void s2_sound_load_settings(const char *settings);
void s2_sound_post_reset(void);
int s2_sound_hook(uint32_t pc);
int s2_sound_dispatch_miss(uint32_t pc);
int s2_sound_available(void);
int s2_sound_uses_rom(void);
int s2_sound_selected(void);
int s2_sound_active(void);
int s2_sound_select(int enabled);
const char *s2_sound_error(void);
int s2_sound_verified(void);
int s2_sound_commit(void);
int s2_sound_netplay_allowed(void);
void s2_sound_config_image(char *out, size_t size);
void s2_sound_state_save(void *out);
int s2_sound_state_valid(const void *data);
const struct RecompLauncherCModProvider *s2_sound_mods(const struct RecompLauncherCModProvider *base);
#endif
