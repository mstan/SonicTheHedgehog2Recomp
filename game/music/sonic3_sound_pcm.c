/* Reused ROM music decoder from Sonic3AndKnucklesRecomp 70d69e6. */
#include "sonic3_sound_assets.h"
#include <stdio.h>
#include <string.h>

enum { BASE=0x2A0000, DRUMS=68, MAX_SAMPLES=126 };
static unsigned le16(const uint8_t *p){return p[0]|((unsigned)p[1]<<8);}
static void put32(uint8_t *p,uint32_t v)
{p[0]=(uint8_t)(v>>24);p[1]=(uint8_t)(v>>16);p[2]=(uint8_t)(v>>8);p[3]=(uint8_t)v;}
static int fail(char *e,size_t n,const char *text)
{if(e && n)snprintf(e,n,"%s",text);return 0;}
static int range(uint32_t a,size_t n,size_t limit)
{return a>=BASE && a-BASE<=limit && n<=limit-(a-BASE);}
int s3_sound_build_pcm(const uint8_t *rom,size_t size,const S3SoundPCMLayout *l,
                       uint8_t *bank,size_t capacity,uint32_t *used,char *e,size_t en)
{
    if(!l || !bank || !used || capacity>0x400000 || *used>capacity ||
       l->sample_count<DRUMS+1 || l->sample_count>MAX_SAMPLES ||
       l->sega_id<=0x80+DRUMS || l->sega_id>0x80+l->sample_count ||
       !range(l->sample_table,l->sample_count*10+2,*used) ||
       !range(l->map_s3,DRUMS,*used) || !range(l->map_sk,DRUMS,*used))
        return fail(e,en,"Invalid PCM output layout");
    uintptr_t a=(uintptr_t)rom,b=(uintptr_t)bank;
    if((b>=a && b-a<size) || (a>=b && a-b<capacity))
        return fail(e,en,"PCM output aliases ROM");
    S3SoundSource s;
    if(!s3_sound_read_source(rom,size,&s,e,en))return 0;
    const uint8_t *map=bank+(s.standalone_sonic3?l->map_s3:l->map_sk)-BASE;
    uint8_t descriptors[MAX_SAMPLES*10];
    memcpy(descriptors,bank+l->sample_table-BASE,l->sample_count*10);
    uint32_t inputs[DRUMS],lengths[DRUMS],cursor=*used;
    unsigned banks=s.standalone_sonic3?0xDC:0xD6;
    unsigned delta=s.standalone_sonic3?0x10EE:0x1116;
    /* Validate the entire input and output before publishing any descriptor. */
    for(unsigned i=0;i<DRUMS;i++){
        unsigned id=map[i];
        if(id<0x81 || id>0x80+l->sample_count || (descriptors[(id-0x81)*10]&15)!=2)
            return fail(e,en,"Invalid PCM rate template");
        uint32_t base=(uint32_t)s.z80[banks+i+1]<<15;
        if(size==0x200000 && !s.standalone_sonic3 && base>=size){inputs[i]=lengths[i]=0;continue;}
        if(base>size || 2*i+2>size-base)return fail(e,en,"Invalid original DAC bank");
        unsigned ptr=le16(rom+base+2*i);
        if(ptr<0x8000 || base+(ptr&0x7FFFu)>size-5)return fail(e,en,"Invalid original DAC header");
        const uint8_t *h=rom+base+(ptr&0x7FFFu);
        lengths[i]=le16(h+1);ptr=le16(h+3);inputs[i]=base+(ptr&0x7FFFu);
        if(ptr<0x8000 || !lengths[i] || inputs[i]>size || lengths[i]>size-inputs[i] ||
           lengths[i]>0x8000u-(ptr&0x7FFFu) || cursor>capacity || 2*lengths[i]>capacity-cursor)
            return fail(e,en,"Invalid original DAC extent");
        cursor+=2*lengths[i];
    }
    enum { SEGA_START=0xF8000,SEGA_BYTES=0x5E2F };
    if(SEGA_START+SEGA_BYTES>size || cursor>capacity || SEGA_BYTES>capacity-cursor)
        return fail(e,en,"PCM output is too small");
    memset(bank+l->sample_table-BASE,0,l->sample_count*10);
    cursor=*used;
    for(unsigned i=0;i<DRUMS;i++){
        if(!lengths[i])continue; /* absent lock-on cartridge */
        uint8_t *d=bank+l->sample_table-BASE+10*i;
        memcpy(d,descriptors+(map[i]-0x81)*10,2);
        put32(d+2,BASE+cursor);
        unsigned accumulator=0x80;
        for(unsigned j=0;j<lengths[i];j++){
            unsigned v=rom[inputs[i]+j];
            accumulator=(accumulator+s.z80[delta+(v>>4)])&255;bank[cursor++]=(uint8_t)accumulator;
            accumulator=(accumulator+s.z80[delta+(v&15)])&255;bank[cursor++]=(uint8_t)accumulator;
        }
        put32(d+6,BASE+cursor);
    }
    uint8_t *sega=bank+l->sample_table-BASE+(l->sega_id-0x81)*10;
    memcpy(sega,descriptors+(l->sega_id-0x81)*10,2);
    /* The retail SEGA loop has a 12-iteration delay: 245 Z80 cycles per
     * sample in the supported machine. Its effective rate is about 14.6 kHz,
     * while the generic Clone table assumes 16 kHz. MegaPCM's PCM rate byte
     * is a fraction of its 25.1 kHz base clock. */
    unsigned sega_rate=3579545u/(102u+13u*11u);
    sega[1]=(uint8_t)((sega_rate*256u+12550u)/25100u);
    put32(sega+2,BASE+cursor);memcpy(bank+cursor,rom+SEGA_START,SEGA_BYTES);cursor+=SEGA_BYTES;put32(sega+6,BASE+cursor);
    *used=cursor;if(e && en)*e=0;return 1;
}
