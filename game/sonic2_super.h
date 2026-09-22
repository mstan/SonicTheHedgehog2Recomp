#pragma once
#include "sonic2_donor_assets.h"
/* Character-local rendering of S2's native Super lifecycle; no engine hooks. */
unsigned s2_super_glow(unsigned phase,unsigned frame);
uint16_t s2_super_color(unsigned kind,unsigned index,uint16_t normal,
                        unsigned phase,unsigned frame,const S2DonorBank *bank);
