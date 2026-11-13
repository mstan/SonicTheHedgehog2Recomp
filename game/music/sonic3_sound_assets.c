/* Reused ROM music decoder from Sonic3AndKnucklesRecomp 70d69e6. */
#include "sonic3_sound_assets.h"
#include "sha256.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct KosReader {
    const uint8_t *rom;
    size_t size, pos;
    unsigned descriptor, bits;
    int bad;
} KosReader;
static unsigned kb(KosReader *k)
{
    if(k->pos>=k->size){k->bad=1;return 0;}
    return k->rom[k->pos++];
}
static unsigned kbit(KosReader *k)
{
    unsigned bit=k->descriptor&1u;k->descriptor>>=1;
    /* The retail decoder fetches the next descriptor immediately after bit
     * 16, before that token's literal/displacement bytes. */
    if(!--k->bits){unsigned lo=kb(k),hi=kb(k);k->descriptor=lo|(hi<<8);k->bits=16;}
    return bit;
}
static int kos(const uint8_t *rom,size_t size,size_t pos,uint8_t *out,size_t cap)
{
    KosReader k={rom,size,pos,0,16,0};unsigned lo=kb(&k),hi=kb(&k);k.descriptor=lo|(hi<<8);
    size_t n=0;
    for(unsigned guard=0;guard<65536 && !k.bad;guard++){
        if(kbit(&k)){
            if(n==cap)return 0;
            out[n++]=(uint8_t)kb(&k);
            continue;
        }
        int offset;unsigned count;
        if(kbit(&k)){
            lo=kb(&k);hi=kb(&k);offset=(int)(0xE000u|((hi&0xF8u)<<5)|lo)-0x10000;
            count=hi&7u;
            if(count)count+=2;
            else{
                count=kb(&k);
                if(!count)return !k.bad;
                if(count==1)continue;
                ++count;
            }
        }else{
            count=kbit(&k)<<1;count|=kbit(&k);count+=2;
            offset=(int)kb(&k)-0x100;
        }
        if(k.bad || offset>=0 || (unsigned)-offset>n || count>cap-n)return 0;
        while(count--){out[n]=out[n-(unsigned)-offset];++n;}
    }
    return 0;
}
static unsigned le16(const uint8_t *p){return p[0]|((unsigned)p[1]<<8);}
static void hex_digest(const uint8_t *rom,size_t size,char out[65])
{
    uint8_t digest[32];recompui_sha256_compute(rom,size,digest);
    for(unsigned i=0;i<32;i++)snprintf(out+2*i,3,"%02x",digest[i]);
}
static int fail(char *error,size_t size,const char *message)
{if(error && size)snprintf(error,size,"%s",message);return 0;}

int s3_sound_read_source(const uint8_t *rom,size_t size,S3SoundSource *out,char *error,size_t error_size)
{
    if(!rom || !out || (size!=0x200000 && size!=0x400000))
        return fail(error,error_size,"Unsupported sound-source ROM size");
    uintptr_t a=(uintptr_t)rom,b=(uintptr_t)out;
    if((b>=a && b-a<size) || (a>=b && a-b<sizeof *out))
        return fail(error,error_size,"Sound-source output aliases the ROM");
    char hash[65];hex_digest(rom,size,hash);
    if(size==0x400000 && strcmp(hash,"fba0677fde9f76df93f3e98d6310d8af68b9847bde16e253d73cd4dd8134ed23") && rom[0x2001F0]=='J'){
        /* The supported Steam dump differs only in the locked-on region
         * header. Normalize a private copy for verification, never the ROM. */
        uint8_t *copy=(uint8_t*)malloc(size);
        if(!copy)return fail(error,error_size,"Unable to verify sound-source ROM");
        memcpy(copy,rom,size);copy[0x2001F0]='U';hex_digest(copy,size,hash);free(copy);
    }
    int s3=!strcmp(hash,"d6404843c5ba32486f4c8c744acb7e1932069376960d886b1de37969787d3f9c");
    int sk=!strcmp(hash,"6e12e6b33c26ebfcd0be433251d21cf6284eafe9f71b027bda3767ae59affec1");
    int combined=!strcmp(hash,"fba0677fde9f76df93f3e98d6310d8af68b9847bde16e253d73cd4dd8134ed23");
    if(!s3 && !sk && !combined)return fail(error,error_size,"Clone Driver requires a supported original Sonic 3-family ROM");
    S3SoundSource source;memset(&source,0,sizeof source);source.rom_size=(uint32_t)size;source.standalone_sonic3=(uint8_t)s3;
    if(s3)memcpy(source.z80,rom+0xE6000,7167); /* original byte-copy count */
    else if(!kos(rom,size,0xF6960,source.z80,0x1300) ||
            !kos(rom,size,0xF7760,source.z80+0x1300,0xD00))
        return fail(error,error_size,"Invalid original sound-driver compression");
    source.modulation_count=8;
    source.music_banks=(uint16_t)(s3?0xB48:0xB65);
    source.music_pointers=(uint16_t)le16(source.z80+0x1304);
    source.sfx_pointers=(uint16_t)le16(source.z80+0x1306);
    source.voice_bank=(uint16_t)le16(source.z80+0x1302);
    source.modulation_pointers=(uint16_t)le16(source.z80+0x1308);
    source.volume_pointers=(uint16_t)le16(source.z80+0x130A);
    unsigned first_volume=0x2000;
    for(unsigned i=0;i<8;i++){
        unsigned ptr=le16(source.z80+source.volume_pointers+2*i);
        if(ptr<first_volume)first_volume=ptr;
    }
    if(first_volume<source.volume_pointers || (first_volume-source.volume_pointers)&1u)
        return fail(error,error_size,"Invalid original PSG table extent");
    source.volume_count=(uint16_t)((first_volume-source.volume_pointers)/2);
    source.music_count=(uint16_t)((source.sfx_pointers-source.music_pointers)/2);
    if(source.music_pointers<0x1300 || source.sfx_pointers>0x1B00 ||
       source.music_count!=(s3?50:51) || source.volume_count>64 ||
       source.voice_bank>=0x1C00 || source.volume_pointers+2u*source.volume_count>0x1C00)
        return fail(error,error_size,"Original sound tables are out of bounds");
    memcpy(out,&source,sizeof source);if(error && error_size)*error=0;return 1;
}
int s3_sound_read_prototype(const uint8_t *rom,size_t size,S3SoundSource *out,char *error,size_t error_size)
{
    if(!rom || !out || size!=0x200000)
        return fail(error,error_size,"Expected the Sonic 3 November 3, 1993 prototype ROM (2 MiB)");
    uintptr_t a=(uintptr_t)rom,b=(uintptr_t)out;
    if((b>=a && b-a<size) || (a>=b && a-b<sizeof *out))
        return fail(error,error_size,"Sound-source output aliases the ROM");
    char hash[65];hex_digest(rom,size,hash);
    if(strcmp(hash,"ace6af3187c1c2da4e71a1c24fb81f2529cb25057f093f5cb5389842a90d4abf"))
        return fail(error,error_size,"Expected the unmodified Sonic 3 November 3, 1993 prototype ROM");
    S3SoundSource source;memset(&source,0,sizeof source);
    source.rom_size=(uint32_t)size;source.standalone_sonic3=1;source.prototype=1;
    memcpy(source.z80,rom+0xE0000,0x1852);
    source.music_banks=0x8DD;source.music_pointers=0x16A0;
    source.sfx_pointers=0x1702;source.modulation_pointers=0x120E;
    source.volume_pointers=0x134B;source.music_count=49;
    source.volume_count=39;source.modulation_count=10;
    memcpy(out,&source,sizeof source);if(error && error_size)*error=0;return 1;
}
uint32_t s3_sound_music_source(const S3SoundSource *s,const uint8_t *rom,unsigned id)
{
    (void)rom;
    if(!s || !id || id>s->music_count)return 0;
    unsigned bank=s->z80[s->music_banks+id-1];
    if(s->standalone_sonic3)bank=(bank&15u)|16u;
    unsigned ptr=le16(s->z80+s->music_pointers+2*(id-1));
    uint32_t address=(bank<<15)+(ptr&0x7FFFu);
    return ptr>=0x8000 && address<=s->rom_size-8?address:0;
}
uint32_t s3_sound_sfx_source(const S3SoundSource *s,const uint8_t *rom,unsigned id)
{
    (void)rom;
    if(!s || id<0x33 || id>0xDB)return 0;
    unsigned ptr=le16(s->z80+s->sfx_pointers+2*(id-0x33));
    return ptr>=0x8000?0xF8000u+(ptr&0x7FFFu):0;
}
