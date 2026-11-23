#include "sonic2_stage_layouts.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void put32(uint8_t *p,unsigned x)
{ p[0]=(uint8_t)(x>>24);p[1]=(uint8_t)(x>>16);p[2]=(uint8_t)(x>>8);p[3]=(uint8_t)x; }
static unsigned get32(const uint8_t *p)
{
    return (unsigned)p[0]<<24|(unsigned)p[1]<<16|(unsigned)p[2]<<8|p[3];
}
static void streaming_contract(void)
{
    static const uint8_t records[]={
        0x00,0x20,0xE0,0x34,0x12,0x78,
        0x01,0x00,0x00,0x56,0x34,0x9A,
        0x03,0x00,0x80,0x78,0x56,0xBC,
        0x05,0x00,0x00,0x9A,0x78,0xDE,
        0xFF,0xFF,0x00,0x00,0x00,0x00
    };
    S2StagePlacementView view={records,sizeof records};
    static uint8_t ram[65536],snapshot[65536];
    memset(ram+0xFC00,0xA5,0xC1);
    s2_stage_layout_prepare(view,ram);
    assert(ram[0xF76C]==2 && get32(ram+0xF770)==0 && get32(ram+0xF778)==0);
    assert(ram[0xFC00]==1 && ram[0xFC01]==1);
    assert(ram[0xFC02]==0 && ram[0xFCBF]==0 && ram[0xFCC0]==0xA5);
    s2_stage_layout_stream(view,ram);
    assert(get32(ram+0xF770)==12 && get32(ram+0xF774)==0);
    assert(ram[0xF7DA]==0xFF && ram[0xF7DB]==0x80);
    assert(ram[0xB400]==0x12 && ram[0xB401]==3 && ram[0xB409]==0x20 && ram[0xB40D]==0x34);
    assert(ram[0xB422]==3 && ram[0xB423]==1 && ram[0xB428]==0x78);
    assert(ram[0xB440]==0x34 && ram[0xB463]==0 && ram[0xFC03]==0x80);
    memcpy(snapshot,ram,sizeof ram);
    s2_stage_layout_stream(view,ram);
    assert(!memcmp(snapshot,ram,sizeof ram));

    ram[0xEE00]=4;
    s2_stage_layout_stream(view,ram);
    assert(get32(ram+0xF770)==24 && get32(ram+0xF774)==18);
    assert(ram[0xFC00]==3 && ram[0xFC01]==3 && ram[0xFC04]==0x80);
    assert(ram[0xB480]==0x56 && ram[0xB4C0]==0x78);
    memset(ram+0xB480,0,64);
    ram[0xFC04]=0;
    ram[0xEE00]=2;
    ram[0xEE01]=0x80;
    s2_stage_layout_stream(view,ram);
    assert(get32(ram+0xF770)==18 && get32(ram+0xF774)==12);
    assert(ram[0xFC00]==3 && ram[0xFC01]==2);
    assert(ram[0xB480]==0x56 && ram[0xB4A3]==2 && ram[0xFC04]==0x80);

    memset(ram,0,sizeof ram);
    for(unsigned slot=0xB400;slot<0xD000;slot+=64) ram[slot]=0xF0;
    s2_stage_layout_prepare(view,ram);
    s2_stage_layout_stream(view,ram);
    assert(get32(ram+0xF770)==0 && ram[0xFC00]==2 && ram[0xFC03]==0x80);
    assert(ram[0xD000]==0);
}
int main(int argc,char **argv)
{
    streaming_contract();
    size_t size=0xDF370+S2_STAGE_LAYOUT_COUNT*4+18;
    uint8_t *rom=calloc(1,size);
    S2StageLayouts *out=calloc(1,sizeof *out);
    assert(rom && out);
    unsigned at=0xDF370+S2_STAGE_LAYOUT_COUNT*4;
    for(unsigned i=0;i<S2_STAGE_LAYOUT_COUNT;++i)put32(rom+0xDF370+i*4,at);
    rom[at+1]=16;rom[at+7]=32;rom[at+12]=rom[at+13]=255;
    assert(s2_stage_layouts_decode(rom,size,out));
    assert(!out->special_ring_requirements_ready);
    assert(out->size==S2_STAGE_LAYOUT_COUNT*18);
    S2StagePlacementView view=s2_stage_layout_view(out,12,1);
    assert(view.data==out->data+25*18 && view.bytes==18);
    assert(!s2_stage_layout_view(out,17,0).data && !s2_stage_layout_view(out,0,2).data);
    assert(!s2_stage_layout_view(NULL,0,0).data);
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
        const uint8_t goals[]={30,70,130,110,50,90,130,130,50,100,140,160,40,90,140,150,
            40,80,130,130,70,130,170,170,50,100,140,140};
        assert(out->special_ring_requirements_ready && !memcmp(out->special_ring_requirements,goals,sizeof goals));
        for(unsigned i=0;i<S2_STAGE_LAYOUT_COUNT;++i) {
            unsigned p=(unsigned)rom[0xDF370+i*4]<<24|(unsigned)rom[0xDF371+i*4]<<16|
                (unsigned)rom[0xDF372+i*4]<<8|rom[0xDF373+i*4];
            size_t n=0;while(rom[p+n]!=255 || rom[p+n+1]!=255)n+=6;n+=6;
            view=s2_stage_layout_view(out,i/2,i%2);
            assert(view.data && view.bytes==n && !memcmp(view.data,rom+p,n));
        }
        printf("Verified all 34 original KiS2 lists, %zu mapped bytes\n",out->size);free(rom);
    }
    free(out);puts("PASS host placement streaming, respawns, pool limits and read-only stage views");
    return 0;
}
