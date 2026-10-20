#include "sonic2_mighty.h"
#include "sonic2_party.h"
#include "sonic2_resources.h"

/* Mighty graphics by Team Megamix; characters SEGA / Sonic Team. Exact v3
 * source bindings match Sonic3AndKnucklesRecomp/tools/megamix/v3_rom.py.
 * The verified owner donor is decoded privately; no ROM art is embedded. */
static int available(void)
{ return s2_party.mighty_enabled && s2_resource_verified(S2_RESOURCE_MEGAMIX); }
static const S2Character mighty={"mighty","MIGHTY",1,available};
void s2_mighty_register(void) { s2_character_register(&mighty); }

unsigned s2_mighty_frame(unsigned frame)
{
    static const uint8_t frames[214]={
#include "sonic2_mighty_frames.inc"
    };
    return frame<sizeof frames?frames[frame]:0;
}
