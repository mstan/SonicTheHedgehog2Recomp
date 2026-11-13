#include "sonic2_stage_layouts.h"
#include <stdlib.h>
#include <string.h>

static unsigned word(const uint8_t *p) { return (unsigned)p[0]<<8|p[1]; }
static uint32_t longword(const uint8_t *p)
{ return (uint32_t)word(p)<<16|word(p+2); }

int s2_stage_layouts_decode(const uint8_t *rom,size_t size,S2StageLayouts *out)
{
    const size_t table=0xDF370;
    if(!rom || !out || size<table+S2_STAGE_LAYOUT_COUNT*4) return 0;
    S2StageLayouts *candidate=calloc(1,sizeof *candidate);
    if(!candidate) return 0;
    for(unsigned i=0;i<S2_STAGE_LAYOUT_COUNT;++i) {
        size_t start=longword(rom+table+i*4),end=start;
        unsigned count=0;
        if((start&1) || start>=size) goto invalid;
        for(;;) {
            if(end>size || size-end<6 || count++>1024) goto invalid;
            unsigned x=word(rom+end);
            if(x==0xFFFF) { end+=6; break; }
            /* ARZ2 contains out-of-order records in the original cartridge.
             * Retain the exact list order consumed by its scrolling loader. */
            end+=6;
        }
        size_t bytes=end-start;
        if(bytes>S2_STAGE_LAYOUT_BYTES-candidate->size) goto invalid;
        candidate->address[i]=S2_STAGE_LAYOUT_ROM+(uint32_t)candidate->size;
        memcpy(candidate->data+candidate->size,rom+start,bytes);
        candidate->size+=bytes;
    }
    *out=*candidate;
    free(candidate);
    return 1;
invalid:
    free(candidate);
    return 0;
}
uint32_t s2_stage_layout_address(const S2StageLayouts *layouts,unsigned zone,unsigned act)
{
    if(!layouts || !layouts->size || act>1 || zone>=S2_STAGE_LAYOUT_COUNT/2) return 0;
    return layouts->address[zone*2+act];
}
