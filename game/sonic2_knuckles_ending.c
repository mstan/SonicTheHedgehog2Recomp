#include "sonic2_knuckles_ending.h"
#include <string.h>
static unsigned word(const uint8_t *p) { return (unsigned)p[0]<<8|p[1]; }
static void putword(uint8_t *p,unsigned x) { p[0]=(uint8_t)(x>>8);p[1]=(uint8_t)x; }
int s2_knuckles_ending_decode(const uint8_t *rom,size_t size,S2KnucklesEnding *out)
{
    /* Byte-matched KiS2 disassembly c336fede: the body/jump artwork is in
     * S&K; the ending palettes and mappings are in the lock-on tail. */
    enum { ART=0xDEA00, ART_BYTES=0x970, PALETTE=0x3090BC, MAP=0x3091E0, FRAMES=26 };
    if(!rom || !out || size!=0x340000 || word(rom+MAP)!=FRAMES*2) return 0;
    S2KnucklesEnding c={0};
    c.art=S2_KNUCKLES_ENDING_ROM;
    memcpy(c.data,rom+ART,ART_BYTES);c.size=ART_BYTES;
    c.palette=S2_KNUCKLES_ENDING_ROM+(uint32_t)c.size;
    memcpy(c.data+c.size,rom+PALETTE,128);c.size+=128;
    memcpy(c.super_palette,rom+0x301EE0,sizeof c.super_palette);
    size_t maps=c.size;c.mappings=S2_KNUCKLES_ENDING_ROM+(uint32_t)maps;
    c.size+=FRAMES*2;
    for(unsigned frame=0;frame<FRAMES;++frame) {
        size_t p=MAP+word(rom+MAP+frame*2);
        if(p>size-2)return 0;
        unsigned pieces=word(rom+p);p+=2;
        if(pieces>64 || pieces*6>size-p || 2+pieces*8>sizeof c.data-c.size)return 0;
        putword(c.data+maps+frame*2,(unsigned)(c.size-maps));
        putword(c.data+c.size,pieces);c.size+=2;
        for(unsigned i=0;i<pieces;++i,p+=6,c.size+=8) {
            /* S3 mappings omit the second tile word used only by S2's VS
             * renderer. Keep the ordinary tile and signed coordinates. */
            memcpy(c.data+c.size,rom+p,4);
            memcpy(c.data+c.size+4,rom+p+2,2);
            memcpy(c.data+c.size+6,rom+p+4,2);
        }
    }
    *out=c;return 1;
}
