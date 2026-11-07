/* Sonic 2 host adapter. Stock object execution owns the world tick. These
 * callbacks adapt player roles only, never replay a level or enemy update. */
#include "sonic2_runtime.h"
#include "sonic2_party.h"
#include "sonic2_resources.h"
#include "sonic2_character.h"
#include "sonic2_super.h"
#include "sonic2_shadow.h"
#include "sonic2_mighty.h"
#include "sonic2_video.h"
#include "sonic2_state_io.h"
#include "sonic2_save_menu.h"
#include "sonic2_options.h"
#include "audio/event_queue.h"
#include "video/genesis_vdp.h"
#include "video/genesis_dac.h"
#include "genesis_runtime.h"
#include "input_map.h"
#include "input_script.h"
#include "sim_step.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#if GENESIS_HAS_RECOMP_NET
#include "netplay/genesis_netplay.h"
#endif
static int active, inside_player, actor = -1;
static uint8_t previous_input[S2_MAX_PLAYERS];
/* The native character decides whether to submit a sprite, including hurt
 * flashing. Reuse that decision instead of approximating its timer phase. */
static uint8_t displayed[S2_MAX_PLAYERS];
/* Tails_control_counter through Tails_CPU_jumping ($F702..$F70F).
 * Each companion runs the original P2 AI with its own native state. */
static uint8_t companion_cpu[S2_MAX_PLAYERS][14];
static int inside_companion_cpu;
static S2CharacterState characters[S2_MAX_PLAYERS];
static uint8_t physics[S2_MAX_PLAYERS][6];
static unsigned objects[S2_MAX_PLAYERS]={0xB000,0xB040};
static S2DonorBank native_banks[2];
static S2NativeHudArt hud_art;
static int hud_art_ready;
const uint8_t *s2_runtime_ui_font(void) {return hud_art_ready?hud_art.font:NULL;}
/* The native status byte only has standing/pushing bits for two players.
 * Extra actors retain those two bits per solid on the host. The actual actor
 * objects have real, reserved native pool addresses, so monitor/platform
 * parent links stay valid across ticks (no temporary Sidekick alias). */
static uint8_t solid_flags[S2_MAX_PLAYERS-2][144], solid_ids[144];
static int inside_solid;
typedef struct {
    int16_t x,y;
    uint16_t art;
    uint8_t frame,flip,visible,wet;
} PartySprite;
/* Final sprite is native Obj05 (P1 Tails' separate tails). */
static PartySprite published_sprites[S2_MAX_PLAYERS+1];
enum { DUST_SLOTS=(0xD000-0xB400)/64 };
static PartySprite published_dust[DUST_SLOTS];
static unsigned dust_emit_age[S2_MAX_PLAYERS];
static uint8_t displayed_tails;
/* Reentrancy guards of the hooks below, hoisted from function-local statics
 * so the rollback snapshot can carry them: the game fiber can be suspended
 * INSIDE a hook (any bus access may yield to the raster scheduler), so at a
 * tick boundary any of these may legitimately be 1. */
static int s_inside_spring, s_inside_palette, s_reward, s_checking_death,
           s_transforming, s_combat;
static int amy_available(void) { return s2_party.amy_enabled && s2_resource_verified(S2_RESOURCE_AMY); }
static int knuckles_available(void) { return s2_party.s3k_enabled && s2_resource_verified(S2_RESOURCE_SK); }
static const S2Character amy={"amy","AMY",1,amy_available};
static const S2Character knuckles={"knuckles","KNUCKLES",1,knuckles_available};
static unsigned kind(unsigned p)
{
    const char *id=s2_party.roster.character[p];
    return !strcmp(id,"amy")?S2_CHAR_AMY:!strcmp(id,"knuckles")?S2_CHAR_KNUCKLES:
        !strcmp(id,"tails")?S2_CHAR_TAILS:!strcmp(id,"shadow")?S2_CHAR_SHADOW:
        !strcmp(id,"mighty")?S2_CHAR_MIGHTY:S2_CHAR_SONIC;
}
static const S2DonorBank *bank(unsigned p)
{
    unsigned k=kind(p);
    if (k==S2_CHAR_SHADOW) return s2_resource_shadow_bank();
    if (k==S2_CHAR_MIGHTY) return s2_resource_bank(S2_RESOURCE_MEGAMIX);
    return k<S2_CHAR_AMY?&native_banks[k]:s2_resource_bank(k==S2_CHAR_AMY?S2_RESOURCE_AMY:S2_RESOURCE_SK);
}
/* Donor controllers adapt native motion and replace animation/art. Mighty
 * retains Sonic's controller and maps its frames at presentation. */
static int imported(void) { return actor>=0 && (kind((unsigned)actor)==S2_CHAR_AMY || kind((unsigned)actor)==S2_CHAR_KNUCKLES || kind((unsigned)actor)==S2_CHAR_SHADOW); }
void s2_runtime_load(void)
{
    active=inside_player=0; actor=-1;
    gvdp_set_unlimited_sprites(0);
    s2_video_prepare_knuckles_items();
    hud_art_ready=0;
    memset(published_sprites,0,sizeof published_sprites);
    memset(published_dust,0,sizeof published_dust);
    memset(dust_emit_age,0xFF,sizeof dust_emit_age);
    memset(objects+2,0,sizeof objects-2*sizeof objects[0]);
    s2_donor_free(&native_banks[0]); s2_donor_free(&native_banks[1]);
    s2_character_register(&knuckles); s2_character_register(&amy);
    s2_shadow_register();
    s2_mighty_register();
}
static uint16_t word(unsigned a) { return (uint16_t)((g_ram[a] << 8) | g_ram[a+1]); }
int s2_runtime_campaign_online(void)
{
#if GENESIS_HAS_RECOMP_NET
    int mode=g_ram[0xF600]&127;
    return genesis_netplay_active() && !word(0xFFD8) && (mode==12 || mode==8);
#else
    return 0;
#endif
}
int s2_runtime_player_position(unsigned p, int *x, int *y)
{
    if (p>=s2_party.roster.slots || p>=S2_MAX_PLAYERS || !objects[p] || !g_ram[objects[p]]) return 0;
    unsigned o=objects[p];
    /* Despawn/flight recovery still belongs to the shared campaign. Follow
     * the leader during recovery instead of showing the x=$4000 sentinel. */
    if (p && companion_cpu[p][6]==0 && (companion_cpu[p][7]==2 || companion_cpu[p][7]==4)) o=objects[0];
    *x=word(o+8); *y=(int16_t)word(o+12);
    return 1;
}
static void putword(unsigned a, unsigned v) { g_ram[a]=(uint8_t)(v>>8); g_ram[a+1]=(uint8_t)v; }
static int custom_super_palette(void)
{ return kind(0)!=S2_CHAR_SONIC && !word(0xFFD8) && g_ram[0xF65F]; }
static int32_t fixed(unsigned a) { return (int32_t)(((uint32_t)word(a)<<16)|word(a+2)); }
static void putfixed(unsigned a, int32_t v) { putword(a,(uint32_t)v>>16); putword(a+2,(uint32_t)v); }
static int16_t sine(unsigned a) { a=0x33CE + (a&255)*2; return (int16_t)((g_rom[a]<<8)|g_rom[a+1]); }
static unsigned ring_address(void) { return actor>0 && word(0xFFD8)?0xFED0:0xFE20; }
static S2Motion motion(unsigned o)
{
    S2Motion m={0}; m.x=fixed(o+8); m.y=fixed(o+12);
    m.vx=(int16_t)word(o+0x10); m.vy=(int16_t)word(o+0x12); m.inertia=(int16_t)word(o+0x14);
    m.status=g_ram[o+0x22]; m.angle=g_ram[o+0x26]; m.radius_y=g_ram[o+0x16]; m.radius_x=g_ram[o+0x17];
    m.animation=g_ram[o+0x1C]; m.frame=g_ram[o+0x1A]; m.jumping=g_ram[o+0x3C];
    m.flip_angle=g_ram[o+0x27];
    m.hurt=g_ram[o+0x24]>=4; m.boosted=word(o+0x34)!=0 || (o==0xB000 && g_ram[0xFE19]);
    /* Stage scripts (notably signposts) retain Ctrl_1_Logical while the
     * global lock is set, independently of an object's movement lock. */
    m.control_locked=!!((g_ram[o+0x2A]&1) || g_ram[0xF7CC]);
    m.rings=word(ring_address()); m.super=actor==0 && !word(0xFFD8) && g_ram[0xFE19];
    m.physics_top=word(0xF760); m.move_lock=word(o+0x2E);
    m.speed_shoes=word(o+0x34)!=0;
    m.invincible_monitor=word(o+0x32)>1 || (actor>=0 && characters[actor].shadow_monitor_ticks!=0);
    return m;
}
static void store_motion(unsigned o, const S2Motion *m)
{
    putfixed(o+8,m->x); putfixed(o+12,m->y); putword(o+0x10,m->vx); putword(o+0x12,m->vy); putword(o+0x14,m->inertia);
    g_ram[o+0x22]=m->status; g_ram[o+0x26]=m->angle; g_ram[o+0x16]=m->radius_y; g_ram[o+0x17]=m->radius_x;
    g_ram[o+0x1C]=m->animation; g_ram[o+0x1A]=m->frame; g_ram[o+0x3C]=m->jumping;
    putword(o+0x2E,m->move_lock);
}
static M68KState native_helper(uint32_t pc)
{
    /* A real, balanced synthetic return slot for native helpers. Their nested
     * calls retain normal strict-stack checks; no generated C is edited. */
    M68KState before=g_cpu;
    recomp_push_return(0x15FDC); recomp_call_addr(pc);
    M68KState result=g_cpu; g_cpu=before; return result;
}
static int16_t host_call(uint32_t pc) { return (int16_t)native_helper(pc).D[1]; }
static uint8_t shadow_angle(int dx,int dy)
{
    M68KState saved=g_cpu;
    g_cpu.D[1]=(uint16_t)dx; g_cpu.D[2]=(uint16_t)dy;
    uint8_t a=(uint8_t)native_helper(0x364E).D[0]; g_cpu=saved; return a;
}
static int shadow_target(unsigned o,int monitors)
{
    unsigned id=g_ram[o],collision=g_ram[o+0x20];
    if (!id || !collision) return 0;
    if (id==0x26) return monitors && g_ram[o+0x24]<4;
    return (!(collision&0xC0) && (collision&63)>=5) || collision==0xCC;
}
static void shadow_contacts(unsigned o,S2Contacts *c)
{
    if (actor<0 || kind((unsigned)actor)!=S2_CHAR_SHADOW) return;
    S2CharacterState *s=&characters[actor]; c->angle=shadow_angle;
    if (s->shadow_target>=0xB400 && s->shadow_target<0xD000 && !(s->shadow_target&63) && shadow_target(s->shadow_target,1)) c->target=s->shadow_target;
    else {
        unsigned distance=0x4000;
        for (unsigned at=0xB400;at<0xD000;at+=0x40) {
            if (at==s->shadow_target || at==s->shadow_last_target || !shadow_target(at,1)) continue;
            int dx=(int16_t)(word(at+8)-word(o+8)),dy=(int16_t)(word(at+12)-word(o+12));
            if (!s->shadow_chain && ((g_ram[o+0x22]&1)?dx>=16:dx<=-16)) continue;
            /* Bound before squaring to keep the signed product portable. */
            if (dx<=-128 || dx>=128 || dy<=-128 || dy>=128) continue;
            unsigned d=(unsigned)(dx*dx+dy*dy);
            if (d<distance) { distance=d; c->target=at; }
        }
    }
    if (c->target) {
        c->target_dx=(int16_t)(word(c->target+8)-word(o+8));
        c->target_dy=(int16_t)(word(c->target+12)-word(o+12));
    }
}
static S2Contacts contacts(unsigned o)
{
    S2Contacts c={64,64,64,sine};
    c.climb_min_enabled=(int16_t)word(0xEECC)!=-256;
    c.climb_min_y=(int16_t)(word(0xEECC)+16);
    if (!g_ram[o+0x16]) return c;
    M68KState cpu=g_cpu;
    uint8_t ry=g_ram[o+0x16],rx=g_ram[o+0x17];
    int knux=actor>=0 && kind((unsigned)actor)==S2_CHAR_KNUCKLES;
    unsigned special=knux?characters[actor].special:0;
    if (knux && (special==S2_SK_GLIDE || special==S2_SK_SLIDE || special==S2_SK_CLIMB))
        g_ram[o+0x16]=g_ram[o+0x17]=10;
    uint8_t angles[4]; memcpy(angles,g_ram+0xF768,4);
    c.floor=host_call(0x1EC4E); /* also selects the player's collision plane */
    g_cpu.D[5]=g_ram[o+0x3F];
    c.wall=host_call((g_ram[o+0x22]&1)?0x1F05E:0x1EEDC);
    c.ceiling=host_call(0x1EF2E);
    if(knux && special==S2_SK_GLIDE && c.wall<=0 && (int16_t)word(o+0x10)>-0x20 && (int16_t)word(o+0x10)<0x20) {
        int left=g_ram[o+0x22]&1;
        M68KState fit=native_helper(left?0x1EFF6:0x1EE7C);
        if(((uint16_t)fit.D[0]|(uint16_t)fit.D[1])==0)c.grab_x=left?1:0;
        else {
            g_cpu.D[5]=g_ram[o+0x3E];g_ram[0xF768]=0;
            g_cpu.D[2]=(uint16_t)(word(o+12)-21);
            g_cpu.D[3]=(uint16_t)(word(o+8)+(left?-11:11));
            /* REV01 adds ten, equal to KiS2's temporary radius. The final
             * probe is at y-11 with TOP solidity, as ChkFloorEdge_Part3. */
            c.grab_y=host_call(0x1ECE6);
        }
    }
    if(knux && special==S2_SK_CLIMB) {
        int left=g_ram[o+0x22]&1;unsigned wall=left?0x1F066:0x1EEE4;
        g_cpu.D[2]=(uint16_t)(word(o+12)-11);
        g_cpu.D[3]=(uint16_t)(word(o+8)-(left?1:0));c.climb_up=host_call(wall);
        g_cpu.D[2]=(uint16_t)(word(o+12)+11);
        g_cpu.D[3]=(uint16_t)(word(o+8)-(left?1:0));c.climb_down=host_call(wall);
        g_cpu.D[2]=(uint16_t)(word(o+12)-8);g_cpu.D[3]=word(o+8);
        c.ceiling=host_call(0x1EF9E);
        g_cpu.D[5]=g_ram[o+0x3E];g_cpu.D[2]=(uint16_t)(word(o+12)+9);g_cpu.D[3]=word(o+8);
        c.floor=host_call(0x1ECE6);c.floor_angle=g_ram[0xF768];
    }
    g_cpu=cpu;memcpy(g_ram+0xF768,angles,4);
    g_ram[o+0x16]=ry; g_ram[o+0x17]=rx; return c;
}
static void shadow_effects(unsigned p,unsigned o,S2Motion *m)
{
    if (kind(p)!=S2_CHAR_SHADOW) return;
    if (m->rings!=word(ring_address())) {
        putword(ring_address(),m->rings);
        g_ram[actor>0 && word(0xFFD8)?0xFEC9:0xFE1D]|=0x81;
    }
    if (m->physics_override) putword(0xF760,m->physics_top);
    if (characters[p].shadow_chaos && m->super && word(0xF670)>10) putword(0xF670,10);
    if (m->blast) {
        fprintf(stderr,"[Shadow] P%u Chaos Blast\n",p+1);
        /* v3 $198F0/$117094. Run native kill/hurt effects, keeping actual
         * boss handlers, points, explosions and stage transitions intact. */
        M68KState saved=g_cpu;
        for (unsigned at=0xB400;at<0xD000;at+=0x40) {
            if (!(g_ram[at+1]&0x80) || !shadow_target(at,0)) continue;
            unsigned hp=g_ram[at+0x21],hits=hp?hp/2:1;
            for (unsigned h=0;h<hits;++h) {
                g_cpu.A[0]=0xFFFF0000u|o; g_cpu.A[1]=0xFFFF0000u|at;
                native_helper(0x3F7C8);
            }
        }
        g_cpu=saved; putword(o+0x10,0); putword(o+0x12,0); putword(o+0x14,0);
    }
}
static int vanilla(void)
{
    const S2Roster *r=&s2_party.roster;
    return r->slots==2 && !strcmp(r->character[0],"sonic") && !strcmp(r->character[1],"tails");
}
const char *s2_runtime_state_unavailable_reason(void)
{
#if GENESIS_HAS_RECOMP_NET
    if (genesis_netplay_active()) return "Quickstates are local-only; disconnect netplay first.";
#endif
    return NULL;
}
static int level(void) { return active && ((g_ram[0xF600]&0x7F)==12); }
static int presented_level(void)
{
    unsigned mode=g_ram[0xF600]&127;
    /* SpecialStage changes mode before fading the still-displayed level.
     * Its initialization retires the native player object only after white. */
    return level() || (active && mode==16 &&
        (g_ram[0xB000]==1 || g_ram[0xB000]==2) && g_ram[0xF711]);
}
int s2_runtime_knuckles_item(unsigned o)
{
    if (!level() || o<0xB000 || o>=0xD400) return 0;
    unsigned id=g_ram[o];
    if(id==0x3A && (g_ram[o+0x1A]==0xC || g_ram[o+0x1A]==0xD))
        return kind(0)==S2_CHAR_KNUCKLES && knuckles_available();
    /* Monitor contents are world objects: the primary character selects the
     * lock-on game's monitor theme. Attached effects follow their own parent. */
    if (id==0x26 || id==0x2E || ((id==0x0D || id==0x04 || id==0x28) && !word(0xFFD8)))
        return kind(0)==S2_CHAR_KNUCKLES && knuckles_available();
    if (id!=0x38 && id!=0x35) return 0;
    unsigned parent=word(o+0x3E);
    for (unsigned p=0;p<S2_MAX_PLAYERS;++p)
        if (objects[p]==parent && kind(p)==S2_CHAR_KNUCKLES && knuckles_available()) return 1;
    return 0;
}
int s2_runtime_shadow_star(unsigned o)
{
    if (!level() || o<0xB000 || o>=0xD400 || g_ram[o]!=0x35 ||
        !s2_party.shadow_enabled || !s2_resource_shadow_bank()) return 0;
    unsigned parent=word(o+0x3E);
    for (unsigned p=0;p<S2_MAX_PLAYERS;++p)
        if (objects[p]==parent && kind(p)==S2_CHAR_SHADOW) return 1;
    return 0;
}
static unsigned native_id(unsigned p)
{
    const S2Character *c=s2_character_find(s2_party.roster.character[p]);
    return c && c->available() ? c->native_object_id : 0;
}
static int human_companion(unsigned p)
{
    /* Sealed with the tick: a connected device or script offline, an
     * occupied seat online (genesis_sim_human_mask). */
    return (int)((genesis_sim_human_mask() >> p) & 1u);
}
static int companion_recovering(void)
{
    unsigned state=word(0xF708);
    return state==2 || state==4;
}
static void companion_control(unsigned p, unsigned o)
{
    if (g_ram[o+0x24]!=2) return;
    /* Host-rendered actors never go through BuildSprites' on-screen writer.
     * Supply its camera-relative visibility instead of forcing bit 7 on. */
    int x=(int16_t)(word(o+8)-word(0xEE00));
    int y=(int16_t)(word(o+12)-word(0xEE04));
    int visible=x>=-(int)g_ram[o+0x19] && x<320+g_ram[o+0x19] &&
        y>=-32 && y<256;
    g_ram[o+1]=(g_ram[o+1]&0x7F)|(visible?0x80:0);
    int human=human_companion(p);
    /* Every online human has a camera. Distance from P1 is no longer a
     * reason to start Tails' 300-tick offscreen recovery timer. Actual death
     * and its normal recovery continue through the original controller. */
    if (human && s2_runtime_campaign_online()) g_ram[o+1]|=0x80;
    /* Preserve the party's explicit controller ownership; the CPU must not
     * take over an idle connected controller after the stock ten seconds. */
    if (human) putword(0xF702,600);
    uint16_t index=word(0xEED2),record_x=0;
    unsigned record=0;
    int six=s2_party.roster.slots>4;
    int varied=(p>=2 || six) && !human && word(0xF708)==6 && !g_ram[o+0x2A] &&
        visible && !(g_ram[o+0x22]&0x2E);
    if (varied) {
        /* Retain native history timing, jump retries and acceleration. Only
         * separate nearby grounded followers slightly; a lagging, airborne
         * or recovering companion follows the exact native target. */
        unsigned at=(index-0x44)&255;
        unsigned status=g_ram[0xE402+at];
        unsigned address=0xE500+at;
        int dx=(int16_t)(word(address)-word(o+8));
        int dy=(int16_t)(word(address+2)-word(o+12));
        int range=six?256:64;
        if (!(status&0xDA) && dx>-range && dx<range && dy>-16 && dy<16) {
            /* Native following ignores errors below 16 pixels. Keep offsets
             * just beyond that deadband so a rejoined actor can separate. */
            unsigned gap=six?32+(p-1)*32:24+(p-2)*16;
            int target=word(address)+((status&1)?(int)gap:-(int)gap);
            if (target>=word(0xEEC8)+16 && target<=word(0xEECA)+288) {
                record=address; record_x=word(record); putword(record,target);
            }
        }
    }
    inside_companion_cpu=1;
    native_helper(0x1BAD4); /* TailsCPU_Control: delayed P1 history, safe flight */
    inside_companion_cpu=0;
    if (record) putword(record,record_x);
    if (!human) {
        /* CPU ABC means an ordinary jump. Amy's A is a hammer and Knuckles'
         * repeated airborne jump is a glide, so never synthesize those moves. */
        for (unsigned at=0xF66A;at<=0xF66B;++at)
            g_ram[at]=(g_ram[at]&~0x70)|((g_ram[at]&0x70)?0x10:0);
        if (kind(p)==S2_CHAR_KNUCKLES && (g_ram[o+0x22]&2)) g_ram[0xF66B]&=~0x10;
    }
    if (companion_recovering()) {
        characters[p].special=0;
        if (native_id(p)!=2) g_ram[o+0x1C]=2; /* use this character's roll art */
        if (p>=2) memset(solid_flags[p-2],0,sizeof solid_flags[p-2]);
    }
}
static void spawn_extra(unsigned object, unsigned p)
{
    /* Level start only. Recovery retains the object and uses TailsCPU_Despawn;
     * never reinitialize a live actor at an arbitrary offset inside terrain. */
    memset(g_ram+object,0,0x40);
    g_ram[object]=(uint8_t)native_id(p);
    putword(object+8,word(0xB008));
    putword(object+12,word(0xB00C));
    g_ram[object+0x22]=2;
    s2_character_reset(&characters[p],kind(p));
}
static void reserve_extras(void)
{
    if (!hud_art_ready) hud_art_ready=s2_native_hud_art_decode(g_rom,0x100000,&hud_art);
    memset(objects+2,0,sizeof objects-2*sizeof objects[0]);
    memset(solid_flags,0,sizeof solid_flags); memset(solid_ids,0,sizeof solid_ids);
    /* P1 Tails uses his native body AND Obj05 tail mappings while glowing.
     * Decode ahead of gameplay, not at transformation or on a render thread. */
    if (kind(0)==S2_CHAR_TAILS && !native_banks[1].count) {
        S2DonorLayout l={139,0x739E2,0x7446C,0x64320,0x29E2,8,0,0};
        char error[160];
        if (!s2_donor_decode(g_rom,0x100000,&l,&native_banks[1],error,sizeof error)) {
            fprintf(stderr,"Sonic 2 P1 Tails art: %s\n",error); exit(2);
        }
    }
    if (word(0xFFD8)) return; /* native VS remains exactly two competitors */
    for (unsigned p=2;p<s2_party.roster.slots;++p) {
        if (!native_id(p)) continue;
        unsigned o=0xCFC0;
        while (o>=0xB400 && g_ram[o]) o-=0x40;
        if (o<0xB400) { fprintf(stderr,"Sonic 2 party: no native object slot for P%u\n",p+1); exit(2); }
        objects[p]=o; spawn_extra(o,p);
        unsigned k=kind(p);
        if (k<2 && !native_banks[k].count) {
            S2DonorLayout l=k==0?(S2DonorLayout){214,0x6FBE0,0x714E0,0x50000,0x29E2,8,0,0}:
                (S2DonorLayout){139,0x739E2,0x7446C,0x64320,0x29E2,8,0,0};
            char error[160];
            if (!s2_donor_decode(g_rom,0x100000,&l,&native_banks[k],error,sizeof error)) {
                fprintf(stderr,"Sonic 2 party: native character art: %s\n",error); exit(2);
            }
        }
    }
}
static void runtime_state(S2StateIO *io)
{
    unsigned header[3+S2_MAX_PLAYERS+7]={4,g_ram[0xF600],s2_party.roster.slots};
    header[3+S2_MAX_PLAYERS]=(unsigned)s2_party.amy_enabled;
    header[4+S2_MAX_PLAYERS]=(unsigned)s2_party.s3k_enabled;
    header[5+S2_MAX_PLAYERS]=(unsigned)s2_party.save_menu_enabled;
    header[6+S2_MAX_PLAYERS]=(unsigned)s2_party.mighty_enabled;
    header[7+S2_MAX_PLAYERS]=(unsigned)s2_party.shadow_enabled;
    header[8+S2_MAX_PLAYERS]=(unsigned)s2_party.infinite_lives;
    header[9+S2_MAX_PLAYERS]=(unsigned)s2_party.infinite_time;
    for (unsigned p=0;p<S2_MAX_PLAYERS;++p) header[3+p]=!strcmp(s2_party.roster.character[p],"none")?S2_CHAR_NONE:kind(p);
    if (io->mode) {
        unsigned saved[3+S2_MAX_PLAYERS+7];
        if (!io->data || io->size<sizeof saved) { io->ok=0; return; }
        memcpy(saved,io->data,sizeof saved);
        if (saved[1]!=12 && saved[1]!=16) io->ok=0;
        header[1]=saved[1];
        if (memcmp(saved,header,sizeof saved)) io->ok=0;
    }
    S2_STATE(io,header); S2_STATE(io,active);
    S2_STATE(io,previous_input); S2_STATE(io,displayed); S2_STATE(io,displayed_tails); S2_STATE(io,companion_cpu);
    S2_STATE(io,characters); S2_STATE(io,physics); S2_STATE(io,objects);
    S2_STATE(io,solid_flags); S2_STATE(io,solid_ids); S2_STATE(io,published_sprites);
    S2_STATE(io,dust_emit_age); S2_STATE(io,published_dust);
    s2_save_menu_state(io); s2_video_state(io);
    size_t audio_size=audio_event_state_size();
    if (io->data) {
        if (io->pos>io->size || audio_size>io->size-io->pos) { io->ok=0; return; }
        int ok=io->mode?audio_event_state_load(io->data+io->pos,audio_size,io->mode==2):
            audio_event_state_save(io->data+io->pos,audio_size);
        if (!ok) io->ok=0;
    }
    io->pos+=audio_size;
}
size_t s2_runtime_state_size(void)
{ S2StateIO io={0}; runtime_state(&io); return io.pos; }
int s2_runtime_state_at_boundary(void)
{
    unsigned sp=g_cpu.A[7]&65535,mode=g_ram[0xF600];
    if (sp>65532 || inside_player || inside_solid || inside_companion_cpu) return 0;
    unsigned caller=((unsigned)word(sp)<<16)|word(sp+2);
    /* Byte-matched REV01 listing: Level_MainLoop, both half-pipe loops,
     * and Pause_Loop. Fades/loading/results intentionally do not qualify. */
    if (mode==12 && g_ram[0xF711]) return caller==0x436E || caller==0x13BC;
    if (mode==16) return caller==0x13BC ||
        (g_ram[0xF62A]==0x0A && (caller==0x5220 || caller==0x5268));
    return 0;
}
int s2_runtime_state_save(void *data,size_t size)
{
    /* Host C stacks are not portable snapshots. Only established native
     * per-frame gameplay loops have source-grounded resume entry points. */
    unsigned mode=g_ram[0xF600];
    if ((mode!=12 && mode!=16) || (mode==12 && !g_ram[0xF711]) ||
        inside_player || inside_solid || inside_companion_cpu || size!=s2_runtime_state_size()) {
        fprintf(stderr,"[SAVE] scene=%02X ready=%u player=%d solid=%d cpu=%d pc=%06X\n",
            mode,g_ram[0xF711],inside_player,inside_solid,inside_companion_cpu,g_cpu.PC);
        return 0;
    }
    S2StateIO io={data,size,0,0,1}; runtime_state(&io); return io.ok && io.pos==size;
}
int s2_runtime_state_load(const void *data,size_t size,int apply)
{
    if (size!=s2_runtime_state_size()) return 0;
    S2StateIO io={(uint8_t *)data,size,0,1,1}; runtime_state(&io);
    if (!io.ok || io.pos!=size) return 0;
    /* Rebind ROM-derived art for a load in a fresh process. These immutable
     * caches are not saved, nor can a state enable an unverified donor. */
    for (unsigned p=0;p<S2_MAX_PLAYERS;++p) {
        const S2Character *c=s2_character_find(s2_party.roster.character[p]);
        if (c && !c->available()) return 0;
        unsigned k=kind(p);
        if ((p<2 && !(p==0 && k==S2_CHAR_TAILS)) || k>=2 || native_banks[k].count) continue;
        S2DonorLayout l=k==0?(S2DonorLayout){214,0x6FBE0,0x714E0,0x50000,0x29E2,8,0,0}:
            (S2DonorLayout){139,0x739E2,0x7446C,0x64320,0x29E2,8,0,0};
        char error[160];
        if (!s2_donor_decode(g_rom,0x100000,&l,&native_banks[k],error,sizeof error)) return 0;
    }
    if (apply) {
        io.pos=0; io.mode=2; runtime_state(&io);
        inside_player=inside_solid=inside_companion_cpu=0; actor=-1;
        gvdp_set_unlimited_sprites(active);
        s2_video_prepare_knuckles_items();
    }
    return io.ok;
}
static uint32_t solid_single(uint32_t pc)
{
    switch (pc) {
    case 0xF872: return 0xF88C; /* Obj11's log-height collision, separate from generic platforms */
    case 0x19718: return 0x19736;
    case 0x19778: return 0x1978E;
    case 0x197D0: return 0x197E6;
    case 0x19880: return 0x19896;
    case 0x19C32: return 0x19C48;
    case 0x19C8A: return 0x19CA0;
    case 0x19CE2: return 0x19CF8;
    default: return 0;
    }
}
static int player_object(unsigned at)
{
    for (unsigned p=0;p<S2_MAX_PLAYERS;++p) if (objects[p]==at) return 1;
    return 0;
}
static void solid_begin(unsigned p, uint8_t saved[144])
{
    for (unsigned n=0;n<144;++n) {
        unsigned at=0xB000+n*0x40;
        saved[n]=g_ram[at+0x22]&0x50;
        if (!player_object(at)) g_ram[at+0x22]=(g_ram[at+0x22]&~0x50)|solid_flags[p-2][n];
    }
}
static void solid_end(unsigned p, const uint8_t saved[144])
{
    for (unsigned n=0;n<144;++n) {
        unsigned at=0xB000+n*0x40;
        if (player_object(at)) continue;
        solid_flags[p-2][n]=g_ram[at+0x22]&0x50;
        g_ram[at+0x22]=(g_ram[at+0x22]&~0x50)|saved[n];
    }
}
static int extra_players(void)
{
    for (unsigned p=2;p<S2_MAX_PLAYERS;++p) if (objects[p]) return 1;
    return 0;
}
static int collide_extras(uint32_t pc, uint32_t single)
{
    if (inside_solid || !extra_players()) return 0;
    M68KState input=g_cpu;
    inside_solid=1; recomp_call_addr(pc); inside_solid=0;
    M68KState result=g_cpu;
    /* Retain the original P1/P2 result registers: native spring/platform code
     * that reads them must not mistake an extra player for P1 or P2. */
    for (unsigned p=2;p<S2_MAX_PLAYERS;++p) {
        unsigned o=objects[p];
        if (!o || !g_ram[o] || g_ram[o+0x24]>=6) continue;
        uint8_t saved[144];
        solid_begin(p,saved);
        g_cpu=input; g_cpu.A[1]=0xFFFF0000u|o; g_cpu.D[6]=4;
        unsigned platform=input.A[0]&0xFFFF;
        uint8_t log=g_ram[platform+0x3B];
        if (pc==0xF872) g_cpu.D[5]=0x3B;
        host_call(single);
        if (pc==0xF872) g_ram[platform+0x3B]=log; /* native Sidekick's log index */
        solid_end(p,saved);
    }
    g_cpu=result; return 1;
}
static int bridge_extra_weight(uint32_t pc)
{
    unsigned o=g_cpu.A[0]&0xFFFF;
    if (!extra_players() || o<0xB400 || o>=0xD000 || (o&63) ||
        g_ram[o]!=0x11 || (g_ram[o+0x22]&0x18)) return 0;
    /* Preserve the source P1/P2 bend whenever either stands. With only extra
     * actors, their stored contacts supply the same envelope and log target.
     * Native Obj11_Depress still computes every actual segment height. */
    unsigned sum=0,count=0,n=(o-0xB000)/64;
    int half=g_ram[o+0x28]*8+8;
    for (unsigned p=2;p<S2_MAX_PLAYERS;++p) {
        if (!objects[p] || !(solid_flags[p-2][n]&0x10)) continue;
        int dx=(int16_t)(word(objects[p]+8)-word(o+8))+half;
        if (dx<0 || dx>=half*2-16) continue;
        sum+=(unsigned)dx>>4; ++count;
    }
    if (!count) return 0;
    unsigned target=sum/count;
    if (g_ram[o+0x3F]<target) ++g_ram[o+0x3F];
    else if (g_ram[o+0x3F]>target) --g_ram[o+0x3F];
    if (g_ram[o+0x3E]<0x40) g_ram[o+0x3E]+=4;
    native_helper(0xF9E8);
    recomp_tail_call(pc==0xF77A?0xF7BC:0xF852);
    return 1;
}
static int spring_extras(uint32_t pc)
{
    if (s_inside_spring || !extra_players()) return 0;
    M68KState input=g_cpu;
    unsigned spring=input.A[0]&0xFFFF;
    s_inside_spring=1; recomp_call_addr(pc); s_inside_spring=0;
    M68KState result=g_cpu;
    unsigned routine=g_ram[spring+0x24];
    if (g_ram[spring]!=0x41 || routine<2 || routine>10) return 1;
    for (unsigned p=2;p<S2_MAX_PLAYERS;++p) {
        unsigned o=objects[p]; if (!o || !g_ram[o] || g_ram[o+0x24]>=6) continue;
        uint8_t saved[144]; solid_begin(p,saved);
        g_cpu=input; g_cpu.A[1]=0xFFFF0000u|o; g_cpu.D[6]=4;
        g_cpu.D[1]=routine==4?0x13:0x1B;
        g_cpu.D[2]=routine==4?0xE:routine>=8?0x10:8;
        g_cpu.D[3]=routine==4?0xF:0x10; g_cpu.D[4]=word(spring+8);
        if (routine>=8) g_cpu.A[2]=routine==8?0x18FAA:0x18FC6;
        M68KState collision=native_helper(routine>=8?0x197E6:0x1978E);
        uint32_t response=0;
        if (routine==2 && (g_ram[spring+0x22]&0x10)) response=0x189CA;
        else if (routine==4 && (g_ram[spring+0x22]&0x40)) {
            unsigned facing=g_ram[spring+0x22]&1;
            if (word(spring+8)>=word(o+8)) facing^=1;
            if (!facing) response=0x18AEE;
        } else if (routine==6 && (int16_t)collision.D[4]==-2) response=0x18CC6;
        else if (routine==8 && (g_ram[spring+0x22]&0x10)) response=0x18DB4;
        else if (routine==10 && (int16_t)collision.D[4]==-2) response=0x18EE6;
        if (response) {
            g_cpu=input; g_cpu.A[1]=0xFFFF0000u|o; host_call(response);
            characters[p].special=0; /* a host spring takes over the trajectory */
        }
        solid_end(p,saved);
    }
    g_cpu=result; return 1;
}
static int update_player(unsigned p, uint32_t entry)
{
    unsigned object=g_cpu.A[0]&0xFFFF;
    if (inside_player) return 0;
    int companion=p && !word(0xFFD8);
    int initializing=!g_ram[object+0x24];
    unsigned previous_special=characters[p].special;
    displayed[p]=0;
    M68KState cpu=g_cpu;
    uint8_t inputs[12]; memcpy(inputs,g_ram+0xF600,sizeof inputs);
    uint16_t mode=word(0xFF70), bias=word(0xEED8);
    uint16_t record_index=word(0xEED2);
    uint8_t history[512]; memcpy(history,g_ram+0xE400,sizeof history);
    /* Native main-player Tails also records into the sidekick's E600 ring.
     * Keep that transient write private: P1 publishes E500 below, while the
     * actual P2 must retain its own ring/index rather than alternate owners. */
    int preserve_sidekick_history=!p && native_id(p)==2 && objects[1] &&
        s2_party.roster.slots>1 && strcmp(s2_party.roster.character[1],"none");
    uint8_t sidekick_history[256];uint16_t sidekick_index=word(0xEED6);
    if(preserve_sidekick_history)memcpy(sidekick_history,g_ram+0xE600,sizeof sidekick_history);
    uint16_t logical2=word(0xF66A);
    uint8_t locked=g_ram[0xF7CC];
    uint8_t super=g_ram[0xFE19];
    uint8_t original_physics[6]; memcpy(original_physics,g_ram+0xF760,6);
    uint8_t original_dust[64]; memcpy(original_dust,g_ram+0xD100,64);
    uint8_t boundaries[8]; memcpy(boundaries,g_ram+0xEEC8,8);
    uint8_t object_control=g_ram[object+0x2A];
    int injected_control=0;
    /* Sonic's initialization writes P1 checkpoint records, even at Sidekick.
     * Keep those fields owned by the real P1 when a Sonic is a companion. */
    uint8_t checkpoint[16]; memcpy(checkpoint,g_ram+0xFE30,sizeof checkpoint);
    uint8_t saved_cpu[14]; memcpy(saved_cpu,g_ram+0xF702,sizeof saved_cpu);
    if (companion) memcpy(g_ram+0xF702,companion_cpu[p],sizeof saved_cpu);
    actor=(int)p; inside_player=1;
    if (!p) displayed_tails=0;
    if (p) g_ram[0xFE19]=0; /* Super Sonic's global state belongs only to P1. */
    if (p) memcpy(g_ram+0xF760,physics[p],6);
    if (p && word(0xFFD8)) memcpy(g_ram+0xEEC8,g_ram+0xEEF8,8);
    if (p) {
        /* The tick's SEALED pad (runner/sim_step.h): the live map and script
         * offline, the published row online. Never a live device read. */
        uint8_t held=word(0xFFD8)?g_ram[0xF606]:(uint8_t)genesis_sim_pad((int)p);
        uint8_t pressed=(uint8_t)(held&~previous_input[p]); previous_input[p]=held;
        g_ram[0xF604]=g_ram[0xF602]=g_ram[0xF606]=g_ram[0xF66A]=held;
        g_ram[0xF605]=g_ram[0xF603]=g_ram[0xF607]=g_ram[0xF66B]=pressed;
        g_ram[0xF7CC]=g_ram[0xF7CD];
        if (companion) {
            companion_control(p,object);
            g_ram[0xF604]=g_ram[0xF602]=g_ram[0xF606]=g_ram[0xF66A];
            g_ram[0xF605]=g_ram[0xF603]=g_ram[0xF607]=g_ram[0xF66B];
        }
    }
    /* Native Tails already has a fully supported main-player path. */
    putword(0xFF70, p==0 && native_id(p)==2 ? 2 : 0);
    if (!p && kind(0)!=S2_CHAR_SONIC && g_ram[0xFE19] && g_ram[object+0x24]>=4) {
        uint8_t timer=g_ram[0xFE1E]; g_ram[0xFE1E]=0;
        native_helper(0x1ABA6); g_ram[0xFE1E]=timer;
        if (kind(0)==S2_CHAR_TAILS) memcpy(g_ram+0xFEC0,g_ram+0xF760,6);
        if (g_ram[object+0x2A]==0x81) g_ram[object+0x2A]=0;
    }
    if (kind(p)==S2_CHAR_SHADOW && !p && g_ram[object+0x24]==2 &&
        (g_ram[object+0x22]&2) && (g_ram[0xF604]&0x30)==0x30 && (g_ram[0xF605]&0x30)) {
        if (g_ram[0xFE19]) native_helper(0x1ABF2);
        else if (g_ram[0xFFB1]==7 && word(0xFE20)>=50 && g_ram[0xFE1E]) {
            putword(0xF65C,0); s_transforming=1; native_helper(0x1AB38); s_transforming=0;
        }
    }
    if (imported() && g_ram[object+0x24]==2) {
        S2Motion m=motion(object); S2Contacts c=contacts(object);
        shadow_contacts(object,&c);
        unsigned input=g_ram[0xF7CC]?0xF602:0xF604;
        s2_character_before(&characters[p],&m,g_ram[input],g_ram[input+1],&c);
        store_motion(object,&m);
        shadow_effects(p,object,&m);
        g_ram[0xF604]=g_ram[0xF602]=characters[p].native_held;
        g_ram[0xF605]=g_ram[0xF603]=characters[p].native_pressed;
        if (characters[p].move!=S2_MOVE_NATIVE) {
            if (characters[p].move!=S2_MOVE_FIXED) {
                host_call(0x163AC); /* ObjectMove, no gravity */
                host_call(characters[p].move==S2_MOVE_AIR?0x1AEAA:0x1E234);
            }
            host_call(0x1A974); /* LevelBound remains authoritative */
            g_ram[object+0x2A]|=1;
            injected_control=!(object_control&1);
        }
    }
    recomp_call_addr(entry);
    if (p==1 && native_id(p)!=2) {
        /* Native sidekick stars follow Tails' position ring, regardless of
         * the sidekick's character ID. Sonic-based companions otherwise
         * publish only the temporary Sonic ring, leaving the stars at (0,0). */
        if (initializing) {
            putword(0xEED6,0);
            for (unsigned at=0;at<256;at+=4) {
                putword(0xE600+at,word(object+8)); putword(0xE602+at,word(object+12));
            }
        }
        native_helper(0x1BF38);
    }
    if (!p && native_id(0)==2) {
        /* Native Tails writes his own position ring. As leader he must also
         * publish the Sonic history consumed by every companion's P2 AI. */
        if (initializing) {
            memset(g_ram+0xE400,0,256);
            for (unsigned at=0;at<256;at+=4) {
                putword(0xE500+at,word(object+8)); putword(0xE502+at,word(object+12));
                g_ram[0xE402+at]=g_ram[object+0x22];
            }
            putword(0xEED2,0);
        }
        native_helper(0x1A15C);
        if(preserve_sidekick_history) {
            memcpy(g_ram+0xE600,sidekick_history,sizeof sidekick_history);
            putword(0xEED6,sidekick_index);
        }
    }
    if (imported()) {
        if (injected_control) g_ram[object+0x2A]&=~1;
        memcpy(g_ram+0xF602,inputs+2,10);
    }
    inside_player=0; actor=-1;
    if (companion) {
        memcpy(companion_cpu[p],g_ram+0xF702,sizeof saved_cpu);
        /* World objects referring to native P2 still see P2's AI state. */
        if (p!=1) memcpy(g_ram+0xF702,saved_cpu,sizeof saved_cpu);
    }
    if (p) {
        /* Preserve Game_Mode changes, but not companion copies of pad state. */
        g_ram[0xFE19]=super;
        memcpy(g_ram+0xF602,inputs+2,10); putword(0xF66A,logical2);
        putword(0xEED8,bias); g_ram[0xF7CC]=locked;
        memcpy(g_ram+0xFE30,checkpoint,sizeof checkpoint);
        memcpy(g_ram+0xE400,history,sizeof history); putword(0xEED2,record_index);
        memcpy(physics[p],g_ram+0xF760,6); memcpy(g_ram+0xF760,original_physics,6);
        memcpy(g_ram+0xD100,original_dust,64);
        memcpy(g_ram+0xEEC8,boundaries,8);
    }
    if (previous_special!=S2_SK_SLIDE && characters[p].special==S2_SK_SLIDE) dust_emit_age[p]=~0u;
    if (p<2 && kind(p)==S2_CHAR_KNUCKLES && previous_special!=S2_SK_SLIDE &&
        characters[p].special==S2_SK_SLIDE && g_ram[object+0x28]>=12) {
        /* KiS2 Knuckles_BeginSlide: reuse Obj08's native skid emitter/art.
         * Never take the drowning countdown's shared dust VRAM. */
        unsigned dust=p?0xD140:0xD100;
        if (g_ram[dust]==8 && g_ram[dust+0x24]==2) {
            g_ram[dust+0x24]=6; g_ram[dust+0x1A]=0x15;
            g_ram[dust+0x34]=0; /* Knuckles' height, including the native P2 role */
            putword(dust+0x3E,object);
        }
    }
    putword(0xFF70,mode);
    g_cpu=cpu;
    return 1;
}
int s2_runtime_hook(uint32_t pc)
{
    if (pc==0xC680 && (g_ram[0xF600]&127)==12 && !g_ram[0xFE10] &&
        !word(0xFFD8) && !s2_roster_is_classic(&s2_party.roster)) {
        /* KiS2 Deform_EHZ appends the missing last two H-scroll rows using
         * the final D4/D3 pair. Stock REV01 returns here after only 222 rows.
         * Preserve only the original solo Sonic/Tails and native Sonic+Tails
         * roles. Validated custom/expanded parties use the corrected rows,
         * independent of which actor leads. Continue the real native RTS. */
        unsigned at=g_cpu.A[1]&65535u;
        if (at<=65528u) {
            for (unsigned n=0;n<2;++n) {
                putword(at+n*4,g_cpu.D[4]); putword(at+n*4+2,g_cpu.D[3]);
            }
            g_cpu.A[1]+=8;
            unsigned last=(uint16_t)g_cpu.D[3];
            g_cpu.SR=(g_cpu.SR&~(SR_N|SR_Z|SR_V|SR_C))|
                (!last?SR_Z:0)|((last&0x8000u)?SR_N:0);
        }
    }
    if(pc==0x1DAAC && level()) {
        /* Obj35 has already checked the parent's invincibility flag. Its
         * original Player_mode==2 shortcut selects E600 for every star when
         * Tails leads. Select by actual party role instead, without changing
         * Player_mode or bypassing the native animation/deletion code. */
        unsigned parent=word((g_cpu.A[0]&65535u)+0x3E);
        for(unsigned p=0;p<2;++p)if(objects[p] && objects[p]==parent) {
            g_cpu.A[5]=p?0xFFFFEED6u:0xFFFFEED2u;
            g_cpu.A[6]=p?0xFFFFE600u:0xFFFFE500u;
            break;
        }
        return 0;
    }
    /* REV01 native decrement/clock compares. Leave the original instruction,
     * flags, death/retry and HUD paths in charge; no patched ROM bytes. */
    if (pc==0x1B248 || pc==0x1CCA6) {
        unsigned at=pc==0x1B248?0xFE12:0xFEC6;
        if (s2_party.infinite_lives && g_ram[at] && g_ram[at]<255) ++g_ram[at];
        return 0;
    }
    if (pc==0x40DEA || pc==0x40F22 || pc==0x40F62 || pc==0x40F9A) {
        unsigned at=pc==0x40F9A?0xFED2:0xFE22;
        if (s2_party.infinite_time && !g_ram[at] && g_ram[at+1]==9 &&
            g_ram[at+2]==59 && g_ram[at+3]==59) g_ram[at+3]=58;
        return 0;
    }
    /* Online too: the party runtime reads only sealed inputs and every piece
     * of its state is in the rollback snapshot (s2_runtime_rb_save). */
    if (pc==0x157C4) {
        /* Original KiS2 LoadTitleCard0 fills two background tiles with ink 4.
         * sonicretro/s2disasm c336fede, ArtTile_ArtNem_TitleCard+$5A.
         * Keep the native card mappings/palette, and the stock attract demos.
         * This runs after NemDec and before the caller loads the letters. */
        if ((g_ram[0xF600]&0x7F)==12 && kind(0)==S2_CHAR_KNUCKLES && knuckles_available()) {
            glue_poke32(0xC00004,0x7B400002u); /* VRAM write $BB40 */
            for (unsigned i=0;i<16;++i) glue_poke32(0xC00000,0x44444444u);
        }
        return 0;
    }
    if (pc==0x4F64) {
        s2_video_hook(pc);
        /* Stock solo roles and the original Amy/Knuckles use the native solo
         * half-pipe controller. The campaign roster survives the round trip;
         * unsupported imported half-pipe roles still use the stock duo. */
        inside_player=0; actor=-1;
        gvdp_set_unlimited_sprites(0);
        memset(objects+2,0,sizeof objects-2*sizeof objects[0]);
        unsigned primary=kind(0),solo=s2_party.roster.slots==1;
        putword(0xFF70,solo && primary==S2_CHAR_TAILS?2:
            solo && (primary==S2_CHAR_SONIC || primary==S2_CHAR_KNUCKLES || primary==S2_CHAR_AMY)?1:0);
        return 0;
    }
    if (pc==0x4450) { /* Level_SetPlayerMode: attract demos remain byte-identical */
        s2_video_hook(pc); /* title-card initialization installs a fresh palette directly */
        active=g_ram[0xF600]!=0x88 && (!vanilla() || s2_runtime_campaign_online());
        gvdp_set_unlimited_sprites(active);
        memset(published_sprites,0,sizeof published_sprites);
        memset(published_dust,0,sizeof published_dust);
        memset(dust_emit_age,0xFF,sizeof dust_emit_age);
        if (!active) return 0;
        s2_roster_validate(&s2_party.roster);
        memset(previous_input,0,sizeof previous_input); memset(companion_cpu,0,sizeof companion_cpu);
        memset(displayed,0,sizeof displayed);
        for (unsigned p=0;p<S2_MAX_PLAYERS;++p) {
            s2_character_reset(&characters[p],kind(p));
            const uint8_t defaults[]={6,0,0,12,0,128}; memcpy(physics[p],defaults,6);
        }
        putword(0xFF70,native_id(0)==2?2:native_id(1)?0:1);
        return 1;
    }
    if (!level()) return 0;
    if (pc==0x12768 && !word(0xFFD8) && s2_party.roster.slots>1 && objects[1] &&
        g_ram[objects[1]] && (g_cpu.A[1]&65535u)==objects[1] &&
        (kind(1)!=S2_CHAR_TAILS || human_companion(1))) {
        /* Stock single-player Tails is always solid against a monitor, even
         * while rolling. Party/human P2 must reach the same rolling hit path
         * as P1; retain the stock CPU Tails contract and native VS path. */
        recomp_tail_call(0x12756); return 1;
    }
    if (pc==0x1DE4A || pc==0x1DE88) {
        unsigned parent=pc==0x1DE4A?word((g_cpu.A[0]&65535)+0x3E):g_cpu.A[2]&65535;
        for (unsigned p=0;p<S2_MAX_PLAYERS;++p) if (objects[p]==parent && kind(p)==S2_CHAR_KNUCKLES &&
            characters[p].special==S2_SK_SLIDE && !(g_ram[parent+0x22]&2)) {
            if (pc==0x1DE4A) {
                /* Original KiS2 CheckSkid also accepts slide state 3. */
                g_cpu.A[2]=0xFFFF0000u|parent;
                recomp_tail_call(0x1DE64); return 1;
            }
            /* Native ADDI.W #16,y(a1) follows. KiS2 uses six pixels for
             * glide slides and retains sixteen for ordinary braking. */
            unsigned child=g_cpu.A[1]&65535;
            putword(child+12,word(child+12)-10); break;
        }
        return 0;
    }
    if ((pc==0xF77A || pc==0xF810) && bridge_extra_weight(pc)) return 1;
    if (pc==0x213E && custom_super_palette()) {
        if (s_inside_palette) return 0;
        /* Native cycle owns timing AND releases transform control at $2168.
         * Its blue shades belong to Sonic, never to a companion sharing CRAM. */
        uint8_t normal[8],water[8];
        memcpy(normal,g_ram+0xFB04,8); memcpy(water,g_ram+0xF084,8);
        s_inside_palette=1; recomp_call_addr(pc); s_inside_palette=0;
        memcpy(g_ram+0xFB04,normal,8); memcpy(g_ram+0xF084,water,8);
        if (!g_ram[0xF65F]) putword(0xF65C,0); /* REV01 byte-clear leaves $F8 */
        return 1;
    }
    if (inside_player && actor==0 && kind(0)==S2_CHAR_TAILS && !word(0xFFD8)) {
        unsigned o=g_cpu.A[0]&65535;
        if (pc==0x1C6F6 && g_ram[o+0x3C] && !g_ram[o+0x12])
            native_helper(0x1AB38); /* same apex check as Sonic_JumpHeight */
        if (pc==0x1C644 && g_ram[0xFE19]) g_cpu.D[2]=(g_cpu.D[2]&0xFFFF0000)|0x800;
        if (pc==0x1BA14) { /* immediately after Tails_Display, like Sonic_Super */
            unsigned was_super=g_ram[0xFE19];
            native_helper(0x1ABA6);
            if (was_super && !g_ram[0xFE19]) memcpy(g_ram+0xFEC0,g_ram+0xF760,6);
        }
        if (pc==0x1BA1C && g_ram[0xFE19]) { /* after native water/shoes updates */
            unsigned water=!!(g_ram[o+0x22]&64);
            putword(0xFEC0,water?0x500:0xA00);
            putword(0xFEC2,water?0x18:0x30); putword(0xFEC4,water?0x80:0x100);
        }
    }
    if ((pc==0x19FE6 || pc==0x1B96E) && inside_player && actor>0 && !word(0xFFD8)) {
        /* Obj01/02_Init_Continued, before the first movement/collision query.
         * Obj01 skips art/plane initialization at checkpoints; Obj79_LoadData
         * restores only MainCharacter. Every companion spawns in P1's current
         * plane, including catch-up on a secondary layer with no checkpoint.
         * Doing this at the init tail also avoids native fresh-start defaults
         * overwriting a plane that was seeded before Obj01_Init. */
        unsigned o=g_cpu.A[0]&0xFFFF;
        putword(o+0x3E,word(0xB03E));
        putword(o+2,(native_id((unsigned)actor)==2?0x7A0:0x780)|(word(0xB002)&0x8000));
        return 0;
    }
    if (pc==0x12A5C && !word(0xFFD8)) {
        unsigned target=g_cpu.A[1]&0xFFFF, p=0;
        while (p<S2_MAX_PLAYERS && objects[p]!=target) ++p;
        if (p<S2_MAX_PLAYERS && kind(p)==S2_CHAR_SHADOW) {
            /* S2 discards this reward while Super. Retain a real monitor's
             * 20-second S-power for v3 Chaos Blast, without expiring Super's
             * permanent invincibility or replacing its music/stars. */
            characters[p].shadow_monitor_ticks=20*60;
            fprintf(stderr,"[Shadow] P%u acquired S-monitor (%u ticks)\n",p+1,characters[p].shadow_monitor_ticks);
            if (!p && g_ram[0xFE19]) {
                unsigned counter=g_cpu.A[2]&0xFFFF;
                putword(counter,word(counter)+1);
                return 1;
            }
        }
    }
    if ((pc==0x1296A || pc==0x129E0 || pc==0x12A2C) && !word(0xFFD8)) {
        if (s_reward) return 0;
        unsigned target=g_cpu.A[1]&0xFFFF, p=0;
        while (p<S2_MAX_PLAYERS && objects[p]!=target) ++p;
        if (p==S2_MAX_PLAYERS || !p) return 0;
        uint32_t parent=g_cpu.A[1]; uint8_t tails_physics[6], shield[64];
        memcpy(tails_physics,g_ram+0xFEC0,6); memcpy(shield,g_ram+0xD1C0,64);
        if (pc==0x1296A) g_cpu.A[1]=0xFFFFB000u; /* shared campaign ring/life pool */
        s_reward=1; recomp_call_addr(pc); s_reward=0; g_cpu.A[1]=parent;
        if (pc==0x129E0 && native_id(p)!=2) {
            memcpy(physics[p],g_ram+0xFEC0,6); memcpy(g_ram+0xFEC0,tails_physics,6);
        }
        if (pc==0x12A2C && p>=2) {
            unsigned free_slot=0;
            for (unsigned at=0xB400;at<0xD000;at+=64) {
                if (g_ram[at]==0x38 && word(at+0x3E)==target) { free_slot=at; break; }
                if (!g_ram[at] && !free_slot) free_slot=at;
            }
            if (free_slot) memcpy(g_ram+free_slot,g_ram+0xD1C0,64);
            memcpy(g_ram+0xD1C0,shield,64);
        }
        return 1;
    }
    if (pc==0x15F9C) {
        for (unsigned n=0;n<144;++n) {
            unsigned id=g_ram[0xB000+n*0x40];
            if (id!=solid_ids[n]) { solid_flags[0][n]=solid_flags[1][n]=0; solid_ids[n]=(uint8_t)id; }
        }
        return 0;
    }
    uint32_t single=solid_single(pc);
    if (single) return collide_extras(pc,single);
    if (pc==0x18888) return spring_extras(pc);
    if (pc==0x189CA || pc==0x18AEE || pc==0x18CC6 || pc==0x18DB4 || pc==0x18EE6) {
        for (unsigned p=0;p<S2_MAX_PLAYERS;++p) if (objects[p] && objects[p]==(g_cpu.A[1]&0xFFFF))
            characters[p].special=0;
        return 0;
    }
    if (pc==0x446E) { /* InitPlayers: host role is distinct from character ID */
        g_ram[0xB000]=(uint8_t)native_id(0); g_ram[0xD100]=8;
        unsigned p2=s2_party.roster.slots>1?native_id(1):0;
        g_ram[0xB040]=(uint8_t)p2;
        if (p2) {
            putword(0xB048,word(0xB008)-32); putword(0xB04C,word(0xB00C)+4);
            g_ram[0xD140]=8;
        }
        if (native_id(0)==2) putword(0xB00C,word(0xB00C)+4);
        reserve_extras();
        return 1;
    }
    if (pc==0x19F50 || pc==0x1B8A4) {
        unsigned address=g_cpu.A[0]&0xFFFF;
        for (unsigned p=0;p<S2_MAX_PLAYERS;++p) if (objects[p] && address==objects[p]) return update_player(p,pc);
        return 0;
    }
    if (pc==0x1BAD4) return inside_player && actor>0 && !inside_companion_cpu;
    if ((pc==0x1B21C || pc==0x1CC6C) && inside_player && actor>0 && !word(0xFFD8)) {
        /* Obj02_CheckGameOver waits for the corpse to leave the level, then
         * enters TailsCPU_Despawn without spending P1's lives or locking P1's
         * camera. Apply it to Sonic/Amy/Knuckles companions as well. */
        if (s_checking_death) return 0;
        uint16_t max_y=word(0xEEFE);
        uint8_t scroll_lock=g_ram[0xEEBF];
        putword(0xEEFE,word(0xEECE));
        s_checking_death=1; native_helper(0x1CC6C); s_checking_death=0;
        putword(0xEEFE,max_y); g_ram[0xEEBF]=scroll_lock;
        if (companion_recovering() && native_id((unsigned)actor)!=2) g_ram[(g_cpu.A[0]&0xFFFF)+0x1C]=2;
        return 1;
    }
    if (pc==0x1ABA6 || pc==0x1AB38) {
        if (!inside_player) return 0;
        if (actor>0 || word(0xFFD8)) return 1; /* never companions or VS */
        if (pc==0x1AB38 && kind(0)==S2_CHAR_SHADOW && !s_transforming) return 1;
        if (pc==0x1AB38 && kind(0)!=S2_CHAR_SONIC) {
            if (s_transforming) return 0;
            if (g_ram[0xFE19] || g_ram[0xFFB1]!=7 || word(0xFE20)<50 || !g_ram[0xFE1E]) return 1;
            putword(0xF65C,0); /* source cycle starts at the first fade frame */
            s_transforming=1; recomp_call_addr(pc); s_transforming=0;
            if (g_ram[0xFE19]) {
                characters[0].special=0;
                /* Imported donors have their own $1F transformation script.
                 * S2 Tails does not: retain his rolling pose, never index it
                 * as Super Sonic art or his unrelated $1F run animation. */
                if (kind(0)==S2_CHAR_TAILS) {
                    g_ram[0xB01C]=2; memcpy(g_ram+0xFEC0,g_ram+0xF760,6);
                }
            }
            return 1;
        }
        return 0; /* native eligibility, power-up, countdown and ring drain */
    }
    if (pc==0x164F4 && !inside_player && kind(0)==S2_CHAR_TAILS && custom_super_palette() &&
        (g_cpu.A[0]&65535)==0xD000 && g_ram[0xD000]==5 && word(0xD03E)==0xB000) {
        displayed_tails=1; return 1;
    }
    if (pc==0x164F4 && !inside_player) {
        unsigned o=g_cpu.A[0]&65535;
        if (g_ram[o]==8 && g_ram[o+0x2E]==0x47) return 1;
        /* RunObjectDisplayOnly freezes the dynamic pool during P1 death.
         * Its submissions bypass update_player, but donor/extra frames must
         * still never enter native Sonic mappings or shared VRAM slots. */
        for (unsigned p=0;p<S2_MAX_PLAYERS;++p) if (objects[p]==(g_cpu.A[0]&65535) &&
            (p>=2 || kind(p)>=S2_CHAR_AMY)) { displayed[p]=1; return 1; }
    }
    if (pc==0x164F4 && inside_player) {
        if ((g_cpu.A[0]&0xFFFF)==objects[actor]) {
            if (actor>0 && !word(0xFFD8) && word(0xF708)==2) return 1;
            displayed[actor]=actor==0 && kind(0)==S2_CHAR_TAILS && custom_super_palette()?2:1;
        }
        if (actor>=2 || kind((unsigned)actor)>=S2_CHAR_AMY || (actor==0 && displayed[0]==2)) return 1;
    }
    if (pc==0x3F73C && inside_player) {
        unsigned o=g_cpu.A[0]&0xFFFF, monitor=g_cpu.A[1]&0xFFFF;
        unsigned animation=characters[actor].animation;
        int hammer=imported() && kind(actor)==S2_CHAR_AMY && animation>=0x23 && animation<=0x25;
        if (hammer || (actor>0 && !word(0xFFD8) && (int16_t)word(o+0x12)>=0)) {
            /* Stock single-player companions cannot break monitors. Local
             * party actors can, retaining a stable native parent for rewards.
             * Amy $41B78/$41C14 also permits an upward-moving hammer strike. */
            if (hammer || g_ram[o+0x1C]==2) {
                if ((int16_t)word(o+0x12)>0) putword(o+0x12,-(int16_t)word(o+0x12));
                g_ram[monitor+0x24]=4; putword(monitor+0x3E,o);
            }
            return 1;
        }
    }
    if (pc==0x1B848 && inside_player && kind((unsigned)actor)==S2_CHAR_MIGHTY) return 1;
    if (!inside_player || !imported()) return 0;
    unsigned o=g_cpu.A[0]&0xFFFF;
    if(pc==0x3F7C8 && kind(actor)==S2_CHAR_KNUCKLES &&
        characters[actor].special==S2_SK_GLIDE && g_ram[((g_cpu.A[1]&65535u)+0x21)&65535u]) {
        /* KiS2 Touch_Enemy_Part2: retain native damage/reversal, then fall
         * instead of having the next glide tick overwrite that rebound.
         * Zero-HP enemies and multi-sprite bosses keep their paths. */
        characters[actor].special=S2_SK_FALL;
        g_ram[o+0x1C]=0x21;g_ram[o+0x16]=19;g_ram[o+0x17]=9;
        return 0;
    }
    if (pc==0x1AF2C && kind(actor)==S2_CHAR_KNUCKLES &&
        (characters[actor].special==S2_SK_GLIDE ||
         characters[actor].special==S2_SK_FALL) && (int16_t)g_cpu.D[1]<0) {
        /* KiS2 Knuckles_DoLevelCollision2 accepts negative floor distance
         * without Sonic's velocity-based penetration rejection. Preserve
         * the real distance/angle and let native landing continue; only the
         * second reject comparison is made equal, including wrapped speed
         * bytes. D0 is otherwise unused by the native landing path. */
        g_cpu.D[0]=(g_cpu.D[0]&0xFFFFFF00u)|(g_cpu.D[2]&0xFFu);
        return 0;
    }
    if (pc==0x3F5A0 && kind(actor)==S2_CHAR_KNUCKLES &&
        g_ram[o+0x1A]==0x9C && (g_cpu.A[1]&0xFFFF)==0xB400) {
        /* KiS2 TouchResponse recognizes Knuckles' final duck frame $9C,
         * rather than Sonic's $4D. Apply its reduced box once, before the
         * original object loop; native collision and damage still decide. */
        g_cpu.D[3]=(g_cpu.D[3]&0xFFFF0000u)|(uint16_t)(g_cpu.D[3]+12);
        g_cpu.D[5]=20;
        return 0;
    }
    if ((pc==0x3F5A0 || pc==0x3F69C) && kind(actor)==S2_CHAR_AMY && characters[actor].animation==0x23 &&
        (g_cpu.A[1]&0xFFFF)==0xB400) {
        /* Amy donor TouchResponse $41964: grounded hammer's forward 34x50 box. */
        g_cpu.D[2]=(uint16_t)(word(o+8)-((g_ram[o+0x22]&1)?25:9));
        g_cpu.D[3]=(uint16_t)(word(o+12)-26); g_cpu.D[4]=34; g_cpu.D[5]=50;
        return 0;
    }
    if (pc==0x1B848) return 1; /* donor art is host-rendered, not native DMA/SAT */
    if (pc==0x1AC3E && kind(actor)==S2_CHAR_AMY) return 1;
    if (pc==0x1B350) {
        S2Motion m=motion(o); S2Contacts c=contacts(o);
        s2_character_after(&characters[actor],&m,&c);
        s2_character_animate(&characters[actor],&m,bank(actor)); store_motion(o,&m);
        shadow_effects((unsigned)actor,o,&m);
        g_ram[o+1]=(g_ram[o+1]&~3)|characters[actor].render_flip|0x80;
        return 1;
    }
    if (pc==0x3F554) {
        /* Native combat outcomes, actor-specific attack predicate. Donor Amy
         * deliberately cannot damage enemies with her ordinary unarmed jump. */
        S2Motion m=motion(o); uint8_t animation=g_ram[o+0x1C],routine=g_ram[o+0x24];
        unsigned attack=s2_character_attacking(&characters[actor],&m)?2:0;
        g_ram[o+0x1C]=(uint8_t)attack;
        if (s_combat) { g_ram[o+0x1C]=animation; return 0; }
        s_combat=1; recomp_call_addr(pc); s_combat=0;
        /* Restore only the temporary attack classification. TouchResponse's
         * hurt/death/other animation transition is an actual combat outcome. */
        if (g_ram[o+0x24]==routine && g_ram[o+0x1C]==attack) g_ram[o+0x1C]=animation;
        return 1;
    }
    return 0;
}
void s2_runtime_capture(void)
{
    /* BuildSprites runs after the complete object tick. Rendering live actor
     * RAM per scanline could use one pose for a head and the next for its body,
     * or observe displayed[] while an actor was being updated. */
    /* A starpost can switch mode during RunObjects, before this final
     * BuildSprites. Keep the last displayed donor actors while the native
     * level SAT is held through the entry fade. Incoming SS objects retire
     * that publication after the level objects have actually been cleared. */
    if (!level() && presented_level()) return;
    memset(published_sprites,0,sizeof published_sprites);
    memset(published_dust,0,sizeof published_dust);
    if (!level()) return;
    /* Extra party roles have no fixed native dust object/VRAM strip. Create
     * the same short-lived Obj08 children after the object tick, respecting
     * its pool/air constraints. Native animation/deletion still own them.
     * Their original 16-tile skid atlas is read privately below, so a Sonic
     * companion's simultaneous spindash cannot overwrite these clouds. */
    for (unsigned p=2;p<s2_party.roster.slots && p<S2_MAX_PLAYERS;++p) {
        unsigned o=objects[p];
        if (!o || kind(p)!=S2_CHAR_KNUCKLES || characters[p].special!=S2_SK_SLIDE ||
            (characters[p].age&3) || dust_emit_age[p]==characters[p].age ||
            g_ram[o+0x24]!=2 || (g_ram[o+0x22]&2) || g_ram[o+0x28]<12) continue;
        dust_emit_age[p]=characters[p].age;
        for (unsigned at=0xB400;at<0xD000;at+=64) if (!g_ram[at]) {
            int reserved=0;
            for (unsigned j=2;j<S2_MAX_PLAYERS;++j) reserved|=objects[j]==at;
            if (reserved) continue;
            memset(g_ram+at,0,64); g_ram[at]=8; g_ram[at+1]=g_ram[0xD101];
            putword(at+2,(word(0xD102)&0x7FFF)|(word(o+2)&0x8000));
            memcpy(g_ram+at+4,g_ram+0xD104,4);
            putword(at+8,word(o+8)); putword(at+12,word(o+12)+6);
            g_ram[at+0x18]=1; g_ram[at+0x19]=4;
            g_ram[at+0x1C]=3; g_ram[at+0x24]=2; g_ram[at+0x2E]=0x47;
            putword(at+0x3E,o); break;
        }
    }
    for (unsigned at=0xB400;at<0xD000;at+=64) if (g_ram[at]==8 && g_ram[at+0x2E]==0x47 &&
        g_ram[at+0x1A]>=17 && g_ram[at+0x1A]<=20) {
        PartySprite *s=&published_dust[(at-0xB400)/64];
        s->x=(int16_t)word(at+8); s->y=(int16_t)word(at+12); s->art=word(at+2);
        s->frame=g_ram[at+0x1A]; s->flip=g_ram[at+1]&3; s->visible=1;
    }
    for (unsigned p=0;p<S2_MAX_PLAYERS;++p) {
        unsigned o=objects[p];
        if (!o || !g_ram[o] || (p<2 && kind(p)<S2_CHAR_AMY && displayed[p]!=2)) continue;
        PartySprite *s=&published_sprites[p];
        s->x=(int16_t)word(o+8); s->y=(int16_t)word(o+12); s->art=word(o+2);
        s->frame=g_ram[o+0x1A]; s->visible=displayed[p];
        s->flip=kind(p)==S2_CHAR_AMY || kind(p)==S2_CHAR_KNUCKLES || kind(p)==S2_CHAR_SHADOW?characters[p].render_flip:g_ram[o+1]&3;
        s->wet=!!(g_ram[o+0x22]&64);
    }
    if (displayed_tails && kind(0)==S2_CHAR_TAILS) {
        PartySprite *s=&published_sprites[S2_MAX_PLAYERS];
        s->x=(int16_t)word(0xD008); s->y=(int16_t)word(0xD00C); s->art=word(0xD002);
        s->frame=g_ram[0xD01A]; s->visible=1; s->flip=g_ram[0xD001]&3;
    }
}
static unsigned card_vram_word(const GVDP *v,unsigned a)
{ return (v->vram[a&65535u]<<8)|v->vram[(a+1)&65535u]; }
static void green_title_card(const GVDP *v,int line,uint32_t *out,int width)
{
    /* KiS2 ink 4 is green ($080), while the mixed-party host must keep
     * Sonic's blue palette available. Recolour only the original card's two
     * solid plane-A tiles; retain overlying text and red edges. */
    uint32_t old=genesis_dac_cram_to_argb(v->cram[4],GENESIS_DAC_NORMAL);
    const S2DonorBank *knux=s2_resource_bank(S2_RESOURCE_SK);
    if (!knux || !(old&255)) return;
    uint32_t color=genesis_dac_cram_to_argb(knux->palette[5],GENESIS_DAC_NORMAL);
    unsigned gain=old&255;
    color=0xFF000000u|((((color>>16)&255)*gain/255)<<16)|
        ((((color>>8)&255)*gain/255)<<8)|((color&255)*gain/255);
    static const unsigned cells[]={32,64,32,128};
    unsigned cols=cells[v->reg[16]&3],rows=cells[(v->reg[16]>>4)&3];
    unsigned table=(v->reg[2]&0x38u)<<10;
    int mode=v->reg[11]&3,scroll_row=mode==0?0:mode==2?line&~7:line;
    int hs=(int16_t)card_vram_word(v,((v->reg[13]&63u)<<10)+scroll_row*4);
    unsigned y=(line+v->vsram[0])&(rows*8-1);
    int center=(width-320)/2;
    for (int dx=center;dx<center+320;++dx) if (dx>=0 && dx<width) {
        unsigned x=(dx-center-hs)&(cols*8-1);
        unsigned attr=card_vram_word(v,table+((y/8)*cols+x/8)*2),tile=attr&2047u;
        if ((tile==0x5DA || tile==0x5DB) && !(attr&0x6000u) && out[dx]==old) out[dx]=color;
    }
}
static void knuckles_hud(const GVDP *v,int line,uint32_t *out,int width,int draw)
{
    if (!s2_resource_knuckles_art() || kind(0)!=S2_CHAR_KNUCKLES ||
        !knuckles_available() || word(0xFFD8) || !presented_level() || !g_ram[0xF711]) return;
    int origin=s2_video_hud_origin(width);
    if (origin<=-width) return;
    /* The wide renderer substitutes art directly in its existing HUD pass.
     * Native output needs the old shape removed before donor actors, then
     * the original high-priority glyphs drawn after them. Render only a
     * private VDP copy; guest VRAM/status/CRAM are never changed. */
    if (!draw && sonic2_video.enabled()) return;
    uint8_t erase[320]={0}; int any=0;
    unsigned table=(v->reg[5]&127u)<<9,slot=0;
    for (unsigned n=0;n<80;++n) {
        unsigned at=table+slot*8,attr=card_vram_word(v,at+4),tile=attr&2047;
        unsigned size=v->vram[(at+2)&65535]&15,cw=(size>>2)+1,ch=(size&3)+1;
        int y=(int)(card_vram_word(v,at)&511)-128;
        if ((attr&0x8000) && (tile==0x7D4 || tile==0x7D8) && line>=y && line<y+(int)ch*8) {
            int x=(int)(card_vram_word(v,at+6)&511)-128,iy=line-y;
            if (attr&0x1000) iy=(int)ch*8-1-iy;
            for (unsigned px=0;px<cw*8;++px) {
                int dx=x+(int)px,ix=attr&0x800?(int)cw*8-1-(int)px:(int)px;
                unsigned cell=(attr&0xE000)|((tile+(unsigned)(ix/8*ch+iy/8))&2047);
                if (dx<0 || dx>=320 || dx+origin<0 || dx+origin>=width) continue;
                if (draw) {
                    uint32_t color;
                    if (s2_video_knuckles_hud_pixel(v,cell,ix,iy,&color) && color) out[dx+origin]=color;
                } else {
                    int fx=ix&7,fy=iy&7;
                    if (cell&0x800) fx^=7; if (cell&0x1000) fy^=7;
                    unsigned byte=v->vram[((cell&2047)*32+fy*4+fx/2)&65535];
                    if ((fx&1?byte&15:byte>>4)) erase[dx]=1,any=1;
                }
            }
        }
        slot=v->vram[(at+3)&65535]&127; if (!slot) break;
    }
    if (any) {
        GVDP blank=*v; uint8_t indices[GVDP_MAX_WIDTH];
        memset(blank.vram+0x7D4*32,0,12*32);
        int count=gvdp_render_scanline(&blank,line,indices);
        for (int x=0;x<320 && x<count;++x) if (erase[x]) {
            unsigned index=indices[x];
            if (index<192) out[x+origin]=genesis_dac_cram_to_argb(v->cram[index%64],
                index<64?GENESIS_DAC_NORMAL:index<128?GENESIS_DAC_SHADOW:GENESIS_DAC_HIGHLIGHT);
        }
    }
}
static unsigned hud_ink(const uint8_t *tiles,unsigned tile,int x,int y)
{
    unsigned b=tiles[tile*32+(y&7)*4+(x&7)/2];
    return x&1?b&15:b>>4;
}
static uint32_t hud_color(const GVDP *v,uint16_t shade)
{
    unsigned fade=0;
    for (unsigned i=0;i<16;++i) {
        unsigned w=v->cram[i],n=(w&14)+((w>>4)&14)+((w>>8)&14);
        if (n>fade) fade=n;
    }
    uint32_t c=genesis_dac_cram_to_argb(shade,GENESIS_DAC_NORMAL);
    return 0xFF000000u|((((c>>16)&255)*fade/42)<<16)|
        ((((c>>8)&255)*fade/42)<<8)|((c&255)*fade/42);
}
static void competition_hud(const GVDP *v,int line,uint32_t *out,int width,int draw)
{
    if (!word(0xFFD8) || !g_ram[0xF711] || !hud_art_ready) return;
    unsigned p=line>=224,which=kind(p);
    int y=line-(p?424:200),origin=s2_video_competition_extended()?0:(width-320)/2;
    if (y<0 || y>=16 || which==(p?S2_CHAR_TAILS:S2_CHAR_SONIC)) return;
    if (!draw) {
        if (s2_video_competition_extended()) return;
        /* Remove only the old HUD sprites in a private VDP. The live H-int
         * SAT is different for each viewport; no guest memory is changed. */
        GVDP blank=*v; unsigned table=(v->reg[5]&127u)<<9,slot=0; int found=0;
        for (unsigned n=0;n<80;++n) {
            unsigned at=table+slot*8,attr=card_vram_word(v,at+4);
            int sx=(card_vram_word(v,at+6)&511)-128;
            int sy=(card_vram_word(v,at)&1023)-256;
            if ((attr&0x8000) && (sx==16 || sx==32) && sy==(p?424:200)) {
                blank.vram[at&65535]=blank.vram[(at+1)&65535]=0; found=1;
            }
            slot=v->vram[(at+3)&65535]&127; if (!slot) break;
        }
        if (found) {
            uint8_t indices[GVDP_MAX_WIDTH]; int count=gvdp_render_scanline(&blank,line,indices);
            for (int x=16;x<96 && x<count;++x) if (x+origin>=0 && x+origin<width) {
                unsigned c=indices[x];
                if (c<192) out[x+origin]=genesis_dac_cram_to_argb(v->cram[c%64],
                    c<64?GENESIS_DAC_NORMAL:c<128?GENESIS_DAC_SHADOW:GENESIS_DAC_HIGHLIGHT);
            }
        }
        return;
    }
    const S2KnucklesArt *knux=which==S2_CHAR_KNUCKLES?s2_resource_knuckles_art():NULL;
    int original=which<2 || knux;
    const uint8_t *tiles=knux?knux->life:hud_art.life[which==S2_CHAR_TAILS?(g_ram[0xFFF8]&128?2:1):0];
    unsigned lives=g_ram[p?0xFEC6:0xFE12]; if (lives>99) lives=99;
    for (int x=0;x<(original?48:80);++x) {
        int dx=origin+16+x; if (dx<0 || dx>=width) continue;
        unsigned ink=0; uint16_t shade=0;
        if (original) {
            unsigned tile=x<16?(x/8)*2+y/8:4+((x-16)/8)*2+y/8;
            if (tile==9 || tile==11) {
                unsigned digit=tile==9?lives/10:lives%10;
                if (tile!=9 || lives>=10) ink=hud_ink(hud_art.numbers,digit,x,y);
            } else ink=hud_ink(tiles,tile,x,y);
            shade=knux?knux->palette[ink]:v->cram[(x<16?0:16)+ink];
        } else if (x<16) {
            const S2DonorBank *b=bank(p);
            const S2DonorFrame *f=b && b->count>1?&b->frames[1]:NULL;
            if (f && f->pixels && f->width && f->height) {
                unsigned ch=f->height<24?f->height:24;
                unsigned sx=x*f->width/16,sy=y*ch/16;
                ink=f->pixels[sy*f->width+sx]; shade=b->palette[ink];
            }
        } else if (y<8) {
            static const char *names[]={"SONIC","TAILS","AMY","KNUCKLES","SHADOW","MIGHTY"};
            const char *name=names[which]; unsigned c=(x-16)/8;
            if (c<strlen(name)) {
                /* MenuScreen uploads the font at tile16: A30 is raw14. */
                ink=hud_ink(hud_art.font,14+(unsigned)(name[c]-'A'),x,y);
                shade=ink==1?0:which==S2_CHAR_KNUCKLES?(y<4?0x08E:0xEEE):0x0EE;
            }
        } else if (x<32) {
            ink=hud_ink(hud_art.life[0],5+(unsigned)(x-16)/8*2,x,y); shade=v->cram[16+ink];
        } else if (x>=32 && x<48) {
            if (x>=40 || lives>=10) ink=hud_ink(hud_art.numbers,x<40?lives/10:lives%10,x,y);
            shade=v->cram[16+ink];
        }
        if (ink) out[dx]=hud_color(v,shade);
    }
}
void s2_runtime_overlay(const GVDP *v, int line, uint32_t *out, int width)
{
    static PartySprite frame_sprites[S2_MAX_PLAYERS+1];
    static PartySprite frame_dust[DUST_SLOTS];
    static GVDP frame_vdp;
    static uint8_t frame_world[65536];
    static int frame_active,frame_vs,left,top,vs_left,vs_top,frame_green_card;
    if (!line) {
        frame_active=presented_level() && (g_ram[0xF711] || !word(0xFFD8));
        memcpy(frame_sprites,published_sprites,sizeof frame_sprites);
        memcpy(frame_dust,published_dust,sizeof frame_dust);
        frame_vdp=*v; memcpy(frame_world,g_ram,sizeof frame_world);
        frame_vs=word(0xFFD8);
        frame_green_card=0;
        if (!frame_vs && (g_ram[0xF600]&127)==12 && kind(0)==S2_CHAR_KNUCKLES && knuckles_available() &&
            frame_vdp.vram[0xBB40]==0x44 && frame_vdp.vram[0xBB7F]==0x44) {
            /* The actual fill is plane A, not a sprite. Follow its displayed
             * tilemap through the exit, after producer objects can be gone. */
            unsigned table=(frame_vdp.reg[2]&0x38u)<<10;
            for (unsigned cell=0;cell<64*32;++cell) {
                unsigned attr=card_vram_word(&frame_vdp,table+cell*2),tile=attr&2047u;
                if ((tile==0x5DA || tile==0x5DB) && !(attr&0x6000u)) { frame_green_card=1; break; }
            }
        }
        s2_video_actor_origin(&frame_vdp,0,width,&left,&top);
        if (!frame_world[0xF711] && !s2_video_intro_world_visible()) {
            /* Title-card scroll belongs to the card, not the actor camera.
             * Native actors are submitted before Level_started becomes true. */
            left=word(0xEE60)-(width-320)/2; top=(int16_t)word(0xEE64);
        }
        if (frame_vs) {
            if (s2_video_competition_extended()) {
                s2_video_actor_origin(v,0,width,&left,&top);
                s2_video_actor_origin(v,224,width,&vs_left,&vs_top);
            } else {
                left=word(0xEE60)-(width-320)/2; top=(int16_t)word(0xEE64);
                vs_left=word(0xEE80)-(width-320)/2; vs_top=(int16_t)word(0xEE84)-224;
            }
        }
    }
    if (frame_green_card && (v->reg[1]&64)) green_title_card(&frame_vdp,line,out,width);
    s2_video_special_actor(v,line,out,width);
    s2_video_special_results(v,line,out,width);
    if (s2_video_competition_divider(v,line,out,width)) return;
    if (!frame_active || !(v->reg[1]&64)) return;
    int overlay_drawn=frame_green_card;
    knuckles_hud(v,line,out,width,0);
    competition_hud(v,line,out,width,0);
    s2_video_results_name(v,line,out,width,0);
    int origin_x=left,origin_y=top;
    if (frame_vs) {
        /* Native interlace pass is 448 rows. Each competitor's 224-row world
         * view contains both characters, with the stock central SAT mask. */
        if (line>=216 && line<224) return;
        if (line>=224) { origin_x=vs_left; origin_y=vs_top; }
    }
    for (int index=S2_MAX_PLAYERS;index>=0;--index) {
        unsigned p=index==S2_MAX_PLAYERS?0:(unsigned)index;
        const PartySprite *s=&frame_sprites[index];
        if (!s->visible) continue;
        const S2DonorBank *b=bank(p); unsigned frame=s->frame;
        unsigned actor_kind=kind(p);
        int shadow=actor_kind==S2_CHAR_SHADOW;
        if (actor_kind==S2_CHAR_MIGHTY) frame=s2_mighty_frame(frame);
        const S2KnucklesArt *original=actor_kind==S2_CHAR_KNUCKLES?s2_resource_knuckles_art():NULL;
        unsigned zone=frame_world[0xFE10];
        /* Native H-int split, including a partly submerged body. The physics
         * underwater flag describes the center, not the current scanline. */
        int wet=!frame_vs && s2_video_water_palette(v);
        const uint16_t *knux_water=original && wet && (zone==13 || zone==15)?
            original->water_palette[zone==15]:NULL;
        const uint16_t *donor_water=knux_water;
        if(!donor_water && wet && b && b->water_palette_count)
            donor_water=zone==15 && b->water_palette_count>1?b->water_palette_alt:b->water_palette;
        const S2DonorFrame *f=b && frame<b->count?&b->frames[frame]:NULL;
        if (!f) continue;
        int x=s->x-origin_x,y=s->y-origin_y;
        unsigned flip=s->flip;
        int fy=line-y-(flip&2?-f->y-f->height:f->y);
        if (fy<0 || fy>=f->height) continue;
        if (flip&2) fy=f->height-1-fy;
        int x0=x+(flip&1?-f->x-f->width:f->x);
        uint32_t palette[16];
        unsigned phase=!p && !frame_vs?frame_world[0xF65F]:0;
        unsigned super_frame=((unsigned)frame_world[0xF65C]<<8)|frame_world[0xF65D];
        for(unsigned color=0;color<16;++color) {
            uint16_t shade=shadow?s2_shadow_color(b,color,wet,phase,super_frame):
                actor_kind<S2_CHAR_AMY?v->cram[color]:donor_water?donor_water[color]:b->palette[color];
            if(!shadow && phase)shade=s2_super_color(kind(p),color,shade,phase,super_frame,b);
            if(actor_kind>=S2_CHAR_AMY)shade=s2_video_donor_color(shade);
            palette[color]=genesis_dac_cram_to_argb(shade,GENESIS_DAC_NORMAL);
        }
        for (unsigned px=0;px<f->width;++px) {
            int dx=x0+px; if (dx<0 || dx>=width) continue;
            unsigned color=f->pixels[fy*f->width+(flip&1?f->width-1-px:px)];
            if (!color) continue;
            if (!frame_world[0xF711]) {
                if (!s2_video_intro_pixel_visible(&frame_vdp,dx-(width-320)/2,line,s->art&0x8000)) continue;
            } else if (!s2_video_actor_pixel_visible(&frame_vdp,dx+origin_x,line+origin_y,s->art&0x8000,frame_world)) continue;
            out[dx]=palette[color];overlay_drawn=1;
        }
    }
    for (unsigned n=0;n<DUST_SLOTS;++n) {
        const PartySprite *s=&frame_dust[n]; if (!s->visible) continue;
        int row=line-(s->y-origin_y-8); if (row<0 || row>=16) continue;
        int fy=s->flip&2?15-row:row;
        for (int px=0;px<16;++px) {
            int dx=s->x-origin_x-8+px; if (dx<0 || dx>=width) continue;
            int fx=s->flip&1?15-px:px;
            /* REV01 Obj08 frames17..20: one 2x2 piece at(-8,-8), tiles
             * 0/4/8/12. DPLC21=$F0BA loads this exact static 16-tile atlas. */
            unsigned tile=(s->frame-17)*4+(fx/8)*2+fy/8;
            unsigned byte=g_rom[0x7373C+tile*32+(fy&7)*4+(fx&7)/2];
            unsigned ink=fx&1?byte&15:byte>>4;
            if (ink && s2_video_actor_pixel_visible(&frame_vdp,dx+origin_x,line+origin_y,s->art&0x8000,frame_world)) {
                out[dx]=genesis_dac_cram_to_argb(v->cram[((s->art>>9)&48)+ink],GENESIS_DAC_NORMAL);
                overlay_drawn=1;
            }
        }
    }
    /* Native UI is already in out. Reapply it on modified rows and retain
     * each viewport's first-row selection of the displayed SAT publication. */
    if(overlay_drawn || !line || (frame_vs && line==224))
        s2_video_front_ui(v,line,out,width);
    s2_video_results_name(v,line,out,width,1);
    knuckles_hud(v,line,out,width,1);
    competition_hud(v,line,out,width,1);
}

/* ---- rollback state (GameSpec rb_state_save/load) --------------------------
 * Everything this adapter keeps outside guest RAM that a tick reads, at ANY
 * tick boundary: role/actor bookkeeping, the reentrancy guards (the game fiber
 * may be suspended inside a hook), per-actor character/physics/CPU state,
 * solid-object ownership, the published overlay sprites, the save-menu and
 * custom-video state, and the character registry (as ids, never pointers).
 * Structs with padding are packed field by field: these bytes are also the
 * digest every peer compares, so they must not carry padding garbage.
 * Immutable ROM-derived caches (decoded donor art) are not state. */
static void rb_bytes(S2StateIO *io, void *v, size_t n) { s2_state_bytes(io, v, n); }
#define RB(io, v) rb_bytes(io, &(v), sizeof(v))
static void rb_character(S2StateIO *io, S2CharacterState *c)
{
    RB(io,c->kind); RB(io,c->special); RB(io,c->age); RB(io,c->charge); RB(io,c->ledge);
    RB(io,c->turn); RB(io,c->held); RB(io,c->pressed); RB(io,c->native_held); RB(io,c->native_pressed);
    RB(io,c->move); RB(io,c->before_air); RB(io,c->before_jumping);
    RB(io,c->jump_adjustment); RB(io,c->wall_x);
    RB(io,c->animation); RB(io,c->animation_index); RB(io,c->animation_timer);
    RB(io,c->frame); RB(io,c->render_flip); RB(io,c->animation_changed);
    RB(io,c->shadow_air_used); RB(io,c->shadow_target); RB(io,c->shadow_last_target); RB(io,c->shadow_target_age);
    RB(io,c->shadow_chain); RB(io,c->shadow_chaos); RB(io,c->source_animation); RB(io,c->shadow_super);
    RB(io,c->shadow_monitor_ticks);
    RB(io,c->climb_delta);
}
static void rb_all(S2StateIO *io)
{
    s2_options_rb_state(io);
    int32_t a=active, ip=inside_player, ac=actor, icc=inside_companion_cpu, is=inside_solid;
    RB(io,a); RB(io,ip); RB(io,ac); RB(io,icc); RB(io,is);
    RB(io,previous_input); RB(io,displayed); RB(io,companion_cpu);
    RB(io,dust_emit_age);
    for (unsigned p=0;p<S2_MAX_PLAYERS;++p) rb_character(io,&characters[p]);
    RB(io,physics); RB(io,objects); RB(io,solid_flags); RB(io,solid_ids);
    for (unsigned i=0;i<S2_MAX_PLAYERS+1;++i) {
        PartySprite *ps=&published_sprites[i];
        RB(io,ps->x); RB(io,ps->y); RB(io,ps->art); RB(io,ps->frame); RB(io,ps->flip); RB(io,ps->visible); RB(io,ps->wet);
    }
    for (unsigned i=0;i<DUST_SLOTS;++i) {
        PartySprite *ps=&published_dust[i];
        RB(io,ps->x); RB(io,ps->y); RB(io,ps->art); RB(io,ps->frame); RB(io,ps->flip); RB(io,ps->visible); RB(io,ps->wet);
    }
    RB(io,displayed_tails);
    int32_t g[6]={s_inside_spring,s_inside_palette,s_reward,s_checking_death,s_transforming,s_combat};
    RB(io,g);
    /* Online campaign rendering can become active after the cold snapshot
     * and inactive before a rematch restore. Always carry its simulation
     * tables so the state schema is independent of connection lifecycle. */
    int32_t menu_on=s2_save_menu_enabled()?1:0, video_on=1;
    int32_t want_menu=menu_on, want_video=video_on;
    RB(io,menu_on); RB(io,video_on);
    if (io->mode && (menu_on!=want_menu || video_on!=want_video)) { io->ok=0; return; }
    if (menu_on) s2_save_menu_state(io);
    if (video_on) s2_video_rb_state(io);
    char reg[S2_MAX_CHARACTERS][S2_CHARACTER_ID_SIZE];
    uint32_t count=s2_character_count();
    memset(reg,0,sizeof reg);
    for (uint32_t i=0;i<count && i<S2_MAX_CHARACTERS;++i)
        snprintf(reg[i],sizeof reg[i],"%s",s2_character_at(i)->id);
    RB(io,count); RB(io,reg);
    if (io->mode==2) {
        active=a; inside_player=ip; actor=ac; inside_companion_cpu=icc; inside_solid=is;
        s_inside_spring=g[0]; s_inside_palette=g[1]; s_reward=g[2];
        s_checking_death=g[3]; s_transforming=g[4]; s_combat=g[5];
    }
    if (io->mode==1 && count!=s2_character_count()) io->ok=0;   /* registry is fixed after init */
}
size_t s2_runtime_rb_save(void *dst, size_t cap)
{
    S2StateIO io={0};
    rb_all(&io);                      /* measure */
    if (!dst) return io.pos;
    if (cap<io.pos) return 0;
    size_t need=io.pos;
    memset(dst,0,need);
    S2StateIO w={(uint8_t *)dst,need,0,0,1};
    rb_all(&w);
    return w.ok && w.pos==need ? need : 0;
}
int s2_runtime_rb_load(const void *src, size_t len)
{
    S2StateIO v={(uint8_t *)src,len,0,1,1};
    rb_all(&v);
    if (!v.ok || v.pos!=len) return 0;
    S2StateIO io={(uint8_t *)src,len,0,2,1};
    rb_all(&io);
    return io.ok;
}
