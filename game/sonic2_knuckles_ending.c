#include "sonic2_knuckles_ending.h"
#include "sonic2_donor_assets.h"
#include <string.h>
static unsigned word(const uint8_t *p) { return (unsigned)p[0]<<8|p[1]; }
static void putword(uint8_t *p,unsigned x) { p[0]=(uint8_t)(x>>8);p[1]=(uint8_t)x; }
int s2_knuckles_ending_decode(const uint8_t *rom,size_t size,S2KnucklesEnding *out)
{
    /* Byte-matched KiS2 disassembly c336fede: the body/jump artwork is in
     * S&K; the ending palettes and mappings are in the lock-on tail. */
    enum { ART=0xDEA00, ART_BYTES=0x970, PALETTE=0x3090BC, MAP=0x3091E0 };
    if(!rom || !out || size!=0x340000 || word(rom+MAP)!=S2_ENDING_FRAMES*2) return 0;
    S2KnucklesEnding c={0};
    c.tile_bytes=(word(rom+ART)&0x7FFFu)*32u;
    if(!c.tile_bytes || !s2_nemesis_decode(rom+ART,ART_BYTES,c.tiles,sizeof c.tiles))return 0;
    memcpy(c.palette,rom+PALETTE,sizeof c.palette);
    memcpy(c.super_palette,rom+0x301EE0,sizeof c.super_palette);
    for(unsigned frame=0;frame<S2_ENDING_FRAMES;++frame) {
        size_t p=MAP+word(rom+MAP+frame*2);
        if(p>size-2)return 0;
        unsigned pieces=word(rom+p);p+=2;
        if(pieces>S2_ENDING_PIECES || pieces*6>size-p)return 0;
        c.frames[frame].count=pieces;
        for(unsigned i=0;i<pieces;++i,p+=6) {
            c.frames[frame].pieces[i]=(S2EndingPiece){
                (int16_t)word(rom+p+4),(int8_t)rom[p],rom[p+1]&15u,(uint16_t)word(rom+p+2)
            };
        }
    }
    *out=c;return 1;
}

unsigned s2_ending_draw_sat(const S2EndingFrame *frame,uint8_t *sat,unsigned count,
                            unsigned attr,unsigned flip,int x,int y)
{
    unsigned first=count;
    for(unsigned i=0;i<frame->count && count<80;++i,++count) {
        const S2EndingPiece *piece=&frame->pieces[i];
        int dx=piece->x,dy=piece->y;
        unsigned tile=piece->attr+attr;
        if(flip&1) {dx=-dx-8*((piece->size>>2)+1);tile^=0x800;}
        if(flip&2) {dy=-dy-8*((piece->size&3)+1);tile^=0x1000;}
        unsigned hardware_x=(x+dx)&511u;
        putword(sat,y+dy);sat[2]=piece->size;sat[3]=(uint8_t)(count+1);
        putword(sat+4,tile);putword(sat+6,hardware_x?hardware_x:1);
        sat+=8;
    }
    return count-first;
}
