#include "sonic2_character.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static int16_t sine(unsigned a)
{ static const int16_t quarters[]={0,256,0,-256}; assert(!(a&63)); return quarters[(a&255)/64]; }
static S2Motion standing(void)
{ S2Motion m={0}; m.radius_x=9; m.radius_y=19; m.animation=5; return m; }
int main(void)
{
    S2Contacts c={64,64,64,sine}; S2CharacterState s; S2Motion m=standing();
    s2_character_reset(&s,S2_CHAR_AMY);
    s2_character_before(&s,&m,0x42,0x40,&c);
    assert(s.native_pressed==0x20 && s.jump_adjustment==-0x250);
    m.status=6; m.jumping=1; m.vy=-0x680;
    s2_character_after(&s,&m,&c);
    assert(m.vy==-0x8D0 && m.animation==0x26 && !s2_character_attacking(&s,&m));
    s2_character_reset(&s,S2_CHAR_AMY); m=standing();
    s2_character_before(&s,&m,0x20,0x20,&c);
    m.status=6; m.jumping=1; m.vy=-0x680; m.animation=2;
    s2_character_after(&s,&m,&c);
    assert(m.animation==0x10 && !s2_character_attacking(&s,&m));
    s2_character_reset(&s,S2_CHAR_AMY); m=standing();
    s2_character_before(&s,&m,0x40,0x40,&c); s2_character_after(&s,&m,&c);
    assert(m.animation==0x28 && s2_character_attacking(&s,&m) && !s.native_pressed);
    s2_character_reset(&s,S2_CHAR_AMY); m=standing();
    s2_character_before(&s,&m,0x22,0x20,&c);
    assert(m.vx==0x900 && m.vy==0x380 && !(s.native_held&2));
    s2_character_reset(&s,S2_CHAR_AMY); m=standing();
    s2_character_before(&s,&m,0x21,0x20,&c);
    assert(s.move==S2_MOVE_FIXED && s.charge==0);
    s2_character_before(&s,&m,0x21,0x20,&c);
    assert(s.charge==0x200);
    s2_character_before(&s,&m,0,0,&c); assert(m.inertia==0x900);
    s2_character_reset(&s,S2_CHAR_KNUCKLES); m=standing();
    s2_character_before(&s,&m,0x20,0x20,&c);
    m.status=6; m.jumping=1; m.vy=-0x680;
    s2_character_after(&s,&m,&c); assert(m.vy==-0x600);
    m.vy=-0x380;
    s2_character_before(&s,&m,0x20,0x20,&c);
    assert(s.special==S2_SK_GLIDE && s.move==S2_MOVE_AIR && m.radius_y==10 && !(m.status&4));
    assert(s2_character_attacking(&s,&m));
    s2_character_before(&s,&m,0,0,&c);
    assert(s.special==S2_SK_FALL && m.radius_y==19);
    s.special=S2_SK_CLIMB; s.wall_x=m.x; c.wall=0;
    s2_character_before(&s,&m,1,0,&c); assert(m.y==-65536);
    s2_character_before(&s,&m,0x20,0x20,&c);
    assert(s.special==S2_SK_NORMAL && m.vx==-0x400 && m.vy==-0x380 && (m.status&4));
    /* Source get-up/landing lock keeps native ground steering from replacing
     * the recovery animation on its very next tick. */
    s2_character_reset(&s,S2_CHAR_KNUCKLES); m=standing();
    s.special=S2_SK_FALL; m.jumping=1; m.vx=0x200;
    s2_character_after(&s,&m,&c);
    assert(m.animation==0x23 && m.move_lock==15 && !m.jumping && !m.vx);
    s.special=S2_SK_SLIDE; m.radius_y=m.radius_x=10; m.y=0; c.floor=0;
    s2_character_before(&s,&m,0,0,&c);
    assert(m.animation==0x22 && m.move_lock==15 && m.y==-9*65536 && m.radius_y==19);
    for (unsigned face=0;face<2;++face) {
        s2_character_reset(&s,S2_CHAR_KNUCKLES); m=standing();
        s.special=S2_SK_CLIMB; m.status=(uint8_t)(2|face); m.jumping=1;
        s.wall_x=m.x; c.wall=4;
        s2_character_before(&s,&m,1,0,&c);
        assert(s.special==S2_SK_LEDGE && s.frame==0xBD && m.y==-3*65536);
        for (unsigned tick=0;tick<21;++tick) s2_character_before(&s,&m,1,0,&c);
        assert(s.special==S2_SK_NORMAL && m.animation==5 && !m.jumping);
        assert(m.x==(face?-12:11)*65536 && m.y==-30*65536 && !(m.status&2));
    }
    S2DonorBank bank={0}; bank.count=253; bank.animation_count=6;
    bank.animation_length[5]=5; memcpy(bank.animations[5],(uint8_t[]){1,10,11,0xFE,1},5);
    s2_character_reset(&s,S2_CHAR_AMY); m=standing();
    s2_character_animate(&s,&m,&bank); assert(m.frame==10);
    s2_character_animate(&s,&m,&bank); assert(m.frame==10);
    s2_character_animate(&s,&m,&bank); assert(m.frame==11);
    s2_character_animate(&s,&m,&bank); s2_character_animate(&s,&m,&bank); assert(m.frame==11);
    /* S3&K Animate_Knuckles $17E42/$17E60: four walking or two running
     * frames per sector unit; the surface pose updates even while its gait
     * timer is held. Explicit donor-derived poses include both loop halves. */
    memset(&bank,0,sizeof bank); bank.count=251; bank.animation_count=6;
    bank.animation_length[0]=10;
    memcpy(bank.animations[0],(uint8_t[]){0xFF,7,8,1,2,3,4,5,6,0xFF},10);
    bank.animation_length[1]=6;
    memcpy(bank.animations[1],(uint8_t[]){0xFF,0x21,0x22,0x23,0x24,0xFF},6);
    static const uint8_t poses[][5]={ /* angle, facing, walk, run, render flip */
        {0x00,1,7,0x21,1}, {0x20,1,15,0x25,1},
        {0x40,1,23,0x29,1}, {0x60,1,31,0x2D,1},
        {0x80,1,7,0x21,2}, {0xA0,1,15,0x25,2},
        {0xC0,1,23,0x29,2}, {0xE0,1,31,0x2D,2},
        {0x00,0,7,0x21,0}, {0x20,0,31,0x2D,3},
        {0x40,0,23,0x29,3}, {0x60,0,15,0x25,3},
        {0x80,0,7,0x21,3}, {0xA0,0,31,0x2D,0},
        {0xC0,0,23,0x29,0}, {0xE0,0,15,0x25,0}
    };
    for (unsigned i=0;i<sizeof poses/sizeof poses[0];++i) {
        for (unsigned run=0;run<2;++run) {
            s2_character_reset(&s,S2_CHAR_KNUCKLES); m=standing(); m.animation=0;
            m.angle=poses[i][0]; m.status=poses[i][1]; m.inertia=run?0x900:0x400;
            s2_character_animate(&s,&m,&bank);
            assert(m.frame==poses[i][2+run] && s.render_flip==poses[i][4]);
        }
    }
    s2_character_reset(&s,S2_CHAR_KNUCKLES); m=standing();
    m.animation=0; m.status=1; m.inertia=0x400;
    s2_character_animate(&s,&m,&bank); assert(m.frame==7 && s.animation_timer==4);
    m.angle=0x40; s2_character_animate(&s,&m,&bank);
    assert(m.frame==24 && s.animation_timer==3 && s.animation_index==1);
    m.angle=0x80; s2_character_animate(&s,&m,&bank);
    assert(m.frame==8 && s.render_flip==2 && s.animation_index==1);
    m.angle=0x60; m.inertia=0x700; s2_character_animate(&s,&m,&bank);
    assert(m.frame==0x2E && s.animation_index==1);
    /* Native spring subtype 1 switches to Walk plus a tumble angle. These
     * explicit poses follow Animate_Knuckles -> Anim_Tumble, including byte
     * wrap at the end of a turn; held gait timing cannot freeze that pose. */
    static const uint8_t tumble[][3]={
        {1,0x31,0x37},{0x16,0x32,0x36},{0x40,0x34,0x34},
        {0x80,0x37,0x31},{0xC0,0x3A,0x3A},{0xE0,0x3B,0x38},{0xF5,0x31,0x38}
    };
    for (unsigned i=0;i<sizeof tumble/sizeof *tumble;++i) for (unsigned face=0;face<2;++face) {
        s2_character_reset(&s,S2_CHAR_KNUCKLES); m=standing();
        m.animation=0; m.status=(uint8_t)(2|face); m.flip_angle=tumble[i][0];
        s.animation_timer=10;
        s2_character_animate(&s,&m,&bank);
        assert(m.frame==tumble[i][1+face] && s.render_flip==(face?3:0));
        assert(!s.animation_timer);
    }
    puts("Source-derived Amy/Knuckles action transitions and animation control codes passed");
    return 0;
}
