#include "sonic2_super.h"
#include "sonic2_character.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    S2DonorBank bank={0};
    bank.super_frames=10;
    for (unsigned f=0;f<10;++f) for (unsigned c=0;c<3;++c)
        bank.super_palette[f][c]=(uint16_t)(f*0x20+c*2);
    assert(!s2_super_glow(0,0x30));
    assert(!s2_super_glow(1,0));
    assert(s2_super_glow(1,0x30)==6);
    for (unsigned f=0x30;f<=0x70;f+=8) {
        assert(s2_super_glow(255,f)>=2 && s2_super_glow(255,f)<=6);
        for (unsigned c=0;c<16;++c) {
            assert(s2_super_color(S2_CHAR_SONIC,c,0xE44,255,f,&bank)==0xE44);
            assert(s2_super_color(S2_CHAR_AMY,c,0xC24,0,f,&bank)==0xC24);
            if (c<2 || c>5) assert(s2_super_color(S2_CHAR_AMY,c,0xC24,255,f,&bank)==0xC24);
            if (c!=11 && c!=14 && c!=15)
                assert(s2_super_color(S2_CHAR_TAILS,c,0x8E,255,f,&bank)==0x8E);
            if (c>=2 && c<=4)
                assert(s2_super_color(S2_CHAR_KNUCKLES,c,0xE,255,f,&bank)==
                    bank.super_palette[((f-0x30)/8*10/9)%10][c-2]);
            else assert(s2_super_color(S2_CHAR_KNUCKLES,c,0xE,255,f,&bank)==0xE);
        }
    }
    assert(s2_super_color(S2_CHAR_AMY,2,0xC24,1,0x30,0)==0xEEE);
    assert(s2_super_color(S2_CHAR_TAILS,15,0x8E,1,0x30,0)==0xEEE);
    assert(s2_super_color(S2_CHAR_KNUCKLES,2,0xE,2,0,&bank)==0xE);
    puts("sonic2_super: lifecycle fade, donor colors and character-local palette isolation OK");
    return 0;
}
