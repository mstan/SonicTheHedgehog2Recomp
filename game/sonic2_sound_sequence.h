#pragma once
#include "sonic2_sound_assets.h"
#include <stdint.h>

enum { S2_FM_CHANNELS=6, S2_PSG_CHANNELS=3, S2_TONE_CHANNELS=9,
       S2_DAC_CHANNEL=9, S2_SEQUENCE_TRACKS=10, S2_PHRASE_DEPTH=8, S2_LOOP_COUNTERS=8 };

typedef struct S2SoundTrack {
    uint16_t pc, period, returns[S2_PHRASE_DEPTH];
    int16_t modulation;
    uint8_t cue, active, channel, depth, loops[S2_LOOP_COUNTERS];
    uint8_t remaining, duration, divider, gate, note, hold, timeout, timeout_master;
    uint8_t transpose, volume, pan, voice, noise, envelope_position, envelope_level, detune;
    uint8_t mod_mode, mod_wait, mod_speed, mod_delta, mod_steps;
    uint8_t mod_delay, mod_counter, mod_direction, mod_remaining;
    uint8_t dirty, raw_register[8], raw_value[8], raw_count, raw_global;
} S2SoundTrack;

/* Snapshot state contains cue IDs and byte offsets. The immutable decoded
 * bank and the chip sink live outside the snapshot. */
typedef struct S2SoundSequence {
    S2SoundTrack music[S2_SEQUENCE_TRACKS], saved[S2_SEQUENCE_TRACKS];
    S2SoundTrack effect[S2_TONE_CHANNELS];
    uint32_t pcm_phase;
    uint16_t pcm_position;
    uint8_t pcm_id, dac_enabled, music_cue, saved_cue;
    uint8_t tempo, tempo_accumulator, saved_tempo, saved_accumulator, speed_up;
    uint8_t pal_count, paused, fade, fade_clock, fade_in, restore_pending;
    uint8_t effect_owner[S2_TONE_CHANNELS], ring_side, communication, effect_priority, push_playing;
    uint8_t fault_cue, fault_channel;
    uint16_t fault_offset;
} S2SoundSequence;

enum { S2_CHIP_FM1, S2_CHIP_FM2, S2_CHIP_PSG };
typedef void (*S2SoundWrite)(void *context, uint32_t cycle, uint8_t chip,
                             uint8_t address, uint8_t value);

void s2_sequence_reset(S2SoundSequence *sequence);
int s2_sequence_playing(const S2SoundSequence *sequence);
/* One update per original sound-queue call. The five bytes are Music0,
 * three SFX slots, and Music1. Only the selected music slot is consumed. */
int s2_sequence_frame(S2SoundSequence *sequence, const S2StockSound *bank,
                      uint8_t queue[5], int pal, uint32_t cpu_hz,
                      uint32_t frame_cycles, S2SoundWrite write, void *context);
