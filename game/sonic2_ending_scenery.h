/* Read-only continuation of the original ending's scenery before its native
 * allocation. The original 320px view, RNG and object table are never changed.
 * ObjCA/CC timers, RandomNumber, slot order and CB/CD seed rotations determine
 * the exact future birth; a margin pose extends that birth's trajectory back.
 */
#ifndef SONIC2_ENDING_SCENERY_H
#define SONIC2_ENDING_SCENERY_H
#include <stdint.h>
#include <string.h>

typedef struct {
    unsigned tick,slot,id,frame;
    int x,y,vx,vy;
} S2EndingBirth;
static unsigned s2_ending_word(const uint8_t *p,unsigned a)
{ return ((unsigned)p[a]<<8)|p[a+1]; }
static uint32_t s2_ending_long(const uint8_t *p,unsigned a)
{ return ((uint32_t)s2_ending_word(p,a)<<16)|s2_ending_word(p,a+2); }
static void s2_ending_put16(uint8_t *p,unsigned a,unsigned v)
{ p[a]=(uint8_t)(v>>8);p[a+1]=(uint8_t)v; }
static void s2_ending_put32(uint8_t *p,unsigned a,uint32_t v)
{ s2_ending_put16(p,a,v>>16);s2_ending_put16(p,a+2,v); }
static uint32_t s2_ending_rotate(uint32_t v,unsigned n)
{ return (v>>n)|(v<<(32-n)); }
static uint32_t s2_ending_random(uint32_t seed)
{
    uint32_t v=(seed?seed:0x2A6D365Au)*41u;
    return ((uint32_t)(uint16_t)((v>>16)+(v&65535u))<<16)|(v&65535u);
}
static void s2_ending_move(uint8_t *p)
{
    for(unsigned a=8;a<=12;a+=4)
        s2_ending_put32(p,a,s2_ending_long(p,a)+
            (uint32_t)((int32_t)(int16_t)s2_ending_word(p,0x10+(a-8)/2)*256));
}
static void s2_ending_allocate(uint8_t objects[128][64],unsigned parent,unsigned id)
{
    for(unsigned i=parent+1;i<128;++i)if(!objects[i][0]) {
        memset(objects[i],0,64);objects[i][0]=(uint8_t)id;return;
    }
}
/* Output includes initialization (not first DisplaySprite) ticks. Native child
 * allocation searches after its parent, so initialization runs later that tick.
 * Existing birds need only their phase timers here: hover motion cannot alter
 * RNG, allocation or a later bird's birth. */
static unsigned s2_ending_births(const uint8_t ram[65536],unsigned horizon,
                               S2EndingBirth *out,unsigned capacity)
{
    if(ram[0xF600]!=0x20 || ram[0xB100]!=0xCA || ram[0xB124]!=14)return 0;
    uint8_t objects[128][64];memcpy(objects,ram+0xB000,sizeof objects);
    uint32_t seed=s2_ending_long(ram,0xF636);
    unsigned horizontal=ram[0xB134],clock=s2_ending_word(ram,0xB132),count=0;
    unsigned end=(ram[0xFFF8]&0x40)?0xE40:0x1140;
    for(unsigned tick=1;tick<=horizon && count<capacity;++tick) {
        seed=s2_ending_random(seed);
        if(++clock==end)break;
        for(unsigned i=4;i<128;++i) {
            uint8_t *p=objects[i];unsigned id=p[0],routine=p[0x24];
            if(id==0xCA && i==4) {
                unsigned timer=(s2_ending_word(p,0x3C)-1)&65535u;
                s2_ending_put16(p,0x3C,timer);
                if(!timer)s2_ending_allocate(objects,i,0xCC);
                timer=(s2_ending_word(p,0x30)-1)&65535u;
                if(timer&0x8000) {
                    timer=seed&31;s2_ending_allocate(objects,i,0xCB);
                }
                s2_ending_put16(p,0x30,timer);
            } else if(id==0xCC && routine==2) {
                if(!p[0x25]) {
                    if(s2_ending_word(p,8)==160) {
                        p[0x25]=2;horizontal=255;
                        s2_ending_put16(p,0x3C,(ram[0xFFF8]&0x40)?0x3D0:0x480);
                        s2_ending_put16(p,0x10,0);s2_ending_put16(p,0x12,0);
                    } else s2_ending_move(p);
                } else if(p[0x25]==2) {
                    unsigned timer=(s2_ending_word(p,0x30)-1)&65535u;
                    if((timer&0x8000) && p[0x35]) {
                        --p[0x35];timer=seed&15;s2_ending_allocate(objects,i,0xCD);
                    }
                    s2_ending_put16(p,0x30,timer);
                    timer=(s2_ending_word(p,0x3C)-1)&65535u;
                    s2_ending_put16(p,0x3C,timer);
                    if(timer&0x8000)p[0x25]=4;
                }
            } else if(id==0xCC && !routine) {
                p[0x24]=2;p[0x35]=20;
                s2_ending_put32(p,8,0xFFF00000u);s2_ending_put32(p,12,192u<<16);
                s2_ending_put16(p,0x10,256);s2_ending_put16(p,0x12,-128);
            } else if(id==0xCB && !routine) {
                static const int speed[]={-3,-2,-1,-3};
                seed=s2_ending_rotate(seed,1);unsigned choice=seed&3;
                p[0x24]=2;p[0x1A]=(uint8_t)(choice==3?0:choice);
                s2_ending_put16(p,0x12,(unsigned)(speed[choice]*256));
                s2_ending_put32(p,8,(horizontal?336u:seed&511u)<<16);
                s2_ending_put32(p,12,(horizontal?seed&255u:256u)<<16);
                if(horizontal && count<capacity)
                    out[count++]=(S2EndingBirth){tick,0xB000+i*64,id,p[0x1A],336,(int)(seed&255u),speed[choice]*256,0};
            } else if(id==0xCB) {
                if(routine==2 && horizontal) {
                    p[0x24]=4;s2_ending_put16(p,0x10,s2_ending_word(p,0x12));
                    s2_ending_put16(p,0x12,0);
                }
                s2_ending_move(p);
                if((int16_t)s2_ending_word(p,horizontal?8:12)<(horizontal?-32:0))p[0]=0;
            } else if(id==0xCD && !routine) {
                seed=s2_ending_rotate(seed,3);uint32_t y=s2_ending_rotate(seed,3)&255u;
                int x=-160+(int)(seed&127u),vy=y<32?32:-32;
                p[0x24]=2;s2_ending_put16(p,0x3C,192);
                if(count<capacity)
                    out[count++]=(S2EndingBirth){tick,0xB000+i*64,id,0,x,8+(int)y,256,vy};
            } else if(id==0xCD) {
                unsigned timer=(s2_ending_word(p,0x3C)-1)&65535u;
                if(timer&0x8000) {
                    if(p[0x25]==4)p[0]=0;
                    else {p[0x25]+=2;timer=p[0x25]==2?384:192;}
                }
                s2_ending_put16(p,0x3C,timer);
            }
        }
    }
    return count;
}
#endif
