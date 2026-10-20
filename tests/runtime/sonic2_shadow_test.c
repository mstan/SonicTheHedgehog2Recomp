#include "sonic2_shadow.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static int16_t sine(unsigned a)
{ static const int16_t quarters[]={0,256,0,-256}; assert(!(a&63)); return quarters[(a&255)/64]; }
static uint8_t angle(int x,int y) { assert(x>0 && !y); return 0; }
static S2Motion standing(void)
{ S2Motion m={0}; m.radius_x=9; m.radius_y=19; m.animation=5; return m; }
static void tick(S2CharacterState *s,S2Motion *m,S2Contacts *c,unsigned held,unsigned pressed)
{ s2_character_before(s,m,(uint8_t)held,(uint8_t)pressed,c); }
int main(void)
{
    S2Contacts c={64,64,64,sine}; c.angle=angle;
    S2CharacterState s; S2Motion m;
    s2_character_reset(&s,S2_CHAR_SHADOW); m=standing(); m.rings=4;
    tick(&s,&m,&c,0x40,0x40); assert(m.rings==4 && !m.inertia && !s.native_pressed);
    m.rings=5; tick(&s,&m,&c,0x40,0x40);
    assert(!m.rings && m.inertia==0xC00 && m.animation==1);
    m=standing(); m.status=1; m.rings=10; m.speed_shoes=1;
    tick(&s,&m,&c,0x40,0x40); assert(m.inertia==-0x1000 && m.rings==5);
    m=standing(); m.rings=10; c.wall=-1;
    tick(&s,&m,&c,0x40,0x40); assert(!m.inertia && m.rings==10); c.wall=64;
    m=standing(); m.status=6; m.animation=2; m.jumping=1;
    tick(&s,&m,&c,0x20,0x20); assert(m.vy==-0x700 && !m.jumping && s.shadow_air_used);
    m.vy=-0x200; tick(&s,&m,&c,0x10,0x10); assert(m.vy==-0x200);
    m.status=0; s2_character_after(&s,&m,&c); assert(!s.shadow_air_used);
    m.status=0x46; tick(&s,&m,&c,0x20,0x20); assert(m.vy==-0x3C0);
    s2_character_reset(&s,S2_CHAR_SHADOW); m=standing(); m.status=2;
    tick(&s,&m,&c,0x40,0x40); assert(m.status==6 && m.animation==2 && m.radius_y==14);
    assert(m.y==5*65536); /* free-fall A rolls in, then permits double jump */
    tick(&s,&m,&c,0x20,0x20); assert(m.vy==-0x700);
    s2_character_reset(&s,S2_CHAR_SHADOW); m=standing(); m.status=6; m.rings=15;
    c.target=0xB900; c.target_dx=64; c.target_dy=0;
    tick(&s,&m,&c,0x60,0x20);
    assert(m.vx==0xC00 && !m.vy && s.move==S2_MOVE_AIR && s.shadow_target==0xB900 && s.shadow_chain);
    assert(m.rings==15); c.target=0xB940;
    tick(&s,&m,&c,0x40,0); assert(m.rings==10 && s.shadow_target==0xB940);
    c.target=0xB980; m.rings=5;
    tick(&s,&m,&c,0x40,0); assert(m.rings==5 && !s.shadow_target);
    c.target=0;
    s2_character_reset(&s,S2_CHAR_SHADOW); m=standing(); m.super=1; m.rings=90;
    tick(&s,&m,&c,0x40,0x40);
    assert(s.shadow_chaos && m.physics_override && m.physics_top==0x1100 && m.inertia==0xC00 && m.rings==90);
    tick(&s,&m,&c,0x40,0x40); assert(m.inertia==0xC40);
    m.status=2; tick(&s,&m,&c,0x40,0); assert(s.shadow_chaos);
    tick(&s,&m,&c,0,0); assert(!s.shadow_chaos && m.physics_top==0xC00);
    m.invincible_monitor=1; m.vx=0x300; tick(&s,&m,&c,0x40,0x40);
    assert(m.blast && !m.vx && !m.vy && m.rings==90);
    m=standing(); m.super=1; m.status=2; tick(&s,&m,&c,0x40,0x40); assert(!m.blast);
    s.shadow_monitor_ticks=2; tick(&s,&m,&c,0,0); assert(s.shadow_monitor_ticks==1);
    tick(&s,&m,&c,0,0); assert(!s.shadow_monitor_ticks);
    s.shadow_monitor_ticks=1200; m.hurt=1; tick(&s,&m,&c,0,0); assert(!s.shadow_monitor_ticks);

    static S2DonorBank bank;
    bank.count=176; bank.animation_count=bank.super_animation_count=38;
    for(unsigned i=0;i<38;++i) {
        bank.animation_length[i]=bank.super_animation_length[i]=3;
        bank.animations[i][0]=bank.super_animations[i][0]=0;
        bank.animations[i][1]=bank.super_animations[i][1]=1;
        bank.animations[i][2]=bank.super_animations[i][2]=255;
    }
    const uint8_t walk[]={255,8,9,10,11,6,7,255},run[]={255,30,31,32,33,255};
    memcpy(bank.animations[0],walk,sizeof walk); bank.animation_length[0]=sizeof walk;
    memcpy(bank.animations[1],run,sizeof run); bank.animation_length[1]=sizeof run;
    const uint8_t falling[]={1,0xA9,0xAA,255},rising[]={3,0xAB,0xAC,0xAD,0xAE,0xAF,255};
    for (unsigned super=0;super<2;++super) {
        uint8_t (*scripts)[256]=super?bank.super_animations:bank.animations;
        uint16_t *lengths=super?bank.super_animation_length:bank.animation_length;
        memcpy(scripts[0x24],falling,sizeof falling); lengths[0x24]=sizeof falling;
        memcpy(scripts[0x25],rising,sizeof rising); lengths[0x25]=sizeof rising;
    }
    bank.super_animations[0][0]=255; bank.super_animations[0][1]=0x78;
    bank.super_animations[1][0]=255; bank.super_animations[1][1]=0x96;
    for(unsigned super=0;super<2;++super) for(unsigned face=0;face<2;++face) for(unsigned a=0;a<256;++a) {
        s2_character_reset(&s,S2_CHAR_SHADOW); m=standing(); m.animation=0;
        m.inertia=0x200; m.status=(uint8_t)face; m.angle=(uint8_t)a; m.super=(uint8_t)super;
        s2_character_animate(&s,&m,&bank);
        unsigned direction=((face?a:(uint8_t)~a)+16)&255,sector=(direction>>4)&6;
        assert(m.frame==(super?0x78+sector*4:8+sector*3));
        assert(s.render_flip==(face^((direction&128)?3:0)));
    }
    s2_character_reset(&s,S2_CHAR_SHADOW); m=standing(); m.animation=0; m.status=2;
    s2_character_animate(&s,&m,&bank); assert(s.source_animation==0x24);
    m.vy=-0x200; s2_character_animate(&s,&m,&bank); assert(s.source_animation==0x25);
    /* A horizontal spring/ledge can leave native Walk/Run unchanged through
     * airtime and landing. Landing must end the donor-only free-air pose,
     * including while its animation timer is still running. */
    for (unsigned super=0;super<2;++super) for (unsigned face=0;face<2;++face)
        for (unsigned movement=0;movement<2;++movement) for (unsigned rising_air=0;rising_air<2;++rising_air) {
            s2_character_reset(&s,S2_CHAR_SHADOW); m=standing();
            m.animation=(uint8_t)movement; m.status=(uint8_t)(face|2); m.super=(uint8_t)super;
            m.inertia=0x300; m.vy=rising_air?-0x200:0x200;
            for (unsigned t=0;t<3;++t) s2_character_animate(&s,&m,&bank);
            assert(m.frame>=0xA9 && m.frame<=0xAF);
            m.status=(uint8_t)face; m.vy=0;
            s2_character_animate(&s,&m,&bank);
            assert(m.animation==movement && s.source_animation==movement);
            assert(m.frame==(super?0x78:8));
            assert(s.render_flip==face);
            /* The next unrolled fall still selects its airborne script. */
            m.status|=2; m.vy=0x200;
            s2_character_animate(&s,&m,&bank);
            assert(s.source_animation==0x24 && m.frame==0xA9);
        }
    bank.palette[2]=0x222; bank.water_palette[2]=0x444; bank.cycle_palette[0][1][0]=0xEEE;
    assert(s2_shadow_color(&bank,2,0,0,0)==0x222 && s2_shadow_color(&bank,2,1,0,0)==0x444);
    assert(s2_shadow_color(&bank,2,0,255,4)==0xEEE);
    puts("PASS v3 Shadow boost costs, double jump, homing/chain, Chaos Control/Blast, all 1024 surface/flip/Super combinations and airborne/idle semantics");
    return 0;
}
