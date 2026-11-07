/* Opt-in Sonic 2 REV01 world renderer.
 * Canonical s2disasm: GetBlock $E244, ObjectsManager $17AA4,
 * BuildSprites $16604, RingsManager $16F88. Native rendering is the default.
 * No collision, layout, decompression or tile-streaming data is rewritten.
 */
#include "sonic2_video.h"
#include "sonic2_runtime.h"
#include "sonic2_party.h"
#include "sonic2_resources.h"
#include "sonic2_shadow.h"
#if GENESIS_HAS_RECOMP_NET
#include "netplay/genesis_netplay.h"
#endif
#include "genesis_runtime.h"
#include "video/genesis_vdp.h"
#include "video/genesis_dac.h"
#include "video/genesis_tile_row.h"
#if OWN_BACKEND
#include "video/genesis_machine.h"
#endif
#include "cmd_server.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { VIDEO_OFF, VIDEO_FIT, VIDEO_RATIO, VIDEO_STAGE };
static int s_mode;
static double s_ratio = 16.0 / 9.0;
/* Drawing scratch is separate from simulation/activation configuration.
 * Fractional presentation borrows an owned context and restores this pointer. */
typedef struct S2RenderContext {
    GVDP video_frame;
    uint8_t world_frame[0xA800];
    int vs_camera[2][4];
    int width;
    int left;
    int camera;
    int stage_width;
    int native_x;
    int camera_y;
    unsigned frames;
    unsigned fallbacks;
    unsigned terrain_checks;
    unsigned terrain_errors;
    unsigned bg_checks;
    unsigned bg_errors;
    unsigned bg_unstreamed;
    unsigned scene_misses;
    unsigned scene_holds;
    unsigned uninitialized_scroll_lines;
    unsigned terrain_unstreamed;
    int bad_x;
    int bad_y;
    unsigned bad_attr;
    unsigned bad_expected;
    int bad_world_y;
    int bad_hscroll;
    int frame_level;
    int frame_special;
    int frame_fg_x;
    int frame_fg_y;
    int frame_bg_y;
    unsigned frame_zone;
    int frame_campaign_view;
    int frame_view_x;
    int frame_view_y;
    int frame_card;
    int frame_card_leaving;
    int frame_sega;
    int frame_vs;
    int vs_ready;
    int vs_world;
} S2RenderContext;
static S2RenderContext s_real_render,*s_render=&s_real_render;
static int s_requested_width=320;
enum { FADE_NONE,FADE_FROM_BLACK,FADE_TO_BLACK,FADE_FROM_WHITE,FADE_TO_WHITE };
typedef struct { unsigned mode,steps; } DonorFade;
static DonorFade s_donor_fade,s_presented_fade;
static uint16_t donor_fade_color(uint16_t color,DonorFade fade)
{
    /* REV01 Pal_Fade*: one DAC step per update, B/G/R on entry and R/G/B
     * on exit. A scalar gain loses the blue/cyan part of the native fade. */
    if (!fade.mode) return color;
    unsigned budget=fade.steps*2,result=0;
    int entry=fade.mode==FADE_FROM_BLACK || fade.mode==FADE_FROM_WHITE;
    int white=fade.mode==FADE_FROM_WHITE || fade.mode==FADE_TO_WHITE;
    for (unsigned n=0;n<3;++n) {
        unsigned shift=(entry?2-n:n)*4,target=(color>>shift)&14;
        unsigned distance=white?14-target:target;
        unsigned change=budget<distance?budget:distance;budget-=change;
        unsigned channel=entry?(white?14-change:change):(white?target+change:target-change);
        result|=channel<<shift;
    }
    return (uint16_t)result;
}
uint16_t s2_video_donor_color(uint16_t shade)
{ return donor_fade_color(shade,s_presented_fade); }
static int s_interpolation_enabled,s_interpolating;
static double s_interpolation_phase;
static void interpolation_reset(void);
static void interpolation_sprite(unsigned index,int *x,int *y);
#define s_width (s_render->width)
#define s_left (s_render->left)
#define s_camera (s_render->camera)
#define s_stage_width (s_render->stage_width)
#define s_native_x (s_render->native_x)
#define s_camera_y (s_render->camera_y)
#define s_frames (s_render->frames)
#define s_fallbacks (s_render->fallbacks)
#define s_terrain_checks (s_render->terrain_checks)
#define s_terrain_errors (s_render->terrain_errors)
#define s_bg_checks (s_render->bg_checks)
#define s_bg_errors (s_render->bg_errors)
#define s_bg_unstreamed (s_render->bg_unstreamed)
#define s_scene_misses (s_render->scene_misses)
#define s_scene_holds (s_render->scene_holds)
#define s_uninitialized_scroll_lines (s_render->uninitialized_scroll_lines)
#define s_terrain_unstreamed (s_render->terrain_unstreamed)
#define s_bad_x (s_render->bad_x)
#define s_bad_y (s_render->bad_y)
#define s_bad_attr (s_render->bad_attr)
#define s_bad_expected (s_render->bad_expected)
#define s_bad_world_y (s_render->bad_world_y)
#define s_bad_hscroll (s_render->bad_hscroll)
#define s_frame_level (s_render->frame_level)
#define s_frame_special (s_render->frame_special)
#define s_frame_fg_x (s_render->frame_fg_x)
#define s_frame_fg_y (s_render->frame_fg_y)
#define s_frame_bg_y (s_render->frame_bg_y)
#define s_frame_zone (s_render->frame_zone)
#define s_frame_campaign_view (s_render->frame_campaign_view)
#define s_frame_view_x (s_render->frame_view_x)
#define s_frame_view_y (s_render->frame_view_y)
#define s_frame_card (s_render->frame_card)
#define s_frame_card_leaving (s_render->frame_card_leaving)
#define s_frame_sega (s_render->frame_sega)
#define s_frame_vs (s_render->frame_vs)
#define s_vs_ready (s_render->vs_ready)
#define s_vs_world (s_render->vs_world)
#define s_video_frame (s_render->video_frame)
#define s_world_frame (s_render->world_frame)
#define s_vs_camera (s_render->vs_camera)
static void scene_mode_changed(int was_enabled);

static uint16_t ram16(unsigned a)
{
    a &= 0xFFFFu;
    return (uint16_t)((g_ram[a] << 8) | g_ram[(a + 1) & 0xFFFFu]);
}
static uint16_t vram16(const GVDP *v, unsigned a)
{
    return (uint16_t)((v->vram[a & 0xFFFFu] << 8) | v->vram[(a + 1) & 0xFFFFu]);
}
static int gameplay(void)
{
    int mode = g_ram[0xF600] & 0x7F;
    /* Game mode changes BEFORE a fade finishes. Scene identity follows the
     * still-live player/layout until the incoming scene replaces them. */
    return (mode == 8 || mode == 12 || mode == 16) && (g_ram[0xB000] == 1 || g_ram[0xB000] == 2) && !ram16(0xFFD8);
}
static int competition(void)
{
    int mode=g_ram[0xF600]&127;
    return ram16(0xFFD8) && (mode==8 || mode==12);
}
static int outgoing_level(void)
{
    int mode=g_ram[0xF600]&127;
    return (mode==0 || mode==4) && g_ram[0xF711] &&
        (g_ram[0xB000]==1 || g_ram[0xB000]==2);
}
static int stage_width(void)
{
    int last = 1;
    for (int row = 0; row < 16; ++row)
        for (int col = 0; col < 128; ++col)
            if (g_ram[0x8000 + row * 256 + col] && col + 1 > last) last = col + 1;
    return last * 128;
}
static int configure(const char *mode)
{
    if (!mode) return 0;
    int was_enabled = s_mode != VIDEO_OFF;
    if (!strcmp(mode, "off")) s_mode = VIDEO_OFF;
    else if (!strcmp(mode, "fit") || !strcmp(mode, "adaptive")) s_mode = VIDEO_FIT;
    else if (!strcmp(mode, "stage")) s_mode = VIDEO_STAGE;
    else {
        char *end;
        double w = strtod(mode, &end);
        if (end == mode || *end != ':') return 0;
        const char *den = end + 1;
        double h = strtod(den, &end);
        if (end == den || *end || !(w > 0) || !(h > 0) ||
            !(w / h > 0) || w / h > (double)INT_MAX / 480.0) return 0;
        s_ratio = w / h; s_mode = VIDEO_RATIO;
    }
    scene_mode_changed(was_enabled);
    return 1;
}
static int enabled(void)
{
#if GENESIS_HAS_RECOMP_NET
    if (genesis_netplay_active()) return 1;
#endif
    return s_mode != VIDEO_OFF;
}
unsigned s2_video_main_cpu_divisor(void)
{
    /* Level_MainLoop ($4360) still waits for the real V-int every tick.
     * Expanded activation must not inherit the 7.67 MHz object's lag budget.
     * Loading, special stages and opt-out keep original timing. */
    int mode=g_ram[0xF600];
    return enabled() && (mode==8 || mode==12) && (gameplay() || competition()) && g_ram[0xF711] ? 4 : 1;
}
static int width(int dw, int dh, int nw, int nh)
{
    if (!enabled()) return 0;
    double result = nw;
    if (s_mode == VIDEO_STAGE) {
        if (gameplay()) s_stage_width = stage_width();
        result = s_stage_width > nw ? s_stage_width : nw;
    }
    else if (s_mode == VIDEO_RATIO) result = nh * s_ratio;
    else if (s_mode == VIDEO_FIT && dw > 0 && dh > 0) result = (double)nh * dw / dh;
    /* Bound by representable dimensions and the SDL texture limit (runner),
     * never by an aspect preset. Even a stage-length viewport is supported. */
    if (result > INT_MAX - 1.0) result = INT_MAX - 1.0;
    int pixels = (int)(result + 0.5);
    s_requested_width = pixels < nw ? nw : pixels;
    return s_requested_width;
}

/* Full 16-bit scroll is unwrapped against the live camera's 10-bit VDP
 * position. This avoids the one-frame camera/streaming seam seen in SMB. */
static int unwrap(int value, int reference)
{
    return reference + ((value - reference + 512) & 1023) - 512;
}
static int native_terrain_streamed(int wy, int copied_camera_y)
{
    /* Draw_FG ($DAF6) streams 16px rows at copied-camera offsets -16 and
     * +224. Live VScroll can run ahead of that copy during a fast fall.
     * Outside this precise coverage, the native name table is not a terrain
     * oracle: the enhanced renderer must still draw the real stage block. */
    int top = (copied_camera_y & ~15) - 16;
    return wy >= top && wy < top + 256;
}
static uint16_t world_attr(const uint8_t *ram,int wx,int wy,int background)
{
    if(wx<0 || wx>=16384)return 0;
    unsigned layout=(background?0x8080u:0x8000u)+
        (((unsigned)wy>>7)&15u)*256u+((unsigned)wx>>7);
    unsigned chunk=ram[layout];
    unsigned address=chunk*128u+((unsigned)wy&112u)+((unsigned)wx&112u)/8u;
    uint16_t block=(uint16_t)((ram[address]<<8)|ram[address+1]);
    int tx=(wx>>3)&1,ty=(wy>>3)&1;
    if(block&0x0400)tx^=1;
    if(block&0x0800)ty^=1;
    address=0x9000u+(block&1023u)*8u+(unsigned)(ty*2+tx)*2u;
    /* Block_Table has 768 entries in Sonic 2; IDs above that are not valid
     * stage blocks. Do not read into the live object/display buffers. */
    if(address+1>=0xA800u)return 0;
    return (uint16_t)(((ram[address]<<8)|ram[address+1])^((block&0x0C00u)<<1));
}
/* The native 2P loader halves pattern indices. An interlace block has
 * two8x16 cells side by side, using only its first pair of map entries. */
static uint16_t world_attr_2p(const uint8_t *ram,int wx,int wy,int background)
{
    if (wx<0 || wx>=16384) return 0;
    unsigned layout=(background?0x8080u:0x8000u)+(((unsigned)wy>>7)&15u)*256u+((unsigned)wx>>7);
    unsigned chunk=ram[layout],at=chunk*128u+((unsigned)wy&112u)+((unsigned)wx&112u)/8u;
    unsigned block=(ram[at]<<8)|ram[at+1],tx=((unsigned)wx>>3)&1u;
    if (block&0x400) tx^=1;
    at=0x9000u+(block&1023u)*8u+tx*2u;
    if (at+1>=0xA800) return 0;
    return (uint16_t)(((ram[at]<<8)|ram[at+1])^((block&0xC00u)<<1));
}
static uint8_t pattern_pixel(const GVDP *v, uint16_t attr, int x, int y)
{
    int cell=(v->reg[12]&6)==6?16:8;
    int fx = x & 7, fy = y & (cell-1);
    if (attr & 0x0800) fx ^= 7;
    if (attr & 0x1000) fy ^= cell-1;
    unsigned a = (attr & 2047u) * (unsigned)(cell*4) + (unsigned)fy * 4u + (unsigned)fx / 2u;
    int byte = v->vram[a & 0xFFFFu];
    int nibble = fx & 1 ? byte & 15 : byte >> 4;
    return nibble ? (uint8_t)(((attr >> 9) & 48) | nibble) : 0;
}
static int card_fill(unsigned attr)
{
    unsigned tile=attr&2047;
    return tile>=0x5D8 && tile<=0x5DD &&
        (attr&0x6000)==(tile>=0x5DC?0x2000u:0u);
}
static int plane_tiles(int bits) { return bits == 1 ? 64 : bits == 3 ? 128 : 32; }
static uint16_t plane_attr(const GVDP *v, unsigned base, int x, int y)
{
    int wt = plane_tiles(v->reg[16] & 3), ht = plane_tiles((v->reg[16] >> 4) & 3);
    int col = (x >> 3) & (wt - 1), row = (y >> (((v->reg[12]&6)==6)?4:3)) & (ht - 1);
    return vram16(v, base + (unsigned)(row * wt + col) * 2u);
}

/* Enhanced scene data lives on the host: no SAT coordinate wrap, 80-piece
 * ceiling, or 20-sprites-per-line ceiling. Native BuildSprites still executes
 * for the hardware pass and publishes a signature used to align our list
 * with the exact VBlank upload displayed by the VDP. */
typedef struct { int x, y; uint16_t attr; uint8_t size, hud; } SceneSprite;
enum { SCENE_SPRITES = 32768, SCENE_PLACEMENTS = 1024 };
typedef struct {
    SceneSprite sprites[SCENE_SPRITES];
    unsigned count, serial;
    uint8_t sat[640];
    int scene; /* 1 level, 2 special, 3 native competition, 0 menu */
    int camera_x, camera_y; /* coordinates used when capturing world sprites */
    int views[S2_MAX_PLAYERS][2]; /* all players' origins, identical on every peer */
} SceneFrame;
static SceneFrame s_build, s_history[3], s_display_frame;
static const SceneFrame *s_display;
/* Source identities belong to presentation only, never to the rollback image. */
static uint32_t s_build_keys[SCENE_SPRITES],s_key_history[3][SCENE_SPRITES],s_display_keys[SCENE_SPRITES];
static unsigned s_key_serial[3],s_display_key_serial;
static SceneFrame s_native_build,s_native_history[3],s_native_display;
static uint32_t s_native_build_keys[SCENE_SPRITES],s_native_keys[3][SCENE_SPRITES],s_native_display_keys[SCENE_SPRITES];
static unsigned s_native_serial;
static int s_native_pending;
static void native_capture_objects(void);
static void native_publish_sprites(void);

static unsigned s_serial, s_scene_tick, s_spawned, s_pool_pressure;
static int s_vs_other_lo,s_vs_other_hi;

/* The native SAT remains authoritative for front-of-player UI. Keep only
 * source-object classifications beside its completed publication; no guest
 * marker, mapping, palette or sprite order is changed. */
typedef struct {
    uint8_t sat[2][640],kind[2][80];
    unsigned serial,ui_mask;
} FrontUIFrame;
static FrontUIFrame s_front_build,s_front_history[3],s_front_display[2];
static unsigned s_front_serial;
typedef struct {
    uint8_t sat[640],piece[80],patterns[S2_SPECIAL_TILES*32];
    unsigned serial,tiles,base_tile;
    uint8_t frame,flip,ready;
} SpecialActorFrame;
static SpecialActorFrame s_special_build,s_special_history[3],s_special_display;
static unsigned s_special_serial;
typedef struct {uint16_t object;int16_t x,y;uint8_t frame;} SpecialResultActor;
typedef struct {
    uint8_t sat[640],owner[80],count,got_emerald,main_routine;
    SpecialResultActor actors[16];
    unsigned serial;
} SpecialResultFrame;
static SpecialResultFrame s_ss_result_build,s_ss_result_history[3],s_ss_result_display;
static unsigned s_ss_result_serial;
static void special_results_begin(void);
static void special_results_piece(unsigned slot,unsigned object);
static void special_results_publish(void);
static void special_actor_piece(unsigned slot,unsigned object,unsigned mapping);
static void special_actor_publish(void);
static const char *custom_result_name(void)
{
    const char *id=s2_party.roster.character[0];
    return !strcmp(id,"knuckles")?"KNUCKLES":!strcmp(id,"amy")?"AMY":
        !strcmp(id,"shadow")?"SHADOW":!strcmp(id,"mighty")?"MIGHTY":NULL;
}
static struct {const char *name;int x,y;unsigned count,slots[8];} s_result_name;
static int result_name_slot(unsigned slot)
{
    for(unsigned n=0;n<s_result_name.count;++n)if(s_result_name.slots[n]==slot)return 1;
    return 0;
}
static void front_ui_begin(void)
{
    memset(&s_front_build,0,sizeof s_front_build);
    memset(&s_special_build,0,sizeof s_special_build);
    special_results_begin();
}
static void front_ui_publish(void)
{
    memcpy(s_front_build.sat[0],g_ram+0xF800,640);
    memcpy(s_front_build.sat[1],g_ram+0xDD00,640);
    for(unsigned p=0;p<2;++p)for(unsigned i=0;i<80;++i)
        if(s_front_build.kind[p][i]){s_front_build.ui_mask|=1u<<p;break;}
    s_front_build.serial=++s_front_serial;
    s_front_history[s_front_serial%3]=s_front_build;
    special_actor_publish();
    special_results_publish();
}

static unsigned s_tick_samples,s_tick_updates,s_tick_lag,s_tick_multi;
static unsigned s_publication_lag;
void s2_video_vblank(void)
{
    /* Palette fades can run without another object/BuildSprites tick. Refresh
     * the presentation-only donor line at the same V-int palette boundary. */
    s_presented_fade=s_donor_fade;
    s2_video_prepare_knuckles_items();
    static int was_active;
    static unsigned last_tick,last_zone,last_serial;
    unsigned tick=ram16(0xFE04),zone=ram16(0xFE10);
    int active=enabled() && g_ram[0xF600]==12 && gameplay() &&
        g_ram[0xF711] && g_ram[0xB024]<6 && !ram16(0xF63A);
    /* Sample at the same IRQ entry, not end-of-wall-frame: native variable
     * V-int DMA debt can shift the next tick across that later sample point. */
    if(active && was_active && zone==last_zone) {
        unsigned delta=(uint16_t)(tick-last_tick);
        ++s_tick_samples;s_tick_updates+=delta;
        s_tick_lag+=delta==0;s_tick_multi+=delta>1;
        s_publication_lag+=s_serial==last_serial;
    }
    was_active=active;last_tick=tick;last_zone=zone;last_serial=s_serial;
}
static unsigned s_visible_objects[144], s_visible_count;
typedef struct { unsigned address; uint16_t x, y,slot; uint8_t id, subtype, state, loaded; } Placement;
static Placement s_placements[SCENE_PLACEMENTS];
static unsigned s_placement_count, s_placement_base;
static int s_loader_active;
/* Native world bookkeeping: visited horizontal cells stay awake until the
 * original object routines need more slots. Under pressure retain the union
 * of ALL player windows; never sacrifice a remote player's live scenery. */
static uint8_t s_active_cells[128];
static int s_streaming_pressure;
static void video_state(S2StateIO *io, int rollback);
void s2_video_state(S2StateIO *io) { video_state(io, 0); }
/* Rollback flavour: the simulation half only. s_display / s_display_frame
 * are chosen by the scanline renderer (select_scene) -- presentation, which a
 * replayed tick does not run -- so carrying them made the determinism probe
 * report forks in the "game" partition whenever custom video was on
 * (2026-09-25: 74 of 460 passes). A restore keeps the presented selection. */
void s2_video_rb_state(S2StateIO *io) { video_state(io, 1); }
static void video_state(S2StateIO *io, int rollback)
{
    /* SceneFrame is pointer-free. Preserve the published frame history and
     * expanded object-loader ownership; scanline scratch is rebuilt at y=0. */
    struct { int mode,width; double ratio; } config={s_mode,s_requested_width,s_ratio},saved=config;
    if (io->mode) {
        if (io->pos>io->size || sizeof saved>io->size-io->pos) { io->ok=0; return; }
        memcpy(&saved,io->data+io->pos,sizeof saved);
        /* Quickstates belong to one video setup. Cold session resets also
         * use the rollback serializer, after the launcher has selected the
         * next setup; keep that new configuration while clearing the world. */
        if (!rollback && (saved.mode!=config.mode || saved.ratio!=config.ratio)) io->ok=0;
    }
    S2_STATE(io,saved);
    S2_STATE(io,s_build); S2_STATE(io,s_history);
    int display=s_display!=NULL;
    if (!rollback) { S2_STATE(io,s_display_frame); S2_STATE(io,display); }
    S2_STATE(io,s_serial); S2_STATE(io,s_scene_tick);
    S2_STATE(io,s_donor_fade); S2_STATE(io,s_presented_fade);
    /* This publication is produced entirely by native BuildSprites and
     * sealed roster state. Preserve it in rollback as well, so the first
     * restored half-pipe picture need not wait for another object tick. */
    S2_STATE(io,s_special_build);S2_STATE(io,s_special_history);S2_STATE(io,s_special_serial);
    S2_STATE(io,s_ss_result_build);S2_STATE(io,s_ss_result_history);S2_STATE(io,s_ss_result_serial);
    /* This SAT classification is presentation metadata, like s_display.
     * Disk states keep the exact picture; rollback rebuilds it from the next
     * native sprite publication instead of hashing renderer activity. */
    if(!rollback) {
        S2_STATE(io,s_front_build);S2_STATE(io,s_front_history);S2_STATE(io,s_front_serial);
        S2_STATE(io,s_front_display);
        S2_STATE(io,s_special_display);
        S2_STATE(io,s_ss_result_display);
    } else if(io->mode==2) {
        memset(&s_front_build,0,sizeof s_front_build);
        memset(s_front_history,0,sizeof s_front_history);
        memset(s_front_display,0,sizeof s_front_display);s_front_serial=0;
        memset(&s_special_display,0,sizeof s_special_display);
        memset(&s_ss_result_display,0,sizeof s_ss_result_display);
    }
    S2_STATE(io,s_placements); S2_STATE(io,s_placement_count); S2_STATE(io,s_placement_base);
    S2_STATE(io,s_loader_active); S2_STATE(io,s_visible_objects); S2_STATE(io,s_visible_count);
    S2_STATE(io,s_active_cells); S2_STATE(io,s_streaming_pressure);
    if (io->mode==2 && rollback) s_requested_width=saved.width;
    if (io->mode==2 && !rollback) s_display=display?&s_display_frame:NULL;
}

static uint8_t scene_read8(unsigned a)
{
    a &= 0xFFFFFFu;
    return a < 0x100000u ? g_rom[a] : a >= 0xFF0000u ? g_ram[a & 65535u] : 0;
}
static uint16_t scene_read16(unsigned a) { return (uint16_t)((scene_read8(a)<<8)|scene_read8(a+1)); }
static uint32_t scene_read32(unsigned a) { return ((uint32_t)scene_read16(a)<<16)|scene_read16(a+2); }
/* Host edits of guest RAM: glue_poke* (genesis_host_mem.h), never the
 * emulated bus. m68k_write* is a guest access -- it bumps the cycle budget
 * and may yield the game fiber to the raster scheduler -- so a host hook
 * that wrote through it made the machine's schedule depend on the host. */
static void write8(unsigned a, unsigned v) { glue_poke8(0xFF0000u|(a&65535u),(uint8_t)v); }
static void write16(unsigned a, unsigned v) { glue_poke16(0xFF0000u|(a&65535u),(uint16_t)v); }
static void write32(unsigned a, unsigned v) { glue_poke32(0xFF0000u|(a&65535u),v); }
static int view_left(int camera, int w)
{
    int end = stage_width()-w, left = camera-(w-320)/2;
    if (end<0) return end/2;
    if (left<0) left=0;
    return left>end ? end : left;
}
static void activation_bounds(int camera,int w,int *lo,int *hi)
{
    int left=view_left(camera,w);
    /* out_of_range tests 128px-rounded positions. OPL must only instantiate
     * cells that this same culler retains. Loading at left+w+320 with an
     * unrounded camera let objects spawn one cell early, get deleted, and
     * remain marked loaded until they left the window (missing platforms). */
    *lo=(left-128)&~127;
    *hi=((left+w+192)&~127)+128;
}
static int clamp(int value,int lo,int hi)
{
    if (hi<lo) hi=lo;
    return value<lo?lo:value>hi?hi:value;
}
static void player_camera(unsigned p,int *x,int *y)
{
    *x=ram16(0xEE00); *y=(int16_t)ram16(0xEE04);
    int px,py;
    if (p && s2_runtime_player_position(p,&px,&py)) {
        *x=clamp(px-144,ram16(0xEEC8),ram16(0xEECA));
        *y=clamp(py-96,(int16_t)ram16(0xEECC),(int16_t)ram16(0xEECE));
    }
}
static int active_x(int x)
{
    return x>=0 && x<16384 && s_active_cells[(unsigned)x>>7];
}
static void campaign_cells(void)
{
    /* All actors participate, regardless of which peer renders them. */
    if (s_streaming_pressure) memset(s_active_cells,0,sizeof s_active_cells);
    for (unsigned p=0;p<S2_MAX_PLAYERS;++p) {
        int x,y,lo,hi;
        if (!s2_runtime_player_position(p,&x,&y)) continue;
        player_camera(p,&x,&y);
        activation_bounds(x,s_requested_width,&lo,&hi);
        for (int cell=clamp(lo/128,0,127);cell<clamp(hi/128,0,128);++cell) s_active_cells[cell]=1;
    }
}
static int scene_contains(int x,int lo,int hi)
{
    if (competition()) {
        return (x>=lo && x<hi) || (x>=s_vs_other_lo && x<s_vs_other_hi);
    }
    return s2_runtime_campaign_online()?active_x(x):x>=lo && x<hi;
}

static void scene_mode_changed(int was_enabled)
{
    if(was_enabled!=enabled()) {
        s_display=NULL;s_display_frame.serial=0;
        for(unsigned n=0;n<3;++n)s_history[n].serial=0;
    }
    /* Once adopted, the host loader also maintains a native-width window
     * on opt-out until the next level init, preserving existing object state. */
}
static int knuckles_item_tile(unsigned tile)
{
    return (tile>=0x456 && tile<0x46E) || (tile>=0x4BE && tile<0x500) ||
        (tile>=0x6AC && tile<0x6B4) || (tile>=0x7D4 && tile<0x7D8) ||
        (tile>=0x5F4 && tile<0x600);
}
static int knuckles_palette_tile(unsigned tile)
{
    return (tile>=0x400 && tile<0x434) || (tile>=0x580 && tile<0x5A8);
}
#if OWN_BACKEND
static int shadow_sprite_tagged(const uint8_t *data,unsigned base)
{
    unsigned slot=0;
    for (unsigned n=0;n<80;++n) {
        unsigned at=(base+slot*8)&65535u;
        if ((data[(at+2)&65535u]&0xC0)==0xC0) return 1;
        slot=data[(at+3)&65535u]&127u;
        if (!slot) break;
    }
    return 0;
}
#endif
static int shadow_palette_needed(void)
{
#if OWN_BACKEND
    for (unsigned o=0xB000;o<0xD400;o+=64)
        if (g_ram[o]==0x35 && s2_runtime_shadow_star(o)) return 1;
    if (s_display) for (unsigned n=0;n<s_display->count;++n)
        if ((s_display->sprites[n].size&0xC0)==0xC0) return 1;
    /* Retain the palette for both queued and currently displayed pieces:
     * an interrupt can precede completion/upload of the next native SAT. */
    return shadow_sprite_tagged(g_ram,0xF800) ||
        (competition() && shadow_sprite_tagged(g_ram,0xDD00)) ||
        shadow_sprite_tagged(g_machine.vdp.vram,(g_machine.vdp.reg[5]&127u)<<9);
#else
    return 1; /* Legacy backend keeps the always-available presentation path. */
#endif
}
/* The guest can prepare the next H-int split while the current scanline is
 * still being presented. Follow the live native palette, not that future RAM
 * split, for private world colours and donor actors on the boundary row. */
int s2_video_water_palette(const GVDP *v)
{
    if (!g_ram[0xF64C] || competition()) return 0;
    unsigned dry=0,wet=0;
    for (unsigned c=0;c<16;++c) {
        dry+=v->cram[c]==ram16(0xFB00+c*2);
        wet+=v->cram[c]==ram16(0xF080+c*2);
    }
    return wet>dry;
}
static uint16_t s_item_top[GVDP_EXT_LINES*16],s_item_bottom[GVDP_EXT_LINES*16];
static int s_item_water_enabled,s_item_water_selected=-1;
static void item_palette_row(void *user,const GVDP *v,const GVDPSpriteLayer *layer)
{
    (void)user;(void)layer;
    if (!s_item_water_enabled) return;
    int wet=s2_video_water_palette(v);
    if (wet!=s_item_water_selected) {
        gvdp_set_ext_colors(s_item_top,s_item_bottom,wet?0:1<<20);
        s_item_water_selected=wet;
    }
}
enum {
    SPECIAL_FONT_K=S2_SPECIAL_FRAMES*S2_SPECIAL_TILES,
    SPECIAL_FONT_Y=SPECIAL_FONT_K+4,
    SPECIAL_HUD_NAME=SPECIAL_FONT_Y+4,
    SPECIAL_RESULT_NAME=SPECIAL_HUD_NAME+5
};
static const S2SpecialArtFrame *special_actor_art(const uint16_t **palette)
{
    if(!strcmp(s2_party.roster.character[0],"knuckles")) {
        const S2KnucklesArt *art=s2_resource_knuckles_art();
        if(art){if(palette)*palette=art->special_palette;return art->special;}
    } else if(!strcmp(s2_party.roster.character[0],"amy")) {
        const S2SpecialStageArt *art=s2_resource_amy_special();
        if(art){if(palette)*palette=art->palette;return art->frames;}
    }
    if(palette)*palette=NULL;
    return NULL;
}
void s2_video_prepare_knuckles_items(void)
{
    static const S2KnucklesArt *cached;
    static const S2SpecialArtFrame *cached_special;
    static uint8_t patterns[65536];
    static uint8_t shadow_patterns[65536],shadow_lines[2048],world_lines[2048],item_lines[2048];
    static int shadow_ready;
    const S2KnucklesArt *art=s2_resource_knuckles_art();
    const uint16_t *special_palette=NULL;
    const S2SpecialArtFrame *special=special_actor_art(&special_palette);
    const S2SpecialStageArt *amy=!strcmp(s2_party.roster.character[0],"amy")?s2_resource_amy_special():NULL;
    if (art!=cached || special!=cached_special) {
        memset(patterns,0,sizeof patterns);
        memset(item_lines,0,sizeof item_lines);
        if(art) {
            memcpy(patterns+0x4BE*32,art->shield_stars,sizeof art->shield_stars);
            memcpy(patterns+0x6AC*32,art->monitor,sizeof art->monitor);
            memcpy(patterns+0x456*32,art->signpost,sizeof art->signpost);
            memcpy(patterns+0x7D4*32,art->life,4*32);
            memcpy(patterns+0x5F4*32,art->continue_icons,sizeof art->continue_icons);
            memcpy(patterns+SPECIAL_FONT_K*32,art->font_k,sizeof art->font_k);
            for (unsigned t=0x456;t<0x46E;++t) item_lines[t]=2;
            for (unsigned t=0x7D4;t<0x7D8;++t) item_lines[t]=2;
            for (unsigned t=0x5F4;t<0x600;++t) item_lines[t]=2;
            for (unsigned t=0x400;t<0x434;++t) item_lines[t]=2;
            for (unsigned t=0x580;t<0x5A8;++t) item_lines[t]=2;
        }
        if(amy) {
            memcpy(patterns+SPECIAL_FONT_Y*32,amy->font_y,sizeof amy->font_y);
            memcpy(patterns+SPECIAL_HUD_NAME*32,amy->hud_name,sizeof amy->hud_name);
            memcpy(patterns+SPECIAL_RESULT_NAME*32,amy->result_name,sizeof amy->result_name);
            for(unsigned t=SPECIAL_HUD_NAME;t<SPECIAL_HUD_NAME+5;++t)
                item_lines[t]=GVDP_EXT_SPRITE_FORCE|3;
        }
        if(special)for(unsigned f=0;f<S2_SPECIAL_FRAMES;++f) {
            memcpy(patterns+f*S2_SPECIAL_TILES*32,special[f].tiles,S2_SPECIAL_TILES*32);
            for(unsigned t=0;t<S2_SPECIAL_TILES;++t)
                item_lines[f*S2_SPECIAL_TILES+t]=GVDP_EXT_SPRITE_FORCE|3;
        }
        /* Palette-only checkpoint pieces retain the native message atlas.
         * This range shares the actor bank's palette map, not its patterns. */
        if(art && special==art->special)for(unsigned t=0x1BC;t<0x1FA;++t)
            item_lines[t]=GVDP_EXT_SPRITE_FORCE|3;
    }
    cached=art;cached_special=special;
    /* Original grey items keep live CRAM. The signpost patch and tagged
     * world objects select a separate, native-timed Knuckles palette. */
    gvdp_set_sprite_patterns_bank(0,art||special?patterns:NULL,art||special?sizeof patterns:0,item_lines);
    const S2DonorBank *shadow=s2_party.shadow_enabled?s2_resource_shadow_bank():NULL;
    int selected=0;
    for (unsigned p=0;p<s2_party.roster.slots && p<S2_MAX_PLAYERS;++p)
        selected|=!strcmp(s2_party.roster.character[p],"shadow");
    if (shadow && selected && !shadow_ready) {
        shadow_ready=s2_native_stars_decode(g_rom,0x100000,shadow_patterns+0x4DE*32);
        if (shadow_ready) for (unsigned t=0x4DE;t<0x500;++t)
            shadow_lines[t]=GVDP_EXT_SPRITE_FORCE|1;
    }
    int shadow_active=shadow && selected && shadow_ready && shadow_palette_needed();
    int knux_world=art && !competition() && (g_ram[0xF600]&127)==12 &&
        !strcmp(s2_party.roster.character[0],"knuckles");
    int actor_special=special && (g_ram[0xF600]&127)==16;
    memset(world_lines,0,sizeof world_lines);
    /* The active colour table is shared, but only object-tagged pieces use
     * its Knuckles line. Planes and unmarked sprites keep native CRAM. */
    s_item_water_enabled=knux_world && g_ram[0xF64C];
    s_item_water_selected=-1;
    gvdp_set_host_sprites(s_item_water_enabled?item_palette_row:NULL,NULL);
    if (!shadow_active) gvdp_set_sprite_patterns_bank(1,NULL,0,NULL);
    if (!shadow_active && !knux_world && !actor_special) {gvdp_set_ext_tiles(NULL);return;}
    uint16_t top[GVDP_EXT_LINES*16]={0},bottom[GVDP_EXT_LINES*16]={0};
    /* Match the existing donor actor fade. Only marked Shadow stars use this
     * private line; native CRAM, world monitors and Knuckles stay unchanged. */
    unsigned phase=!competition() && !strcmp(s2_party.roster.character[0],"shadow")?g_ram[0xF65F]:0;
    unsigned frame=ram16(0xF65C);
    for (unsigned c=0;shadow_active && c<16;++c) {
        uint16_t dry=s2_shadow_color(shadow,c,0,phase,frame);
        uint16_t wet=s2_shadow_color(shadow,c,1,phase,frame);
        top[c]=s2_video_donor_color(dry);
        bottom[c]=s2_video_donor_color(wet);
    }
    if (knux_world) {
        unsigned zone=g_ram[0xFE10];
        const uint16_t *wet=(zone==13 || zone==15)?art->water_palette[zone==15]:art->palette;
        for (unsigned c=0;c<16;++c) {
            top[16+c]=s2_video_donor_color(art->palette[c]);
            bottom[16+c]=s2_video_donor_color(wet[c]);
        }
    }
    if(actor_special)for(unsigned c=0;c<16;++c)
        top[32+c]=bottom[32+c]=s2_video_donor_color(special_palette[c]);
    int split=1<<20;
    if (!competition() && g_ram[0xF64C])
        split=g_ram[0xF64E]?0:g_ram[0xF625]+1;
    /* Half-pipe colours belong only to marked sprites. Keep native planes
     * on their ordinary path when no world palette provider needs them. */
    gvdp_set_ext_tiles(knux_world || shadow_active?world_lines:NULL);
    memcpy(s_item_top,top,sizeof s_item_top);memcpy(s_item_bottom,bottom,sizeof s_item_bottom);
    gvdp_set_ext_colors(top,bottom,split);
    if (shadow_active) gvdp_set_sprite_patterns_bank(1,shadow_patterns,sizeof shadow_patterns,shadow_lines);
}
/* Half-pipe actors retain native simulation and SAT ordering. Presentation
 * records the original source piece, then substitutes original donor
 * geometry in a private VDP. No donor-dependent bytes enter guest state. */
static void special_actor_piece(unsigned slot,unsigned object,unsigned mapping)
{
    if(slot>=80 || object!=0xB000 || g_ram[object]!=9 ||
       (g_ram[0xF600]&127)!=16 || (strcmp(s2_party.roster.character[0],"knuckles") &&
                                 strcmp(s2_party.roster.character[0],"amy")))return;
    unsigned frame=g_ram[object+0x1A];if(frame>=S2_SPECIAL_FRAMES)return;
    unsigned base=0x34212+scene_read16(0x34212+frame*2)+2;
    if(mapping<base+8 || (mapping-base)%8 || (mapping-base)/8>scene_read16(base-2))return;
    s_special_build.frame=(uint8_t)frame;s_special_build.flip=g_ram[object+1]&3;
    s_special_build.piece[slot]=(uint8_t)((mapping-base)/8);s_special_build.ready=1;
}
static void special_actor_publish(void)
{
    if(!s_special_build.ready)return;
    SpecialActorFrame *f=&s_special_build;
    f->tiles=0;
    memcpy(f->sat,g_ram+0xF800,sizeof f->sat);
    unsigned frame=f->frame,group=frame<4?0:frame<12?1:frame<16?2:3;
    unsigned source=scene_read32(0x33AA2+group*4)&65535u;
    unsigned dp=0x345FA+scene_read16(0x345FA+frame*2),cues=scene_read16(dp);
    if(cues>S2_SPECIAL_TILES){f->ready=0;return;}
    for(unsigned i=0;i<cues;++i) {
        unsigned code=scene_read16(dp+2+i*2),count=(code>>12)+1;
        unsigned at=source+(code&4095)*2;
        if(f->tiles+count>S2_SPECIAL_TILES || at>65536-count*32){f->ready=0;return;}
        memcpy(f->patterns+f->tiles*32,g_ram+at,count*32);f->tiles+=count;
    }
    for(unsigned slot=0;slot<80;++slot)if(f->piece[slot]) {
        unsigned mp=0x34212+scene_read16(0x34212+frame*2)+2+(f->piece[slot]-1)*8;
        f->base_tile=((unsigned)((f->sat[slot*8+4]<<8)|f->sat[slot*8+5])-scene_read16(mp+2))&2047;
        break;
    }
    f->serial=++s_special_serial;s_special_history[f->serial%3]=*f;
}
static int special_actor_matches(const GVDP *v,const SpecialActorFrame *f)
{
    if(!f->ready || !f->serial || !f->tiles)return 0;
    unsigned sat=(v->reg[5]&127u)<<9;
    for(unsigned i=0;i<640;++i)if(f->sat[i]!=v->vram[(sat+i)&65535])return 0;
    /* Identical SAT geometry can accompany a different animation frame.
     * Match the uploaded native DPLC patterns too, never the next queued
     * object frame. This also keeps pause, cold-load and rewind pictures. */
    for(unsigned i=0;i<f->tiles*32;++i)
        if(f->patterns[i]!=v->vram[(f->base_tile*32+i)&65535])return 0;
    return 1;
}
static void special_actor_compose(const GVDP *v,GVDP *copy,const uint16_t *palette,
                                  int line,uint32_t *out,int width)
{
    GVDPRasterRow row;uint8_t indices[GVDP_MAX_WIDTH],owner[GVDP_MAX_WIDTH]={0};
    if(!gvdp_capture_presented_scanline(copy,line,&row))return;
    unsigned private_base=GVDP_HOST_PALETTE_BASE+2*16;
    for(unsigned i=0;i<row.width;++i)if(row.opaque[i] && row.sprite[i]>=private_base && row.sprite[i]<private_base+16) {
        owner[i]=(uint8_t)(row.high[i] || ((row.plane_a[i]&0xC0)!=0xC0 && (row.plane_b[i]&0xC0)!=0xC0));
        row.sprite[i]=(uint8_t)(16+row.sprite[i]-private_base);
    }
    if(!gvdp_compose_captured(&row,indices))return;
    int nw=256,source_width=enabled() && width>=320?(int)((int64_t)width*4/5):width;
    int origin=(source_width-nw)/2,offset=(int)row.start+((int)row.count-nw)/2;
    for(int ox=0;ox<width;++ox) {
        int nx=(int)((int64_t)ox*source_width/width)-origin;
        if(nx<0 || nx>=nw || nx+offset<0 || nx+offset>=(int)row.width)continue;
        unsigned at=(unsigned)(nx+offset),index=indices[at];
        if(index<192) {
            uint16_t shade=owner[at]?s2_video_donor_color(palette[index&15]):v->cram[index&63];
            out[ox]=genesis_dac_cram_to_argb(shade,index<64?GENESIS_DAC_NORMAL:index<128?GENESIS_DAC_SHADOW:GENESIS_DAC_HIGHLIGHT);
        } else if(index>=GVDP_HOST_PALETTE_BASE && index<GVDP_HOST_PALETTE_BASE+GVDP_HOST_PALETTE_SIZE)
            out[ox]=row.host_palette[index-GVDP_HOST_PALETTE_BASE];
    }
}
/* Original KiS2 Obj5E omits the primary name pieces. Amy retains the
 * native mappings with her own name tiles. Checkpoint wings/hand share the
 * native art and geometry, but use Knuckles' original half-pipe colours.
 * A NULL destination only checks whether this scanline needs recomposition. */
static int special_primary_ui_sat(const GVDP *v,GVDP *copy,int line)
{
    int knuckles=!strcmp(s2_party.roster.character[0],"knuckles");
    if(!knuckles && (strcmp(s2_party.roster.character[0],"amy") || line<16 || line>=24))return 0;
    unsigned table=(v->reg[5]&127u)<<9,slot=0;int changed=0;
    for(unsigned n=0;n<64 && slot<80;++n) {
        unsigned at=table+slot*8,size=v->vram[(at+2)&65535];
        int y=(((v->vram[at&65535]<<8)|v->vram[(at+1)&65535])&1023)-128;
        if(!(size&0x80) && line>=y && line<y+((size&3)+1)*8) {
            unsigned attr=(v->vram[(at+4)&65535]<<8)|v->vram[(at+5)&65535],tile=attr&2047;
            unsigned cells=(((size>>2)&3)+1)*((size&3)+1);
            if((attr&0xE000)==0xA000 && tile>=0x1FA &&
               tile+cells<=0x1FF && line>=16 && line<24 && line<y+8) {
                if(!copy)return 1;
                if(knuckles) {
                    copy->vram[at&65535]=0;copy->vram[(at+1)&65535]=0;
                } else {
                    attr=(attr&0xF800)|(SPECIAL_HUD_NAME+tile-0x1FA);
                    copy->vram[(at+2)&65535]=(uint8_t)(size|0x80);
                    copy->vram[(at+4)&65535]=(uint8_t)(attr>>8);
                    copy->vram[(at+5)&65535]=(uint8_t)attr;
                }
                changed=1;
            } else if(knuckles && (attr&0x6000)==0x2000 &&
                      tile>=0x1BC && tile+cells<=0x1FA) {
                if(!copy)return 1;
                copy->vram[(at+2)&65535]=(uint8_t)(size|GVDP_SPRITE_PALETTE_ONLY);
                changed=1;
            }
        }
        slot=v->vram[(at+3)&65535]&127;if(!slot)break;
    }
    return changed;
}
static int special_actor_sat(const GVDP *v,const SpecialActorFrame *picture,
                             const S2SpecialArtFrame *art,int line,GVDP *copy)
{
    const S2SpecialArtFrame *f=&art[picture->frame];if(!f->count)return 0;
    unsigned sat=(v->reg[5]&127u)<<9,first=80;
    for(unsigned i=0;i<80;++i)if(picture->piece[i]){first=i;break;}
    if(first==80)return 0;
    unsigned mp=0x34212+scene_read16(0x34212+picture->frame*2)+2+(picture->piece[first]-1)*8;
    unsigned old_size=scene_read8(mp+1);
    int dx=(int16_t)scene_read16(mp+6),dy=(int8_t)scene_read8(mp);
    if(picture->flip&1)dx=-dx-(((old_size>>2)&3)+1)*8;
    if(picture->flip&2)dy=-dy-((old_size&3)+1)*8;
    unsigned at=first*8;
    int x=(int)(((picture->sat[at+6]<<8)|picture->sat[at+7])&1023)-128-dx;
    int y=(int)(((picture->sat[at]<<8)|picture->sat[at+1])&1023)-128-dy;
    int affected=0;
    for(unsigned i=0;i<80;++i)if(picture->piece[i]) {
        unsigned p=i*8;int sy=((picture->sat[p]<<8)|picture->sat[p+1])-128;
        if(line>=sy && line<sy+((picture->sat[p+2]&3)+1)*8)affected=1;
    }
    for(unsigned i=0;i<f->count;++i) {
        const S2SpecialPiece *p=&f->pieces[i];int py=p->y,h=((p->size&3)+1)*8;
        if(picture->flip&2)py=-py-h;
        if(line>=y+py && line<y+py+h)affected=1;
    }
    if(!affected)return 0;
    *copy=*v;uint8_t sprites[640]={0};unsigned slot=0,count=0;int inserted=0;
    for(unsigned n=0;n<64;++n) {
        const uint8_t *native=picture->sat+slot*8;
        if(picture->piece[slot]) {
            if(!inserted)for(unsigned i=0;i<f->count && count<64;++i) {
                const S2SpecialPiece *p=&f->pieces[i];int px=p->x,py=p->y;
                if(picture->flip&1)px=-px-(((p->size>>2)&3)+1)*8;
                if(picture->flip&2)py=-py-((p->size&3)+1)*8;
                unsigned attr=(p->attr&0x9800)|((unsigned)(native[4]&0x60)<<8)|
                    (picture->frame*S2_SPECIAL_TILES+(p->attr&2047));
                attr^=(picture->flip&1?0x800:0)|(picture->flip&2?0x1000:0);
                uint8_t *dst=sprites+count++*8;unsigned sx=(x+px+128)&1023,sy=(y+py+128)&1023;
                dst[0]=(uint8_t)(sy>>8);dst[1]=(uint8_t)sy;dst[2]=p->size|0x80;
                dst[4]=(uint8_t)(attr>>8);dst[5]=(uint8_t)attr;
                dst[6]=(uint8_t)(sx>>8);dst[7]=(uint8_t)sx;
            }
            inserted=1;
        } else if(count<64)memcpy(sprites+count++*8,native,8);
        slot=native[3]&127;if(!slot || slot>=80)break;
    }
    if(!inserted || !count)return 0;
    for(unsigned i=0;i<count;++i)sprites[i*8+3]=(uint8_t)(i+1<count?i+1:0);
    for(unsigned i=0;i<640;++i)copy->vram[(sat+i)&65535]=sprites[i];
    return 1;
}
void s2_video_special_actor(const GVDP *v,int line,uint32_t *out,int width)
{
    static SpecialActorFrame picture;
    static int valid;
    const uint16_t *palette=NULL;
    const S2SpecialArtFrame *art=special_actor_art(&palette);
    if(!line) {
        valid=0;
        if(art && (v->reg[12]&15)==8) {
            const SpecialActorFrame *best=NULL;
            for(unsigned i=0;i<4;++i) {
                const SpecialActorFrame *f=i==3?&s_special_display:&s_special_history[i];
                if((!best || f->serial>best->serial) && special_actor_matches(v,f))best=f;
            }
            if(best){picture=*best;s_special_display=*best;valid=1;}
        }
    }
    if(!art || !(v->reg[1]&64) || (v->reg[12]&15)!=8)return;
    int ui=special_primary_ui_sat(v,NULL,line);
    GVDP copy;
    int body=valid?special_actor_sat(v,&picture,art,line,&copy):0;
    if(!body && !ui)return;
    if(!body)copy=*v;
    if(ui)special_primary_ui_sat(&copy,&copy,line);
    special_actor_compose(v,&copy,palette,line,out,width);
}
static void special_results_begin(void)
{
    memset(&s_ss_result_build,0,sizeof s_ss_result_build);
    if((g_ram[0xF600]&127)!=16 || s2_party.roster.slots!=1 ||
       (strcmp(s2_party.roster.character[0],"knuckles") && strcmp(s2_party.roster.character[0],"amy")) ||
       g_ram[0xB800]!=0x6F)return;
    SpecialResultFrame *f=&s_ss_result_build;
    f->got_emerald=g_ram[0xFFB0];f->main_routine=g_ram[0xB824];
    for(unsigned o=0xB000;o<0xD400 && f->count<16;o+=64) {
        if(g_ram[o]!=0x6F || scene_read32(0xFF0000u+o+4)!=0x14ED0 || g_ram[o+0x1A]>=29)continue;
        SpecialResultActor *a=&f->actors[f->count++];a->object=(uint16_t)o;
        a->x=(int16_t)(ram16(o+8)-128);a->y=(int16_t)(ram16(o+10)-128);a->frame=g_ram[o+0x1A];
    }
}
static void special_results_piece(unsigned slot,unsigned object)
{
    if(slot>=80)return;
    for(unsigned i=0;i<s_ss_result_build.count;++i)if(s_ss_result_build.actors[i].object==object) {
        s_ss_result_build.owner[slot]=(uint8_t)(i+1);break;
    }
}
static void special_results_publish(void)
{
    if(!s_ss_result_build.count)return;
    memcpy(s_ss_result_build.sat,g_ram+0xF800,sizeof s_ss_result_build.sat);
    s_ss_result_build.serial=++s_ss_result_serial;
    s_ss_result_history[s_ss_result_serial%3]=s_ss_result_build;
}
static unsigned special_results_emit(uint8_t sat[640],unsigned count,
                                    const S2SpecialResultArtFrame *frame,int x,int y,int amy)
{
    for(unsigned i=0;i<frame->count && count<80;++i) {
        const S2SpecialPiece *p=&frame->pieces[i];uint8_t *dst=sat+count++*8;
        unsigned sx=(x+p->x+128)&1023,sy=(y+p->y+128)&1023,attr=p->attr;
        dst[0]=(uint8_t)(sy>>8);dst[1]=(uint8_t)sy;dst[2]=p->size;
        /* The stock result font has no K. Its four original donor tiles
         * follow the half-pipe atlas; every other piece keeps live VRAM. */
        unsigned tile=attr&2047;
        if(!amy && tile==0x3E) {
            attr=(attr&0xF800)|SPECIAL_FONT_K;dst[2]|=0x80;
        } else if(amy && tile==0x5D8) {
            attr=(attr&0xF800)|SPECIAL_FONT_Y;dst[2]|=0x80;
        } else if(amy && tile>=0x5B0 && tile<0x5B8) {
            attr=(attr&0xF800)|(SPECIAL_RESULT_NAME+tile-0x5B0);dst[2]|=0x80;
        }
        dst[4]=(uint8_t)(attr>>8);dst[5]=(uint8_t)attr;
        dst[6]=(uint8_t)(sx>>8);dst[7]=(uint8_t)sx;
    }
    return count;
}
void s2_video_special_results(const GVDP *v,int line,uint32_t *out,int width)
{
    static SpecialResultFrame picture;static int valid;
    int amy=!strcmp(s2_party.roster.character[0],"amy");
    const S2KnucklesArt *knuckles=!strcmp(s2_party.roster.character[0],"knuckles")?s2_resource_knuckles_art():NULL;
    const S2SpecialStageArt *amy_art=amy?s2_resource_amy_special():NULL;
    const S2SpecialResultArtFrame *art=knuckles?knuckles->special_results:amy_art?amy_art->results:NULL;
    if(!line) {
        valid=0;
        if(art && s2_party.roster.slots==1 && (v->reg[12]&15)==1) {
            const SpecialResultFrame *best=NULL;unsigned table=(v->reg[5]&127u)<<9;
            for(unsigned i=0;i<4;++i) {
                const SpecialResultFrame *f=i==3?&s_ss_result_display:&s_ss_result_history[i];
                if(!f->serial || !f->count || (best && f->serial<best->serial))continue;
                unsigned n=0;for(;n<640 && f->sat[n]==v->vram[(table+n)&65535];++n){}
                if(n==640)best=f;
            }
            if(best){picture=*best;s_ss_result_display=*best;valid=1;}
        }
    }
    if(!valid || !art || !(v->reg[1]&64) || (v->reg[12]&15)!=1)return;
    unsigned table=(v->reg[5]&127u)<<9,slot=0;int affected=0;
    for(unsigned n=0;n<80 && slot<80;++n) {
        const uint8_t *p=picture.sat+slot*8;
        if(picture.owner[slot]) {
            int y=((p[0]<<8)|p[1])-128,h=((p[2]&3)+1)*8;
            const SpecialResultActor *a=&picture.actors[picture.owner[slot]-1];
            if((line>=y && line<y+h) || (line>=a->y && line<a->y+16))affected=1;
        }
        slot=p[3]&127;if(!slot)break;
    }
    if(!affected)return;
    GVDP copy=*v;uint8_t sat[640]={0};unsigned count=0,seen=0;slot=0;
    for(unsigned n=0;n<80 && slot<80;++n) {
        const uint8_t *native=picture.sat+slot*8;unsigned owner=picture.owner[slot];
        if(owner) {
            unsigned i=owner-1;
            if(!(seen&(1u<<i))) {
                seen|=1u<<i;const SpecialResultActor *a=&picture.actors[i];unsigned frame=a->frame;
                int offset=amy?0:frame==1?80:frame==22?(picture.got_emerald?68:80):0;
                count=special_results_emit(sat,count,&art[frame],a->x+offset,a->y,amy);
                if(!amy && (frame==1 || frame==22) && picture.got_emerald) {
                    int name_x=a->x+(frame==22?-36:8),hardware_x=name_x+128;
                    int visible=picture.main_routine>=0x30?hardware_x>32:hardware_x>=80 && hardware_x<=512;
                    if(visible)count=special_results_emit(sat,count,&art[29],name_x,a->y,0);
                }
            }
        } else if(count<80)memcpy(sat+count++*8,native,8);
        slot=native[3]&127;if(!slot)break;
    }
    if(!count)return;
    for(unsigned i=0;i<count;++i)sat[i*8+3]=(uint8_t)(i+1<count?i+1:0);
    for(unsigned i=0;i<640;++i)copy.vram[(table+i)&65535]=sat[i];
    GVDPRasterRow row;uint8_t indices[GVDP_MAX_WIDTH];
    if(!gvdp_capture_presented_scanline(&copy,line,&row) || !gvdp_compose_captured(&row,indices))return;
    int origin=(width-320)/2,offset=(int)row.start+((int)row.count-320)/2;
    for(int x=0;x<320;++x)if(x+origin>=0 && x+origin<width && x+offset>=0 && x+offset<(int)row.width) {
        unsigned c=indices[x+offset];
        if(c<192)out[x+origin]=genesis_dac_cram_to_argb(v->cram[c&63],
            c<64?GENESIS_DAC_NORMAL:c<128?GENESIS_DAC_SHADOW:GENESIS_DAC_HIGHLIGHT);
        else if(c>=GVDP_HOST_PALETTE_BASE)out[x+origin]=row.host_palette[c-GVDP_HOST_PALETTE_BASE];
    }
}
static void add_mapping(SceneFrame *dst,uint32_t *keys,uint32_t owner,unsigned map,unsigned frame,unsigned gfx,unsigned flags,
                        int x,int y,int anchor,int static_mapping)
{
    unsigned p=map;int count=1;
    if(!static_mapping) {
        p+=(int16_t)scene_read16(p+(frame&255u)*2u);
        count=scene_read16(p);p+=2;
    }
    if(count>1024)return;
    for(int n=0;n<count && dst->count<SCENE_SPRITES;++n,p+=8) {
        int dy=(int8_t)scene_read8(p),size=scene_read8(p+1)&15;
        int dx=(int16_t)scene_read16(p+6);
        int vs=competition();
        uint16_t attr=(uint16_t)(scene_read16(p+(vs?4:2))+gfx);
        if(flags&1){dx=-dx-(((size>>2)&3)+1)*8;attr^=0x800;}
        if(flags&2){dy=-dy-((size&3)+1)*8;attr^=0x1000;}
        if(!vs && !s2_runtime_campaign_online() && (y+dy>=224 || y+dy+((size&3)+1)*8<=0))continue;
        if(keys)keys[dst->count]=owner|(unsigned)n;
        SceneSprite *q=&dst->sprites[dst->count++];
        q->x=x+dx;q->y=y+dy;q->attr=attr;q->size=(uint8_t)(vs?((size&12)|((size&3)>>1)):size);
        q->hud=(uint8_t)(anchor|((flags&0x400)?128:0));
        if ((flags&0x100) && knuckles_item_tile((attr&2047)*(vs?2:1))) q->size|=0x80;
        if ((flags&0x100) && knuckles_palette_tile((attr&2047)*(vs?2:1))) q->size|=GVDP_SPRITE_PALETTE_ONLY;
        if ((flags&0x200) && (attr&2047)*(vs?2u:1u)>=0x4DE &&
            (attr&2047)*(vs?2u:1u)<0x500) q->size|=0xC0;
    }
}

static void load_placements(void)
{
    unsigned zone=ram16(0xFE10),index=((zone>>8)*4u)+((zone&1u)*2u);
    unsigned base=0xE6800u+(int16_t)scene_read16(0xE6800u+index);
    if (competition() && g_ram[0xFE10]==12) base=(zone&1)?0x18492:0x1802A;
    if(base==s_placement_base && s_placement_count)return;
    s_placement_base=base;s_placement_count=0;unsigned state=1;
    for(unsigned p=base;p+6<=0x100000u && s_placement_count<SCENE_PLACEMENTS;p+=6) {
        unsigned x=scene_read16(p);if(x==65535u)break;
        Placement *q=&s_placements[s_placement_count++];
        q->address=p;q->x=(uint16_t)x;q->y=scene_read16(p+2);
        q->id=scene_read8(p+4);q->subtype=scene_read8(p+5);
        q->state=(uint8_t)((q->y&0x8000)?state++:0);q->loaded=0;q->slot=0;
    }
}
static int placement_alive(Placement *p)
{
    for(unsigned o=0xB400;o<0xD000;o+=64) {
        if(!g_ram[o])continue;
        if(p->state && g_ram[o+0x23]==p->state){p->slot=(uint16_t)o;return 1;}
        if(!p->state && g_ram[o]==p->id && ram16(o+8)==p->x){p->slot=(uint16_t)o;return 1;}
    }
    return 0;
}
static void spawn_scene(void)
{
    if (competition()) activation_bounds(ram16(0xEE20),enabled()?s_requested_width:320,&s_vs_other_lo,&s_vs_other_hi);
    load_placements();s_loader_active=1;
    if (competition()) {
        /* Replace the native six fixed object blocks with the same bounded
         * global pool used by expanded single-player worlds. Both viewports
         * retain objects; leaving both windows releases ordinary live roots. */
        int lo,hi;activation_bounds(ram16(0xEE00),enabled()?s_requested_width:320,&lo,&hi);
        for (unsigned i=0;i<s_placement_count;++i) {
            Placement *p=&s_placements[i];unsigned o=p->slot;
            if (!o || g_ram[o]!=p->id || g_ram[o+0x23]!=p->state ||
                scene_contains(p->x,lo,hi) || scene_contains(ram16(o+8),lo,hi)) continue;
            if (p->state && g_ram[o+0x23]==p->state) write8(0xFC02+p->state,g_ram[0xFC02+p->state]&127);
            for(unsigned k=0;k<64;k+=4)write32(o+k,0);p->slot=0;p->loaded=0;
        }
        for (unsigned o=0xB400;o<0xD000;o+=64)
            if (g_ram[o] && (g_ram[o+1]&0x4C) && !scene_contains(ram16(o+8),lo,hi))
                for(unsigned k=0;k<64;k+=4)write32(o+k,0);
    }
    int camera=ram16(0xEE00),w=enabled()?s_requested_width:320;
    int lo,hi;activation_bounds(camera,w,&lo,&hi);
    if (s2_runtime_campaign_online()) campaign_cells();
    int player=ram16(0xB008);s_pool_pressure=0;
    for(unsigned i=0;i<s_placement_count;++i) {
        Placement *p=&s_placements[i];
        if(!scene_contains(p->x,lo,hi))p->loaded=0;
        else if(!p->loaded && placement_alive(p))p->loaded=1;
    }
    for(unsigned pass=0;pass<s_placement_count;++pass) {
        int best=-1,distance=INT_MAX;
        for(unsigned i=0;i<s_placement_count;++i) {
            Placement *p=&s_placements[i];
            if(p->loaded || !scene_contains(p->x,lo,hi))continue;
            if(p->state && (g_ram[0xFC02+p->state]&128)){p->loaded=1;continue;}
            int d=abs((int)p->x-player);
            if (s2_runtime_campaign_online() || competition()) for (unsigned n=1;n<S2_MAX_PLAYERS;++n) {
                int x,y;
                if (s2_runtime_player_position(n,&x,&y) && abs((int)p->x-x)<d) d=abs((int)p->x-x);
            }
            if(d<distance){best=(int)i;distance=d;}
        }
        if(best<0)break;
        Placement *p=&s_placements[best];unsigned o;
        for(o=0xB400;o<0xD000 && g_ram[o];o+=64){}
        if(o==0xD000){++s_pool_pressure;if(s2_runtime_campaign_online())s_streaming_pressure=1;break;}
        write16(o+8,p->x);write16(o+12,p->y&4095);
        unsigned flip=(p->y>>13)&3;
        write8(o+1,flip);write8(o+0x22,flip);write8(o+0x23,p->state);
        write8(o+0x28,p->subtype);
        if(p->state)write8(0xFC02+p->state,g_ram[0xFC02+p->state]|128);
        write8(o,p->id);p->slot=(uint16_t)o;p->loaded=1;++s_spawned;
    }
    unsigned r=0,l=0,rs=1,ls=1;
    while(r<s_placement_count && s_placements[r].x<(unsigned)((camera&~127)+640)){rs+=s_placements[r].state!=0;++r;}
    while(l<s_placement_count && (int)s_placements[l].x<((camera-128)&~127)){ls+=s_placements[l].state!=0;++l;}
    write32(0xF770,s_placement_base+r*6);write32(0xF774,s_placement_base+l*6);
    write8(0xFC00,rs);write8(0xFC01,ls);write16(0xF76E,camera&0xFF80);
    write16(0xF7DA,(camera-128)&0xFF80);
}
static unsigned rings_end(void)
{
    unsigned o=0xE806;
    while(o+6<=0xEE00 && ram16(o+2)!=65535u)o+=6;
    return o;
}
static void capture_objects_into(SceneFrame *dst,int render_width,int read_only,uint32_t *keys)
{
    dst->count=0;if(!read_only){s_visible_count=0;++s_scene_tick;}
    dst->scene=competition() && (g_ram[0xF711] || ram16(0xB0BE))?3:gameplay() && g_ram[0xF711]?1:(g_ram[0xF600]&127)==16?2:0;
    int vs=competition();
    if (vs) activation_bounds(ram16(0xEE20),render_width,&s_vs_other_lo,&s_vs_other_hi);
    int cam=ram16(vs?0xEE00:0xEEF0),cy=ram16(vs?0xEE04:0xEEF4);
    dst->camera_x=cam;dst->camera_y=cy;
    for (unsigned p=0;p<S2_MAX_PLAYERS;++p) player_camera(p,&dst->views[p][0],&dst->views[p][1]);
    int campaign=s2_runtime_campaign_online();
    int vs_left[2]={0,0};
    if (vs) {vs_left[0]=view_left(cam,render_width);vs_left[1]=view_left(ram16(0xEE20),render_width);}
    int left=gameplay()?view_left(cam,render_width):cam-(render_width-320)/2;
    if(gameplay() && g_ram[0xF711]) {
        unsigned blink=(g_ram[0xFE05]&8)==0;
        unsigned frame=blink*((ram16(0xFE20)==0?1:0)+(g_ram[0xFE23]==9?2:0));
        /* BuildHUD uses spriteScreenPositionYCentered(24), not Y=24. */
        add_mapping(dst,keys,0x00010000u,0x40A9A,frame,0x86CA,0,16,136,1,0);
        unsigned end=rings_end();
        for(unsigned o=0xE806;o<end;o+=6) {
            if(ram16(o)&0x8000)continue;
            int x=ram16(o+2),y=ram16(o+4);
            if(!campaign && (x<left-16 || x>=left+render_width+16))continue;
            unsigned fr=g_ram[o+1]?g_ram[o+1]:g_ram[0xFEA3];
            unsigned map=0x1736Au+(int16_t)scene_read16(0x1736Au+fr*2u);
            add_mapping(dst,keys,o<<16,map,0,0x26BC,0,x-cam,(int16_t)(y-cy),0,1);
        }
    }
    if (vs && dst->scene==3) {
        int lo,hi;activation_bounds(cam,render_width,&lo,&hi);
        for (unsigned o=0xE806,end=rings_end();o<end;o+=6) {
            if ((ram16(o)&0x8000) || !scene_contains(ram16(o+2),lo,hi)) continue;
            unsigned fr=g_ram[o+1]?g_ram[o+1]:g_ram[0xFEA3];
            unsigned map=0x1736Au+(int16_t)scene_read16(0x1736Au+fr*2u);
            add_mapping(dst,keys,o<<16,map,0,0x235E,0,(int)ram16(o+2)-cam,(int16_t)(ram16(o+4)-cy),0,1);
        }
    }
    for(unsigned pri=0;pri<8;++pri) {
        unsigned queue=0xAC00+pri*128,bytes=ram16(queue);if(bytes>126)bytes=126;
        for(unsigned k=2;k<=bytes;k+=2) {
            unsigned o=ram16(queue+k);if(o<0xB000 || o>=0xD400 || !g_ram[o])continue;
            unsigned flags=g_ram[o+1],multi=flags&64,space=flags&12;
            if(g_ram[o]==0x3A && g_ram[o+0x1A]<=2 && custom_result_name())continue;
            if (s2_runtime_knuckles_item(o)) flags|=0x100;
            if (s2_runtime_shadow_star(o)) flags|=0x200;
            int x,y,anchor=0;
            if(space || multi) {
                x=(int16_t)(ram16(o+8)-cam);
                y=(int16_t)(ram16(o+12)-cy);
                int radius=g_ram[o+(multi?0xE:0x19)];
                if(!campaign && !vs) {
                    if(x+radius<left-cam || x-radius>=left-cam+render_width)continue;
                    if(flags&16){int h=g_ram[o+(multi?0x14:0x16)];if(y+h<0 || y-h>=224)continue;}
                    else {y=((y+128)&2047)-128;if(y<-32 || y>=256)continue;}
                }
                if (vs) {
                    int visible=0;
                    for (unsigned p=0;p<2;++p) {
                        int dy=(int16_t)(ram16(o+12)-ram16(p?0xEE24:0xEE04));
                        int px=ram16(o+8);
                        if (px+radius>=vs_left[p] && px-radius<vs_left[p]+render_width && dy>=-32 && dy<256) visible=1;
                    }
                    if (!visible) continue;
                }
                if(!read_only && s_visible_count<144)s_visible_objects[s_visible_count++]=o;
            } else {
                x=(int16_t)ram16(o+8)-128;y=(int16_t)ram16(o+10)-128;anchor=2;
                /* The drowning number has become native screen-space, but
                 * still belongs to that camera viewport. At a clamped edge
                 * its horizontal origin differs from centered menu text. */
                if(g_ram[o]==0x0A && !vs)
                    x+=cam-left-(render_width-320)/2;
                /* The red serrated edge follows its widened plane boundary;
                 * text keeps its original glyph size and centered layout. */
                if (g_ram[0xF600]==0 && g_ram[o]==0xB0)
                    x=(int)((int64_t)x*render_width/320)-(render_width-320)/2;
                if (g_ram[o]==0x34) {
                    if (g_ram[o+0x1A]==0x16)
                        x=x*render_width/320-(render_width-320)/2;
                    else {
                        /* Preserve the settled text layout. Travel to/from
                         * its off-screen source spans the selected viewport,
                         * so the native deletion cannot pop a visible word. */
                        int target=(int16_t)ram16(o+0x30)-128;
                        x=target+(int)((int64_t)(x-target)*render_width/320);
                    }
                }
            }
            unsigned map=scene_read32(0xFF0000u+o+4),gfx=ram16(o+2);
            if(g_ram[o]==0x3A || g_ram[o]==0x34 || g_ram[o]==0x0A)flags|=0x400;
            if(multi) {
                if(g_ram[o+0xB])add_mapping(dst,keys,o<<16,map,g_ram[o+0xB],gfx,flags,x,y,anchor,0);
                unsigned count=g_ram[o+0xF];if(count>8)count=8;
                for(unsigned n=0;n<count;++n) {
                    unsigned child=o+0x10+n*6;
                    int sx=(int16_t)(ram16(child)-cam),sy=(int16_t)(ram16(child+2)-cy);
                    if (!campaign) sy=((sy+128)&2047)-128;
                    add_mapping(dst,keys,(o<<16)|((n+1)<<12),map,g_ram[child+5],gfx,flags,sx,sy,0,0);
                }
            } else add_mapping(dst,keys,o<<16,map,g_ram[o+0x1A],gfx,flags,x,y,anchor,(flags&32)!=0);
        }
    }
}

static void capture_objects(void)
{
    capture_objects_into(&s_build,s_requested_width,0,
        s_interpolation_enabled && !competition()?s_build_keys:NULL);
}
static void native_capture_objects(void)
{
    s_native_pending=g_ram[0xF600]==12 && g_ram[0xF711] && !competition();
    if(s_native_pending)capture_objects_into(&s_native_build,320,1,s_native_build_keys);
}
static void native_publish_sprites(void)
{
    if(!s_native_pending)return;
    s_native_pending=0;s_native_build.serial=++s_native_serial;
    memcpy(s_native_build.sat,g_ram+0xF800,640);
    SceneFrame *f=&s_native_history[s_native_serial%3];
    f->count=s_native_build.count;f->serial=s_native_serial;f->scene=s_native_build.scene;
    f->camera_x=s_native_build.camera_x;f->camera_y=s_native_build.camera_y;
    memcpy(f->views,s_native_build.views,sizeof f->views);memcpy(f->sat,s_native_build.sat,640);
    memcpy(f->sprites,s_native_build.sprites,f->count*sizeof *f->sprites);
    memcpy(s_native_keys[s_native_serial%3],s_native_build_keys,f->count*sizeof *s_native_build_keys);
}
static void publish_sprites(void)
{
    for(unsigned n=0;n<s_visible_count;++n) {
        unsigned o=s_visible_objects[n]; if(g_ram[o])write8(o+1,g_ram[o+1]|128);
    }
    s_build.serial=++s_serial;memcpy(s_build.sat,g_ram+0xF800,640);
    SceneFrame *dst=&s_history[s_serial%3];
    dst->count=s_build.count;dst->serial=s_serial;dst->scene=s_build.scene;
    dst->camera_x=s_build.camera_x;dst->camera_y=s_build.camera_y;
    memcpy(dst->views,s_build.views,sizeof dst->views);
    memcpy(dst->sat,s_build.sat,640);
    memcpy(dst->sprites,s_build.sprites,s_build.count*sizeof(SceneSprite));
    unsigned slot=s_serial%3;s_key_serial[slot]=0;
    if(s_interpolation_enabled && !competition()) {
        memcpy(s_key_history[slot],s_build_keys,s_build.count*sizeof *s_build_keys);
        s_key_serial[slot]=s_serial;
    }
}
int s2_video_hook(uint32_t pc)
{
    switch (pc) {
    case 0x4450: s_donor_fade=(DonorFade){0}; /* fresh level retires SS publications */
        /* fall through */
    case 0x4F64:
        memset(&s_special_build,0,sizeof s_special_build);memset(s_special_history,0,sizeof s_special_history);
        memset(&s_special_display,0,sizeof s_special_display);s_special_serial=0;
        memset(&s_ss_result_build,0,sizeof s_ss_result_build);memset(s_ss_result_history,0,sizeof s_ss_result_history);
        memset(&s_ss_result_display,0,sizeof s_ss_result_display);s_ss_result_serial=0;
        break;
    case 0x23C6: s_donor_fade=(DonorFade){FADE_FROM_BLACK,0};break;
    case 0x246A: s_donor_fade=(DonorFade){FADE_TO_BLACK,0};break;
    case 0x24E8: s_donor_fade=(DonorFade){FADE_FROM_WHITE,0};break;
    case 0x2592: s_donor_fade=(DonorFade){FADE_TO_WHITE,0};break;
    case 0x23FE: case 0x248A: case 0x2522: case 0x25B2:
        if(s_donor_fade.steps<22)++s_donor_fade.steps;
        break;
    }
    if (pc==0x16604) {s2_video_prepare_knuckles_items();front_ui_begin();}
    if(pc==0x16712 || pc==0x1671A || pc==0x16B90 || pc==0x16B98)front_ui_publish();
    if (pc==0x1684E || pc==0x1688E || pc==0x168E6 || pc==0x16938 ||
        pc==0x16DEE || pc==0x16E40 || pc==0x16E9C || pc==0x16F00) {
        /* REV01 normal/interlace DrawSprite DBFs, after a complete SAT piece.
         * Size bit7 is ignored by hardware and selects the existing private
         * sprite bank. Mark independent of asset availability so guest state
         * remains identical for peers with different presentation resources. */
        unsigned next=g_cpu.A[2]&65535u,o=g_cpu.A[0]&65535u;
        int competition=ram16(0xFFD8)!=0;
        if (!(next&7) && ((next>=0xF808 && next<=0xFA80) ||
            (competition && next>=0xDD08 && next<=0xDF80))) {
            /* REV01 P2 builds at DD00; H-int uploads it over the same VRAM
             * SAT halfway through the frame (LEA at $16A80). */
            unsigned tile=ram16(next-4)&2047u;
            unsigned p=next>=0xDD08 && next<=0xDF80;
            unsigned slot=(next-(p?0xDD00:0xF800))/8-1;
            /* BuildHUD submits its mappings directly, without an object in
             * A0. Include both normal and interlaced text/number mappings;
             * A1 points just past the completed piece in this ROM interval. */
            unsigned mapping=g_cpu.A[1]&0xFFFFFFu;
            if(!competition){special_actor_piece(slot,o,mapping);special_results_piece(slot,o);}
            if(slot<80 && mapping>0x40A9A && mapping<=0x40D06)
                s_front_build.kind[p][slot]=1;
            else if(o>=0xB000 && o<0xD800 && !(o&63) && slot<80)
                s_front_build.kind[p][slot]=g_ram[o]==0x3A?2:
                    (g_ram[o]==0x34 || g_ram[o]==0x0A)?3:0;
            /* Mode2 cells are8x16: each index addresses two ordinary tiles. */
            if (competition) tile*=2;
            if (s2_runtime_knuckles_item(o) && knuckles_item_tile(tile)) g_ram[next-6]|=0x80;
            if (s2_runtime_knuckles_item(o) && knuckles_palette_tile(tile)) g_ram[next-6]|=GVDP_SPRITE_PALETTE_ONLY;
            if (s2_runtime_shadow_star(o) && tile>=0x4DE && tile<0x500) g_ram[next-6]|=0xC0;
        }
        return 0;
    }
    if(pc==0x17AA4 && !g_ram[0xF76C]) {
        s_placement_base=s_placement_count=0;s_loader_active=0;
        memset(s_active_cells,0,sizeof s_active_cells);s_streaming_pressure=0;
    }
    if(pc==0x17B84 && gameplay() && (enabled() || s_loader_active)) {spawn_scene();return 1;}
    if (competition() && s_loader_active && pc==0x17CCC) {spawn_scene();return 1;}
    if(!enabled()) {
        if(s_interpolation_enabled) {
            if(pc==0x16604)native_capture_objects();
            if(pc==0x16712 || pc==0x1671A)native_publish_sprites();
        }
        return 0;
    }
    if (competition()) {
        switch (pc) {
        case 0x17C50:case 0x17CCC:
            write8(0xF76C,4);spawn_scene();return 1;
        case 0x16604:capture_objects();return 0;
        case 0x16B90:case 0x16B98:publish_sprites();return 0;
        case 0x170D0:g_cpu.A[1]=0xFFFFE806u;g_cpu.A[2]=0xFFFF0000u|rings_end();return 0;
        default:return 0;
        }
    }
    switch(pc) {
    case 0x17AA4:return 0;
    case 0x16604:capture_objects();return 0;
    case 0x16712:case 0x1671A:publish_sprites();return 0;
    case 0x170D0:
        /* Use the game's original collision/reward/consumption routines for
         * both Sonic and Tails. Only lift the old camera's ring search gate. */
        if(gameplay()){g_cpu.A[1]=0xFFFFE806u;g_cpu.A[2]=0xFFFF0000u|rings_end();}
        return 0;
    case 0x16F16:case 0x16F3E: {
        if(!gameplay())return 0;
        unsigned o=g_cpu.A[0]&65535u;
        int x=ram16(o+8),y=(int16_t)(ram16(o+12)-ram16(0xEE04));
        int radius=pc==0x16F3E?g_ram[o+0x19]:0;
        int left=view_left(ram16(0xEE00),s_requested_width);
        g_cpu.D[0]=(x+radius<left || x-radius>=left+s_requested_width || y<0 || y>=224)?1:0;
        if (s2_runtime_campaign_online()) g_cpu.D[0]=!active_x(x);
        g_cpu.SR=(uint16_t)((g_cpu.SR&~15u)|(g_cpu.D[0]?0:4));
        return 1;
    }
    default:
        if(gameplay()) {
            int x=(uint16_t)((uint16_t)g_cpu.D[0]+ram16(0xF7DA));
            int lo,hi;activation_bounds(ram16(0xEE00),enabled()?s_requested_width:320,&lo,&hi);
            g_cpu.D[0]=(g_cpu.D[0]&0xFFFF0000u)|(scene_contains(x,lo,hi)?320u:641u);
        }
        return 0;
    }
}

static uint8_t *s_priority;
static uint32_t *s_front_rgb;
static int s_priority_capacity;
static int reserve_sprite_row(int width)
{
    if(width<=s_priority_capacity && s_priority && s_front_rgb)return 1;
    uint8_t *p=(uint8_t *)realloc(s_priority,(size_t)width);
    uint32_t *f=(uint32_t *)realloc(s_front_rgb,(size_t)width*sizeof *f);
    if(p)s_priority=p;if(f)s_front_rgb=f;
    if(!p || !f)return 0;
    s_priority_capacity=width;return 1;
}
static void select_scene(const GVDP *v)
{
    s_display=NULL;
    unsigned sat=(v->reg[5]&127u)<<9;
    /* Keep the front buffer's publication alive even if the producer cycles
     * through every history slot before the next native sprite DMA. */
    for(unsigned n=0;n<4;++n) {
        const SceneFrame *p=n==3?&s_display_frame:&s_history[n];
        if(!p->serial || (s_display && p->serial<s_display->serial))continue;
        int matches=1;
        for(unsigned k=0;k<640;++k)
            if(p->sat[k]!=v->vram[(sat+k)&65535u]) {matches=0;break;}
        if(matches)s_display=p;
    }
    if(!s_display) {
        ++s_scene_misses;
        int scene=competition() && (g_ram[0xF711] || ram16(0xB0BE))?3:gameplay() && g_ram[0xF711]?1:(g_ram[0xF600]&127)==16?2:0;
        /* An in-progress native SAT is not a custom-scene publication. Keep
         * the completed scene until its replacement is ready, without
         * flashing the native centered HUD or dropping host-only rings. */
        if(scene && s_display_frame.serial && s_display_frame.scene==scene) {
            s_display=&s_display_frame;++s_scene_holds;
        }
    }
    if(s_interpolation_enabled && s_display && s_display!=&s_display_frame) {
        unsigned slot=s_display->serial%3;s_display_key_serial=0;
        if(s_key_serial[slot]==s_display->serial) {
            memcpy(s_display_keys,s_key_history[slot],s_display->count*sizeof *s_display_keys);
            s_display_key_serial=s_display->serial;
        }
    }
    /* Keep the selected front buffer immutable for all displayed scanlines. */
    if(s_display && s_display!=&s_display_frame) {
        s_display_frame.count=s_display->count;s_display_frame.scene=s_display->scene;
        s_display_frame.serial=s_display->serial;
        s_display_frame.camera_x=s_display->camera_x;s_display_frame.camera_y=s_display->camera_y;
        memcpy(s_display_frame.views,s_display->views,sizeof s_display_frame.views);
        memcpy(s_display_frame.sat,s_display->sat,sizeof s_display_frame.sat);
        memcpy(s_display_frame.sprites,s_display->sprites,s_display->count*sizeof(SceneSprite));
        s_display=&s_display_frame;
    }
}
static void draw_scene_sprites(const GVDP *v,int line,uint32_t *out,int width,
                               int origin,const uint32_t *palette)
{
    if(!s_display)return;
    for(unsigned n=0;n<s_display->count;++n) {
        const SceneSprite *s=&s_display->sprites[n];
        int cw=((s->size>>2)&3)+1,ch=(s->size&3)+1;
        unsigned anchor=s->hud&3u;
        int world=!anchor && (s_display->scene==1 || s_display->scene==3);
        int cell_height=(v->reg[12]&6)==6?16:8;
        int sx=s->x,sy=s->y;
        if(world && s_interpolating)interpolation_sprite(n,&sx,&sy);
        int y=sy+(world?s_display->camera_y-s_camera_y:0);
        if(line<y || line>=y+ch*cell_height)continue;
        int base=sx+(anchor==1?0:anchor==2?(width-320)/2:origin);
        /* Native camera scroll may advance while a completed host frame is
         * retained. Reproject its world coordinates, not its old screen
         * coordinates; otherwise stationary rings jitter with the camera. */
        if(world)base+=s_display->camera_x-s_camera;
        int iy=line-y;
        if(s->attr&0x1000)iy=ch*cell_height-1-iy;
        for(int i=0;i<cw*8;++i) {
            int x=base+i;
            if(x<0 || x>=width || (s_priority[x]&2))continue;
            int ix=(s->attr&0x800)?cw*8-1-i:i;
            uint16_t cell=(uint16_t)((s->attr&0xE000u)|
                (((s->attr&2047u)+(unsigned)(ix/8*ch+iy/cell_height))&2047u));
            uint32_t custom=0;
            int knux=!s_interpolating && anchor==1 && s2_video_knuckles_hud_pixel(v,cell,ix,iy,&custom);
            uint8_t p=knux?(custom!=0):pattern_pixel(v,cell,ix,iy);
            if (s->size&0x80) {
                unsigned bytes=0;const uint8_t *patterns=gvdp_sprite_patterns(s->size,&bytes);
                unsigned at=(cell&2047)*((v->reg[12]&6)==6?64:32)+(iy&(((v->reg[12]&6)==6)?15:7))*4+(ix&7)/2;
                if (patterns && at<bytes) p=(ix&1)?patterns[at]&15:patterns[at]>>4;
                p=(uint8_t)((cell>>9&48)|p); /* native palette, including water/fades */
                if (!(p&15)) p=0;
            }
            if(!p)continue;
            int extra=gvdp_sprite_palette_line(s->size,cell&2047,(cell>>13)&3);
            if (extra) {
                custom=genesis_dac_cram_to_argb(gvdp_ext_color(line,extra,p&15),GENESIS_DAC_NORMAL);
                knux=1;
            }
            s_priority[x]|=2;
            if(anchor || (s->attr&0x8000) || !(s_priority[x]&1)) {
                out[x]=knux?custom:palette[p];
                if((s->hud&128) && s_front_rgb){s_priority[x]|=4;s_front_rgb[x]=out[x];}
            }
        }
    }
}
static int attributes_differ(const GVDP *v,uint16_t a,uint16_t b,int x,int y)
{
    if(a==b)return 0;
    /* Compare visible pixels/priority, not irrelevant transparent-tile flags. */
    for(int i=0;i<8;++i) {
        uint8_t ap=pattern_pixel(v,a,x+i,y),bp=pattern_pixel(v,b,x+i,y);
        if(ap!=bp || (ap && ((a^b)&0x8000)))return 1;
    }
    return 0;
}
/* Emerald Hill's camera-dependent component of SwScrl_EHZ ($C562).
 * Retain live water ripple phase; only reproject camera motion at stage edges. */
static int ehz_parallax(int camera,int line)
{
    int n=-camera;
    if(line<22 || (line>=101 && line<112))return 0;
    if(line<101)return -(n>>6);
    if(line<128)return -(n>>4);
    if(line<144){int a=n>>4;return -(a+(a>>1));}
    int row=line-144;
    if(row>=15 && row<33)row=15+(row-15)/2*2;
    else if(row>=33)row=33+(row-33)/3*3;
    int step=(((n>>1)-(n>>3))*256)/48;
    return -((n>>3)+((step*row)>>8));
}
/* SwScrl_EHZ_2P emits one scroll word for each pair of IM2 output rows.
 * Its bands differ from 1P, and the lower gradient advances every two rows. */
static int ehz_2p_parallax(int camera,int line)
{
    int n=-camera;
    if(line<22 || (line>=102 && line<112))return 0;
    if(line<102)return -(n>>6);
    if(line<128)return -(n>>4);
    if(line<144){int a=n>>4;return -(a+(a>>1));}
    int step=(((n>>1)-(n>>3))*256)/48;
    return -((n>>3)+((step*((line-144)/2))>>8));
}
/* SwScrl_CPZ uses BG1 at one-eighth speed through row18 (including its
 * animated ripple), then BG2 at one-half speed. Keep the live ripple. */
static int cpz_parallax(int camera,int background_y)
{ return camera>>((background_y&1023)<304?3:1); }

/* Native IM2 remains two independent224-row world views. Reuse the world
 * maps and completed sprite publication; the native SAT still owns each HUD.
 * This path does not change the source camera or either player's controls. */
static void competition_hud_sprites(const GVDP *v,int line,uint32_t *out,int width,const uint32_t *palette)
{
    unsigned table=(v->reg[5]&127u)<<9,slot=0,p=line>=224;
    int local=line-(p?224:0);
    for (unsigned n=0;n<80;++n) {
        unsigned at=table+slot*8,attr=vram16(v,at+4),tile=attr&2047u;
        int x=(vram16(v,at+6)&1023)-128,y=(vram16(v,at)&1023)-256-(p?224:0);
        unsigned size=v->vram[(at+2)&65535u],cw=((size>>2)&3)+1,ch=(size&3)+1;
        int life=y==200 && (x==16 || x==32);
        int hud=(attr&0x8000) && (life || (y>=0 && y<64 && tile>=0x365 && tile<0x390));
        int replace=strcmp(s2_party.roster.character[p],p?"tails":"sonic")!=0;
        if (hud && !(life && replace) && local>=y && local<y+(int)ch*16) {
            int iy=local-y;if(attr&0x1000)iy=(int)ch*16-1-iy;
            for (unsigned i=0;i<cw*8;++i) {
                int dx=x+(int)i,ix=attr&0x800?(int)cw*8-1-(int)i:(int)i;
                if (dx<0 || dx>=width) continue;
                unsigned cell=(attr&0xE000u)|((tile+(unsigned)(ix/8)*ch+(unsigned)iy/16)&2047u);
                unsigned color=pattern_pixel(v,(uint16_t)cell,ix,iy);
                if(color)out[dx]=palette[color];
            }
        }
        slot=v->vram[(at+3)&65535u]&127;if(!slot)break;
    }
}
static int competition_scanline(const GVDP *v,int line,const uint32_t *native,int nw,
                                uint32_t *out,int width,GameVideoNativeRow native_row,void *native_ctx,
                                const uint32_t *palette)
{
    if (!s_frame_vs || (v->reg[12]&6)!=6 || nw!=320 || s_priority_capacity<width) return 0;
    unsigned p=line>=224;int y=line-(p?224:0),ready=s_vs_ready;
    int world=s_vs_world;
    if (!(v->reg[1]&64)) {for(int x=0;x<width;++x)out[x]=palette[v->reg[7]&63];return 1;}
    int camera=s_vs_camera[p][0],cy=s_vs_camera[p][1],left=s_vs_camera[p][3],origin=camera-left;
    int margin=(width-nw)/2;
    unsigned scroll=(v->reg[13]&63u)<<10,mode=v->reg[11]&3;
    int row=mode==0?0:mode==2?(line>>1)&~7:line>>1;
    int hs_a=(int16_t)vram16(v,scroll+row*4),hs_b=(int16_t)vram16(v,scroll+row*4+2);
    unsigned base_a=(v->reg[2]&56u)<<10,base_b=(v->reg[4]&7u)<<13;
    int by=unwrap(v->vsram[1]+line,s_vs_camera[p][2]+y),bg_width=0;
    int plane_y=unwrap(v->vsram[0]+line,cy+y);
    if (world) for (int col=0;col<128;++col)
        if(s_world_frame[0x8080+(((by>>7)&15)*256)+col])bg_width=(col+1)*128;
    int bg_base=-origin-hs_b;
    if(world && s_frame_zone==0)
        bg_base=-margin-hs_b+ehz_2p_parallax(left+margin,y)-ehz_2p_parallax(camera,y);
    for(int x=0;x<width;++x) {
        int wx=left+x,wy=plane_y,bx=x+bg_base;
        if(bg_width){bx%=bg_width;if(bx<0)bx+=bg_width;}
        uint16_t a,b;unsigned ap,bp;
        if(world) {
            a=world_attr_2p(s_world_frame,wx,wy,0);ap=pattern_pixel(&s_video_frame,a,wx,wy);
            b=world_attr_2p(s_world_frame,bx,by,1);bp=pattern_pixel(&s_video_frame,b,bx,by);
        } else {
            int ax=x-margin-hs_a,ay=line+v->vsram[0],py=line+v->vsram[1];
            a=plane_attr(v,base_a,ax,ay);ap=pattern_pixel(v,a,ax,ay);
            b=plane_attr(v,base_b,x-margin-hs_b,py);bp=pattern_pixel(v,b,x-margin-hs_b,py);
        }
        if(!ready) {
            int ax=(int)((int64_t)x*nw/width)-hs_a,ay=line+v->vsram[0];
            uint16_t card=plane_attr(v,base_a,ax,ay);
            if(card_fill((card&0xF800u)|((card&2047u)*2u))) {a=card;ap=pattern_pixel(v,a,ax,ay);}
        }
        unsigned index=ap && ((a&0x8000) || !bp || !(b&0x8000))?ap:bp?bp:v->reg[7]&63;
        out[x]=palette[index];s_priority[x]=(uint8_t)((ap && (a&0x8000)) || (bp && (b&0x8000)));
    }
    s_camera=camera;s_camera_y=cy;s_left=left;s_native_x=origin;
    if(s_display)draw_scene_sprites(&s_video_frame,y,out,width,ready?origin:margin,palette);
    if(!ready) {
        if(!native)native=native_row(native_ctx);
        memcpy(out+margin,native,(size_t)nw*sizeof *out);
    } else competition_hud_sprites(v,line,out,width,palette);
    return 1;
}
static void render_source_scanline(const GVDP *v, int line, const uint32_t *native, int nw,
                            uint32_t *out, int width,
                            GameVideoNativeRow native_row, void *native_ctx)
{
    item_palette_row(NULL,v,NULL);
    /* Publish one immutable art/scroll snapshot per frame. Palettes stay
     * live so water-line HBlank colour changes are retained. */
    uint32_t palette[64],shadow[64];
    int uniform_palette=1;
    for(int i=0;i<64;++i) {
        palette[i]=genesis_dac_cram_to_argb(v->cram[i],GENESIS_DAC_NORMAL);
        shadow[i]=genesis_dac_cram_to_argb(v->cram[i],GENESIS_DAC_SHADOW);
        if(palette[i]!=palette[0])uniform_palette=0;
    }
    if(line==0 && !s_interpolating) {
        memcpy(&s_video_frame,v,sizeof s_video_frame);
        memcpy(s_world_frame,g_ram,sizeof s_world_frame);
        int outgoing=outgoing_level();
        s_frame_vs=competition() || (outgoing && ram16(0xFFD8));
        s_vs_ready=g_ram[0xF711]!=0;s_vs_world=s_vs_ready || ram16(0xB0BE)!=0;
        if (s_frame_vs) for (unsigned p=0;p<2;++p) {
            unsigned at=p?0xEE80:0xEE60;
            s_vs_camera[p][0]=ram16(at);s_vs_camera[p][1]=(int16_t)ram16(at+4);
            s_vs_camera[p][2]=(int16_t)ram16(at+12);
            s_vs_camera[p][3]=view_left(s_vs_camera[p][0],width);
        }
        s_frame_level=(gameplay() || (outgoing && !ram16(0xFFD8))) && g_ram[0xF711];
        s_frame_special=(g_ram[0xF600]&127)==16 && !gameplay();
        s_frame_zone=g_ram[0xFE10];
        s_frame_card=0;s_frame_card_leaving=ram16(0xB0BE)!=0;
        s_frame_sega=width>nw && !g_ram[0xF600] && g_ram[0xB040]==0xB0 &&
            v->reg[2]==0x30 && v->reg[4]==5;
        if (width>nw && (g_ram[0xF600]&127)==12 && !ram16(0xFFD8) &&
            (v->vram[0xBB40]==0x33 || v->vram[0xBB40]==0x44) &&
            v->vram[0xBB7F]==v->vram[0xBB40])
            for (unsigned o=0xB080;o<=0xB1C0;o+=64)
                if (g_ram[o]==0x34) s_frame_card=1;
        s_frame_fg_x=ram16(0xEE60);s_frame_fg_y=ram16(0xEE64);s_frame_bg_y=ram16(0xEE6C);
        /* Presentation width only. s_requested_width is SIMULATION (object
         * activation, spawn culling) and comes from width() -- the sealed
         * mode -- never from the width this machine could present (GPU
         * texture clamp, allocation), or two peers desync (2026-09-25:
         * a forced 360-px clamp on one peer failed the boot-digest gate). */
        s_width=width;
        s_terrain_checks=s_terrain_errors=s_bg_checks=s_bg_errors=s_bg_unstreamed=0;
        s_uninitialized_scroll_lines=s_terrain_unstreamed=0;
        s_bad_x=s_bad_y=-1;s_bad_attr=s_bad_expected=0;
        reserve_sprite_row(width);
        select_scene(v);
        s_frame_campaign_view=0;
#if GENESIS_HAS_RECOMP_NET
        if (s2_runtime_campaign_online() && s_display && s_display->scene==1) {
            int p=genesis_netplay_local_player();
            if (p<0 || p>=S2_MAX_PLAYERS) p=0; /* spectators watch the campaign leader */
            s_frame_campaign_view=1;
            s_frame_view_x=s_display->views[p][0];s_frame_view_y=s_display->views[p][1];
        }
#endif
    }
    if(competition_scanline(v,line,native,nw,out,width,native_row,native_ctx,palette))return;
    v=&s_video_frame;
    uint32_t backdrop=palette[v->reg[7]&63];
    /* During a completed fade the game can clear/reload name tables before
     * retiring its old player/display list. A flat palette displays no stage
     * geometry and is not a valid tile-streaming comparison. Keep the full
     * canvas flat; never expose in-progress stage decompression. */
    if(uniform_palette && !(v->reg[12]&8)) {
        for(int x=0;x<width;++x)out[x]=palette[0];
        if(line==0){++s_frames;s_native_x=(width-nw)/2;}
        return;
    }
    /* Display-off/fades fill the same full-width canvas, not black sidebars.
     * A rare unsupported mode also retains the selected output dimensions. */
    if((nw!=320 && nw!=256) || !(v->reg[1]&64) || (v->reg[12]&6) ||
       ((v->reg[12]&8) && !s_frame_special) || s_priority_capacity<width) {
        if (!native) native = native_row(native_ctx);
        uint32_t fill=!(v->reg[1]&64)?native[0]:backdrop;
        for(int x=0;x<width;++x)out[x]=fill;
        int x0=(width-nw)/2;
        if(x0>=0)memcpy(out+x0,native,(size_t)nw*sizeof(uint32_t));
        if(line==0){++s_fallbacks;s_native_x=x0;}
        return;
    }
    unsigned hsbase=(v->reg[13]&63u)<<10;
    int hmode=v->reg[11]&3,hi=hmode==0?0:hmode==2?line&~7:line;
    int hs_a=(int16_t)vram16(v,hsbase+(unsigned)hi*4);
    int hs_b=(int16_t)vram16(v,hsbase+(unsigned)hi*4+2);
    int level=s_frame_level,special=s_frame_special;
    /* The next player object can exist while the title-card SAT is still
     * displayed. Do not expose the incoming level before its own display
     * list/planes reach VBlank. */
    if(s_display && s_display->scene!=1)level=0;
    if(s_frame_card && s_frame_card_leaving)level=1;
    /* Stock SwScrl_EHZ only initializes 222/224 rows. The last two contain
     * stale scroll words, not a valid terrain reference. Extend its last row
     * in the enhanced renderer and report the excluded native rows explicitly. */
    int missing_scroll=level && s_frame_zone==0 && line>=222;
    if(missing_scroll) {
        hs_a=(int16_t)vram16(v,hsbase+221u*4u);
        hs_b=(int16_t)vram16(v,hsbase+221u*4u+2u);
        ++s_uninitialized_scroll_lines;
    }
    int camera=level?unwrap(-hs_a,s_frame_fg_x):ram16(0xEE00);
    int camera_y=s_frame_card && s_frame_card_leaving?s_frame_fg_y:unwrap(v->vsram[0],s_frame_fg_y);
    int native_camera=camera,native_camera_y=camera_y;
    if (level && s_frame_campaign_view) {
        camera=s_frame_view_x;camera_y=s_frame_view_y;
    }
    if(line==0) {
        if(level && !s_interpolating)s_stage_width=stage_width();
        s_camera=camera;s_camera_y=camera_y;++s_frames;
        if(level && s_interpolating) {
            int end=s_stage_width-width;
            s_left=end<0?end/2:clamp(camera-(width-320)/2,0,end);
        } else s_left=level?view_left(camera,width):camera-(width-nw)/2;
    }
    int origin=level?s_camera-s_left:(width-nw)/2;
    s_native_x=origin;
    unsigned base_a=(v->reg[2]&56u)<<10,base_b=(v->reg[4]&7u)<<13;
    int wy=camera_y+line;
    int bg_world=level && !special;
    int by=bg_world?unwrap(v->vsram[1],s_frame_bg_y)+line:line+(v->vsram[1]&1023);
    if (bg_world && s_frame_campaign_view) by+=(camera_y-native_camera_y)/4;
    int bg_width=0;
    if(bg_world) {
        int row=((by>>7)&15)*256;
        for(int col=0;col<128;++col)if(s_world_frame[0x8080+row+col])bg_width=(col+1)*128;
    }
    int bg_base=-origin-hs_b;
    if(level && s_frame_zone==0) {
        int margin=(width-nw)/2;
        bg_base=-margin-hs_b+ehz_parallax(s_left+margin,line)-ehz_parallax(native_camera,line);
    } else if(level && s_frame_zone==13 && !s_frame_campaign_view) {
        int margin=(width-nw)/2;
        bg_base=-margin-hs_b+cpz_parallax(s_left+margin,by)-cpz_parallax(native_camera,by);
    } else if(level && s_frame_campaign_view) bg_base+=(camera-native_camera)/2;
    int background_x=bg_base;
    if(bg_width>0) {
        background_x%=bg_width;
        if(background_x<0)background_x+=bg_width;
    }
    GenesisTileRow fg = {0}, bg = {0};
    for(int x=0;x<width;++x) {
        int nx=x-origin,wx=level?nx+camera:nx-hs_a;
        int reference_bx=nx-hs_b,bx=bg_base+x;
        /* MenuScreen loads only 40 columns of its 64-column plane. Repeat
         * the populated composition instead of exposing the unused cells. */
        if (!level && (g_ram[0xF600]==0x24 || g_ram[0xF600]==0x18) && base_b==0xE000) {
            bx%=320; if (bx<0) bx+=320;
        }
        if(!fg.remaining) {
            uint16_t attr=level?world_attr(s_world_frame,wx,wy,0):plane_attr(v,base_a,nx-hs_a,line+(v->vsram[0]&1023));
            gtile_row_begin(&fg,v->vram,attr,level?wx:nx-hs_a,level?wy:line+(v->vsram[0]&1023));
        }
        if(!bg.remaining) {
            uint16_t attr=bg_world?world_attr(s_world_frame,background_x,by,1):plane_attr(v,base_b,bx,by);
            gtile_row_begin(&bg,v->vram,attr,bg_world?background_x:bx,by);
        }
        uint16_t a=fg.attr,b=bg.attr;
        uint8_t ap=gtile_row_pixel(&fg),bp=gtile_row_pixel(&bg);
        if(level && (wx<0 || wx>=s_stage_width))ap=0;
        /* Menus have a centered foreground composition, with full-width
         * scenery underneath. Special-stage planes are repeating art. */
        if(!level && (nx<0 || nx>=nw))ap=0;
        if (s_frame_sega) {
            /* The streak plane follows Sonic across the whole viewport.
             * Logo/TM artwork remains centered at its original pixel size. */
            int px=(int)((int64_t)x*nw/width)-hs_a,py=line+v->vsram[0];
            a=plane_attr(v,base_a,px,py);ap=pattern_pixel(v,a,px,py);
            if (nx<0 || nx>=nw) {
                int edge=(nx<0?0:nw-1)-hs_b;
                b=plane_attr(v,base_b,edge,by);bp=pattern_pixel(v,b,edge,by);
            }
        }
        if (s_frame_card) {
            /* Stretch only the original geometric fill. Its live name table
             * supplies every entry/exit row; never repeat a 512px card seam. */
            if (!level && (card_fill(a) || nx<0 || nx>=nw)) {
                a=s_frame_card_leaving?world_attr(s_world_frame,nx+ram16(0xEE60),line+ram16(0xEE64),0):0;
                ap=pattern_pixel(v,a,nx+ram16(0xEE60),line+ram16(0xEE64));
            }
            int card_x=(int)((int64_t)x*nw/width)-hs_a,card_y=line+v->vsram[0];
            unsigned fill=plane_attr(v,base_a,card_x,card_y);
            if (card_fill(fill)) { a=(uint16_t)fill;ap=pattern_pixel(v,a,card_x,card_y); }
        }
        s_priority[x]=(uint8_t)(((a&0x8000)&&ap)||((b&0x8000)&&bp));
        uint8_t p=ap&&(a&0x8000)?ap:bp&&(b&0x8000)?bp:ap?ap:bp;
        out[x]=p?palette[p]:backdrop;
        if (s_frame_card && (a&2047)>=0x5DA && (a&2047)<=0x5DB && ap==4 &&
            s2_party.s3k_enabled && !strcmp(s2_party.roster.character[0],"knuckles")) {
            const S2DonorBank *bank=s2_resource_bank(S2_RESOURCE_SK);
            if (bank) {
                uint32_t green=genesis_dac_cram_to_argb(bank->palette[5],GENESIS_DAC_NORMAL);
                unsigned gain=palette[4]&255;
                out[x]=0xFF000000u|((((green>>16)&255)*gain/255)<<16)|
                    ((((green>>8)&255)*gain/255)<<8)|((green&255)*gain/255);
            }
        }
        /* The half-pipe uses H32 plus shadow/highlight. Outside the original
         * track projection only planes are drawn, so plane priority alone
         * determines shadow; native operator sprites remain in the center. */
        if(special && (v->reg[12]&8) && !s_priority[x])
            out[x]=shadow[p?p:v->reg[7]&63];
        if(nx>=0 && nx<nw && !(x&7) && !missing_scroll && !s_frame_campaign_view) {
            if(level && !native_terrain_streamed(wy,s_frame_fg_y)) {
                ++s_terrain_unstreamed;
            } else if(level) {
                uint16_t expected=plane_attr(v,base_a,nx-hs_a,line+(v->vsram[0]&1023));
                ++s_terrain_checks;
                int bad=attributes_differ(v,a,expected,wx,wy);
                if(bad && s_bad_y<0){s_bad_x=wx;s_bad_y=line;s_bad_attr=a;s_bad_expected=expected;s_bad_world_y=wy;s_bad_hscroll=hs_a;}
                s_terrain_errors+=bad;
            }
            if(bg_world) {
                int rx=reference_bx;
                if(bg_width>0){rx%=bg_width;if(rx<0)rx+=bg_width;}
                uint16_t expected=plane_attr(v,base_b,reference_bx,by);
                ++s_bg_checks;
                s_bg_errors+=attributes_differ(v,world_attr(s_world_frame,rx,by,1),expected,reference_bx,by);
            }

        }
        ++background_x;
        if(bg_width>0 && background_x==bg_width)background_x=0;
    }
    if((s_frame_card || s_frame_sega) && s_display) {
        draw_scene_sprites(v,line,out,width,origin,palette);
    } else if(!level) {
        if (!native) native = native_row(native_ctx);
        /* Logos/copyright/menus remain centered at their original pixel
         * scale. The full-width background follows the live scene palette. */
        if(origin>=0)memcpy(out+origin,native,(size_t)nw*sizeof(uint32_t));
    } else if(s_display) {
        draw_scene_sprites(v,line,out,width,origin,palette);
    } else {
        /* No matching DMA yet (e.g. load/fade): preserve the live native
         * scene while the host display list catches its next publication. */
        if (!native) native = native_row(native_ctx);
        int first=origin<0?-origin:0,last=origin+nw>width?width-origin:nw;
        if(last>first)memcpy(out+origin+first,native+first,(size_t)(last-first)*sizeof(uint32_t));
    }
}

static void render_scanline(const GVDP *v, int line, const uint32_t *native, int nw,
                            uint32_t *out, int width,
                            GameVideoNativeRow native_row, void *native_ctx)
{
    /* The H32 half-pipe has 256 horizontal samples across the same display
     * span as H40's 320. Extend in H32 sample space, then expand the complete
     * row so the track, HUD and extra scenery share the proper pixel shape.
     * Native aspect keeps its untouched 256-pixel image for the runner. */
    int source_width=nw==256 && (g_ram[0xF600]&127)==16 && width>=320 ?
        (int)((int64_t)width*4/5):width;
    render_source_scanline(v,line,native,nw,out,source_width,native_row,native_ctx);
    if(source_width!=width) {
        for(int x=width-1;x>=0;--x)out[x]=out[(int)((int64_t)x*source_width/width)];
        s_width=width;s_native_x=(width-320)/2;
    }
}

static void scanline(const GVDP *v, int line, const uint32_t *native, int nw,
                     uint32_t *out, int width)
{
    render_scanline(v,line,native,nw,out,width,NULL,NULL);
}

static void scanline_lazy(const GVDP *v, int line, int nw, uint32_t *out, int width,
                          GameVideoNativeRow native_row, void *native_ctx)
{
    render_scanline(v,line,NULL,nw,out,width,native_row,native_ctx);
}

/* These snapshots are deliberately absent from video_state(). Native aspect
 * captures use their own read-only publication, without enabling the expanded
 * object loader, changing sprite flags or spending additional guest cycles. */
typedef struct {
    S2RenderContext render;
    SceneFrame scene;
    uint32_t keys[SCENE_SPRITES];
    uint16_t cram[224][64],hscroll[224][2],vscroll[224][2];
    uint32_t *image;
    size_t capacity;
    unsigned tick,zone;
    int width,lines,valid;
} PresentationFrame;
static PresentationFrame s_present[2];
static unsigned s_present_front;
static const PresentationFrame *s_lerp_old,*s_lerp_new;

static void interpolation_reset(void)
{
    for(unsigned i=0;i<2;++i)s_present[i].valid=s_present[i].lines=0;
    for(unsigned i=0;i<3;++i) {
        s_native_history[i].serial=0;s_key_serial[i]=0;
    }
    s_native_display.serial=s_native_serial=s_display_key_serial=0;
    s_native_pending=0;
}
static void presentation_configure(int on)
{
    s_interpolation_enabled=on!=0;interpolation_reset();
}
static const SceneFrame *native_select_scene(const GVDP *v)
{
    const SceneFrame *best=NULL;
    unsigned sat=(v->reg[5]&127u)<<9;
    for(unsigned i=0;i<4;++i) {
        const SceneFrame *p=i==3?&s_native_display:&s_native_history[i];
        if(!p->serial || (best && p->serial<best->serial))continue;
        unsigned k=0;
        for(;k<640 && p->sat[k]==v->vram[(sat+k)&65535u];++k){}
        if(k==640)best=p;
    }
    if(best && best!=&s_native_display) {
        unsigned count=best->count;
        memcpy(s_native_display_keys,s_native_keys[best->serial%3],count*sizeof(uint32_t));
        s_native_display.count=count;s_native_display.serial=best->serial;
        s_native_display.scene=best->scene;s_native_display.camera_x=best->camera_x;
        s_native_display.camera_y=best->camera_y;
        memcpy(s_native_display.views,best->views,sizeof best->views);
        memcpy(s_native_display.sat,best->sat,sizeof best->sat);
        memcpy(s_native_display.sprites,best->sprites,count*sizeof(SceneSprite));
        best=&s_native_display;
    }
    return best;
}
static void presentation_capture(const GVDP *v,int line,const uint32_t *image,
                                 int width,int native_w,const struct GVDPRasterRow *raster)
{
    (void)raster;
    if(!s_interpolation_enabled || line<0 || line>=224)return;
    if(line==0) {
        s_present_front^=1;
        PresentationFrame *f=&s_present[s_present_front];
        f->valid=0;f->lines=0;f->width=width;
        /* Donor actors/providers, native interlace and transition screens
         * retain ordinary completed pictures until an adapter is qualified. */
        if(width<native_w || native_w!=320 || g_ram[0xF600]!=12 ||
           !g_ram[0xF711] || ram16(0xF63A) || competition() ||
           (v->reg[12]&14) || !(v->reg[1]&64) ||
           s2_runtime_campaign_online() || s2_party.roster.slots>2 ||
           strcmp(s2_party.roster.character[0],"sonic") ||
           (s2_party.roster.slots>1 && strcmp(s2_party.roster.character[1],"tails")))return;
        const SceneFrame *scene=enabled()?s_display:native_select_scene(v);
        const uint32_t *keys=enabled()?s_display_keys:s_native_display_keys;
        if(!scene || scene->scene!=1 || (enabled() && s_display_key_serial!=scene->serial))return;
        for(unsigned i=0;i<scene->count;++i)if(scene->sprites[i].size&0x80)return;
        size_t need=(size_t)width*224;
        if(need>f->capacity) {
            uint32_t *next=(uint32_t*)realloc(f->image,need*sizeof *next);
            if(!next)return;
            f->image=next;f->capacity=need;
        }
        if(!reserve_sprite_row(width))return;
        f->render=s_real_render;
        if(!enabled()) {
            memset(&f->render,0,sizeof f->render);
            memcpy(&f->render.video_frame,v,sizeof *v);
            memcpy(f->render.world_frame,g_ram,sizeof f->render.world_frame);
            f->render.frame_level=1;f->render.frame_fg_x=ram16(0xEE60);
            f->render.frame_fg_y=ram16(0xEE64);f->render.frame_bg_y=ram16(0xEE6C);
            f->render.frame_zone=g_ram[0xFE10];f->render.stage_width=stage_width();
            f->render.width=width;
        }
        f->scene.count=scene->count;f->scene.scene=scene->scene;
        f->scene.camera_x=scene->camera_x;f->scene.camera_y=scene->camera_y;
        memcpy(f->scene.sprites,scene->sprites,scene->count*sizeof(SceneSprite));
        memcpy(f->keys,keys,scene->count*sizeof(uint32_t));
        f->tick=ram16(0xFE04);f->zone=ram16(0xFE10);f->valid=1;
    }
    PresentationFrame *f=&s_present[s_present_front];
    if(!f->valid || line!=f->lines || width!=f->width || !image) {f->valid=0;return;}
    memcpy(f->image+(size_t)line*width,image,(size_t)width*sizeof *image);
    memcpy(f->cram[line],v->cram,sizeof f->cram[line]);
    int mode=v->reg[11]&3,row=mode==0?0:mode==2?line&~7:line;
    unsigned base=(v->reg[13]&63u)<<10;
    for(unsigned p=0;p<2;++p) {
        f->hscroll[line][p]=vram16(v,base+(unsigned)row*4+p*2);
        f->vscroll[line][p]=v->vsram[p];
    }
    ++f->lines;
}
static int lerp_position(int a,int b)
{ return a+(int)((b-a)*s_interpolation_phase+((b>=a)?0.5:-0.5)); }
static void interpolation_sprite(unsigned index,int *x,int *y)
{
    const SceneFrame *a=&s_lerp_old->scene,*b=&s_lerp_new->scene;
    *x=lerp_position(a->sprites[index].x+a->camera_x,b->sprites[index].x+b->camera_x)-b->camera_x;
    *y=lerp_position(a->sprites[index].y+a->camera_y,b->sprites[index].y+b->camera_y)-b->camera_y;
}
static int interpolate(double phase,uint32_t *image,int stride,int height)
{
    const PresentationFrame *b=&s_present[s_present_front],*a=&s_present[s_present_front^1];
    if(!s_interpolation_enabled || !image || height!=224 || !(phase>=0 && phase<=1) ||
       !a->valid || !b->valid || a->lines!=224 || b->lines!=224 ||
       a->width!=b->width || stride<b->width || a->zone!=b->zone ||
       (uint16_t)(b->tick-a->tick)>2 || a->scene.count!=b->scene.count ||
       abs(a->render.frame_fg_x-b->render.frame_fg_x)>128 ||
       abs(a->render.frame_fg_y-b->render.frame_fg_y)>128)return 0;
    for(unsigned i=0;i<b->scene.count;++i) {
        const SceneSprite *p=&a->scene.sprites[i],*q=&b->scene.sprites[i];
        if(a->keys[i]!=b->keys[i] || p->attr!=q->attr || p->size!=q->size || p->hud!=q->hud ||
                       (!(q->hud&3) && (abs(p->x+a->scene.camera_x-q->x-b->scene.camera_x)>48 ||
                       abs(p->y+a->scene.camera_y-q->y-b->scene.camera_y)>48)))return 0;
    }
    if(phase==0 || phase==1) {
        const PresentationFrame *f=phase==0?a:b;
        for(int y=0;y<224;++y)memcpy(image+(size_t)y*stride,f->image+(size_t)y*f->width,(size_t)f->width*sizeof *image);
        return 1;
    }
    static S2RenderContext fractional;
    fractional=b->render;
    s_interpolation_phase=phase;s_lerp_old=a;s_lerp_new=b;
    fractional.frame_fg_x=lerp_position(a->render.frame_fg_x,b->render.frame_fg_x);
    fractional.frame_fg_y=lerp_position(a->render.frame_fg_y,b->render.frame_fg_y);
    fractional.frame_bg_y=lerp_position(a->render.frame_bg_y,b->render.frame_bg_y);
    S2RenderContext *saved_render=s_render;
    const SceneFrame *saved_display=s_display;
    s_render=&fractional;s_display=&b->scene;s_interpolating=1;
    unsigned base=(fractional.video_frame.reg[13]&63u)<<10;
    int mode=fractional.video_frame.reg[11]&3;
    for(int y=0;y<224;++y) {
        memcpy(fractional.video_frame.cram,b->cram[y],sizeof b->cram[y]);
        int row=mode==0?0:mode==2?y&~7:y;
        for(unsigned p=0;p<2;++p) {
            int old=(int16_t)a->hscroll[y][p],delta=(int16_t)(b->hscroll[y][p]-a->hscroll[y][p]);
            unsigned hs=(uint16_t)lerp_position(old,old+delta),at=(base+row*4+p*2)&65535;
            fractional.video_frame.vram[at]=(uint8_t)(hs>>8);
            fractional.video_frame.vram[(at+1)&65535]=(uint8_t)hs;
            old=a->vscroll[y][p];delta=((b->vscroll[y][p]-old+512)&1023)-512;
            fractional.video_frame.vsram[p]=(uint16_t)lerp_position(old,old+delta);
        }
        render_scanline(&fractional.video_frame,y,b->image+(size_t)y*b->width,320,
                        image+(size_t)y*stride,b->width,NULL,NULL);
    }
    s_interpolating=0;s_display=saved_display;s_render=saved_render;
    return 1;
}

void s2_video_command(int id, const char *json)
{
    (void)json;
    char reply[1536];
    snprintf(reply, sizeof reply,
        "{\"id\":%d,\"enabled\":%d,\"width\":%d,\"view_left\":%d,\"camera\":%d,"
        "\"native_x\":%d,\"stage_width\":%d,\"frames\":%u,\"fallback_frames\":%u,"
        "\"terrain_checks\":%u,\"terrain_errors\":%u,\"background_checks\":%u,\"background_errors\":%u,\"background_unstreamed\":%u,"
        "\"sprites\":%u,\"scene\":%d,\"spawned\":%u,\"pool_pressure\":%u,\"native_uninitialized_scroll_lines\":%u,"
        "\"native_unstreamed_terrain_samples\":%u,\"first_bad_x\":%d,\"first_bad_y\":%d,\"first_bad_attr\":%u,\"first_bad_expected\":%u,\"first_bad_world_y\":%d,\"first_bad_hscroll\":%d,"
        "\"scene_match_misses\":%u,\"scene_held_frames\":%u,\"main_cpu_divisor\":%u,"
        "\"tick_samples\":%u,\"tick_updates\":%u,\"tick_lag\":%u,\"tick_multi\":%u,\"publication_lag\":%u}",
        id, enabled(), s_width, s_left, s_camera, s_native_x, s_stage_width,
        s_frames, s_fallbacks, s_terrain_checks, s_terrain_errors,s_bg_checks,s_bg_errors,s_bg_unstreamed,
        s_display?s_display->count:0,s_display?s_display->scene:-1,s_spawned,s_pool_pressure,s_uninitialized_scroll_lines,
        s_terrain_unstreamed,s_bad_x,s_bad_y,s_bad_attr,s_bad_expected,s_bad_world_y,s_bad_hscroll,
        s_scene_misses,s_scene_holds,s2_video_main_cpu_divisor(),
        s_tick_samples,s_tick_updates,s_tick_lag,s_tick_multi,s_publication_lag);
    cmd_send_response(reply);
}
int s2_video_competition_extended(void)
{ return enabled() && s_frame_vs && s_width>320 && s_priority_capacity>=s_width; }
int s2_video_competition_divider(const GVDP *v,int line,uint32_t *out,int width)
{
    int mode=g_ram[0xF600]&127;
    /* P1's final HUD row is215; P2's view starts at224 (HUD at232).
     * The larger P1 SAT mask is retired by H-int, not a global 32-row bar.
     * Keep both planes and host/native actors out of the actual gap. */
    if (!ram16(0xFFD8) || (mode!=8 && mode!=12) || (v->reg[12]&6)!=6 ||
        line<216 || line>=224) return 0;
    for(int x=0;x<width;++x) out[x]=0xFF000000u;
    return 1;
}
int s2_video_intro_world_visible(void) { return s_frame_card && s_frame_card_leaving; }
void s2_video_actor_origin(const GVDP *v, int line, int width, int *left, int *top)
{
    if (s2_video_competition_extended()) {
        unsigned p=line>=224;*left=s_vs_camera[p][3];*top=s_vs_camera[p][1]-(p?224:0);return;
    }
    if (s2_video_intro_world_visible()) { *left=s_left;*top=s_camera_y;return; }
    if (s_frame_campaign_view && s2_runtime_campaign_online()) {
        *left=s_left;*top=s_camera_y;return;
    }
    int mode=v->reg[11]&3, row=mode==0?0:mode==2?line&~7:line;
    if (!g_ram[0xFE10] && row>=222) row=221;
    unsigned base=(v->reg[13]&63u)<<10;
    int camera=unwrap(-(int16_t)vram16(v,base+row*4),ram16(0xEE60));
    *left=enabled()?view_left(camera,width):camera-(width-320)/2;
    *top=unwrap(v->vsram[0],ram16(0xEE64));
}
int s2_video_actor_pixel_visible(const GVDP *v, int wx, int wy, int high, const unsigned char *world)
{
    if (high) return 1;
    if(enabled() && s_frame_level && wx>=s_left && wx<s_left+s_width &&
       s_priority_capacity>=s_width)return !(s_priority[wx-s_left]&1);
    uint16_t attr=(v->reg[12]&6)==6?world_attr_2p(world,wx,wy,0):world_attr(world,wx,wy,0);
    if((attr&0x8000) && pattern_pixel(v,attr,wx,wy))return 0;
    if((v->reg[12]&6)==6)return 1;
    int mode=v->reg[11]&3,cy=(int16_t)((world[0xEE64]<<8)|world[0xEE65]);
    int line=wy-unwrap(v->vsram[0],cy),row=mode==0?0:mode==2?line&~7:line;
    unsigned scroll=(v->reg[13]&63u)<<10;
    int camera=unwrap(-(int16_t)vram16(v,scroll+row*4),(world[0xEE60]<<8)|world[0xEE61]);
    int bx=wx-camera-(int16_t)vram16(v,scroll+row*4+2),by=line+v->vsram[1];
    attr=plane_attr(v,(v->reg[4]&7u)<<13,bx,by);
    return !(attr&0x8000) || !pattern_pixel(v,attr,bx,by);
}

void s2_video_front_ui(const GVDP *v,int line,uint32_t *out,int width)
{
    if(enabled() && s_priority_capacity>=width && s_front_rgb && (s_frame_level || s_frame_vs)) {
        for(int x=0;x<width;++x)if(s_priority[x]&4)out[x]=s_front_rgb[x];
        return;
    }
    int vs=ram16(0xFFD8)!=0;unsigned p=vs && line>=224;
    FrontUIFrame *front=&s_front_display[p];
    unsigned table=(v->reg[5]&127u)<<9;
    if(line==0 || (vs && line==224)) {
        const FrontUIFrame *best=NULL;
        for(unsigned n=0;n<4;++n) {
            const FrontUIFrame *f=n==3?front:&s_front_history[n];
            if(!f->serial || f->serial>s_front_serial || (best && f->serial<=best->serial))continue;
            unsigned slot=0;int match=1;
            for(unsigned i=0;i<80;++i) {
                if(slot>=80){match=0;break;}
                for(unsigned j=0;j<8;++j)if(f->sat[p][slot*8+j]!=v->vram[(table+slot*8+j)&65535]){match=0;break;}
                if(!match)break;
                slot=v->vram[(table+slot*8+3)&65535]&127;if(!slot)break;
            }
            if(match)best=f;
        }
        if(best && best!=front)*front=*best;
        else if(!best)front->serial=0;
    }
    if(!front->serial || !(front->ui_mask&(1u<<p)))return;
    int donor_hud=!vs && !strcmp(s2_party.roster.character[0],"knuckles") &&
        s2_resource_knuckles_art()!=NULL;
    int competition_hud=vs && s2_runtime_ui_font()!=NULL &&
        strcmp(s2_party.roster.character[p],p?"tails":"sonic");
    int cell=(v->reg[12]&6)==6?16:8,slot=0;uint8_t covered[320]={0};
    for(unsigned i=0;i<80 && slot<80;++i) {
        unsigned at=table+(unsigned)slot*8,kind=front->kind[p][slot];
        unsigned attr=vram16(v,at+4),size=v->vram[(at+2)&65535],cw=((size>>2)&3)+1,ch=(size&3)+1;
        int x=(vram16(v,at+6)&511)-128,y=(vram16(v,at)&1023)-(cell==16?256:128);
        /* Character HUDs own both their ink and transparent silhouette;
         * do not put the old role's icon/name back under the replacement. */
        int replaced=kind==1 && ((donor_hud &&
            ((attr&2047)==0x7D4 || (attr&2047)==0x7D8)) ||
            (competition_hud && (x==16 || x==32) && y==(p?424:200)));
        {
            if(!replaced && line>=y && line<y+(int)ch*cell) {
                int iy=line-y;if(attr&0x1000)iy=(int)ch*cell-1-iy;
                for(unsigned px=0;px<cw*8;++px) {
                    int dx=x+(int)px,ix=(attr&0x800)?(int)cw*8-1-(int)px:(int)px;
                    if(dx<0 || dx>=320 || covered[dx])continue;
                    unsigned tile=(attr&0xE000u)|((attr+(unsigned)(ix/8)*ch+(unsigned)iy/cell)&2047u);
                    unsigned ink=pattern_pixel(v,(uint16_t)tile,ix,iy);
                    if(size&0x80) {
                        unsigned bytes=0;const uint8_t *patterns=gvdp_sprite_patterns(size,&bytes);
                        unsigned address=(tile&2047u)*(unsigned)(cell*4)+(unsigned)(iy&(cell-1))*4+(unsigned)(ix&7)/2;
                        if(patterns && address<bytes) {
                            unsigned n=(ix&1)?patterns[address]&15:patterns[address]>>4;
                            ink=n?((tile>>9)&48)|n:0;
                        }
                    }
                    if(ink) {
                        covered[dx]=1;
                        int target=dx+(width-320)/2;
                        if(kind && !result_name_slot((unsigned)slot) && target>=0 && target<width) {
                            int extra=gvdp_sprite_palette_line(size,tile&2047,(tile>>13)&3);
                            uint16_t shade=extra?gvdp_ext_color(line,extra,ink&15):v->cram[ink];
                            out[target]=genesis_dac_cram_to_argb(shade,GENESIS_DAC_NORMAL);
                        }
                    }
                }
            }
        }
        slot=v->vram[(at+3)&65535]&127;if(!slot)break;
    }
}
static unsigned results_ink(const GVDP *v,unsigned tile,int x,int y,int letter_k)
{
    unsigned local=(unsigned)(x/8*2+y/8),offset=(unsigned)(y&7)*4+(unsigned)(x&7)/2;
    const S2KnucklesArt *art=letter_k?s2_resource_knuckles_art():NULL;
    unsigned byte=art?art->font_k[local*32+offset]:v->vram[((tile+local)*32+offset)&65535u];
    return x&1?byte&15:byte>>4;
}
void s2_video_results_name(const GVDP *v,int line,uint32_t *out,int width,int draw)
{
    unsigned table=(v->reg[5]&127u)<<9;
    if(!line && !draw) {
        memset(&s_result_name,0,sizeof s_result_name);
        const char *name=custom_result_name();
        if((g_ram[0xF600]&127)!=12 || ram16(0xFFD8) || !name)return;
        /* Match the displayed SONIC GOT row, including all three GOT glyphs.
         * The SAT is authoritative while the result objects slide or retire. */
        unsigned slot=0;
        for(unsigned n=0;n<80 && slot<80;++n) {
            unsigned at=table+slot*8;
            if(vram16(v,at+4)==0x85B8) {
                s_result_name.x=(int)(vram16(v,at+6)&511)-144;
                s_result_name.y=(int)(vram16(v,at)&511)-128;
                break;
            }
            slot=v->vram[(at+3)&65535]&127;if(!slot)break;
        }
        if(slot>=80 || vram16(v,table+slot*8+4)!=0x85B8)return;
        unsigned got=0;slot=0;
        for(unsigned n=0;n<80 && slot<80;++n) {
            unsigned at=table+slot*8,attr=vram16(v,at+4);
            int x=(int)(vram16(v,at+6)&511)-128,y=(int)(vram16(v,at)&511)-128;
            if(y==s_result_name.y && (attr&0xE000)==0x8000 &&
                x>=s_result_name.x-64 && x<=s_result_name.x+47 && s_result_name.count<8) {
                s_result_name.slots[s_result_name.count++]=slot;
                got|=(attr==0x85B8 && x==s_result_name.x+16)?1:
                    (attr==0x8588 && x==s_result_name.x+32)?2:
                    (attr==0x85D4 && x==s_result_name.x+47)?4:0;
            }
            slot=v->vram[(at+3)&65535]&127;if(!slot)break;
        }
        if(got!=7){s_result_name.count=0;return;}
        s_result_name.name=name;
    }
    if(!s_result_name.name || !s_result_name.count)return;
    int y=line-s_result_name.y,origin=(width-320)/2;
    if(y<0 || y>=16)return;
    if(!draw) {
        if(enabled())return; /* enhanced scene omitted the native name row */
        GVDP blank=*v;uint8_t indices[GVDP_MAX_WIDTH];
        for(unsigned n=0;n<s_result_name.count;++n) {
            unsigned at=table+s_result_name.slots[n]*8;
            blank.vram[at&65535]=blank.vram[(at+1)&65535]=0;
        }
        int count=gvdp_render_scanline(&blank,line,indices);
        for(int x=s_result_name.x-64;x<s_result_name.x+63;++x)
            if(x>=0 && x<count && x+origin>=0 && x+origin<width) {
                unsigned c=indices[x];if(c<192)out[x+origin]=genesis_dac_cram_to_argb(v->cram[c%64],
                    c<64?GENESIS_DAC_NORMAL:c<128?GENESIS_DAC_SHADOW:GENESIS_DAC_HIGHLIGHT);
            }
        return;
    }
    const char *name=s_result_name.name;unsigned length=(unsigned)strlen(name);
    int x0=origin+s_result_name.x+8-(int)length*16;
    const uint8_t *font=s2_runtime_ui_font();
    for(unsigned c=0;c<length;++c)for(int x=0;x<16;++x) {
        int dx=x0+(int)c*16+x;if(dx<0 || dx>=width)continue;
        unsigned ink=0;
        if(!strcmp(name,"KNUCKLES") && s2_resource_knuckles_art()) {
            static const unsigned tiles[]={0x5C6,0x584,0x5D8,0x5B4,0x5C6,0x5C2,0x580,0x5D0};
            ink=results_ink(v,tiles[c],x,y,name[c]=='K');
        } else if(font) {
            unsigned tile=14+(unsigned)(name[c]-'A');
            unsigned byte=font[tile*32+(unsigned)(y/2)*4+(unsigned)(x/2)/2];
            ink=(x/2)&1?byte&15:byte>>4;
            if(ink) {out[dx]=genesis_dac_cram_to_argb(ink==1?0:v->cram[6],GENESIS_DAC_NORMAL);continue;}
        }
        if(ink)out[dx]=genesis_dac_cram_to_argb(v->cram[ink],GENESIS_DAC_NORMAL);
    }
    static const unsigned tiles[]={0x5B8,0x588,0x5D4};
    static const int offsets[]={16,32,47};
    for(unsigned c=0;c<3;++c)for(int x=0;x<16;++x) {
        int dx=origin+s_result_name.x+offsets[c]+x;if(dx<0 || dx>=width)continue;
        unsigned ink=results_ink(v,tiles[c],x,y,0);
        if(ink)out[dx]=genesis_dac_cram_to_argb(v->cram[ink],GENESIS_DAC_NORMAL);
    }
}
int s2_video_intro_pixel_visible(const GVDP *v,int x,int line,int high)
{
    if (high) return 1;
    if (s_frame_card && s_width>320) x=(int)((int64_t)(x+(s_width-320)/2)*320/s_width);
    /* Before Level_started the foreground is still the sliding title-card
     * plane. Its displayed high-priority pixels hide actors, while the newly
     * revealed stage must already show the native DisplaySprite submission. */
    int mode=v->reg[11]&3,row=mode==0?0:mode==2?line&~7:line;
    unsigned scroll=(v->reg[13]&63u)<<10;
    for (unsigned plane=0;plane<2;++plane) {
        int px=x-(int16_t)vram16(v,scroll+row*4+plane*2);
        int py=line+v->vsram[plane];
        unsigned base=plane?(v->reg[4]&7u)<<13:(v->reg[2]&56u)<<10;
        uint16_t attr=plane_attr(v,base,px,py);
        if ((attr&0x8000) && pattern_pixel(v,attr,px,py)) return 0;
    }
    return 1;
}
int s2_video_hud_origin(int width)
{
    if (!enabled()) return (width-320)/2;
    return s_display && s_display->scene==1?0:-width;
}
int s2_video_knuckles_hud_pixel(const GVDP *v,unsigned attr,int x,int y,uint32_t *color)
{
    const S2KnucklesArt *a=s2_resource_knuckles_art(); unsigned tile=attr&2047;
    if (!a || !s2_party.s3k_enabled || strcmp(s2_party.roster.character[0],"knuckles") ||
        tile<0x7D4 || tile>=0x7E0) return 0;
    x&=7; y&=7; if (attr&0x800) x^=7; if (attr&0x1000) y^=7;
    /* Column-major 2-row pieces: the live digits are tiles 9 and 11.
     * Tile 10 is the final name column, between the two digit tiles in VRAM.
     * Keep KiS2's complete K.T.E. name instead of borrowing Sonic's last glyph. */
    unsigned offset=tile-0x7D4;
    const uint8_t *pixels=offset==9 || offset==11?v->vram+tile*32:a->life+offset*32;
    unsigned byte=pixels[y*4+x/2],ink=x&1?byte&15:byte>>4;
    *color=0; if (!ink) return 1;
    unsigned zone=g_ram[0xFE10];
    const uint16_t *palette=s2_video_water_palette(v) && (zone==13 || zone==15)?
        a->water_palette[zone==15]:a->palette;
    *color=genesis_dac_cram_to_argb(s2_video_donor_color(palette[ink]),GENESIS_DAC_NORMAL);
    return 1;
}
extern void s2_options_overlay(const GVDP *, int, uint32_t *, int);
const GameVideo sonic2_video = {
    .configure=configure,.enabled=enabled,.width=width,.scanline=scanline,
    .overlay_scanline=s2_options_overlay,.scanline_lazy=scanline_lazy,
    .presentation_configure=presentation_configure,.presentation_capture=presentation_capture,
    .interpolate=interpolate,.presentation_reset=interpolation_reset
};
