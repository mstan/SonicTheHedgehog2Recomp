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
    s2_character_after(&s,&m,&c);
    assert(m.radius_x==9 && m.radius_y==19 && s.special==S2_SK_GLIDE);
    s2_character_before(&s,&m,0x20,0,&c);
    assert(m.radius_x==9 && m.radius_y==19);
    s2_character_after(&s,&m,&c);
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
    s2_character_reset(&s,S2_CHAR_KNUCKLES);m=standing();
    s.special=S2_SK_FALL;s.glide_collision=0x80;m.status=2;m.angle=0xC8;
    m.vx=m.inertia=0x400;m.jumping=1;
    s2_character_after(&s,&m,&c);
    assert(!s.special && !m.animation && !m.move_lock && !(m.status&2));
    assert(!m.vx && !m.vy && !m.inertia && !m.jumping && !m.y);
    s.special=S2_SK_SLIDE; m.radius_y=m.radius_x=10; m.y=0; c.floor=0;
    s2_character_before(&s,&m,0,0,&c);
    assert(m.animation==0x22 && m.move_lock==15 && m.y==-9*65536 && m.radius_y==19);
    /* The KiS2 glide box is local to terrain collision, including a slide.
     * Solid-object landing uses the full box and returns directly to Walk,
     * preserving the object's movement rather than the terrain get-up lock. */
    s2_character_reset(&s,S2_CHAR_KNUCKLES); m=standing();
    s.special=S2_SK_SLIDE; m.vx=0x200; c.floor=0;
    s2_character_before(&s,&m,0x20,0,&c);
    assert(m.radius_y==10 && m.vx==0x1E0);
    s2_character_after(&s,&m,&c);
    assert(m.radius_y==19 && m.radius_x==9);
    assert(s2_character_attacking(&s,&m));
    m.y=80*65536;
    s2_character_before(&s,&m,0,0,&c);
    assert(m.y==71*65536 && m.animation==0x22 && !m.jumping && m.radius_y==19);
    for (unsigned action=S2_SK_GLIDE;action<=S2_SK_LEDGE;++action) {
        s2_character_reset(&s,S2_CHAR_KNUCKLES); m=standing();
        s.special=action; m.status=8; m.animation=0x20; m.jumping=1;
        m.vx=m.inertia=0x180; m.y=123*65536;
        s2_character_before(&s,&m,0x20,0,&c);
        assert(s.special==S2_SK_NORMAL && s.move==S2_MOVE_NATIVE);
        assert(m.animation==0 && !m.jumping && !m.move_lock);
        assert(m.y==123*65536 && m.vx==0x180 && m.radius_y==19);
    }
    s2_character_reset(&s,S2_CHAR_KNUCKLES);m=standing();
    s.special=S2_SK_GLIDE;m.radius_x=m.radius_y=10;m.hurt=1;
    s2_character_after(&s,&m,&c);
    assert(!s.special && m.radius_x==9 && m.radius_y==19);
    for (unsigned face=0;face<2;++face) {
        s2_character_reset(&s,S2_CHAR_KNUCKLES); m=standing();
        s.special=S2_SK_CLIMB; m.status=(uint8_t)(2|face); m.jumping=1;
        s.wall_x=m.x; c.wall=c.climb_up=4;
        s2_character_before(&s,&m,1,0,&c);
        assert(s.special==S2_SK_LEDGE && s.frame==0xBD && m.y==-3*65536);
        for (unsigned tick=0;tick<21;++tick) s2_character_before(&s,&m,1,0,&c);
        assert(s.special==S2_SK_NORMAL && m.animation==5 && !m.jumping);
        assert(m.x==(face?-12:11)*65536 && m.y==-30*65536 && !(m.status&2));
    }
    /* KiS2 BeginClimb aligns a near upper edge, rejects gaps >=12, and
     * uses separate probes above/below the player's center while climbing. */
    c.climb_up=c.climb_down=0;c.wall=0;c.grab_y=11;c.grab_x=1;
    s2_character_reset(&s,S2_CHAR_KNUCKLES);m=standing();
    s.special=S2_SK_GLIDE;m.status=3;m.jumping=1;m.animation=0x20;
    s2_character_after(&s,&m,&c);
    assert(s.special==S2_SK_CLIMB && m.x==65536 && m.y==11*65536 && s.wall_x==m.x);
    c.grab_y=12;s2_character_reset(&s,S2_CHAR_KNUCKLES);m=standing();
    s.special=S2_SK_GLIDE;m.status=2;m.animation=0x20;
    s2_character_after(&s,&m,&c);assert(s.special==S2_SK_FALL && m.animation==0x21);
    c.grab_y=-1;s.special=S2_SK_GLIDE;
    s2_character_after(&s,&m,&c);assert(s.special==S2_SK_FALL);
    c.grab_y=c.grab_x=0;
    s.special=S2_SK_GLIDE;m.status=0;
    s2_character_after(&s,&m,&c);
    assert(s.special==S2_SK_CLIMB && (m.status&2)); /* KiS2 wall before floor */
    s2_character_reset(&s,S2_CHAR_KNUCKLES);m=standing();
    s.special=S2_SK_GLIDE;s.glide_collision=0x82;m.status=2;m.vx=0;
    s2_character_after(&s,&m,&c);
    assert(s.special==S2_SK_GLIDE); /* touching is not penetration */
    for(unsigned face=0;face<2;++face) {
        s2_character_reset(&s,S2_CHAR_KNUCKLES);m=standing();
        s.special=S2_SK_GLIDE;m.status=(uint8_t)face;m.jumping=1;
        s.glide_collision=0x80; /* actual floor, no wall penetration */
        m.angle=face?0x40:0xC0;m.vx=0;m.inertia=face?-0x480:0x480;
        s2_character_after(&s,&m,&c);
        assert(s.special==S2_SK_NORMAL && !m.jumping && !(m.status&6));
        assert(m.vx==m.inertia && !m.vy && !m.animation);
        assert(m.radius_y==19 && m.radius_x==9);
    }
    s.special=S2_SK_CLIMB;m.status=2;s.wall_x=m.x;c.climb_up=2;unsigned age=s.age;
    s2_character_before(&s,&m,1,0,&c);
    assert(s.special==S2_SK_CLIMB && m.y==0 && s.age==age && !s.climb_delta);
    c.climb_up=0;c.ceiling=-3;
    s2_character_before(&s,&m,1,0,&c);
    assert(m.y==3*65536 && s.climb_delta==1);
    c.ceiling=64;c.climb_min_enabled=1;c.climb_min_y=100;m.y=100*65536;m.super=1;
    s2_character_before(&s,&m,1,0,&c);
    assert(m.y==100*65536 && s.climb_delta==1);
    c.climb_min_enabled=0;
    s2_character_before(&s,&m,1,0,&c);
    assert(m.y==98*65536); /* wrapping stages retain upward movement */
    m.y=3*65536;m.super=0;
    c.ceiling=64;c.climb_down=0;c.floor=-1;c.floor_angle=8;m.jumping=1;
    s2_character_before(&s,&m,2,0,&c);
    assert(s.special==S2_SK_NORMAL && m.y==2*65536 && m.angle==8 && !m.jumping && !(m.status&2));
    c.floor=0;
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
    /* Grounded wall contact keeps Walk selected and sets Status_Push.
     * Animate_Knuckles loc_17ECC then uses AniKnuckles04, advancing every
     * nine ticks at zero inertia; it does not display a walking gait. */
    bank.animation_length[4]=6;
    memcpy(bank.animations[4],(uint8_t[]){0xFD,0xCE,0xCF,0xD0,0xD1,0xFF},6);
    for (unsigned face=0;face<2;++face) {
        s2_character_reset(&s,S2_CHAR_KNUCKLES);m=standing();
        m.animation=0;s.animation=0;m.status=(uint8_t)(32|face);
        for(unsigned tick=0;tick<72;++tick) {
            s2_character_animate(&s,&m,&bank);
            assert(m.frame==0xCE + (tick/9)%4 && s.render_flip==face);
            assert(m.animation==0 && (m.status&32));
        }
        bank.animation_length[5]=3;
        memcpy(bank.animations[5],(uint8_t[]){5,0x56,0xFF},3);
        m.animation=5;s2_character_animate(&s,&m,&bank);
        assert(!(m.status&32) && m.frame==0x56 && s.render_flip==face);
    }
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
