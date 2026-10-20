#pragma once
#include "sonic2_character.h"

/* Exact Megamix v3 owner-donor port; no sprite-sheet fallback. */
void s2_shadow_register(void);
void s2_shadow_before(S2CharacterState *, S2Motion *, const S2Contacts *);
void s2_shadow_after(S2CharacterState *, S2Motion *);
void s2_shadow_animate(S2CharacterState *, S2Motion *, const S2DonorBank *);
uint16_t s2_shadow_color(const S2DonorBank *, unsigned index, int wet, unsigned phase, unsigned frame);
