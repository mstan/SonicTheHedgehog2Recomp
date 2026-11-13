/* Reused ROM music decoder from Sonic3AndKnucklesRecomp 70d69e6. */
#include "sonic3_sound_assets.h"
#include <stdio.h>
#include <string.h>
enum { MUSIC_BASE=0x2A0000 };
static unsigned le16(const uint8_t *p){return p[0]|((unsigned)p[1]<<8);}
static void put32(uint8_t *p,uint32_t v)
{p[0]=(uint8_t)(v>>24);p[1]=(uint8_t)(v>>16);p[2]=(uint8_t)(v>>8);p[3]=(uint8_t)v;}
int s3_sound_build_prototype_pcm(const uint8_t *rom,size_t size,const S3SoundPCMLayout *l,
                                 uint8_t *bank,size_t capacity,uint32_t *used,char *e,size_t en)
{
    S3SoundSource source;
    if(!l || !bank || !used || *used>capacity || capacity>0x400000 || l->sample_count<97 ||
       l->sample_table<MUSIC_BASE || l->sample_table-MUSIC_BASE>*used ||
       l->sample_count*10u+2u>*used-(l->sample_table-MUSIC_BASE) ||
       !s3_sound_read_prototype(rom,size,&source,e,en))return 0;
    uintptr_t a=(uintptr_t)rom,b=(uintptr_t)bank;
    if((b>=a && b-a<size) || (a>=b && a-b<capacity))return 0;
    uint32_t input[27],length[27],rate[27],cursor=*used;
    for(unsigned i=0;i<27;i++){
        unsigned ptr=le16(rom+0xF0000+i*2);
        if(ptr<0x8000)goto bad;
        unsigned h=0xF0000+(ptr&0x7FFFu),delay=rom[h];
        ptr=le16(rom+h+3);input[i]=0xF0000+(ptr&0x7FFFu);length[i]=le16(rom+h+1);
        if(!delay || ptr<0x8000 || !length[i] || length[i]>0x8000u-(ptr&0x7FFFu) ||
           cursor>capacity || length[i]*2u>capacity-cursor)goto bad;
        unsigned hz=7159090u/(303u+26u*(delay-1));
        rate[i]=(hz*256u+12550u)/25100u;if(!rate[i] || rate[i]>255)goto bad;
        cursor+=2u*length[i];
    }
    cursor=*used;
    for(unsigned i=0;i<27;i++){
        uint8_t *d=bank+l->sample_table-MUSIC_BASE+(i+70)*10;
        memset(d,0,10);d[0]=2;d[1]=(uint8_t)rate[i];put32(d+2,MUSIC_BASE+cursor);
        unsigned accumulator=0x80;
        for(unsigned j=0;j<length[i];j++){
            unsigned v=rom[input[i]+j];
            accumulator=(accumulator+source.z80[0xE39+(v>>4)])&255;bank[cursor++]=(uint8_t)accumulator;
            accumulator=(accumulator+source.z80[0xE39+(v&15)])&255;bank[cursor++]=(uint8_t)accumulator;
        }
        put32(d+6,MUSIC_BASE+cursor);
    }
    *used=cursor;if(e && en)*e=0;return 1;
bad:
    if(e && en)snprintf(e,en,"Prototype PCM data or output layout is invalid");return 0;
}
