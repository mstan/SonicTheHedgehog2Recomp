/* Reused ROM music decoder from Sonic3AndKnucklesRecomp 70d69e6. */
#include "sonic3_sound_assets.h"
#include "sonic2_sound_assets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { DRUMS=S2_DONOR_PCM_SLOTS };
static unsigned le16(const uint8_t *p){return p[0]|((unsigned)p[1]<<8);}

int s3_sound_decode_drum(const uint8_t *rom,const S3SoundSource *source,unsigned note,
                         S2SoundWave *wave)
{
    /* Compatibility rates for the previously qualified retail playback
     * profile, in units of 25.1 kHz / 256. Only the rate metadata is fixed.
     * The original image supplies the compressed waveform and delta table. */
    static const uint8_t retail_rates[DRUMS]={
        193,114,91,77,65,193,193,170,136,91,74,151,130,101,151,208,160,
        136,114,170,136,119,98,130,95,86,124,136,81,81,124,124,136,136,
        81,81,124,143,81,81,124,136,119,170,105,81,143,98,81,86,65,
        124,124,124,81,124,124,81,81,81,124,124,72,130,109,101,98,130
    };
    if(note<0x81 || note>0x80+DRUMS)return 0;
    unsigned index=note-0x81,rate,delta,base;
    if(source->prototype) {
        if(index>=27)return 0;
        base=0xF0000;
        delta=0xE39;
    } else {
        unsigned bank_table=source->standalone_sonic3?0xDC:0xD6;
        base=(unsigned)source->z80[bank_table+index+1]<<15;
        delta=source->standalone_sonic3?0x10EE:0x1116;
    }
    unsigned size=source->rom_size;
    if(base>size || 2*index+2>size-base)return 0;
    unsigned header=le16(rom+base+2*index);
    unsigned offset=header&0x7FFFu;
    if(header<0x8000 || offset>size-base || size-base-offset<5)return 0;
    const uint8_t *desc=rom+base+offset;
    unsigned length=le16(desc+1),address=le16(desc+3),start=address&0x7FFFu;
    if(address<0x8000 || !length || length>0x8000u-start || start>size-base || length>size-base-start)return 0;
    if(source->prototype) {
        if(!desc[0])return 0;
        unsigned hz=7159090u/(303u+26u*(desc[0]-1));
        rate=(hz*256u+12550u)/25100u;
    } else rate=index==50 && !source->standalone_sonic3?66:retail_rates[index];
    uint8_t *out=malloc(length*2u);
    if(!out)return 0;
    unsigned accumulator=0x80;
    for(unsigned i=0;i<length;++i) {
        unsigned value=rom[base+start+i];
        accumulator=(accumulator+source->z80[delta+(value>>4)])&255u;
        out[2*i]=(uint8_t)accumulator;
        accumulator=(accumulator+source->z80[delta+(value&15u)])&255u;
        out[2*i+1]=(uint8_t)accumulator;
    }
    *wave=(S2SoundWave){out,length*2u,(rate*25100u+128u)/256u};
    return 1;
}
