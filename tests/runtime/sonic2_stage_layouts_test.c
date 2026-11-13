#include "sonic2_stage_layouts.h"
#include "sonic2_knuckles_ending.h"
#include "sonic2_sound_assets.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void put32(uint8_t *p,unsigned x)
{ p[0]=(uint8_t)(x>>24);p[1]=(uint8_t)(x>>16);p[2]=(uint8_t)(x>>8);p[3]=(uint8_t)x; }
int main(int argc,char **argv)
{
    assert(S2_STAGE_LAYOUT_ROM>=0x140000);
    assert(S2_STAGE_LAYOUT_ROM+S2_STAGE_LAYOUT_BYTES<=S2_KNUCKLES_ENDING_ROM);
    assert(S2_KNUCKLES_ENDING_ROM+S2_KNUCKLES_ENDING_BYTES<=S2_SOUND_MUSIC_BASE);
    size_t size=0xDF370+S2_STAGE_LAYOUT_COUNT*4+18;
    uint8_t *rom=calloc(1,size);
    S2StageLayouts *out=calloc(1,sizeof *out);
    assert(rom && out);
    unsigned at=0xDF370+S2_STAGE_LAYOUT_COUNT*4;
    for(unsigned i=0;i<S2_STAGE_LAYOUT_COUNT;++i)put32(rom+0xDF370+i*4,at);
    rom[at+1]=16;rom[at+7]=32;rom[at+12]=rom[at+13]=255;
    assert(s2_stage_layouts_decode(rom,size,out));
    assert(out->size==S2_STAGE_LAYOUT_COUNT*18);
    assert(s2_stage_layout_address(out,12,1)==S2_STAGE_LAYOUT_ROM+25*18);
    assert(!s2_stage_layout_address(out,17,0) && !s2_stage_layout_address(out,0,2));
    assert(!s2_stage_layout_address(NULL,0,0));
    rom[at+7]=8; /* Original ARZ2 has unsorted records: preserve, never sort. */
    assert(s2_stage_layouts_decode(rom,size,out) && out->data[7]==8);
    rom[at+7]=32;
    assert(!s2_stage_layouts_decode(rom,size-1,out) && out->size==S2_STAGE_LAYOUT_COUNT*18);
    put32(rom+0xDF370,at+1);assert(!s2_stage_layouts_decode(rom,size,out));
    put32(rom+0xDF370,0xFFFFFFFF);assert(!s2_stage_layouts_decode(rom,size,out));
    free(rom);
    if(argc==2) {
        FILE *f=fopen(argv[1],"rb");assert(f);fseek(f,0,SEEK_END);size=(size_t)ftell(f);rewind(f);
        rom=malloc(size);assert(rom && fread(rom,1,size,f)==size);fclose(f);
        assert(s2_stage_layouts_decode(rom,size,out));
        for(unsigned i=0;i<S2_STAGE_LAYOUT_COUNT;++i) {
            unsigned p=(unsigned)rom[0xDF370+i*4]<<24|(unsigned)rom[0xDF371+i*4]<<16|
                (unsigned)rom[0xDF372+i*4]<<8|rom[0xDF373+i*4];
            size_t n=0;while(rom[p+n]!=255 || rom[p+n+1]!=255)n+=6;n+=6;
            unsigned mapped=s2_stage_layout_address(out,i/2,i%2)-S2_STAGE_LAYOUT_ROM;
            assert(mapped+n<=out->size && !memcmp(out->data+mapped,rom+p,n));
        }
        printf("Verified all 34 original KiS2 lists, %zu mapped bytes\n",out->size);free(rom);
    }
    free(out);puts("PASS stage-list decoding, complete lists, bounds and invalid-input preservation");
    return 0;
}
