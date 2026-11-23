#pragma once
#include <stddef.h>
#include <stdint.h>

typedef struct S2Vgm S2Vgm;
struct S2StateIO;

S2Vgm *s2_vgm_open(const uint8_t *data, size_t bytes, int looping);
void s2_vgm_close(S2Vgm *track);
void s2_vgm_reset(void);
void s2_vgm_start(unsigned track);
void s2_vgm_stop(void);
void s2_vgm_suspend(void);
void s2_vgm_discard_suspended(void);
void s2_vgm_resume(void);
void s2_vgm_frame(const S2Vgm *track, unsigned mask);
void s2_vgm_mix(int16_t *output, size_t frames, double rate, unsigned volume);
void s2_vgm_state(struct S2StateIO *io, const S2Vgm *const *tracks,
                  unsigned active_track, unsigned suspended_track);
