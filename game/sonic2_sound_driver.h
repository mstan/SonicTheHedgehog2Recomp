#ifndef SONIC2_SOUND_DRIVER_H
#define SONIC2_SOUND_DRIVER_H
#include <stddef.h>
#include <stdint.h>
struct S2StateIO;
enum { S2_SOUND_STATE_BYTES = 40 };
void s2_sound_load_settings(const char *settings);
void s2_sound_post_reset(void);
int s2_sound_hook(uint32_t pc);
/* Audition status: 1 playing, 0 finished, -1 before runtime initialization. */
int s2_sound_music_playing(void);
unsigned s2_sound_music_channels(unsigned native);
const char *s2_sound_music_channel_name(unsigned channel,int playing);
int s2_sound_netplay_allowed(void);
void s2_sound_state_save(void *out);
void s2_sound_sequence_state(struct S2StateIO *io);
int s2_sound_state_valid(const void *data);
#endif
