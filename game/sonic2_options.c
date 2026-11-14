#include "sonic2_options.h"
#include "sonic2_sound_driver.h"
#include "sonic2_party.h"
#include "sonic2_resources.h"
#include "sonic2_runtime.h"
#include "sonic2_save_menu.h"
#include "sonic2_state_io.h"
#ifdef SONIC2_MUSIC_DRIVER
#include "sonic2_music.h"
#include "sonic2_cd_themes.h"
#include "audio/ym2612.h"
#include "audio/sn76489.h"
#endif
#include "genesis_runtime.h"
#include "video/genesis_vdp.h"
#include "video/genesis_dac.h"
#include <stdio.h>
#include <string.h>
#if GENESIS_HAS_RECOMP_NET
#include "netplay/genesis_netplay.h"
#endif

static unsigned s_cursor;
static int s_ready;
static unsigned s_panel_age,s_departing,s_bg_frame;
enum { NOTICE_NONE, NOTICE_VS_PLAYERS, NOTICE_SAVE_FAILED, NOTICE_KIS2 };
enum { SOUND_ROW=S2_MAX_PLAYERS+1,
#ifdef SONIC2_MUSIC_DRIVER
       PLAYER_ROW,
#else
       PLAYER_ROW=SOUND_ROW,
#endif
       LEVEL_ROW, BACK_ROW, OPTIONS_ROWS };
static unsigned s_notice;
#ifdef SONIC2_MUSIC_DRIVER
static unsigned s_player_open,s_player_track,s_player_solo,s_player_stopped,s_player_pending,s_player_delay;
static const char *const player_channels[]={"ALL CHANNELS","FM 1","FM 2","FM 3","FM 4","FM 5","FM 6","DAC","PSG 1","PSG 2","PSG 3 / NOISE"};
static void player_mask(void)
{
 unsigned fm=0x7f,psg=15;
 if(s_player_open) {
  if(s_player_stopped)fm=psg=0;
  else if(s_player_solo){fm=s_player_solo<=7?1u<<(s_player_solo-1):0;psg=s_player_solo>=8?(s_player_solo==10?12u:1u<<(s_player_solo-8)):0;}
 }
 ym2612_set_output_mask(fm);psg_set_output_mask(psg);
 s2_cd_theme_audition(s_player_open?s_player_track:S2_MUSIC_COUNT,s_player_open?s_player_solo:0,s_player_open&&s_player_stopped);
}
static int player_used(unsigned t)
{return strcmp(s2_theme_ids[t],"ending")&&strcmp(s2_theme_ids[t],"hidden-palace");}
static void player_change(int direction)
{
 do{s_player_track=(s_player_track+S2_MUSIC_COUNT+direction)%S2_MUSIC_COUNT;}while(!player_used(s_player_track));
}
static void player_queue(unsigned native)
{
 /* A native stop ends a preceding 1-UP backup before the next preview. */
 g_ram[0xFFE0]=0xFD;s_player_pending=native;s_player_delay=2;
}
static void player_play(void)
{
 s_player_stopped=0;s_player_solo%=s2_cd_theme_channel_count(s_player_track);player_mask();player_queue(s2_music_target(s_player_track)+0x80);
}
static void player_close(void)
{s_player_open=s_player_stopped=s_player_solo=0;player_mask();player_queue(0x91);}
#endif
void s2_options_gameplay_restored(void)
{
    /* File snapshots resume gameplay loops. Discard the current transient
     * menu/audition without queuing music over the restored sequencer. */
    s_ready=s_panel_age=s_departing=0;
#ifdef SONIC2_MUSIC_DRIVER
    s_player_open=s_player_stopped=s_player_solo=s_player_pending=s_player_delay=0;
    player_mask();
#endif
}
static int missing_knuckles_campaign(void)
{
    return !strcmp(s2_party.roster.character[0],"knuckles") &&
        (!s2_party.knuckles_campaign_enabled || !s2_resource_knuckles_stages());
}

static int online(void)
{
#if GENESIS_HAS_RECOMP_NET
    return genesis_netplay_active();
#else
    return 0;
#endif
}

void s2_options_load(const char *settings_path)
{
    s2_sound_load_settings(settings_path);
    s2_party_load(settings_path);
    s2_resources_load(settings_path);
    if (!s2_resource_verified(S2_RESOURCE_SK)) s2_party.s3k_enabled = 0;
    if (!s2_resource_save_assets()) s2_party.save_menu_enabled=0;
    if (!s2_resource_verified(S2_RESOURCE_MEGAMIX)) s2_party.mighty_enabled=0;
    s2_save_menu_load(settings_path);
    s2_runtime_load();
    s2_roster_validate(&s2_party.roster);
}

int s2_options_netplay_allowed(void)
{
    /* A validated party of up to six runs online: its roster travels in the
     * session config image (s2_netplay_config_image) and every peer adopts
     * the host's. The campaign save menu stays local-only: it reads and
     * writes the player's own campaign file, which no peer shares. */
    S2Roster r = s2_party.roster;
    /* s2_roster_validate REPAIRS and returns 1 when it had to change
     * something: a roster it leaves untouched is a valid one. */
    return s2_sound_netplay_allowed() && r.slots<=S2_NETPLAY_PLAYERS && !s2_party.save_menu_enabled && !s2_roster_validate(&r);
}

/* Stable compact IDs keep six long character names within the shared lobby's
 * 48-byte game configuration. They do not depend on registration order.
 * The parser also accepts the original named form. */
static const char *const wire_characters[]={"sonic","tails","amy","knuckles","shadow","mighty","none"};
static const char *const wire_ids[]={"0","1","2","3","4","5","6"};
static const char *wire_id(const char *id)
{
    for (unsigned i=0;i<sizeof wire_characters/sizeof *wire_characters;++i)
        if (!strcmp(id,wire_characters[i])) return wire_ids[i];
    return id;
}
/* Session config seal: "r=4,1,0,3,2,5 m=1111", in player order. */
void s2_netplay_config_image(char *out, size_t cap)
{
    const S2Roster *r = &s2_party.roster;
    size_t n = (size_t)snprintf(out, cap, "r=");
    for (unsigned p = 0; p < r->slots && p < S2_MAX_PLAYERS && n < cap; ++p)
        n += (size_t)snprintf(out + n, cap - n, "%s%s", p ? "," : "", wire_id(r->character[p]));
    /* The EFFECTIVE owner features: enabled AND backed by verified assets
     * (an enabled feature without its donor ROM changes nothing). */
    if (n < cap)
        snprintf(out + n, cap - n, " m=%d%d%d%d k=%d d=%d f=%d c=%d%d",
                 s2_party.amy_enabled && s2_resource_verified(S2_RESOURCE_AMY) ? 1 : 0,
                 s2_party.s3k_enabled && s2_resource_verified(S2_RESOURCE_SK) ? 1 : 0,
                 s2_party.mighty_enabled && s2_resource_verified(S2_RESOURCE_MEGAMIX) ? 1 : 0,
                 s2_party.shadow_enabled && s2_resource_shadow_bank() ? 1 : 0,
                 s2_party.s3k_enabled && s2_party.knuckles_campaign_enabled && s2_resource_knuckles_stages() ? 1 : 0,
                 s2_party.easy_debug?1:0,s2_party.knuckles_flow?1:0,
                 s2_party.infinite_lives!=0,s2_party.infinite_time!=0);
}

int s2_netplay_config_adopt(const char *line)
{
    if(!s2_sound_netplay_allowed()) return -1;
    const char *flow=line?strstr(line," f="):NULL;
    int knuckles_flow=0;
    if(flow) {
        if((flow[3]!='0'&&flow[3]!='1') || (flow[4]&&flow[4]!=' '))return -1;
        knuckles_flow=flow[3]=='1';
    }
    const char *debug=line?strstr(line," d="):NULL;
    int easy_debug=0;
    if(debug) {
        if((debug[3]!='0' && debug[3]!='1') || (debug[4] && debug[4]!=' ')) return -1;
        easy_debug=debug[3]=='1';
    }
    const char *world=line?strstr(line," k="):NULL;
    int knuckles_world=0;
    if(world) {
        if((world[3]!='0' && world[3]!='1') || (world[4] && world[4]!=' ')) return -1;
        knuckles_world=world[3]=='1';
    }
    if(knuckles_world && !s2_resource_knuckles_stages()) return -1;
    const char *practice=line?strstr(line," c="):NULL;
    int lives=0,time=0;
    if (practice) {
        if (strlen(practice)!=5 || (practice[3]!='0' && practice[3]!='1') ||
            (practice[4]!='0' && practice[4]!='1')) return -1;
        lives=practice[3]=='1';time=practice[4]=='1';
    }
    S2Roster roster;
    int amy = 0, s3k = 0, mighty = 0, shadow = 0;
    char ids[S2_MAX_PLAYERS * S2_CHARACTER_ID_SIZE];
    if (!line || strncmp(line, "r=", 2)) return -1;
    const char *m = strstr(line, " m=");
    size_t len = m ? (size_t)(m - line - 2) : strlen(line + 2);
    if (len >= sizeof ids) return -1;
    memcpy(ids, line + 2, len); ids[len] = 0;
    if (m && strlen(m) >= 5) { amy = m[3] == '1'; s3k = m[4] == '1'; }
    if (m && strlen(m) >= 6) mighty = m[5] == '1';
    if (m && strlen(m) >= 7) shadow = m[6] == '1';
    /* Owner-gated characters need this build's verified owner assets. */
    if ((amy && !s2_resource_verified(S2_RESOURCE_AMY)) || (s3k && !s2_resource_verified(S2_RESOURCE_SK)) ||
        (mighty && !s2_resource_verified(S2_RESOURCE_MEGAMIX)) || (shadow && !s2_resource_shadow_bank()))
        return -1;
    memset(&roster, 0, sizeof roster);
    for (unsigned i = 0; i < S2_MAX_PLAYERS; ++i) strcpy(roster.character[i], "none");
    unsigned p = 0;
    for (char *tok = strtok(ids, ","); tok; tok = strtok(NULL, ","), ++p) {
        if (p>=S2_NETPLAY_PLAYERS) return -1;
        const char *id=tok;
        if (strlen(tok)==1 && tok[0]>='0' && tok[0]<='6')
            id=wire_characters[tok[0]-'0'];
        snprintf(roster.character[p], sizeof roster.character[p], "%s", id);
    }
    roster.slots = p;
    int keep_amy = s2_party.amy_enabled, keep_s3k = s2_party.s3k_enabled, keep_mighty=s2_party.mighty_enabled;
    int keep_shadow=s2_party.shadow_enabled;
    int keep_world=s2_party.knuckles_campaign_enabled;
    s2_party.amy_enabled = amy; s2_party.s3k_enabled = s3k; s2_party.mighty_enabled=mighty; s2_party.shadow_enabled=shadow;
    s2_party.knuckles_campaign_enabled=knuckles_world;
    if (s2_roster_validate(&roster)) {   /* 1 = had to repair: not the host's roster */
        s2_party.amy_enabled = keep_amy; s2_party.s3k_enabled = keep_s3k;
        s2_party.mighty_enabled=keep_mighty;
        s2_party.shadow_enabled=keep_shadow;
        s2_party.knuckles_campaign_enabled=keep_world;
        return -1;
    }
    /* Session-scoped: set in memory, never written to sonic2-party.ini. */
    s2_party.roster = roster;
    s2_party.infinite_lives=lives;s2_party.infinite_time=time;
    s2_party.easy_debug=easy_debug;
    s2_party.knuckles_flow=knuckles_flow;
    s2_party.save_menu_enabled = 0;
    return 0;
}

int s2_options_hook(uint32_t pc)
{
    if(pc==0x111C) {
#ifdef SONIC2_MUSIC_DRIVER
        if(s_player_delay&&!--s_player_delay){g_ram[0xFFE0]=(uint8_t)s_player_pending;s_player_pending=0;}
#endif
        if(s_departing) {if(s_panel_age)--s_panel_age;else s_ready=s_departing=0;}
        else if(s_ready&&s_panel_age<12)++s_panel_age;
        return 0;
    }
    /* The native pressed-button bytes come from synchronized simulation input
     * online. Menu edits are therefore replayable game state, just like play.
     * Only the agreed, verified donor features are selectable in this session. */
    /* REV01 audited routine boundaries. Replacing an RTS-style routine leaves
     * the synthetic return slot to the generated BSR/JSR caller, as native
     * generated RTS does. No manual stack pop, no patched ROM bytes. */
    if (pc == 0x3CCE) { /* One-player reset complete, before A+Start routing. */
        if(missing_knuckles_campaign()) {
            g_ram[0xF600]=0x24;g_ram[0xFF8C]=0;s_notice=NOTICE_KIS2;return 1;
        }
        return 0;
    }
    if (pc == 0x3CF6) { /* TitleScreen_CheckIfChose2P, D0=selection */
        if ((uint8_t)g_cpu.D[0] == 1 && !s2_roster_vs_ready(&s2_party.roster)) {
            g_ram[0xF600] = 0x24;
            g_ram[0xFF8C] = 0;
            s_notice = NOTICE_VS_PLAYERS;
            return 1;
        }
        return 0;
    }
    if (g_ram[0xF600] != 0x24) return 0;
    switch (pc) {
    case 0x8FCC: /* initialization still owns art, background, palette and music */
        s_cursor = 0;
        s_ready = 0;
        s_panel_age=s_departing=0;s_bg_frame=~0u;
#ifdef SONIC2_MUSIC_DRIVER
        s_player_open=s_player_stopped=s_player_solo=s_player_pending=s_player_delay=0;player_mask();
#endif
        s2_roster_validate(&s2_party.roster);
        return 0;
    case 0x9186: case 0x91F8: /* replace three-row box drawing only */
        return 1;
    case 0x90E0: {
        s_ready = 1;
        if(!online() && s2_party.presentation_enabled&&s2_party.options_slow_backdrop &&
           s_bg_frame!=g_ram[0xF7F1] && g_ram[0xF7F0]<=5)
            g_ram[0xF7F0]=(uint8_t)(2*g_ram[0xF7F0]+1);
        s_bg_frame=g_ram[0xF7F1];
        unsigned press = g_ram[0xF605] | g_ram[0xF607];
#ifdef SONIC2_MUSIC_DRIVER
        if(s_player_open) {
            if(online() || (press&16)){player_close();return 1;}
            if(press&3){player_change((press&1)?-1:1);player_play();}
            if(press&12){unsigned count=s2_cd_theme_channel_count(s_player_track);s_player_solo=(s_player_solo+count+((press&4)?-1:1))%count;player_mask();}
            if(press&(0x40|0x80))player_play();
            if(press&0x20){s_player_stopped=1;s_player_pending=s_player_delay=0;g_ram[0xFFE0]=0xFD;player_mask();}
            g_ram[0xF605]&=0x7f;return 1;
        }
        if(s_cursor==PLAYER_ROW && (press&0xE0) && !online()) {
            s_player_open=1;s_player_track=0;s_player_solo=0;player_play();g_ram[0xF605]&=0x7f;return 1;
        }
#endif
        if ((press & 3) == 1) s_cursor = (s_cursor + OPTIONS_ROWS-1) % OPTIONS_ROWS;
        if ((press & 3) == 2) s_cursor = (s_cursor + 1) % OPTIONS_ROWS;
        if (s_cursor==SOUND_ROW && !(press&(3|128)) && (press&0x7C)) {
            /* The original third row owns sound selection, playback and
             * CheckCheats. Keep its controller and command handling intact. */
            g_ram[0xFF8C]=2;
            return 0;
        }
        int direction = (press & 12) == 4 ? -1 : (press & 12) == 8 ? 1 : 0;
        if (direction) {
            s_notice = NOTICE_NONE;
            if (!s_cursor) {
                unsigned slots = s2_party.roster.slots;
                unsigned limit=online()?S2_NETPLAY_PLAYERS:S2_MAX_PLAYERS;
                slots = direction > 0 ? slots % limit + 1 : (slots + limit-2) % limit + 1;
                s2_roster_slots(&s2_party.roster, slots);
            } else if (s_cursor <= S2_MAX_PLAYERS) {
                s2_roster_cycle(&s2_party.roster, s_cursor - 1, direction);
            }
        }
        if (press&16) s_cursor=BACK_ROW;
        /* Native Start exits this loop before the selected game mode runs. */
        if ((press & 16) || (s_cursor>=LEVEL_ROW && (press&96))) g_ram[0xF605] |= 128;
        return 1;
    }
    case 0x909A:
#ifdef SONIC2_MUSIC_DRIVER
        /* Native Start branches here before MenuScreen_Controls. Audition
         * stays in the same Options loop and never applies/exits the menu. */
        if(!online()&&(s_player_open||s_cursor==PLAYER_ROW)) {
            if(!s_player_open){s_player_open=1;s_player_track=s_player_solo=0;}
            player_play();g_ram[0xF605]&=0x7f;
            recomp_tail_call(0x9060);return 1;
        }
#endif
        s2_roster_validate(&s2_party.roster);
        if(s_cursor==LEVEL_ROW && missing_knuckles_campaign()) {
            s_notice=NOTICE_KIS2;recomp_tail_call(0x9060);return 1;
        }
        /* Applying a shared roster must never write a guest's local settings
         * or perform filesystem I/O again during rollback replay. */
        if (!online() && !s2_party_save()) {
            s_notice = NOTICE_SAVE_FAILED;
            recomp_tail_call(0x9060);
        } else {
            s_departing=1;
            s_notice = NOTICE_NONE;
            g_ram[0xF600] = s_cursor==LEVEL_ROW?0x28:4;
        }
        return 1;
    default: return 0;
    }
}

/* The mutable menu and roster must rewind together. Donor paths and enable
 * flags stay local/session configuration and are never changed by this menu.
 * Notices use stable IDs instead of process-local string pointers. */
void s2_options_rb_state(S2StateIO *io)
{
    if(io->mode){
#ifdef SONIC2_MUSIC_DRIVER
        unsigned incoming[12];
#else
        unsigned incoming[6];
#endif
        if(!io->data||io->pos>io->size||sizeof incoming>io->size-io->pos){io->ok=0;return;}
        memcpy(incoming,io->data+io->pos,sizeof incoming);
        if(incoming[0]>=OPTIONS_ROWS||incoming[1]>1||incoming[2]>NOTICE_KIS2){io->ok=0;return;}
#ifdef SONIC2_MUSIC_DRIVER
        if(incoming[3]>1||incoming[4]>=S2_MUSIC_COUNT||incoming[5]>10||incoming[6]>1||
           incoming[8]>2||(incoming[7]&&(incoming[7]<0x81||incoming[7]>0x9f))||
           !!incoming[7]!=!!incoming[8]||incoming[9]>12||incoming[10]>1){io->ok=0;return;}
#else
        if(incoming[3]>12||incoming[4]>1){io->ok=0;return;}
#endif
    }
    S2_STATE(io, s_cursor);
    S2_STATE(io, s_ready);
    S2_STATE(io, s_notice);
    #ifdef SONIC2_MUSIC_DRIVER
    S2_STATE(io,s_player_open);S2_STATE(io,s_player_track);S2_STATE(io,s_player_solo);S2_STATE(io,s_player_stopped);
    S2_STATE(io,s_player_pending);S2_STATE(io,s_player_delay);
    if(io->mode==2&&io->ok)player_mask();
#endif
    S2_STATE(io,s_panel_age);S2_STATE(io,s_departing);S2_STATE(io,s_bg_frame);
    S2_STATE(io, s2_party.roster.slots);
    S2_STATE(io, s2_party.roster.character);
}

/* Sample the actual font already loaded by MenuScreen. A=30, digits=16,
 * space=0, matching s2disasm's menu charset. Colors are host UI accents;
 * the background/palette animation is still the original Options screen. */
static unsigned glyph(char c)
{
    if (c >= 'A' && c <= 'Z') return (unsigned)(30 + c - 'A');
    if (c >= '0' && c <= '9') return (unsigned)(16 + c - '0');
    return 0;
}
static unsigned s_draw_alpha=255;
static uint32_t blend(uint32_t under,uint32_t ink)
{
    unsigned a=s_draw_alpha,b=255-a;
    return 0xFF000000u|(((((ink>>16)&255)*a+((under>>16)&255)*b)/255)<<16)|
        (((((ink>>8)&255)*a+((under>>8)&255)*b)/255)<<8)|((ink&255)*a+(under&255)*b)/255;
}
static void text_row(const GVDP *v, int line, uint32_t *out, int width,
                     int x, int y, const char *text, uint32_t color)
{
    if (line < y || line >= y + 8) return;
    for (; *text; ++text, x += 8) {
        unsigned tile = glyph(*text);
        if (!tile) continue;
        for (int px = 0; px < 8; ++px) {
            if (x + px < 0 || x + px >= width) continue;
            unsigned byte = v->vram[tile * 32 + (line - y) * 4 + px / 2];
            unsigned nibble = px & 1 ? byte & 15 : byte >> 4;
            if (nibble) {
                uint32_t ink = genesis_dac_cram_to_argb(v->cram[nibble], GENESIS_DAC_NORMAL);
                unsigned light = (((ink >> 16) & 255) + ((ink >> 8) & 255) + (ink & 255)) / 3;
                /* The native font includes opaque black outlines/background.
                 * Tint its bright ink, not every nonzero palette index. */
                if (light > 32) out[x + px] = blend(out[x+px],color);
            }
        }
    }
}
void s2_options_overlay(const GVDP *v, int line, uint32_t *out, int width)
{
    /* The party and Options overlays present synchronized simulation state.
     * The campaign save menu remains local-only. */
    s2_runtime_overlay(v, line, out, width);
    if (!online() && s2_save_menu_overlay(line,out,width)) return;
    int fade=s2_party.presentation_enabled&&s2_party.options_panel_fade;
    if ((g_ram[0xF600] != 0x24 && !(s_departing&&fade)) || !s_ready) return;
    s_draw_alpha=fade?(s_panel_age<12?s_panel_age*255/12:255):255;
    int left = (width - 288) / 2;
    if (line >= 16 && line < 208) {
        for (int x = left; x < left + 288; ++x) if (x >= 0 && x < width) {
            int border = line == 16 || line == 207 || x == left || x == left + 287;
            out[x] = blend(out[x],border ? 0xFF708ADCu : 0xFF101A42u);
        }
    }
    #ifdef SONIC2_MUSIC_DRIVER
    if(s_player_open) {
        text_row(v,line,out,width,left+88,28,"MUSIC PLAYER",0xFFFFD85Cu);
        char name[36];snprintf(name,sizeof name,"%.35s",s2_theme_names[s_player_track]);
        for(char *p=name;*p;++p)if(*p>='a'&&*p<='z')*p-=32;
        text_row(v,line,out,width,left+12,56,name,0xFFF4F5FFu);
        const S2MusicChoice *choice=s2_music_active(s_player_track)?s2_theme_choice(s_player_track,(unsigned)s2_themes[s_player_track].choice):NULL;
        snprintf(name,sizeof name,"%.35s",choice?choice->label:"SONIC 2 ORIGINAL");
        for(char *p=name;*p;++p)if(*p>='a'&&*p<='z')*p-=32;
        text_row(v,line,out,width,left+12,76,name,0xFFB5C5F5u);
        const char *channel=s2_cd_theme_channel_count(s_player_track)==1?"STEREO RECORDING":s2_cd_theme_channel_name(s_player_track,s_player_solo);
        snprintf(name,sizeof name,"%.35s",*channel?channel:player_channels[s_player_solo]);
        for(char *p=name;*p;++p)if(*p>='a'&&*p<='z')*p-=32;
        text_row(v,line,out,width,left+12,104,name,0xFFFFD85Cu);
        int playing=s2_sound_music_playing();
        text_row(v,line,out,width,left+12,128,s_player_stopped?"STOPPED":s_player_pending?"STARTING":playing==0?"FINISHED":playing>0?"PLAYING":"PREVIEW",0xFFF4F5FFu);
        text_row(v,line,out,width,left+12,154,"UP DOWN TRACK   LEFT RIGHT CHANNEL",0xFFB5C5F5u);
        text_row(v,line,out,width,left+12,174,"A START PLAY   C STOP   B BACK",0xFFB5C5F5u);
        return;
    }
#endif
    text_row(v,line,out,width,left+104,28,"OPTIONS",0xFFFFD85Cu);
    for (unsigned row = 0; row < OPTIONS_ROWS; ++row) {
        int y = 44 + row * 14;
        int disabled = row >= 1 && row <= S2_MAX_PLAYERS && row > s2_party.roster.slots;
        uint32_t color = disabled ? 0xFF6C7390u : row == s_cursor ? 0xFFFFD85Cu : 0xFFF4F5FFu;
        if (row == s_cursor && line >= y && line < y + 8) {
            int size = 4 - (line < y + 4 ? y + 3 - line : line - y - 4);
            for (int x = 0; x <= size; ++x) if (left + 12 + x >= 0 && left + 12 + x < width)
                out[left + 12 + x] = blend(out[left+12+x],color);
        }
        char label[32], value[40];
        value[0] = 0;
        if (!row) { strcpy(label,"PLAYERS"); snprintf(value,sizeof value,"%u",s2_party.roster.slots); }
        else if (row <= S2_MAX_PLAYERS) {
            snprintf(label,sizeof label,"PLAYER %u",row);
            const S2Character *c = s2_character_find(s2_party.roster.character[row-1]);
            snprintf(value,sizeof value,"%s",c ? c->label : "NONE");
        } else if(row==SOUND_ROW) {
            strcpy(label,"SOUND TEST");
            snprintf(value,sizeof value,"%02X",(g_ram[0xFF84]<<8)|g_ram[0xFF85]);
        } else strcpy(label,row==PLAYER_ROW?"MUSIC PLAYER":row==LEVEL_ROW?"LEVEL SELECT":"BACK");
        text_row(v,line,out,width,left+28,y,label,color);
        text_row(v,line,out,width,left+176,y,value,color);
    }
    const char *hint = s_notice == NOTICE_VS_PLAYERS ? "VS NEEDS PLAYER 1 AND PLAYER 2"
                     : s_notice == NOTICE_SAVE_FAILED ? "SETTINGS SAVE FAILED"
                     : s_notice == NOTICE_KIS2 ? "KNUCKLES CAMPAIGN NEEDS KIS2"
                     : s_cursor==LEVEL_ROW && missing_knuckles_campaign() ? "KNUCKLES CAMPAIGN NEEDS KIS2"
                     : s_cursor==1 && s2_party.s3k_enabled && !s2_resource_knuckles_stages() ? "KNUCKLES LEAD NEEDS KIS2 ROM"
                     : s_cursor==SOUND_ROW ? "LEFT RIGHT CHANGE A TEN B C PLAY"
                     : s_cursor==PLAYER_ROW ? online()?"MUSIC PLAYER IS LOCAL ONLY":"A START OPEN   B BACK"
                     : s_cursor==LEVEL_ROW ? "A C OPEN   B BACK   START OPEN"
                     : online() ? "D PAD SELECT    START APPLY"
                     : "D PAD SELECT    START SAVE";
    text_row(v,line,out,width,(width-(int)strlen(hint)*8)/2,198,hint,0xFFB5C5F5u);
}
