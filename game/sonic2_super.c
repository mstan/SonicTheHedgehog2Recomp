#include "sonic2_super.h"
#include "sonic2_character.h"

unsigned s2_super_glow(unsigned phase,unsigned frame)
{
    if (!phase) return 0;
    if (phase==1 || phase==2) return frame>=0x30?6:frame/8;
    unsigned step=frame>=0x30?(frame-0x30)/8:0;
    step%=9;
    return 2+(step<=4?step:8-step);
}
uint16_t s2_super_color(unsigned kind,unsigned index,uint16_t normal,
                        unsigned phase,unsigned frame,const S2DonorBank *bank)
{
    unsigned strength=s2_super_glow(phase,frame);
    if (!strength) return normal;
    if (kind==S2_CHAR_KNUCKLES && bank && bank->super_frames && index>=2 && index<=4) {
        unsigned step=frame>=0x30?(frame-0x30)/8:0;
        uint16_t target=bank->super_palette[(step*bank->super_frames/9)%bank->super_frames][index-2];
        if (phase!=1 && phase!=2) return target;
        unsigned out=0;
        for (unsigned shift=0;shift<12;shift+=4) {
            int base=(normal>>shift)&14, end=(target>>shift)&14;
            out|=(unsigned)((base+(end-base)*(int)strength/6)&14)<<shift;
        }
        return (uint16_t)out;
    }
    /* Tails' S2 fur is 14/15/11 (S3 rearranged these to 8/9/11).
     * Amy keeps her own pink shades. Derived highlights need no new donor. */
    if (!((kind==S2_CHAR_TAILS && (index==14 || index==15 || index==11)) ||
          (kind==S2_CHAR_AMY && index>=2 && index<=5))) return normal;
    unsigned out=0;
    for (unsigned shift=0;shift<12;shift+=4) {
        unsigned value=(normal>>shift)&14;
        out|=((value+(14-value)*strength/6)&14)<<shift;
    }
    return (uint16_t)out;
}
