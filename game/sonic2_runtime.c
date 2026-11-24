#ifdef SONIC2_MUSIC_DRIVER
#include "sonic2_cd_themes.h"
#endif
/* Sonic 2 host adapter. Stock object execution owns the world tick. These
 * callbacks adapt player roles only, never replay a level or enemy update. */
#include "sonic2_runtime.h"
#include "sonic2_sound_driver.h"
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
static uint32_t input_poll_serial;
static uint16_t ss_return_zone,ss_return_rings;
static uint8_t ss_return_post,ss_return_lives,ss_return_pending;
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
const uint8_t *s2_runtime_title_font(void) {return hud_art_ready?hud_art.title_font:NULL;}
/* The native status byte only has standing/pushing bits for two players.
 * Extra actors retain those two bits per solid on the host. The actual actor
 * objects have real, reserved native pool addresses, so monitor/platform
 * parent links stay valid across ticks (no temporary Sidekick alias). */
/* Launcher occupancy is private to the extra seats as well. The low two
 * bits record idle, charging or releasing, outside the native contact bits. */
enum { EXTRA_CONTACT_BITS = 0x50, EXTRA_LAUNCHER_STATE = 3 };
static uint8_t solid_flags[S2_MAX_PLAYERS-2][144], solid_ids[144];
static int inside_solid;
typedef S2RuntimeSprite PartySprite;
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
           s_transforming, s_combat, s_departing_objects;
static int amy_available(void) { return s2_party.amy_enabled && s2_resource_verified(S2_RESOURCE_AMY); }
static int knuckles_available(void) { return s2_party.s3k_enabled && s2_resource_verified(S2_RESOURCE_SK); }
static int knuckles_lead_available(void)
{ return s2_party.knuckles_campaign_enabled && s2_resource_knuckles_stages()!=NULL; }
static const S2Character amy={"amy","AMY",1,amy_available};
static const S2Character knuckles={"knuckles","KNUCKLES",1,knuckles_available};
static unsigned kind(unsigned p)
{
    const char *id=s2_party.roster.character[p];
    return !strcmp(id,"amy")?S2_CHAR_AMY:!strcmp(id,"knuckles")?S2_CHAR_KNUCKLES:
        !strcmp(id,"tails")?S2_CHAR_TAILS:!strcmp(id,"shadow")?S2_CHAR_SHADOW:
        !strcmp(id,"mighty")?S2_CHAR_MIGHTY:S2_CHAR_SONIC;
}
uint16_t s2_runtime_title_card_ink(void)
{
 unsigned lead=kind(0);
 if(lead==S2_CHAR_KNUCKLES&&knuckles_available()){
  const S2DonorBank *knux=s2_resource_bank(S2_RESOURCE_SK);return knux?knux->palette[5]:0;
 }
 if(s2_party.presentation_enabled&&s2_party.title_card_colors){
  static const uint16_t inks[]={0,0x08E,0xA0E,0x080,0x444,0x00E};
  if(lead<sizeof inks/sizeof *inks)return inks[lead];
 }
 return 0;
}
S2StagePlacementView s2_runtime_stage_layout(unsigned zone,unsigned act)
{
    if((g_ram[0xF600]&127)!=12 || g_ram[0xFFD8] || g_ram[0xFFD9] || kind(0)!=S2_CHAR_KNUCKLES ||
       !knuckles_available() || !knuckles_lead_available()) return (S2StagePlacementView){0};
    return s2_stage_layout_view(s2_resource_knuckles_stages(),zone,act);
}
static int knuckles_ending(void)
{
    return (g_ram[0xF600]&127)==0x20 && kind(0)==S2_CHAR_KNUCKLES &&
        knuckles_available() && s2_resource_knuckles_ending()!=NULL;
}
const S2EndingFrame *s2_runtime_ending_frame(unsigned map,unsigned frame)
{
    if(map!=S2_ENDING_JUMP_MAP || frame>=S2_ENDING_FRAMES || !knuckles_ending())return NULL;
    return &s2_resource_knuckles_ending()->frames[frame];
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
/* The original Walk/Run sequences are also needed by display-only runoff.
 * Native control continues to own all normal gameplay animation. */
static void prepare_native_bank(unsigned k)
{
    if(k>=2 || native_banks[k].count)return;
    S2DonorLayout l=k==0?(S2DonorLayout){214,0x6FBE0,0x714E0,0x50000,0x29E2,8,0x1B618,2}:
        (S2DonorLayout){139,0x739E2,0x7446C,0x64320,0x29E2,8,0x1D038,2};
    char error[160];
    if(!s2_donor_decode(g_rom,0x100000,&l,&native_banks[k],error,sizeof error)) {
        fprintf(stderr,"Sonic 2 native presentation art: %s\n",error);exit(2);
    }
}
static int imported(void) { return actor>=0 && (kind((unsigned)actor)==S2_CHAR_AMY || kind((unsigned)actor)==S2_CHAR_KNUCKLES || kind((unsigned)actor)==S2_CHAR_SHADOW); }
void s2_runtime_load(void)
{
    active=inside_player=s_departing_objects=0; actor=-1;
    input_poll_serial=0;
    ss_return_zone=ss_return_rings=0;
    ss_return_post=ss_return_lives=ss_return_pending=0;
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
static unsigned local_campaign_party(void)
{
    if (!s2_party.local_split_screen || word(0xFFD8) || (g_ram[0xF600]&127)!=12) return 0;
#if GENESIS_HAS_RECOMP_NET
    if (genesis_netplay_active()) return 0;
#endif
    unsigned mask=1u;
    for (unsigned p=1;p<s2_party.roster.slots && p<S2_MAX_PLAYERS;++p)
        if (strcmp(s2_party.roster.character[p],"none")) mask|=1u<<p;
    return mask;
}
int s2_runtime_campaign_independent(void)
{
    return s2_runtime_campaign_online() || local_campaign_party()>1u;
}
unsigned s2_runtime_local_view_players(void)
{
    unsigned party=local_campaign_party();
    return party ? 1u | (party & genesis_sim_human_mask()) : 0;
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
    m.fluid_movement=s2_party.knuckles_flow!=0;
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
static int normal_super_edge(unsigned o)
{
    return kind(0)!=S2_CHAR_KNUCKLES && kind(0)!=S2_CHAR_SHADOW &&
        g_ram[o+0x24]==2 && !word(0xFFF0) && !word(0xFFD8) &&
        (g_ram[o+0x22]&2) && g_ram[o+0x3C] &&
        !(g_ram[o+0x2A]&1) && !g_ram[0xF7CC] &&
        (g_ram[0xF605]&0x70) && !(word(0xFFFA) && (g_ram[0xF605]&0x10)) &&
        !g_ram[0xFE19] && g_ram[0xFFB1]==7 && word(0xFE20)>=50 && g_ram[0xFE1E];
}
static void normal_super_transform(void)
{
    putword(0xF65C,0);s_transforming=1;native_helper(0x1AB38);s_transforming=0;
    if(g_ram[0xFE19]) {
        characters[0].special=0;
        if(kind(0)==S2_CHAR_TAILS) {
            g_ram[0xB01C]=2;memcpy(g_ram+0xFEC0,g_ram+0xF760,6);
        }
    }
}
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
static unsigned knuckles_glide_collision(unsigned o, int gliding)
{
    /* KiS2 Knuckles_DoLevelCollision2 uses the original terrain helpers,
     * without Sonic_ResetOnFloor or replacement of glide inertia. Its
     * airborne/pushing result is separate from the object's status. */
    M68KState cpu=g_cpu;
    unsigned flags=2;
    if(gliding)g_ram[o+0x16]=g_ram[o+0x17]=10;
    host_call(0x1EC4E); /* select primary/secondary collision table */
    unsigned quadrant=(shadow_angle((int16_t)word(o+0x10),(int16_t)word(o+0x12))-0x20)&0xC0;
    int floor=0,ceiling=0;
    if (quadrant!=0xC0) {
        g_cpu.D[5]=g_ram[o+0x3F];
        int d=host_call(0x1F05E);
        if(d<0){putword(o+8,word(o+8)-d);putword(o+0x10,0);flags|=32;}
    }
    if (quadrant!=0x40) {
        g_cpu.D[5]=g_ram[o+0x3F];
        int d=host_call(0x1EEDC);
        if(d<0){putword(o+8,word(o+8)+d);putword(o+0x10,0);flags|=32;}
    }
    if(!quadrant) floor=1;
    else {
        g_cpu.D[5]=g_ram[o+0x3F];
        int d=host_call(0x1EF2E);
        if(d<0) {
            if(quadrant==0x40 && -d>=20) {
                g_cpu.D[5]=g_ram[o+0x3F];d=host_call(0x1EEDC);
                if(d<0){putword(o+8,word(o+8)+d);putword(o+0x10,0);flags|=32;}
            } else {
                putword(o+12,word(o+12)-d);
                if(quadrant==0x80 || (int16_t)word(o+0x12)<0)putword(o+0x12,0);
            }
            ceiling=1;
        }
        if(!ceiling && quadrant!=0x80 && (int16_t)word(o+0x12)>=0)floor=1;
    }
    if(floor) {
        M68KState result=native_helper(0x1EC4E);
        int d=(int16_t)result.D[1];
        if(d<0){putword(o+12,word(o+12)+d);g_ram[o+0x26]=(uint8_t)result.D[3];putword(o+0x12,0);flags&=~2u;}
    }
    g_ram[o+0x16]=19;g_ram[o+0x17]=9;
    g_cpu=cpu;
    return flags|0x80;
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
    M68KState floor=native_helper(0x1EC4E); /* also selects the collision plane */
    c.floor=(int16_t)floor.D[1];c.floor_angle=(uint8_t)floor.D[3];
    g_cpu.D[5]=g_ram[o+0x3F];
    c.wall=host_call((g_ram[o+0x22]&1)?0x1F05E:0x1EEDC);
    c.ceiling=host_call(0x1EF2E);
    if(knux && special==S2_SK_GLIDE &&
        ((characters[actor].glide_collision&32) ||
         (c.wall<=0 && (int16_t)word(o+0x10)>-0x20 && (int16_t)word(o+0x10)<0x20))) {
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
static int level(void)
{
    unsigned mode=g_ram[0xF600]&127;
    return active && (mode==12 || (mode==4 && s_departing_objects));
}
static int presented_level(void)
{
    unsigned mode=g_ram[0xF600]&127;
    /* Both the title return and Special Stage change mode before the
     * current level's palette fade. The outgoing scene still owns its actors. */
    return level() || (active && (s2_video_outgoing_level() || (mode==16 &&
        (g_ram[0xB000]==1 || g_ram[0xB000]==2) && g_ram[0xF711])));
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
int s2_runtime_amy_item(unsigned o)
{
    if (!level() || o<0xB000 || o>=0xD400) return 0;
    unsigned id=g_ram[o];
    /* Monitors use the world's lead identity. The result mini and flying
     * life icon keep their native mapping, movement and reward behavior. */
    if (id==0x3A && (g_ram[o+0x1A]==0xC || g_ram[o+0x1A]==0xD))
        return s2_party.amy_enabled && kind(0)==S2_CHAR_AMY && s2_resource_amy_items()!=NULL;
    if (id==0x0D) {
        const S2ItemArt *items=s2_resource_primary_items();
        return !word(0xFFD8) && items && items->signpost_tiles!=0;
    }
    if (id!=0x26 && id!=0x2E) return 0;
    return s2_resource_primary_items()!=NULL;
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
static int host_actor_visibility(unsigned o)
{
    /* Host-rendered actors never go through BuildSprites' on-screen writer.
     * Supply its camera-relative visibility instead of forcing bit 7 on. */
    int x=(int16_t)(word(o+8)-word(0xEE00));
    int y=(int16_t)(word(o+12)-word(0xEE04));
    int visible=x>=-(int)g_ram[o+0x19] && x<320+g_ram[o+0x19] &&
        y>=-32 && y<256;
    g_ram[o+1]=(g_ram[o+1]&0x7F)|(visible?0x80:0);
    return visible;
}
static void companion_control(unsigned p, unsigned o, int act_end)
{
    if (g_ram[o+0x24]!=2) return;
    int visible=host_actor_visibility(o);
    int human=human_companion(p);
    /* The signpost lets P1 run beyond the fixed camera's native viewport.
     * Settled party followers reach that same boundary. They remain in the
     * act, so do not restart the offscreen recovery timer during the tally. */
    if (act_end && !human) g_ram[o+1]|=0x80;
    /* Every human in an independent-view party has a camera. Distance from P1 is no longer a
     * reason to start Tails' 300-tick offscreen recovery timer. Actual death
     * and its normal recovery continue through the original controller. */
    if (human && s2_runtime_campaign_independent()) g_ram[o+1]|=0x80;
    /* Preserve the party's explicit controller ownership; the CPU must not
     * take over an idle connected controller after the stock ten seconds. */
    if (human) putword(0xF702,600);
    uint16_t index=word(0xEED2),record_x=0;
    unsigned record=0;
    int six=s2_party.roster.slots>4;
    int varied=!act_end && (p>=2 || six) && !human && word(0xF708)==6 && !g_ram[o+0x2A] &&
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
        if(act_end && !companion_recovering()) {
            /* Once the real tally has stopped P1's timer, expanded-party
             * followers keep native horizontal following without retrying
             * jumps/spindashes against the act's fixed right boundary. */
            g_ram[0xF66A]&=~0x72;g_ram[0xF66B]&=~0x72;
            g_ram[0xF70F]=0;
            if(word(0xF708)==8)putword(0xF708,6);
        }
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
        S2DonorLayout l={139,0x739E2,0x7446C,0x64320,0x29E2,8,0x1D038,2};
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
            S2DonorLayout l=k==0?(S2DonorLayout){214,0x6FBE0,0x714E0,0x50000,0x29E2,8,0x1B618,2}:
                (S2DonorLayout){139,0x739E2,0x7446C,0x64320,0x29E2,8,0x1D038,2};
            char error[160];
            if (!s2_donor_decode(g_rom,0x100000,&l,&native_banks[k],error,sizeof error)) {
                fprintf(stderr,"Sonic 2 party: native character art: %s\n",error); exit(2);
            }
        }
    }
}
static void sound_state(S2StateIO *io)
{
    uint8_t expected[S2_SOUND_STATE_BYTES];s2_sound_state_save(expected);
    if(io->data && io->mode && (io->pos>io->size || sizeof expected>io->size-io->pos ||
       !s2_sound_state_valid(io->data+io->pos))) {io->ok=0;return;}
    s2_state_bytes(io,expected,sizeof expected);
}
static void runtime_state(S2StateIO *io)
{
    sound_state(io);
#ifdef SONIC2_MUSIC_DRIVER
    s2_cd_theme_state(io);
#endif
    unsigned header[3+S2_MAX_PLAYERS+11]={8,g_ram[0xF600],s2_party.roster.slots};
    header[3+S2_MAX_PLAYERS]=(unsigned)s2_party.amy_enabled;
    header[4+S2_MAX_PLAYERS]=(unsigned)s2_party.s3k_enabled;
    header[5+S2_MAX_PLAYERS]=(unsigned)s2_party.save_menu_enabled;
    header[6+S2_MAX_PLAYERS]=(unsigned)s2_party.mighty_enabled;
    header[7+S2_MAX_PLAYERS]=(unsigned)s2_party.shadow_enabled;
    header[8+S2_MAX_PLAYERS]=(unsigned)s2_party.infinite_lives;
    header[9+S2_MAX_PLAYERS]=(unsigned)s2_party.infinite_time;
    header[10+S2_MAX_PLAYERS]=(unsigned)s2_party.knuckles_campaign_enabled;
    header[11+S2_MAX_PLAYERS]=(unsigned)s2_party.easy_debug;
    header[13+S2_MAX_PLAYERS]=(unsigned)s2_party.knuckles_flow;
    header[12+S2_MAX_PLAYERS]=(g_ram[0xF600]==12 || g_ram[0xF600]==16) &&
        !word(0xFFD8) && kind(0)==S2_CHAR_KNUCKLES;
    for (unsigned p=0;p<S2_MAX_PLAYERS;++p) header[3+p]=!strcmp(s2_party.roster.character[p],"none")?S2_CHAR_NONE:kind(p);
    if (io->mode) {
        unsigned saved[3+S2_MAX_PLAYERS+11];
        if (!io->data || io->pos>io->size || sizeof saved>io->size-io->pos) { io->ok=0; return; }
        memcpy(saved,io->data+io->pos,sizeof saved);
        if (saved[1]!=12 && saved[1]!=16) io->ok=0;
        header[1]=saved[1];
        header[12+S2_MAX_PLAYERS]=saved[12+S2_MAX_PLAYERS];
        if(saved[12+S2_MAX_PLAYERS]>1 || (saved[12+S2_MAX_PLAYERS] && !knuckles_lead_available())) io->ok=0;
        if (memcmp(saved,header,sizeof saved)) io->ok=0;
    }
    S2_STATE(io,header); S2_STATE(io,active);
    S2_STATE(io,s_departing_objects);
    S2_STATE(io,previous_input); S2_STATE(io,input_poll_serial);
    S2_STATE(io,ss_return_zone); S2_STATE(io,ss_return_rings);
    S2_STATE(io,ss_return_post); S2_STATE(io,ss_return_lives); S2_STATE(io,ss_return_pending);
    S2_STATE(io,displayed); S2_STATE(io,displayed_tails); S2_STATE(io,companion_cpu);
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
int s2_runtime_state_needs_frame_end(void)
{
    /* Campaign 2D waits after its completed object/sprite tick. Half-pipe
     * track drawing and split-screen H-int retain the full raster gate. */
    return g_ram[0xF600]==16 || word(0xFFD8)!=0;
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
        if (k>=2 || native_banks[k].count) continue;
        S2DonorLayout l=k==0?(S2DonorLayout){214,0x6FBE0,0x714E0,0x50000,0x29E2,8,0x1B618,2}:
            (S2DonorLayout){139,0x739E2,0x7446C,0x64320,0x29E2,8,0x1D038,2};
        char error[160];
        if (!s2_donor_decode(g_rom,0x100000,&l,&native_banks[k],error,sizeof error)) return 0;
    }
    if (apply) {
        io.pos=0; io.mode=2; runtime_state(&io);
        s2_options_gameplay_restored();
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
        saved[n]=g_ram[at+0x22]&EXTRA_CONTACT_BITS;
        if (!player_object(at)) g_ram[at+0x22]=(g_ram[at+0x22]&~EXTRA_CONTACT_BITS)|
            (solid_flags[p-2][n]&EXTRA_CONTACT_BITS);
    }
}
static void solid_end(unsigned p, const uint8_t saved[144])
{
    for (unsigned n=0;n<144;++n) {
        unsigned at=0xB000+n*0x40;
        if (player_object(at)) continue;
        solid_flags[p-2][n]=(solid_flags[p-2][n]&~EXTRA_CONTACT_BITS)|
            (g_ram[at+0x22]&EXTRA_CONTACT_BITS);
        g_ram[at+0x22]=(g_ram[at+0x22]&~EXTRA_CONTACT_BITS)|saved[n];
    }
}
static int extra_players(void)
{
    for (unsigned p=2;p<S2_MAX_PLAYERS;++p) if (objects[p]) return 1;
    return 0;
}
static int launcher_with_extras(int diagonal)
{
    unsigned launcher=g_cpu.A[0]&0xFFFF;
    if (!extra_players() || word(0xFFD8) || launcher<0xB400 || launcher>=0xD000 ||
        (launcher&63) || g_ram[launcher]!=0x85) return 0;

    /* The head and both original seats have already advanced. Give each
     * extra seat the same native contact handler and shared stroke. */
    unsigned index=(launcher-0xB000)/64, seat=launcher+0x37;
    uint32_t contact=diagonal?0x2AF06:0x2AD26;
    M68KState caller=g_cpu;
    unsigned occupied=0, buttons=0;
    for(unsigned p=2;p<S2_MAX_PLAYERS;++p) {
        unsigned player=objects[p];
        if(!player || !g_ram[player]) continue;
        uint8_t *extra=&solid_flags[p-2][index];
        unsigned state=*extra&EXTRA_LAUNCHER_STATE;
        if(!state && abs((int16_t)(word(player+8)-word(launcher+8)))>0x40) continue;
        uint8_t tag=g_ram[seat], saved[144];
        unsigned pad=(uint16_t)(genesis_sim_pad((int)p)<<8);
        solid_begin(p,saved);
        g_ram[seat]=(uint8_t)state;
        g_cpu=caller;
        g_cpu.A[1]=0xFFFF0000u|player;
        g_cpu.A[2]=0xFFFF0000u|seat;
        g_cpu.D[1]=0x23; g_cpu.D[2]=diagonal?8:0x20; g_cpu.D[3]=diagonal?5:0x1D;
        g_cpu.D[4]=word(launcher+8); g_cpu.D[5]=pad; g_cpu.D[6]=4;
        native_helper(contact);
        state=g_ram[seat];
        solid_end(p,saved);
        *extra=(*extra&~EXTRA_LAUNCHER_STATE)|state;
        g_ram[seat]=tag;
        occupied|=state;
        if(state==1) buttons|=pad;
    }
    g_cpu=caller;
    if(!occupied) return 0;
    if(g_ram[launcher+0x36]==1) buttons|=word(0xF602);
    if(g_ram[launcher+0x37]==1) buttons|=word(0xF606);
    if(word(launcher+0x38) && !(buttons&0x7000)) {
        putword(launcher+0x36,0x0202);
        for(unsigned p=2;p<S2_MAX_PLAYERS;++p) {
            uint8_t *flags=&solid_flags[p-2][index];
            if((*flags&EXTRA_LAUNCHER_STATE)==1) *flags=(*flags&~EXTRA_LAUNCHER_STATE)|2;
        }
    }
    return 1;
}
static void switch_extra_planes(void)
{
    unsigned trigger=g_cpu.A[0]&65535u;
    if(word(0xFFD8) || word(0xFE08) || !extra_players() ||
       trigger<0xB000 || trigger>=0xD000 || g_ram[trigger]!=3)return;
    unsigned routine=g_ram[trigger+0x24];
    if(routine!=2 && routine!=4)return;
    unsigned axis=routine==2?8:12;
    /* Obj03 owns only34/35 for P1/P2. Its remaining object bytes are unused;
     * keep the extra crossing sides in36..39 and an initialization tag in3A.
     * Native object allocation, current states and rollback carry them. */
    if(g_ram[trigger+0x3A]!=0xA7) {
        for(unsigned p=2;p<S2_MAX_PLAYERS;++p) {
            unsigned o=objects[p];
            g_ram[trigger+0x34+p]=o && g_ram[o] && word(o+axis)>word(trigger+axis);
        }
        g_ram[trigger+0x3A]=0xA7;
        return;
    }
    M68KState saved=g_cpu;
    for(unsigned p=2;p<s2_party.roster.slots && p<S2_MAX_PLAYERS;++p) {
        unsigned o=objects[p];if(!o || !g_ram[o])continue;
        /* Exact native single-character entry: A0 trigger, A1 actor, A2 its
         * side flag, D1 trigger axis. The original range, air, solid-plane
         * and art-priority decisions remain authoritative for this actor. */
        g_cpu=saved;g_cpu.A[1]=0xFFFF0000u|o;
        g_cpu.A[2]=0xFFFF0000u|(trigger+0x34+p);
        g_cpu.D[1]=(g_cpu.D[1]&0xFFFF0000u)|word(trigger+axis);
        native_helper(routine==2?0x1FDBE:0x1FEC8);
    }
    g_cpu=saved;
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
static void restore_player_inputs(const uint8_t saved[12], uint32_t poll, unsigned player)
{
    /* P1's native controller publishes Ctrl_1_Logical for world objects.
     * A companion instead borrows that logical slot temporarily. Physical
     * pads live at F604/F606; a real ReadJoypads call during this scope
     * replaces our substitutions with the next poll's actual edges. */
    uint16_t logical=word(0xF602),pad1=word(0xF604),pad2=word(0xF606);
    memcpy(g_ram+0xF602,saved+2,10);
    if (!player) putword(0xF602,logical);
    if (input_poll_serial!=poll) {putword(0xF604,pad1);putword(0xF606,pad2);}
}
static int update_player(unsigned p, uint32_t entry)
{
    unsigned object=g_cpu.A[0]&0xFFFF;
    if (inside_player) return 0;
    if(!p && !word(0xFE08) && word(0xFFFA) &&
       (g_ram[0xF605]&0x10) && (g_ram[object+0x24]==2 || g_ram[object+0x24]==4)) {
        /* Apply Obj01_Control/Hurt's entry before an imported controller can
         * consume B, including Obj02 which has no original cursor entry. */
        putword(0xFE08,1);g_ram[0xF7CC]=0;return 1;
    }
    if(!p && word(0xFE08)) {
        /* The cursor belongs to the leader, independently of its native
         * character controller. In particular Obj02 has no DebugMode entry.
         * Run the original cursor once; companions must never advance it. */
        displayed[p]=0;s2_character_reset(&characters[p],kind(p));
        native_helper(0x41A78);
        if(!word(0xFE08) && native_id(0)==2) {
            putfixed(object+4,0x739E2);putword(object+2,0x7A0);
            g_ram[object+0x16]=15;g_ram[object+0x17]=9;
        }
        return 1;
    }
    int companion=p && !word(0xFFD8);
    int initializing=!g_ram[object+0x24];
    unsigned previous_special=characters[p].special;
    displayed[p]=0;
    M68KState cpu=g_cpu;
    uint8_t inputs[12]; memcpy(inputs,g_ram+0xF600,sizeof inputs);
    uint32_t input_poll=input_poll_serial;
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
    int act_end=companion && s2_party.roster.slots>2 && locked &&
        (g_ram[0xF602]&8) && !g_ram[0xFE1E] && g_ram[0xB024]==2;
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
        g_ram[0xF7CC]=g_ram[0xF7CF]; /* native P2 lock; F7CD is starpost entry */
        if (companion) {
            companion_control(p,object,act_end);
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
    if (kind(p)==S2_CHAR_KNUCKLES && !p && g_ram[object+0x24]==2 &&
        !word(0xFFF0) && !word(0xFFD8) && !characters[p].special &&
        (g_ram[object+0x22]&2) && g_ram[object+0x3C] &&
        !(g_ram[object+0x2A]&1) && !g_ram[0xF7CC] &&
        (int16_t)word(object+0x12)>=((g_ram[object+0x22]&64)?-0x200:-0x400) &&
        (g_ram[0xF605]&0x70) && !g_ram[0xFE19] &&
        g_ram[0xFFB1]==7 && word(0xFE20)>=50 && g_ram[0xFE1E]) {
        /* KiS2 JumpHeight checks the new airborne press once the variable
         * jump-height interval ends. Transform before the same press can
         * start a glide; holding the first jump never transforms Knuckles. */
        putword(0xF65C,0); s_transforming=1; native_helper(0x1AB38); s_transforming=0;
        if(g_ram[0xFE19])characters[p].special=0;
    }
    if(!p && normal_super_edge(object)) {
        /* A fresh airborne edge owns transformation, never the original
         * unconditional apex check reached after the first jump. */
        normal_super_transform();
    }
    if (imported() && g_ram[object+0x24]==2) {
        S2Motion m=motion(object);
        int moved_early=0;
        if (s2_character_moves_before(&characters[p],&m)) {
            /* KiS2 $3156AE moves before climb/slide/ledge control. World bumpers
             * assign impulses after the previous player tick; apply one
             * movement before clearing velocity or applying slide friction. */
            host_call(0x163AC);
            m=motion(object);moved_early=1;
        }
        S2Contacts c=contacts(object);
        shadow_contacts(object,&c);
        unsigned input=g_ram[0xF7CC]?0xF602:0xF604;
        s2_character_before(&characters[p],&m,g_ram[input],g_ram[input+1],&c);
        store_motion(object,&m);
        shadow_effects(p,object,&m);
        g_ram[0xF604]=g_ram[0xF602]=characters[p].native_held;
        g_ram[0xF605]=g_ram[0xF603]=characters[p].native_pressed;
        if (characters[p].move!=S2_MOVE_NATIVE) {
            if (characters[p].move!=S2_MOVE_FIXED) {
                if(!moved_early)host_call(0x163AC); /* ObjectMove, no gravity */
                int gliding=kind(p)==S2_CHAR_KNUCKLES && characters[p].special==S2_SK_GLIDE;
                int falling=kind(p)==S2_CHAR_KNUCKLES && characters[p].special==S2_SK_FALL;
                int sliding=kind(p)==S2_CHAR_KNUCKLES && characters[p].special==S2_SK_SLIDE;
                if(falling) {
                    host_call(0x1A8E8); /* KiS2 steers/applies gravity after movement */
                    putword(object+0x12,word(object+0x12)+((g_ram[object+0x22]&64)?0x10:0x38));
                }
                if(gliding || falling || sliding)characters[p].glide_collision=(uint8_t)knuckles_glide_collision(object,gliding || sliding);
                else if(characters[p].move==S2_MOVE_AIR) {
                    host_call(0x1AEAA);
                    if(kind(p)==S2_CHAR_SHADOW)
                        s2_shadow_after_collision(&characters[p],m.vx,(int16_t)word(object+0x10));
                } else host_call(0x1E234);
            }
            host_call(0x1A974); /* LevelBound remains authoritative */
            g_ram[object+0x2A]|=1;
            injected_control=!(object_control&1);
        }
    }
    if(p && entry==0x19F50) {
        /* Obj01's debug entry and B shortcut belong to the leader. Enter
         * its ordinary state dispatcher directly for a Sonic-based follower
         * and express B as the equivalent native C jump. Keep the shared
         * debug globals intact, including across a mid-update frame yield. */
        entry=0x19F5C;
        for(unsigned a=0xF602;a<=0xF605;++a)
            if(g_ram[a]&0x10)g_ram[a]=(g_ram[a]&~0x10)|0x20;
    }
    /* Extra Tails bodies use private artwork. Preserve the fixed Obj05
     * accessory's native owner across their Obj02 initialization. */
    int extra_tails_init=p>=2 && initializing && native_id(p)==2;
    uint8_t tails_id=g_ram[0xD000];
    uint16_t tails_parent=word(0xD03E);
    recomp_call_addr(entry);
    if (extra_tails_init) {
        g_ram[0xD000]=tails_id;
        putword(0xD03E,tails_parent);
    }
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
        restore_player_inputs(inputs,input_poll,p);
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
        restore_player_inputs(inputs,input_poll,p); putword(0xF66A,logical2);
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
    if(pc==0x437A || pc==0x4380) {
        /* Pause can select Title before Level_MainLoop's last RunObjects.
         * Keep the custom controller and companion context for this exact
         * native call. The title's own object list uses its ordinary path. */
        s_departing_objects=pc==0x437A && active && !word(0xFFD8) &&
            (g_ram[0xF600]&127)==4 && s2_video_outgoing_level();
        return 0;
    }
    if(pc==0x9C7C) {
        if(knuckles_ending()) {
            s2_character_reset(&characters[0],S2_CHAR_KNUCKLES);displayed[0]=0;
            memset(published_sprites,0,sizeof published_sprites);
            memset(published_dust,0,sizeof published_dust);
        }
        return s2_video_hook(pc);
    }
    if(knuckles_ending()) {
        const S2KnucklesEnding *e=s2_resource_knuckles_ending();
        unsigned o=g_cpu.A[0]&65535u;
        if(pc==0xABE2) {
            /* KiS2 uses its ordinary ending for either emerald outcome. */
            putword(0xF750,0);
            glue_poke32(0xC00004,0x43200000); /* ArtTile_EndingCharacter=$19 */
            for(size_t i=0;i<e->tile_bytes;i+=2)
                glue_poke16(0xC00000,(uint16_t)(e->tiles[i]<<8|e->tiles[i+1]));
            return 1;
        }
        if(pc==0x9E14) {
            memcpy(g_ram+0xFB80,e->palette,sizeof e->palette);
            g_cpu.A[1]+=sizeof e->palette;
            g_cpu.A[2]=0xFFFFFC00u;
            g_cpu.D[0]=0xFFFF;
            recomp_tail_call(0x9E20);return 1;
        }
        if(pc==0x1332E && g_cpu.A[2]==0xAC7E) {
            unsigned bytes=((g_cpu.D[0]&65535u)+1)*2, dest=g_cpu.A[3]&65535u;
            if(bytes<=sizeof e->palette && dest>=0xFB80 && dest+bytes<=0xFC00) {
                memcpy(g_ram+dest,e->palette,bytes);
                g_cpu.A[2]+=bytes;g_cpu.A[3]+=bytes;
                g_cpu.D[0]|=0xFFFFu;
                recomp_tail_call(0x13334);return 1;
            }
        }
        if(pc==0x213E) {
            /* Original KiS2 cycle: three shades at palette entries 2,3,5.
             * Both the native large jump sprite and the host body use CRAM. */
            unsigned phase=g_ram[0xF65F],frame=word(0xF65C);
            const uint8_t *colors=e->super_palette;
            if(!phase)return 1;
            if(phase==1) {
                g_ram[0xF65F]=0xFF;putword(0xF65C,0);g_ram[0xB02A]=0;return 1;
            }
            if(!(phase&0x80)) {
                g_ram[0xF65F]=0;putword(0xF65C,0);colors+=60;
            } else {
                if((int8_t)--g_ram[0xF65E]>=0)return 1;
                g_ram[0xF65E]=2;
                if(frame>=60 || frame%6)frame=0;
                putword(0xF65C,frame+6);
                if(frame+6>=60) {putword(0xF65C,0);g_ram[0xF65E]=14;}
                colors+=frame;
            }
            memcpy(g_ram+0xFB04,colors,4);memcpy(g_ram+0xFB0A,colors+4,2);
            if(g_ram[0xF730]) {memcpy(g_ram+0xF084,colors,4);memcpy(g_ram+0xF08A,colors+4,2);}
            return 1;
        }
        if(pc==0xAC30) {recomp_tail_call(g_ram[0xFFB1]==7?0xAC56:0xAC42);return 1;}
        if(pc==0xA554) {
            /* KiS2 keeps the current character frame, then restarts Wait.
             * The preceding stock write instead forces Sonic's frame 1. */
            g_ram[0xB01A]=characters[0].frame;
            putword(0xB01C,0x0500);characters[0].animation=0;
            return 0;
        }
        if(pc==0x166CC && o>=0xB000 && o<0xD400 && word(o+4)==0 && word(o+6)==S2_ENDING_JUMP_MAP) {
            unsigned frame=g_ram[o+0x1A],count=g_cpu.D[5]&255u;
            if(frame<S2_ENDING_FRAMES && count<80) {
                unsigned added=s2_ending_draw_sat(&e->frames[frame],g_ram+(g_cpu.A[2]&65535u),count,
                    word(o+2),g_cpu.D[4]&3u,(int16_t)g_cpu.D[3],(int16_t)g_cpu.D[2]);
                g_cpu.A[2]+=added*8;
                g_cpu.D[5]=(g_cpu.D[5]&~255u)|(count+added);
            }
            recomp_tail_call(0x166EC);return 1;
        }
        if(o==0xB000) {
            if(pc==0x164F4) {displayed[0]=1;return 1;}
            if(pc==0x1B350) {
                S2Motion m=motion(o);s2_character_animate(&characters[0],&m,bank(0));
                g_ram[o+0x1A]=m.frame;
                g_ram[o+1]=(g_ram[o+1]&~3)|characters[0].render_flip|0x80;
                return 1;
            }
            if(pc==0x1B848)return 1;
        }
    }
    if (pc==0x41C3A) {
        /* Debug_SpawnObject copies the cursor's on-screen flag before the
         * new object has mappings. While the player is dead, RunObjects
         * dispatches display-only objects and would interpret a null mapping
         * as sprite data, overflowing the sprite table into global RAM.
         * Match the disassembly's fixBugs guard: initialization must publish
         * the first visible sprite. Preserve the cursor and copied status. */
        unsigned spawned=g_cpu.A[1]&65535u;
        if(spawned>=0xB000 && spawned<0xD400 && !(spawned&0x3Fu))
            g_ram[spawned+1]&=0x7F;
        return 0;
    }
    if (pc==0x111C) {
        ++input_poll_serial;
        if(s2_party.easy_debug) g_ram[0xFFFA]=1;
        return 0;
    }
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
    if(pc==0x771C) {
        /* SSStartNewAct has selected its native table and index. Substitute
         * the verified KiS2 solo requirement before the ordinary goal/BCD
         * writer. Keep ROM immutable across boot, quickload and rollback. */
        const S2StageLayouts *original=s2_resource_knuckles_stages();
        unsigned index=(uint16_t)g_cpu.D[0];
        if(word(0xFF70) && kind(0)==S2_CHAR_KNUCKLES && original &&
           original->special_ring_requirements_ready && index<28)
            g_cpu.D[1]=(g_cpu.D[1]&0xFFFF0000u)|original->special_ring_requirements[index];
        return 0;
    }
    if (pc==0x4F64) {
        /* KiS2 keeps the starpost's saved ring/extra-life pool. The stars
         * do not save it again, so rings collected afterwards are excluded. */
        ss_return_pending=kind(0)==S2_CHAR_KNUCKLES && !word(0xFFD8) &&
            g_ram[0xFE31] && g_ram[0xFE30]==g_ram[0xFE31];
        if(ss_return_pending) {
            ss_return_zone=word(0xFE10);ss_return_rings=word(0xFE36);
            ss_return_post=g_ram[0xFE31];ss_return_lives=g_ram[0xFE54];
        }
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
        if(ss_return_pending && (kind(0)!=S2_CHAR_KNUCKLES || word(0xFFD8) ||
           word(0xFE10)!=ss_return_zone || g_ram[0xFE31]!=ss_return_post))ss_return_pending=0;
        s2_video_hook(pc); /* title-card initialization installs a fresh palette directly */
        for(unsigned p=0;p<s2_party.roster.slots && p<2;++p)prepare_native_bank(kind(p));
        active=g_ram[0xF600]!=0x88 && (!vanilla() || s2_runtime_campaign_independent());
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
    if(pc==0x1F384) {
        /* The stock ring/extra-life clears have completed. Restore only the
         * matching Knuckles Special Stage return, leaving native MOVE.L and
         * its flags/register contract untouched. Ordinary retries stay stock. */
        if(ss_return_pending) {
            if(kind(0)==S2_CHAR_KNUCKLES && !word(0xFFD8) &&
               (g_ram[0xF600]&127)==12 && word(0xFE10)==ss_return_zone &&
               g_ram[0xFE30]==ss_return_post && g_ram[0xFE31]==ss_return_post) {
                putword(0xFE20,ss_return_rings);g_ram[0xFE1B]=ss_return_lives;
            }
            ss_return_pending=0;
        }
        return 0;
    }
    if(!active && g_ram[0xF600]==12 && (g_cpu.A[0]&65535)==0xB000) {
        /* The original Sonic+Tails party keeps its native controller/AI.
         * The input correctness fix applies there too, without enabling the
         * expanded-party adapter or changing ordinary world execution. */
        if(pc==0x1AB38)return !s_transforming;
        if(pc==0x19F50 && normal_super_edge(0xB000))normal_super_transform();
    }
    if (!level()) return 0;
    if(pc==0x1FCEA) {switch_extra_planes();return 0;}
    if(pc==0x3AB46 && kind(0)==S2_CHAR_KNUCKLES && knuckles_available()) {
        /* The same KiS2 pose correction applies after boarding the plane. */
        g_ram[0xB01A]=characters[0].frame;
        putword(0xB01C,0x0500);characters[0].animation=0;
        return 0;
    }
    if(pc==0x3AC56 && kind(0)==S2_CHAR_KNUCKLES && knuckles_available()) {
        /* KiS2 ObjB2_Waiting_animation resets the animation instead of
         * forcing Sonic's frame 1 into Knuckles' unrelated walking art. */
        g_cpu.A[1]=0xFFFFB000u;
        characters[0].animation=0;
        putword(0xB01C,0x0500);putword(0xB01E,0x0100);
        putword(0xB010,0);putword(0xB012,0);
        g_cpu.SR=(g_cpu.SR&~(SR_N|SR_Z|SR_V|SR_C))|SR_Z;
        return 1;
    }
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
            characters[p].special==S2_SK_SLIDE) {
            if (pc==0x1DE4A) {
                /* Original KiS2 CheckSkid also accepts slide state 3.
                 * Its separate glide contact result need not clear the
                 * ordinary in-air status bit; do not add that condition. */
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
            if (id!=solid_ids[n]) {
                for(unsigned p=0;p<S2_MAX_PLAYERS-2;++p) solid_flags[p][n]=0;
                solid_ids[n]=(uint8_t)id;
            }
        }
        return 0;
    }
    if (pc==0x2ACE0 || pc==0x2AEC0) return launcher_with_extras(pc==0x2AEC0);
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
        unsigned o=g_cpu.A[0]&0xFFFF;
        /* Catch-up can still be marked active when a companion dies. Keep
         * the native death animation until CheckGameOver releases the
         * object into the ordinary recovery routine. */
        if (g_ram[o+0x24]==2 && companion_recovering() && native_id((unsigned)actor)!=2)
            g_ram[o+0x1C]=2;
        return 1;
    }
    if (pc==0x1ABA6 || pc==0x1AB38) {
        if (!inside_player) return 0;
        if (actor>0 || word(0xFFD8)) return 1; /* never companions or VS */
        if (pc==0x1AB38) return !s_transforming;
        return 0; /* native eligibility, power-up, countdown and ring drain */
    }
    if (pc==0x164F4 && !inside_player && kind(0)==S2_CHAR_TAILS && custom_super_palette() &&
        (g_cpu.A[0]&65535)==0xD000 && g_ram[0xD000]==5 && word(0xD03E)==0xB000) {
        displayed_tails=1; return 1;
    }
    if(pc==0x164F4 && word(0xFE08) && (g_cpu.A[0]&65535)==0xD000 &&
       g_ram[0xD000]==5 && word(0xD03E)==0xB000)return 1;
    if (pc==0x164F4 && (g_cpu.A[0]&65535)==0xB000 &&
        (word(0xFE08)>>8)==2) return 0; /* native debug-object cursor */
    if (pc==0x164F4 && !inside_player) {
        unsigned o=g_cpu.A[0]&65535;
        if (g_ram[o]==8 && g_ram[o+0x2E]==0x47) return 1;
        /* RunObjectDisplayOnly freezes the dynamic pool during P1 death.
         * Its submissions bypass update_player, but donor/extra frames must
         * still never enter native Sonic mappings or shared VRAM slots. */
        for (unsigned p=0;p<S2_MAX_PLAYERS;++p) if (objects[p]==o &&
            (p>=2 || kind(p)>=S2_CHAR_AMY)) {
            if(!p && kind(0)==S2_CHAR_MIGHTY)host_actor_visibility(o);
            displayed[p]=1;return 1;
        }
    }
    if (pc==0x164F4 && inside_player) {
        if ((g_cpu.A[0]&0xFFFF)==objects[actor]) {
            if (actor>0 && !word(0xFFD8) && word(0xF708)==2) return 1;
            /* Native plungers use this flag while they control P1. Mighty
             * borrows Sonic's controller but skips the native sprite list. */
            if(actor==0 && kind(0)==S2_CHAR_MIGHTY)host_actor_visibility(objects[0]);
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
    if (pc==0x1CB5C) {
        unsigned player=g_cpu.A[0]&0xFFFFu;
        /* RideObject_SetRide selects Tails' landing reset for sidekick
         * slots, even when their actual controller is Sonic's. Preserve
         * the native reset and its foot position adjustment for that body. */
        if (player_object(player) && g_ram[player]==1) {
            recomp_tail_call(0x1B0AC);
            return 1;
        }
        return 0;
    }
    if (inside_player && actor>=2 && (pc==0x1B848 || pc==0x1D1AC)) {
        /* Extra seats draw from decoded poses. Their DPLCs would replace
         * the tiles and cached pose shared by the hardware Sonic/Tails. */
        return 1;
    }
    if (pc==0x1B848 && inside_player && kind((unsigned)actor)==S2_CHAR_MIGHTY) return 1;
    if (!inside_player || !imported()) return 0;
    unsigned o=g_cpu.A[0]&0xFFFF;
    if(pc==0x1A92E && kind(actor)==S2_CHAR_KNUCKLES && !word(0xFFF0)) {
        /* Native air steering has not stored x_vel yet. Recreate KiS2's
         * uncapped steering from that original word, retaining native face
         * changes and every other register. The following MOVE supplies
         * NZVC; only its source word and the arithmetic's X differ. */
        uint8_t extend=!!(g_cpu.SR&0x10);
        int16_t speed=s2_knuckles_air_speed((int16_t)word(o+0x10),
            (uint16_t)g_cpu.D[5],(uint16_t)g_cpu.D[6],g_ram[0xF602],&extend);
        g_cpu.D[0]=(g_cpu.D[0]&0xFFFF0000u)|(uint16_t)speed;
        g_cpu.SR=(g_cpu.SR&~0x10u)|(extend?0x10u:0);
        return 0;
    }
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
    if(knuckles_ending()) {
        memset(published_sprites,0,sizeof published_sprites);
        memset(published_dust,0,sizeof published_dust);
        unsigned shown=displayed[0];displayed[0]=0;
        if(g_ram[0xB000]==1 && shown) {
            PartySprite *s=&published_sprites[0];
            s->x=(int16_t)word(0xB008);s->y=(int16_t)word(0xB00C);
            s->art=word(0xB002);s->frame=g_ram[0xB01A];s->visible=1;
            s->flip=characters[0].render_flip;
        }
        return;
    }
    /* BuildSprites runs after the complete object tick. Rendering live actor
     * RAM per scanline could use one pose for a head and the next for its body,
     * or observe displayed[] while an actor was being updated. */
    /* When the displayed level is fading out, keep its final actor
     * publication with the native SAT until the incoming scene replaces it. */
    int native_only=!active && g_ram[0xF711] && !word(0xFFD8) &&
        (g_ram[0xB000]==1 || g_ram[0xB000]==2);
    if (!level() && (presented_level() || (native_only && (g_ram[0xF600]&127)!=12))) return;
    memset(published_sprites,0,sizeof published_sprites);
    memset(published_dust,0,sizeof published_dust);
    if (!level() && !native_only) return;
    /* Extra party roles have no fixed native dust object/VRAM strip. Create
     * the same short-lived Obj08 children after the object tick, respecting
     * its pool/air constraints. Native animation/deletion still own them.
     * Slide state owns ground contact separately from the in-air status bit.
     * Their original 16-tile skid atlas is read privately below, so a Sonic
     * companion's simultaneous spindash cannot overwrite these clouds. */
    for (unsigned p=2;p<s2_party.roster.slots && p<S2_MAX_PLAYERS;++p) {
        unsigned o=objects[p];
        if (!o || kind(p)!=S2_CHAR_KNUCKLES || characters[p].special!=S2_SK_SLIDE ||
            (characters[p].age&3) || dust_emit_age[p]==characters[p].age ||
            g_ram[o+0x24]!=2 || g_ram[o+0x28]<12) continue;
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
        unsigned o=native_only?(p<2 && p<s2_party.roster.slots?0xB000+p*64:0):objects[p];
        if(!o || !g_ram[o])continue;
        if(!p && word(0xFE08))continue;
        int display_x=s2_video_runoff_x(p,o,word(o+8));
        int runoff=s2_video_runoff_velocity(p);
        if (p<2 && kind(p)<S2_CHAR_AMY && displayed[p]!=2 && !runoff) continue;
        if(native_only && !runoff)continue;
        PartySprite *s=&published_sprites[p];
        s->x=(int16_t)display_x; s->y=(int16_t)word(o+12); s->art=word(o+2);
        s->frame=g_ram[o+0x1A]; s->visible=displayed[p];
        s->flip=kind(p)==S2_CHAR_AMY || kind(p)==S2_CHAR_KNUCKLES || kind(p)==S2_CHAR_SHADOW?characters[p].render_flip:g_ram[o+1]&3;
        s->wet=!!(g_ram[o+0x22]&64);
        if(p && !word(0xFFD8) && kind(p)==S2_CHAR_KNUCKLES &&
           companion_cpu[p][6]==0 && companion_cpu[p][7]==4) {
            /* Native recovery owns the flight and rejoin. Only its visible
             * pose differs: Knuckles glides instead of using roll artwork. */
            s->frame=0xC0;s->flip=g_ram[o+0x22]&1;
        }
        if(runoff) {
            s->visible=1;
            /* A glide can cross the signpost and reach the native camera
             * limit before Knuckles lands. Keep the published air pose. */
            if (kind(p)==S2_CHAR_KNUCKLES && (g_ram[o+0x22]&2)) continue;
            /* Only the offscreen continuation changes pose. Native movement,
             * rolling flags, radius, animation state and tally remain intact. */
            S2Motion m=motion(o);S2CharacterState gait=characters[p];
            const S2DonorBank *b=bank(p);
            m.animation=0;m.angle=m.flip_angle=0;m.inertia=(int16_t)runoff;
            m.status&=~(1u|32u); /* cosmetic continuation faces right, without a stale push pose */
            gait.special=0;gait.animation=0;gait.animation_timer=0;
            unsigned anim=runoff>=0x600?1:0;
            unsigned length=b && anim<b->animation_count?b->animation_length[anim]:0;
            unsigned delay=runoff<0x800?(0x800-runoff)>>8:0;
            gait.animation_index=length>2?(word(0xFE04)/(delay+1))%(length-2):0;
            s2_character_animate(&gait,&m,b);
            s->frame=m.frame;s->flip=gait.render_flip;
            if(g_ram[o+0x16]<19)s->y-=19-g_ram[o+0x16];
        }
    }
    if (displayed_tails && kind(0)==S2_CHAR_TAILS) {
        PartySprite *s=&published_sprites[S2_MAX_PLAYERS];
        s->x=(int16_t)(word(0xD008)+s2_video_runoff_x(0,0xB000,word(0xB008))-word(0xB008)); s->y=(int16_t)word(0xD00C); s->art=word(0xD002);
        s->frame=g_ram[0xD01A]; s->visible=1; s->flip=g_ram[0xD001]&3;
    }
}
void s2_runtime_published_actors(S2RuntimeActors *out)
{
    memset(out,0,sizeof *out);
    memcpy(out->sprites,published_sprites,sizeof out->sprites);
    memcpy(out->dust,published_dust,sizeof out->dust);
    for(unsigned p=0;p<S2_MAX_PLAYERS;++p) {
        unsigned o=objects[p];
        if(!o || !g_ram[o])continue;
        out->object[p]=(uint16_t)o;
        out->x[p]=(int16_t)word(o+8);out->y[p]=(int16_t)word(o+12);
    }
}
static unsigned card_vram_word(const GVDP *v,unsigned a)
{ return (v->vram[a&65535u]<<8)|v->vram[(a+1)&65535u]; }
static void green_title_card(const GVDP *v,int line,uint32_t *out,int width,uint16_t ink)
{
    /* KiS2 ink 4 is green ($080), while the mixed-party host must keep
     * Sonic's blue palette available. Recolour only the original card's two
     * solid plane-A tiles; retain overlying text and red edges. Retail
     * uses ink3, while imported title-card art may use ink4. */
    uint32_t old=genesis_dac_cram_to_argb(v->cram[(v->vram[0xBB40]>>4)&15],GENESIS_DAC_NORMAL);
    if (!(old&255)) return;
    uint32_t color=genesis_dac_cram_to_argb(ink,GENESIS_DAC_NORMAL);
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
    uint32_t probe;
    if (!s2_video_knuckles_hud_pixel(v,0x7D4,0,0,&probe) ||
        word(0xFFD8) || !presented_level() || !g_ram[0xF711]) return;
    int origin=s2_video_hud_origin(width);
    if (origin<=-width) return;
    /* The expanded renderer owns the complete HUD substitution. Replaying
     * native SAT positions here would also redraw world rewards that share
     * the life-icon tiles, using the HUD's different horizontal origin.
     * Native output still removes the old shape before donor actors and
     * restores the high-priority glyphs through a private VDP copy. */
    if (sonic2_video.enabled()) return;
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
static S2RuntimePresentation s_overlay_frame;
static int actor_lerp(int a,int b,double phase)
{ return a+(int)((b-a)*phase+((b>=a)?0.5:-0.5)); }
static int actor_delta_y(int difference)
{ return ((difference+1024)&2047)-1024; }
static void runtime_overlay_frame(const GVDP *v,int line,uint32_t *out,int width,
                                  const S2RuntimePresentation *picture,const S2RuntimePresentation *previous,
                                  unsigned actors,double phase)
{
    const GVDP *visibility=previous?v:&picture->vdp;
    if (picture->green_card && (v->reg[1]&64)) green_title_card(visibility,line,out,width,picture->card_color);
    s2_video_special_actor(v,line,out,width);
    s2_video_special_results(v,line,out,width);
    if (s2_video_competition_divider(v,line,out,width)) return;
    if (!picture->active || !(v->reg[1]&64)) return;
    v=s2_video_slots_overlay(v,line,out,width);
    int overlay_drawn=picture->green_card;
    knuckles_hud(v,line,out,width,0);
    competition_hud(v,line,out,width,0);
    s2_video_results_name(v,line,out,width,0);
    int origin_x=picture->left,origin_y=picture->top;
    if(previous) {origin_x=actor_lerp(previous->left,picture->left,phase);origin_y=actor_lerp(previous->top,picture->top,phase);}
    if (picture->vs) {
        /* Native interlace pass is 448 rows. Each competitor's 224-row world
         * view contains both characters, with the stock central SAT mask. */
        if (line>=216 && line<224) return;
        if (line>=224) { origin_x=picture->vs_left; origin_y=picture->vs_top; }
    }
    s2_video_preserve_foreground_row(out,width);
    for (int index=S2_MAX_PLAYERS;index>=0;--index) {
        unsigned p=index==S2_MAX_PLAYERS?0:(unsigned)index;
        const PartySprite *s=&picture->sprites[index];
        if (!s->visible) continue;
        const S2DonorBank *b=bank(p); unsigned frame=s->frame;
        unsigned actor_kind=kind(p);
        int shadow=actor_kind==S2_CHAR_SHADOW;
        if (actor_kind==S2_CHAR_MIGHTY) frame=s2_mighty_frame(frame);
        unsigned zone=picture->world[0xFE10];
        /* Native H-int split, including a partly submerged body. The physics
         * underwater flag describes the center, not the current scanline. */
        int wet=!picture->vs && s2_video_water_palette(v);
        const uint16_t *donor_water=NULL;
        if(wet && b && b->water_palette_count)
            donor_water=zone==15 && b->water_palette_count>1?b->water_palette_alt:b->water_palette;
        const S2DonorFrame *f=b && frame<b->count?&b->frames[frame]:NULL;
        if (!f) continue;
        int wrap=(picture->world[0xF600]&127)==12;
        int x=s->x-origin_x,y=s->y-origin_y;
        /* Native level sprites wrap vertically through the 2048px world.
         * MTZ can publish a negative camera while the actor still has its
         * wrapped positive coordinate. Subtracting them directly loses the
         * entire donor body despite valid movement and display submission. */
        if(wrap)y=actor_delta_y(y);
        if(previous && (actors&(1u<<index))) {
            const PartySprite *old=&previous->sprites[index];
            x=actor_lerp(old->x,s->x,phase)-origin_x;
            if(wrap)y+=actor_lerp(actor_delta_y(old->y-s->y),0,phase);
            else y=actor_lerp(old->y,s->y,phase)-origin_y;
        }
        unsigned flip=s->flip;
        int fy=line-y-(flip&2?-f->y-f->height:f->y);
        if (fy<0 || fy>=f->height) continue;
        if (flip&2) fy=f->height-1-fy;
        int x0=x+(flip&1?-f->x-f->width:f->x);
        uint32_t palette[16];
        unsigned phase=!p && !picture->vs?picture->world[0xF65F]:0;
        unsigned super_frame=((unsigned)picture->world[0xF65C]<<8)|picture->world[0xF65D];
        const S2KnucklesArt *ending_art=(picture->world[0xF600]&127)==0x20 &&
            actor_kind==S2_CHAR_KNUCKLES?s2_resource_knuckles_art():NULL;
        int ending=ending_art!=NULL;
        uint16_t ending_palette[16];
        if(ending) {
            /* S3&K's body and KiS2's native ending art index the same
             * colours differently, as with the original water palettes. */
            s2_donor_water_palette(b->palette,ending_art->palette,v->cram,ending_palette);
        }
        for(unsigned color=0;color<16;++color) {
            uint16_t shade=ending?ending_palette[color]:shadow?s2_shadow_color(b,color,wet,phase,super_frame):
                actor_kind<S2_CHAR_AMY?v->cram[color]:donor_water?donor_water[color]:b->palette[color];
            if(!ending && !shadow && phase)shade=s2_super_color(kind(p),color,shade,phase,super_frame,b);
            if(!ending && actor_kind>=S2_CHAR_AMY)shade=s2_video_donor_color(shade);
            palette[color]=genesis_dac_cram_to_argb(shade,GENESIS_DAC_NORMAL);
        }
        for (unsigned px=0;px<f->width;++px) {
            int dx=x0+px; if (dx<0 || dx>=width) continue;
            unsigned color=f->pixels[fy*f->width+(flip&1?f->width-1-px:px)];
            if (!color) continue;
            if ((picture->world[0xF600]&127)==0x20) {
                /* The cutscene owns the actor's screen position. */
            } else if (!picture->world[0xF711]) {
                if (!s2_video_intro_pixel_visible(visibility,dx-(width-320)/2,line,s->art&0x8000)) continue;
            } else if (!s2_video_actor_pixel_visible(visibility,dx+origin_x,line+origin_y,s->art&0x8000,picture->world)) continue;
            out[dx]=palette[color];overlay_drawn=1;
        }
    }
    for (unsigned n=0;n<DUST_SLOTS;++n) {
        const PartySprite *s=&picture->dust[n]; if (!s->visible) continue;
        int y=s->y-origin_y;
        if((picture->world[0xF600]&127)==12)y=actor_delta_y(y);
        int row=line-(y-8); if (row<0 || row>=16) continue;
        int fy=s->flip&2?15-row:row;
        for (int px=0;px<16;++px) {
            int dx=s->x-origin_x-8+px; if (dx<0 || dx>=width) continue;
            int fx=s->flip&1?15-px:px;
            /* REV01 Obj08 frames17..20: one 2x2 piece at(-8,-8), tiles
             * 0/4/8/12. DPLC21=$F0BA loads this exact static 16-tile atlas. */
            unsigned tile=(s->frame-17)*4+(fx/8)*2+fy/8;
            unsigned byte=g_rom[0x7373C+tile*32+(fy&7)*4+(fx&7)/2];
            unsigned ink=fx&1?byte&15:byte>>4;
            if (ink && s2_video_actor_pixel_visible(visibility,dx+origin_x,line+origin_y,s->art&0x8000,picture->world)) {
                out[dx]=genesis_dac_cram_to_argb(v->cram[((s->art>>9)&48)+ink],GENESIS_DAC_NORMAL);
                overlay_drawn=1;
            }
        }
    }
    /* Native UI is already in out. Reapply it on modified rows and retain
     * each viewport's first-row selection of the displayed SAT publication. */
    if(overlay_drawn || !line || (picture->vs && line==224))
        s2_video_front_ui(v,line,out,width);
    s2_video_results_name(v,line,out,width,1);
    knuckles_hud(v,line,out,width,1);
    competition_hud(v,line,out,width,1);
}
void s2_runtime_overlay(const GVDP *v,int line,uint32_t *out,int width)
{
    S2RuntimePresentation *f=&s_overlay_frame;
    if(!line) {
        int native_runoff=!active && g_ram[0xF711] && !word(0xFFD8) &&
            (g_ram[0xB000]==1 || g_ram[0xB000]==2) &&
            (published_sprites[0].visible || published_sprites[1].visible);
        /* Credits remain inside EndingSequence but stop building objects.
         * Their native clear retires the actor even without another sprite
         * publication; do not keep its last pose over the credits slides. */
        int ending_actor=knuckles_ending() && g_ram[0xB000]==1;
        f->active=(presented_level() || native_runoff || ending_actor) && (g_ram[0xF711] || !word(0xFFD8));
        memcpy(f->sprites,published_sprites,sizeof f->sprites);
        memcpy(f->dust,published_dust,sizeof f->dust);
        f->vdp=*v; memcpy(f->world,g_ram,sizeof f->world);
        s2_video_presented_actors(&f->vdp,f->sprites,f->dust);
        if(f->active) {
            unsigned zone=s2_video_level_zone();
            f->world[0xFE10]=(uint8_t)(zone>>8);f->world[0xFE11]=(uint8_t)zone;
        }
        f->vs=word(0xFFD8);
        f->green_card=0;
        uint16_t card_ink=s2_runtime_title_card_ink();
        f->card_color=card_ink;
        if (!f->vs && (g_ram[0xF600]&127)==12 && card_ink &&
            (f->vdp.vram[0xBB40]==0x33 || f->vdp.vram[0xBB40]==0x44) && f->vdp.vram[0xBB7F]==f->vdp.vram[0xBB40]) {
            /* The actual fill is plane A, not a sprite. Follow its displayed
             * tilemap through the exit, after producer objects can be gone. */
            unsigned table=(f->vdp.reg[2]&0x38u)<<10;
            for (unsigned cell=0;cell<64*32;++cell) {
                unsigned attr=card_vram_word(&f->vdp,table+cell*2),tile=attr&2047u;
                if ((tile==0x5DA || tile==0x5DB) && !(attr&0x6000u)) { f->green_card=1; break; }
            }
        }
        s2_video_actor_origin(&f->vdp,0,width,&f->left,&f->top);
        if (!f->world[0xF711] && !s2_video_intro_world_visible()) {
            /* Title-card scroll belongs to the card, not the actor camera.
             * Native actors are submitted before Level_started becomes true. */
            f->left=word(0xEE60)-(width-320)/2; f->top=(int16_t)word(0xEE64);
        }
        if (f->vs) {
            if (s2_video_competition_extended()) {
                s2_video_actor_origin(v,0,width,&f->left,&f->top);
                s2_video_actor_origin(v,224,width,&f->vs_left,&f->vs_top);
            } else {
                f->left=word(0xEE60)-(width-320)/2; f->top=(int16_t)word(0xEE64);
                f->vs_left=word(0xEE80)-(width-320)/2; f->vs_top=(int16_t)word(0xEE84)-224;
            }
        }
    }
    runtime_overlay_frame(v,line,out,width,f,NULL,0,1);
}
void s2_runtime_presentation_capture(S2RuntimePresentation *out)
{ *out=s_overlay_frame; }
void s2_runtime_overlay_view(const GVDP *v,int line,uint32_t *out,int width)
{
    S2RuntimePresentation *f=&s_overlay_frame;
    int left=f->left,top=f->top;
    s2_video_actor_origin(v,line,width,&f->left,&f->top);
    runtime_overlay_frame(v,line,out,width,f,NULL,0,1);
    f->left=left;f->top=top;
}
void s2_runtime_presentation_row(const S2RuntimePresentation *a,const S2RuntimePresentation *b,
                                unsigned actors,double phase,const GVDP *v,int line,uint32_t *out,int width)
{ runtime_overlay_frame(v,line,out,width,b,a,actors,phase); }

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
    RB(io,c->landing_buffer); RB(io,c->landing_jump);
    RB(io,c->animation); RB(io,c->animation_index); RB(io,c->animation_timer);
    RB(io,c->frame); RB(io,c->render_flip); RB(io,c->animation_changed);
    RB(io,c->shadow_air_used); RB(io,c->shadow_target); RB(io,c->shadow_last_target); RB(io,c->shadow_target_age);
    RB(io,c->shadow_chain); RB(io,c->shadow_chaos); RB(io,c->source_animation); RB(io,c->shadow_super);
    RB(io,c->shadow_monitor_ticks);
    RB(io,c->climb_delta);
}
static void rb_all(S2StateIO *io)
{
    sound_state(io);
#ifdef SONIC2_MUSIC_DRIVER
    s2_cd_theme_state(io);
#endif
    s2_options_rb_state(io);
    int32_t a=active, ip=inside_player, ac=actor, icc=inside_companion_cpu, is=inside_solid;
    RB(io,a); RB(io,ip); RB(io,ac); RB(io,icc); RB(io,is);
    RB(io,previous_input); RB(io,displayed); RB(io,companion_cpu);
    RB(io,ss_return_zone); RB(io,ss_return_rings);
    RB(io,ss_return_post); RB(io,ss_return_lives); RB(io,ss_return_pending);
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
    int32_t g[7]={s_inside_spring,s_inside_palette,s_reward,s_checking_death,
                  s_transforming,s_combat,s_departing_objects};
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
        s_checking_death=g[3]; s_transforming=g[4]; s_combat=g[5]; s_departing_objects=g[6];
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
