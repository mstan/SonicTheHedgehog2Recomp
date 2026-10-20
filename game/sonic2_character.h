#pragma once
#include <stdint.h>
#include "sonic2_donor_assets.h"
/* Portable character-local state. No guest addresses, global camera, ROM
 * pointers or native object allocation are part of a character controller. */
enum { S2_CHAR_SONIC, S2_CHAR_TAILS, S2_CHAR_AMY, S2_CHAR_KNUCKLES, S2_CHAR_SHADOW, S2_CHAR_MIGHTY, S2_CHAR_NONE };
enum { S2_MOVE_NATIVE, S2_MOVE_AIR, S2_MOVE_GROUND, S2_MOVE_FIXED };
enum { S2_SK_NORMAL, S2_SK_GLIDE, S2_SK_FALL, S2_SK_SLIDE, S2_SK_CLIMB, S2_SK_LEDGE };
typedef struct S2Motion {
    int32_t x, y; /* 16.16 positions; velocities and inertia are 8.8 */
    int16_t vx, vy, inertia;
    uint8_t status, angle, radius_x, radius_y, animation, frame, jumping;
    uint8_t hurt, boosted, control_locked;
    uint16_t rings, physics_top;
    uint8_t super, speed_shoes, invincible_monitor, physics_override, blast;
} S2Motion;
typedef struct S2Contacts {
    int floor, wall, ceiling; /* signed distances in pixels, in facing direction */
    int16_t (*sine)(unsigned angle); /* host's verified integer sine table */
    unsigned target;
    int target_dx, target_dy;
    uint8_t (*angle)(int dx, int dy);
} S2Contacts;
typedef struct S2CharacterState {
    unsigned kind, special, age, charge, ledge;
    uint8_t turn, held, pressed, native_held, native_pressed;
    uint8_t move, before_air, before_jumping;
    int16_t jump_adjustment;
    int32_t wall_x;
    unsigned animation, animation_index, animation_timer;
    uint8_t frame, render_flip, animation_changed;
    unsigned shadow_air_used, shadow_target, shadow_last_target, shadow_target_age;
    unsigned shadow_chain, shadow_chaos, source_animation, shadow_super;
    unsigned shadow_monitor_ticks; /* S-monitor prerequisite, independent of Super invincibility */
} S2CharacterState;
void s2_character_reset(S2CharacterState *s, unsigned kind);
void s2_character_before(S2CharacterState *s, S2Motion *m, uint8_t held, uint8_t pressed, const S2Contacts *c);
void s2_character_after(S2CharacterState *s, S2Motion *m, const S2Contacts *c);
void s2_character_animate(S2CharacterState *s, S2Motion *m, const S2DonorBank *bank);
int s2_character_attacking(const S2CharacterState *s, const S2Motion *m);
