#include "sonic2_save_draw.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    S2SaveAssets *assets=calloc(1,sizeof *assets);
    assert(assets);
    /* Moving through every card must not drag the uncovered background,
     * including either margin of an adaptive widescreen canvas. */
    for (unsigned tile=1;tile<=40;++tile) {
        memset(assets->tiles+tile*32,(tile&1)?0x11:0x22,32);
        for (unsigned row=0;row<28;++row) assets->background[row*40+tile-1]=(uint16_t)tile;
    }
    assets->palette[1]=0x00E; assets->palette[2]=0x0E0;
    for (int width=320;width<=896;width+=288) {
        uint32_t still[896],moving[896];
        S2SaveView background={0};
        /* Row 0 is uncovered by cards and sprites at every scroll position. */
        s2_save_draw_line(assets,&background,0,still,width);
        for (int scroll=0;scroll<=704;scroll+=8) {
            background.scroll=scroll;
            s2_save_draw_line(assets,&background,0,moving,width);
            assert(!memcmp(still,moving,(size_t)width*sizeof *still));
        }
    }
    /* Wide output keeps one complete background, rather than repeating its
     * distinctive ends. Native output remains pixel-for-pixel identical. */
    memset(assets,0,sizeof *assets);
    assets->palette[1]=0x00E; assets->palette[2]=0x0E0;
    memset(assets->tiles+32,0x11,32);
    memset(assets->tiles+64,0x22,32);
    for(unsigned row=0;row<28;++row)for(unsigned col=0;col<40;++col)
        assets->background[row*40+col]=(uint16_t)(col<20?1:2);
    S2SaveView backdrop={0};
    static const int background_widths[]={320,398,401,796,896};
    for(unsigned mode=0;mode<sizeof background_widths/sizeof *background_widths;++mode) {
        int width=background_widths[mode];
        uint32_t row[896];
        s2_save_draw_line(assets,&backdrop,0,row,width);
        for(int x=0;x<width;++x)
            assert(row[x]==(x<(width+1)/2?0xFFFF0000:0xFF00FF00));
    }
    memset(assets,0,sizeof *assets);
    /* Synthetic donor font and counter art; no ROM required. */
    static const uint16_t icons[3][15]={
        {0x6E,0x70,0x5A,0x6F,0x71,0x5B,0x5C,0x5F,0,0x5D,0x60,0x5A,0x5E,0x61,0x5B},
        {0x72,0x74,0x5A,0x73,0x75,0x5B,0x62,0x65,0,0x63,0x66,0x5A,0x64,0x67,0x5B},
        {0x76,0x78,0x5A,0x77,0x79,0x5B,0x68,0x6B,0,0x69,0x6C,0x5A,0x6A,0x6D,0x5B}
    };
    memcpy(assets->counter_icons,icons,sizeof icons);
    assets->palette[17]=0x00E;
    for (unsigned glyph=16;glyph<56;++glyph)
        memset(assets->tiles+(0x552+glyph)*32,0x11,32);
    for (unsigned tile=0x49A;tile<=0x4CD;++tile)
        memset(assets->tiles+tile*32,0x11,32);
    S2SaveView empty={0},filled={0};
    filled.selection=1;
    for (unsigned stage=0;stage<S2_CAMPAIGN_STAGES;++stage) {
        filled.data.slots[0]=(S2CampaignSlot){S2_SAVE_ACTIVE,(uint8_t)stage,0,3,0};
        unsigned caption_pixels=0,counter_pixels=0;
        for (int y=0;y<224;++y) {
            uint32_t baseline[320],actual[320];
            s2_save_draw_line(assets,&empty,y,baseline,320);
            s2_save_draw_line(assets,&filled,y,actual,320);
            for (int x=0;x<320;++x) if (actual[x]!=baseline[x]) {
                assert(x>=112 && x<176); /* eight-glyph card-stem width */
                assert((y>=80 && y<88) || (y>=144 && y<184));
                if (y<88) ++caption_pixels; else ++counter_pixels;
            }
        }
        assert(caption_pixels==6*64 && counter_pixels>0); /* ZONE + two digits */
    }
    /* Only the selected primary character's indicator changes. Live digits
     * and the roster-independent slot data must remain identical. */
    assets->palette[18]=0xE00;
    for (unsigned i=0;i<15;++i) if (i%3<2 && icons[2][i])
        memset(assets->tiles+(0x454+icons[2][i])*32,0x22,32);
    uint32_t sonic[320],knuckles[320];
    S2CampaignData saved_counters=filled.data;
    for (int y=144;y<=160;y+=16) {
        filled.counter_character=0; s2_save_draw_line(assets,&filled,y,sonic,320);
        filled.counter_character=2; s2_save_draw_line(assets,&filled,y,knuckles,320);
        assert(sonic[128]==0xFFFF0000 && knuckles[128]==0xFF0000FF);
        assert(!memcmp(sonic+152,knuckles+152,16*sizeof *sonic));
    }
    assert(!memcmp(&saved_counters,&filled.data,sizeof saved_counters));
    filled.counter_character=0;
    /* Character-selection arrows must never appear, picked or unpicked. */
    memset(assets->tiles+0x100*32,0x11,4*32);
    assets->frames[15].count=2;
    assets->frames[15].pieces[0]=(S2MenuPiece){-8,-58,0x2100,2,2};
    assets->frames[15].pieces[1]=(S2MenuPiece){-8,2,0x2100,2,2};
    filled.data.slots[0].state=S2_SAVE_COMPLETE;
    for (int picked=0;picked<2;++picked) for (int y=0;y<224;++y) {
        uint32_t quiet[320],arrows[320];
        filled.replay_selected=picked;
        filled.frame=1; s2_save_draw_line(assets,&filled,y,quiet,320);
        filled.frame=17; s2_save_draw_line(assets,&filled,y,arrows,320);
        assert(!memcmp(quiet,arrows,sizeof quiet));
    }
    /* Completed cards keep static until picked; CLEAR occupies the same row. */
    assets->stages_ready=1;
    for (unsigned i=0;i<80*56;++i) assets->stages[19][i]=0xEEE;
    uint32_t unpicked[320],picked[320];
    filled.replay_selected=0; s2_save_draw_line(assets,&filled,16,unpicked,320);
    filled.replay_selected=1; s2_save_draw_line(assets,&filled,16,picked,320);
    assert(unpicked[144]!=picked[144]);
    filled.replay_selected=0; s2_save_draw_line(assets,&filled,80,unpicked,320);
    unsigned count=0;
    for (int x=112;x<176;++x) if (unpicked[x]!=0xFF000000) ++count;
    assert(count==40); /* CLEAR, no ZONE/ACT/FILE text */
    /* All seven emerald frames contribute independent visible pieces. */
    for (unsigned e=0;e<7;++e) {
        assets->frames[16+e].count=1;
        assets->frames[16+e].pieces[0]=(S2MenuPiece){(int16_t)(-28+e*8),-36,0x2100,1,1};
    }
    filled.data.slots[0].emeralds=0; s2_save_draw_line(assets,&filled,100,unpicked,320);
    filled.data.slots[0].emeralds=0x7F; s2_save_draw_line(assets,&filled,100,picked,320);
    for (unsigned e=0;e<7;++e) assert(unpicked[116+e*8]!=picked[116+e*8]);
    /* Every member of a six-seat party remains identifiable within the card
     * window. Composition must not touch neighbouring cards or save bytes. */
    S2SavePortrait party[6]={{0}};
    S2DonorFrame frames[6]; uint8_t ink[20*32]; memset(ink,1,sizeof ink);
    uint16_t palettes[6][16]={{0}};
    static const uint16_t colours[]={0x00E,0x0E0,0xE00,0x0EE,0xE0E,0xEE0};
    for (unsigned p=0;p<6;++p) {
        frames[p]=(S2DonorFrame){0,0,20,32,ink}; palettes[p][1]=colours[p];
        party[p].image=&frames[p]; party[p].palette=palettes[p];
    }
    struct { uint32_t before,pixels[80*48],after; } composite;
    composite.before=0x12345678; composite.after=0x87654321;
    s2_save_portraits(assets,party,1,composite.pixels);
    for(unsigned x=0;x<80;++x)
        assert((composite.pixels[16*80+x]!=0)==(x>=30 && x<50));
    /* A custom pair is a centered composition with a small visible gap,
     * rather than two portraits centered in distant halves of the card. */
    s2_save_portraits(assets,party,2,composite.pixels);
    for(unsigned x=0;x<80;++x) {
        int occupied=(x>=19 && x<39)||(x>=41 && x<61);
        assert((composite.pixels[16*80+x]!=0)==occupied);
    }
    uint32_t compact[80*48];memcpy(compact,composite.pixels,sizeof compact);
    uint8_t padding[32*40]={0};
    for(unsigned y=4;y<36;++y)memset(padding+y*32+6,1,20);
    S2DonorFrame padded={0,0,32,40,padding};
    party[0].image=party[1].image=&padded;
    s2_save_portraits(assets,party,2,composite.pixels);
    assert(!memcmp(compact,composite.pixels,sizeof compact));
    party[0].image=&frames[0];party[1].image=&frames[1];
    s2_save_portraits(assets,party,6,composite.pixels);
    assert(composite.before==0x12345678 && composite.after==0x87654321);
    for (unsigned p=0;p<6;++p) {
        unsigned hits=0;
        for (unsigned i=0;i<80*48;++i)
            hits+=composite.pixels[i]==(0xFF000000u|((colours[p]&14)*255/14<<16)|
                 (((colours[p]>>4)&14)*255/14<<8)|(((colours[p]>>8)&14)*255/14));
        assert(hits>0);
    }
    memset(assets,0,sizeof *assets); memset(&filled,0,sizeof filled);
    filled.portrait=composite.pixels; S2CampaignData progress=filled.data;
    for (int width=320;width<=796;width+=476) for (int y=0;y<224;++y) {
        uint32_t row[796]; s2_save_draw_line(assets,&filled,y,row,width);
        if (y==112) {
            int center=(width-320)/2+144;
            assert(row[center]!=0xFF000000);
            assert(row[center-41]==0xFF000000 && row[center+40]==0xFF000000);
        }
    }
    assert(!memcmp(&progress,&filled.data,sizeof progress));
    free(assets);
    puts("PASS numbered zone cards, CLEAR/static, counters, seven emeralds and no character arrows");
    return 0;
}
