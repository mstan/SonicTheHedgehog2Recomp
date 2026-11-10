#include "sonic2_donor_assets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(c) do { if (!(c)) { fprintf(stderr,"line %d: %s\n",__LINE__,#c); exit(1); } } while (0)
static void putword(uint8_t *p,unsigned at,unsigned word){p[at]=(uint8_t)(word>>8);p[at+1]=(uint8_t)word;}
static void amy_special_bounds_test(void)
{
    uint8_t *rom=calloc(1,0x200000);CHECK(rom);
    static S2SpecialStageArt art,before;
    unsigned mp=0x36490+36,dp=0x36878+36;
    for(unsigned f=0;f<18;++f){putword(rom,0x36490+f*2,36);putword(rom,0x36878+f*2,36);}
    putword(rom,mp,6);putword(rom,dp,1);putword(rom,dp+2,0x5000);
    for(unsigned i=0;i<6;++i) {
        rom[mp+2+i*8]=0xFC;putword(rom,mp+4+i*8,i);putword(rom,mp+8+i*8,i*8);
    }
    memset(rom+0x106DC2,0x67,6*32);putword(rom,0x3162+4,0x0E0);
    CHECK(s2_amy_special_decode(rom,0x200000,&art));
    CHECK(art.frames[17].count==6 && art.frames[17].pieces[5].x==40);
    CHECK(art.frames[17].tiles[5*32]==0x67 && art.palette[2]==0x0E0);
    memcpy(&before,&art,sizeof before);
    putword(rom,dp,3);for(unsigned i=0;i<3;++i)putword(rom,dp+2+i*2,0xF000);
    CHECK(!s2_amy_special_decode(rom,0x200000,&art) && !memcmp(&art,&before,sizeof art));
    putword(rom,dp,1);putword(rom,dp+2,0x5000);putword(rom,mp+4+5*8,6);
    CHECK(!s2_amy_special_decode(rom,0x200000,&art) && !memcmp(&art,&before,sizeof art));
    putword(rom,mp+4+5*8,5);putword(rom,mp,7);
    CHECK(!s2_amy_special_decode(rom,0x200000,&art) && !memcmp(&art,&before,sizeof art));
    CHECK(!s2_amy_special_decode(rom,0x1FFFFF,&art) && !memcmp(&art,&before,sizeof art));
    free(rom);
}
int main(void)
{
    /* Identical shades occupy different indices in the two sprite banks.
     * In particular, donor white must not become the native black's tint. */
    uint16_t donor[16]={0},normal[16]={0},water[16]={0},mapped[16];
    for(unsigned i=1;i<16;++i){normal[i]=0x222;water[i]=0x622;donor[i]=0x222;}
    normal[3]=0xEEE;water[3]=0xEEC;donor[1]=0xEEE;
    normal[7]=0x64E;water[7]=0xE2A;donor[2]=0x64E;
    donor[4]=0xECC; /* additional near-white donor shade */
    s2_donor_water_palette(donor,normal,water,mapped);
    CHECK(mapped[1]==0xEEC && mapped[2]==0xE2A && mapped[4]==0xEEC);
    CHECK(mapped[15]==0x622 && !mapped[0]);
    uint8_t rom[256]={0}; char error[160]; S2DonorBank bank={0};
    S2DonorLayout layout={1,0,2,128,64,6};
    rom[1]=4; /* mapping ->4 */ rom[3]=20; /* DPLC ->22 */
    rom[5]=1; /* one mapping piece */ rom[6]=0xFC; /* y=-4 */
    rom[10]=0xFF;rom[11]=0xFC; /* x=-4, 8x8 tile 0 */
    rom[23]=1; /* one DPLC cue, source tile 0, length 1 */
    memset(rom+128,0x12,32);
    CHECK(s2_donor_decode(rom,sizeof rom,&layout,&bank,error,sizeof error));
    CHECK(bank.count==1 && bank.frames[0].width==8 && bank.frames[0].height==8);
    CHECK(bank.frames[0].x==-4 && bank.frames[0].y==-4);
    CHECK(bank.frames[0].pixels[0]==1 && bank.frames[0].pixels[1]==2);
    rom[8]=8; /* horizontal flip */
    CHECK(s2_donor_decode(rom,sizeof rom,&layout,&bank,error,sizeof error));
    CHECK(bank.frames[0].pixels[0]==2 && bank.frames[0].pixels[1]==1);
    uint8_t *previous=bank.frames[0].pixels;
    rom[9]=2; /* references missing tile */
    CHECK(!s2_donor_decode(rom,sizeof rom,&layout,&bank,error,sizeof error));
    CHECK(bank.frames[0].pixels==previous && bank.frames[0].pixels[0]==2);
    CHECK(!s2_donor_decode(rom,10,&layout,&bank,error,sizeof error));
    rom[9]=0; rom[5]=255;
    CHECK(!s2_donor_decode(rom,sizeof rom,&layout,&bank,error,sizeof error));
    rom[5]=1;
    layout.super_palette=240; layout.super_frames=2;
    rom[240]=0x0E; rom[241]=0xEE; rom[251]=0x42;
    CHECK(s2_donor_decode(rom,sizeof rom,&layout,&bank,error,sizeof error));
    CHECK(bank.super_frames==2 && bank.super_palette[0][0]==0xEEE && bank.super_palette[1][2]==0x42);
    previous=bank.frames[0].pixels;
    layout.super_frames=11;
    CHECK(!s2_donor_decode(rom,sizeof rom,&layout,&bank,error,sizeof error));
    layout.super_frames=3;
    CHECK(!s2_donor_decode(rom,sizeof rom,&layout,&bank,error,sizeof error));
    CHECK(bank.frames[0].pixels==previous && bank.super_frames==2);
    s2_donor_free(&bank); CHECK(!bank.count && !bank.frames[0].pixels);
    /* S1/Megamix byte counts, five-byte pieces, signed byte X and flips. */
    memset(rom,0,sizeof rom); layout=(S2DonorLayout){1,0,2,128,64,5};
    rom[1]=4; rom[3]=20; rom[4]=1; rom[5]=0xFC; rom[9]=0xFC;
    rom[22]=1; memset(rom+128,0x12,32);
    CHECK(s2_donor_decode(rom,sizeof rom,&layout,&bank,error,sizeof error));
    CHECK(bank.frames[0].x==-4 && bank.frames[0].y==-4 && bank.frames[0].pixels[0]==1);
    rom[7]=8; /* tile high byte: horizontal flip */
    CHECK(s2_donor_decode(rom,sizeof rom,&layout,&bank,error,sizeof error));
    CHECK(bank.frames[0].pixels[0]==2);
    previous=bank.frames[0].pixels; rom[8]=2; /* tile low byte: missing tile */
    CHECK(!s2_donor_decode(rom,sizeof rom,&layout,&bank,error,sizeof error));
    CHECK(bank.frames[0].pixels==previous);
    s2_donor_free(&bank);
    amy_special_bounds_test();
    puts("sonic2_donor: mapping/DPLC bounds, flip, geometry and transactional failure OK");
    return 0;
}
