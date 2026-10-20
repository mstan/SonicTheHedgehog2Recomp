/* Megamix v3 Shadow. Graphics/character work: Team Megamix / SEGA.
 * Behavior bindings and host adaptations: docs/SONIC2_MEGAMIX_CHARACTERS.md.
 * No donor code executes and no donor artwork is embedded in this module. */
#include "sonic2_shadow.h"
#include "sonic2_super.h"

enum { UP=1,DOWN=2,LEFT=4,RIGHT=8,BC=0x30,A=0x40 };
enum { FACE=1,AIR=2,ROLL=4,ROLLJUMP=16,PUSH=32,WATER=64 };
enum { NORMAL,DOUBLE_JUMP,HOMING };
static int magnitude(int v) { return v<0?-v:v; }
static int facing(const S2Motion *m) { return m->status&FACE?-1:1; }
static void cancel_chaos(S2CharacterState *s,S2Motion *m)
{
    if (!s->shadow_chaos) return;
    s->shadow_chaos=0; m->physics_override=1;
    m->physics_top=m->super?0xC00:((m->status&WATER)?0x300:0x600);
}
static void ball(S2Motion *m)
{
    if (!(m->status&ROLL)) m->y+=(19-14)*65536;
    m->status|=ROLL; m->radius_x=7; m->radius_y=14; m->animation=2;
}
static void home(S2CharacterState *s,S2Motion *m,const S2Contacts *c)
{
    unsigned a=c->angle(c->target_dx,c->target_dy);
    m->angle=(uint8_t)a;
    m->vx=(int16_t)((c->sine((a+0x40)&255)*0xC00)>>8);
    m->vy=(int16_t)((c->sine(a)*0xC00)>>8);
    ball(m); m->status|=AIR; m->jumping=0;
    s->special=HOMING; s->shadow_target=c->target;
    s->move=S2_MOVE_AIR;
}
void s2_shadow_before(S2CharacterState *s,S2Motion *m,const S2Contacts *c)
{
    if (s->shadow_monitor_ticks) --s->shadow_monitor_ticks;
    s->native_held&=~A; s->native_pressed&=~A;
    if (m->hurt || m->control_locked) {
        if (m->hurt) s->shadow_monitor_ticks=0;
        s->special=NORMAL; s->shadow_air_used=s->shadow_target=s->shadow_chain=0;
        cancel_chaos(s,m); return;
    }
    if (!(m->status&AIR)) {
        s->shadow_air_used=s->shadow_target=s->shadow_last_target=s->shadow_chain=0;
        s->special=NORMAL;
    }
    /* v3 $11D10C: release A, lose Super or slow below $600 to cancel.
     * Ground taps launch at $C00 and add $40 thereafter. */
    if (s->shadow_chaos && (!m->super || !(s->held&A) || magnitude(m->inertia)<0x600)) cancel_chaos(s,m);
    if (m->super && !s->shadow_chaos && !(m->status&WATER) && m->physics_top==0xA00) {
        m->physics_override=1; m->physics_top=0xC00; /* v3 $11CE84 */
    }
    if (s->shadow_chaos) {
        if (magnitude(m->inertia)<0xC00) m->inertia=(int16_t)(facing(m)*0xC00);
    }
    if (!(m->status&AIR) && (s->pressed&A) && c->wall>=0 && !(m->status&PUSH)) {
        s->native_pressed&=~BC;
        if (m->super || (m->rings>=5 && !(m->status&ROLL))) {
            if (!m->super) m->rings-=5;
            else {
                if (m->status&ROLL) { m->status&=~ROLL; m->radius_x=9; m->radius_y=19; m->y-=5*65536; }
                if (s->held&LEFT) m->status|=FACE;
                if (s->held&RIGHT) m->status&=~FACE;
                s->shadow_chaos=1; m->physics_override=1; m->physics_top=0x1100;
            }
            int speed=magnitude(m->inertia);
            speed=m->super && speed>=0xC00?speed+0x40:m->speed_shoes?0x1000:0xC00;
            m->inertia=(int16_t)(facing(m)*speed); m->animation=1;
        }
    }
    if ((m->status&AIR) && (s->pressed&A)) {
        if (m->super && m->invincible_monitor) { m->blast=1; m->vx=m->vy=m->inertia=0; }
        else if (!(m->status&ROLL)) { ball(m); m->jumping=1; }
    }
    /* BC+BC transformation belongs to the host's native Super lifecycle. */
    if ((s->held&BC)==BC && (s->pressed&BC)) return;
    if (!(m->status&AIR)) return;
    if (s->special==HOMING) {
        if (c->target==s->shadow_target && s->shadow_target_age) {
            --s->shadow_target_age; home(s,m,c); return;
        }
        s->shadow_last_target=s->shadow_target; s->shadow_target=0;
        if (s->shadow_chain && (s->held&A) && c->target && c->target!=s->shadow_last_target && m->rings>5) {
            m->rings-=5; s->shadow_target_age=32; home(s,m,c); return;
        }
        s->special=DOUBLE_JUMP;
    }
    /* v3 $19DD0: nearest front target within 128px. Without one,
     * $19644 gives one $700 jump ($3C0 underwater), without short-hop clamp. */
    if ((s->pressed&BC) && !s->shadow_air_used && (m->status&ROLL)) {
        s->shadow_air_used=1; s->native_pressed&=~BC;
        if (c->target && c->angle) {
            s->shadow_chain=!!(s->held&A); s->shadow_target_age=32; home(s,m,c);
        } else {
            m->status&=~ROLLJUMP; m->vy=(m->status&WATER)?-0x3C0:-0x700;
            m->jumping=0; s->special=DOUBLE_JUMP;
        }
    }
}
void s2_shadow_after(S2CharacterState *s,S2Motion *m)
{
    /* $11D10C runs after native motion: retain the $C00 ground speed floor
     * against ordinary no-direction friction while A holds Chaos Control. */
    if (s->shadow_chaos && m->super && (s->held&A) && magnitude(m->inertia)>=0x600 && magnitude(m->inertia)<0xC00)
        m->inertia=(int16_t)(facing(m)*0xC00);
    if (!(m->status&AIR) || m->hurt) {
        s->shadow_air_used=s->shadow_target=s->shadow_last_target=s->shadow_chain=0;
        s->special=NORMAL;
    }
    if (m->hurt) cancel_chaos(s,m);
}
void s2_shadow_animate(S2CharacterState *s,S2Motion *m,const S2DonorBank *b)
{
    if (!b || b->animation_count<38) return;
    static const uint8_t translation[34]={0,1,2,3,4,5,6,7,8,0x1F,5,0,6,0xD,0xE,0xF,
        0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x1A,0x1A,0x1B,0x1C,6,6,0x23,5,5};
    unsigned source=m->animation<34?translation[m->animation]:5;
    unsigned free_air=(m->status&6)==AIR && m->animation<=1;
    if (free_air) source=m->vy<0?0x25:0x24;
    unsigned super=m->super && b->super_animation_count==38;
    unsigned was_free_air=s->source_animation==0x24 || s->source_animation==0x25;
    if (s->animation!=m->animation || s->shadow_super!=super ||
        ((free_air || was_free_air) && s->source_animation!=source)) {
        /* Native Walk/Run can survive a ledge or horizontal spring unchanged.
         * End the donor-only airborne override on landing as well as entry. */
        s->animation=m->animation; s->source_animation=source;
        s->animation_index=s->animation_timer=0; s->shadow_super=super;
    }
    unsigned a=s->source_animation<38?s->source_animation:5;
    const uint8_t *seq=super?b->super_animations[a]:b->animations[a];
    unsigned length=super?b->super_animation_length[a]:b->animation_length[a];
    unsigned delay=seq[0],offset=0,speed=(unsigned)magnitude(m->inertia);
    s->render_flip=!!(m->status&FACE);
    if (delay==0xFF) {
        unsigned angle=m->angle;
        if (!(m->status&FACE)) angle=(uint8_t)~angle;
        angle=(angle+16)&255;
        if (angle&128) s->render_flip^=3;
        unsigned sector=(angle>>4)&6;
        a=speed>=(super?0x800u:0x600u)?1:0;
        offset=super?(a?sector/2:sector*4):sector*(a?2:3);
        seq=super?b->super_animations[a]:b->animations[a];
        length=super?b->super_animation_length[a]:b->animation_length[a];
        delay=speed<0x800?(0x800-speed)>>8:0;
    } else if (delay==0xFE) {
        a=speed<0x600?2:3; seq=b->animations[a]; length=b->animation_length[a];
        delay=speed<0x400?(0x400-speed)>>8:0;
    } else if (delay==0xFD) delay=speed<0x800?(0x800-speed)>>6:0;
    /* Update surface orientation every tick; the timer gates gait only. */
    for (unsigned attempts=0;attempts<3;++attempts) {
        if (s->animation_index+1>=length) s->animation_index=0;
        unsigned code=seq[1+s->animation_index];
        if (code==0xFF) { s->animation_index=0; continue; }
        if (code==0xFE) {
            unsigned back=seq[2+s->animation_index];
            s->animation_index=s->animation_index>=back?s->animation_index-back:0; continue;
        }
        if (code==0xFD) {
            s->source_animation=seq[2+s->animation_index]; s->animation_index=s->animation_timer=0;
            if (!s->source_animation) { m->animation=0; s->animation=0; }
            break;
        }
        if (code+offset<b->count) s->frame=(uint8_t)(code+offset);
        if (s->animation_timer) --s->animation_timer;
        else { s->animation_timer=delay; ++s->animation_index; }
        break;
    }
    m->frame=s->frame;
}
uint16_t s2_shadow_color(const S2DonorBank *b,unsigned index,int wet,unsigned phase,unsigned frame)
{
    if (!b || index>=16) return 0;
    uint16_t normal=wet?b->water_palette[index]:b->palette[index];
    if (!phase || index<2 || index>5) return normal;
    uint16_t target=b->cycle_palette[wet!=0][frame/4%16][index-2];
    if (phase!=1 && phase!=2) return target;
    unsigned strength=s2_super_glow(phase,frame),out=0;
    for (unsigned shift=0;shift<12;shift+=4) {
        int start=(normal>>shift)&14,end=(target>>shift)&14;
        out|=(unsigned)((start+(end-start)*(int)strength/6)&14)<<shift;
    }
    return (uint16_t)out;
}
