/* Opt-in Sonic 2 REV01 world renderer.
 * Canonical s2disasm: GetBlock $E244, ObjectsManager $17AA4,
 * BuildSprites $16604, RingsManager $16F88. Native rendering is the default.
 * Native collision, layouts and decompression remain authoritative.
 */
#include "sonic2_video.h"
#include "sonic2_runtime.h"
#include "sonic2_party.h"
#include "sonic2_resources.h"
#include "sonic2_shadow.h"
#include "sonic2_ending_scenery.h"
#if GENESIS_HAS_RECOMP_NET
#include "netplay/genesis_netplay.h"
#endif
#include "genesis_runtime.h"
#include "video/genesis_vdp.h"
#include "video/genesis_dac.h"
#include "video/genesis_dac_inverse8.h"
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
static unsigned s_level_zone;
static int s_level_zone_valid;
static unsigned s_boss_preview_complete;
unsigned s2_video_level_zone(void)
{
    return s_level_zone_valid?s_level_zone:
        (unsigned)((g_ram[0xFE10]<<8)|g_ram[0xFE11]);
}
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
    int32_t frame_bg_x[2];
    unsigned frame_cloud,frame_htz_shake;
    unsigned frame_zone,frame_act,frame_resize;
    int frame_campaign_view;
    int frame_view_x;
    int frame_view_y;
    int frame_card;
    int frame_card_leaving;
    int frame_sega;
    int frame_menu_background;
    int frame_vs;
    int vs_ready;
    int vs_world;
} S2RenderContext;
static S2RenderContext s_real_render,*s_render=&s_real_render;
static int s_requested_width=320;
static uint16_t s_menu_background_row[40];
static int s_menu_background_ready;
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
void s2_video_donor_rgb_row(uint32_t *pixels,int width)
{
    if(!pixels || width<=0 || !s_presented_fade.mode)return;
    static DonorFade cached={~0u,~0u};
    static uint32_t colors[512];
    if(cached.mode!=s_presented_fade.mode || cached.steps!=s_presented_fade.steps) {
        for(unsigned i=0;i<512;++i) {
            unsigned shade=((i&7)<<1)|((i&0x38)<<2)|((i&0x1C0)<<3);
            colors[i]=genesis_dac_cram_to_argb(
                donor_fade_color((uint16_t)shade,s_presented_fade),GENESIS_DAC_NORMAL);
        }
        cached=s_presented_fade;
    }
    /* Menu portraits, thumbnails and text were composed from the same DAC.
     * Recover their exact indices and apply the native channel sequence. */
    for(int x=0;x<width;++x) {
        uint32_t c=pixels[x];
        unsigned i=genesis_dac_inverse8[(c>>16)&255]|
            (genesis_dac_inverse8[(c>>8)&255]<<3)|(genesis_dac_inverse8[c&255]<<6);
        pixels[x]=colors[i];
    }
}
static int s_interpolation_enabled,s_interpolating;
static double s_interpolation_phase;
static int interpolation_background_base(int line);
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
#define s_frame_act (s_render->frame_act)
#define s_frame_resize (s_render->frame_resize)
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
static int presented_competition(void)
{ return competition() || (outgoing_level() && ram16(0xFFD8)); }
static int presented_gameplay(void)
{ return gameplay() || (outgoing_level() && !ram16(0xFFD8)); }
static int native_demo(void)
{ return (g_ram[0xF600]&127)==8 || (outgoing_level() && ram16(0xFFF0)); }
static int slot_theme(unsigned role)
{
    return !native_demo() && (s2_video_level_zone()>>8)==12 && g_ram[0xF711] &&
        (presented_gameplay() || presented_competition()) && s2_party.s3k_enabled &&
        role<s2_party.roster.slots && !strcmp(s2_party.roster.character[role],"knuckles") &&
        s2_resource_knuckles_art()!=NULL;
}
static unsigned slot_role(int line)
{ return presented_competition() && line>=224?1u:0u; }
static unsigned slot_material(const GVDP *v,unsigned attr)
{
    unsigned bytes=(attr&2047u)*((v->reg[12]&6)==6?64u:32u);
    unsigned base=(v->reg[12]&6)==6?0xEA00u:0xAA00u;
    return !(attr&0x6000) && bytes>=base && bytes<base+1536?(bytes-base)/512+1:0;
}
static unsigned slot_patterns(GVDP *v,int line)
{
    if(!slot_theme(slot_role(line)))return 0;
    unsigned base=(v->reg[12]&6)==6?0xEA00:0xAA00,replaced=0;
    const S2KnucklesArt *art=s2_resource_knuckles_art();uint8_t out[512];
    for(unsigned s=0;s<3;++s) {
        uint8_t *source=v->vram+base+s*512;
        if(s2_knuckles_slot_patterns(g_rom,0x100000,art,s,source,out) && memcmp(source,out,512)) {
            memcpy(source,out,512);replaced|=1u<<s;
        }
    }
    return replaced;
}
static const uint32_t *slot_colors(void)
{
    static uint16_t source[16];static uint32_t colors[16];
    static DonorFade fade={~0u,~0u};
    const S2KnucklesArt *art=s2_resource_knuckles_art();
    if(!art)return NULL;
    if(fade.mode!=s_presented_fade.mode || fade.steps!=s_presented_fade.steps ||
       memcmp(source,art->palette,sizeof source)) {
        memcpy(source,art->palette,sizeof source);fade=s_presented_fade;
        for(unsigned i=0;i<16;++i)colors[i]=genesis_dac_cram_to_argb(
            s2_video_donor_color(source[i]),GENESIS_DAC_NORMAL);
    }
    return colors;
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
     * Attract inputs also depend on the original sub-frame CPU timing.
     * Loading, demos, special stages and opt-out retain that timing. */
    int mode=g_ram[0xF600];
    return enabled() && mode==12 && (gameplay() || competition()) && g_ram[0xF711] ? 4 : 1;
}
static int width(int dw, int dh, int nw, int nh)
{
    if (!enabled()) return 0;
    /* The half-pipe's operator sprites project only the native H32 track.
     * Restore the native display span rather than expose unprojected planes.
     * Keep the outgoing level wide until its objects have actually retired. */
    if((g_ram[0xF600]&127)==16 && !gameplay()) {
        s_requested_width=nw;return nw;
    }
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
static uint16_t chunk_attr(const uint8_t *ram,int wx,int wy,unsigned chunk)
{
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
static uint16_t world_attr(const uint8_t *ram,int wx,int wy,int background)
{
    if(wx<0 || wx>=16384)return 0;
    unsigned layout=(background?0x8080u:0x8000u)+
        (((unsigned)wy>>7)&15u)*256u+((unsigned)wx>>7);
    return chunk_attr(ram,wx,wy,ram[layout]);
}
static uint16_t foreground_attr(const uint8_t *ram,int wx,int wy,int margin,unsigned zone,unsigned act)
{
    /* CNZ2 closes the left arena cell after the native camera passes it.
     * Its old DD scenery still belongs to a visible added margin. Keep the
     * actual F9 wall everywhere inside the original view and in the guest. */
    if(margin && zone==12 && act==1 && ram[0x8C50]==0xF9 &&
       wx>=0x2800 && wx<0x2880 && wy>=0x600 && wy<0x680)
        return chunk_attr(ram,wx,wy,0xDD);
    return world_attr(ram,wx,wy,0);
}
static int wfz_background_source(int raw_x,int source,int wy,int native_begin,int native_width,int margin,unsigned zone)
{
    /* WFZ's first two BG rows contain two finite scenes. The original
     * viewport reaches Earth/Space only at $2B00 and finishes at $3480.
     * Looking ahead during an earlier vertical wrap must retain sky; past
     * the terminal docking composition, continue its authored128px tail.
     * Native samples and the independent cloud/ship rows are unchanged. */
    if(zone!=6 || !margin || ((unsigned)wy&2047u)>=256)return source;
    if(native_begin+native_width<=0x2B00) {
        if(source>=0x2B00 && source<0x3480)
            return 0x2A80+((unsigned)raw_x&127u);
    } else if(native_begin<0x3480 && native_begin+native_width>0x2B00 && raw_x>=0x3480) {
        return 0x3400+((unsigned)raw_x&127u);
    }
    return source;
}
static uint16_t background_attr(const uint8_t *ram,int wx,int wy,int raw_x,int margin,unsigned zone,unsigned resize)
{
    /* WFZ's cloud rows end with the future getaway plane. A negative added
     * margin wraps into that finite scenery before the native escape starts.
     * Keep real cloud samples, every original pixel, and the actual getaway. */
    if(margin && zone==6 && resize<4 && raw_x<0 &&
       wx>=0x2B80 && wx<0x2D80 && wy>=0x580 && wy<0x680) {
        unsigned layout=0x8080u+(((unsigned)wy>>7)&15u)*256u+((unsigned)wx>>7);
        unsigned chunk=ram[layout];
        if(chunk>=0x90 && chunk<=0x97)return 0;
    }
    return world_attr(ram,wx,wy,1);
}
static int scroll_floor(int64_t value,int divisor)
{
    int64_t q=value/divisor;
    return (int)(q-(value<0 && value%divisor!=0));
}
static unsigned scroll_segment(int y,const uint8_t *height,unsigned count)
{
    if(y<0)y=0;
    unsigned i=0;
    while(i+1<count && y>=height[i])y-=height[i++];
    return i;
}
static int scroll_background_x(const S2RenderContext *r,int camera,unsigned which,int scale)
{
    return scroll_floor((int64_t)r->frame_bg_x[which]+(int64_t)(camera-r->frame_fg_x)*scale,65536);
}
static int source_background_scroll(const S2RenderContext *r,int camera,int line,int by)
{
    /* Camera-dependent terms of REV01 SwScrl. The live line buffer remains
     * authoritative for animation, ripples, shakes and event-driven motion.
     * Evaluating this term at two camera origins changes only the viewport;
     * it never advances a native accumulator or runs another world tick. */
    int c=camera,n=-c;
    switch(r->frame_zone) {
    case 0: /* SwScrl_EHZ $C57E: exact shifts and fixed-point grass gradient. */
        if(line<22 || (line>=101 && line<112))return 0;
        if(line<101)return scroll_floor(n,64);
        if(line<128)return scroll_floor(n,16);
        if(line<144){int q=scroll_floor(n,16);return q+scroll_floor(q,2);}
        {int y=line-144;if(y>77)y=77;
         int step=y<15?y:y<33?15+((y-15)/2)*2:33+((y-33)/3)*3;
         int q=(scroll_floor(n,2)-scroll_floor(n,8))*256/48;
         return scroll_floor(n,8)+scroll_floor((int64_t)step*q*256,65536);}
    case 7: /* SwScrl_HTZ $C964, including its original quantized cloud delta. */
        if(r->frame_htz_shake)return -scroll_background_x(r,c,0,65536);
        if(line<128)return scroll_floor(n,8);
        {int delta=(int16_t)(n-(int16_t)(r->frame_cloud-4));
         int q=(scroll_floor(delta,2)-scroll_floor(delta,16))*256/112;
         int steps=line<131?17:line<136?21:line<143?25:line<151?33:
                   line<161?41:line<176?53:65+((line-176)/16)*16;
         return scroll_floor(delta,16)+scroll_floor((int64_t)q*steps*256,65536);}
    case 13: /* CPZ: primary1/8, secondary1/2; the ripple row belongs to BG1. */
        return -scroll_background_x(r,c,by/16>18?1:0,by/16>18?32768:8192);
    case 4:case 5:case 2:case 8:case 1:case 3:case 9:
        return -scroll_background_x(r,c,0,8192); /* SwScrl_Minimal/MTZ. */
    case 15: { /* ARZ's source row heights, not guessed screen-space seams. */
        static const uint8_t heights[]={176,112,48,96,21,12,14,6,12,31,48,192,240,240,240,240};
        unsigned row=scroll_segment(by,heights,sizeof heights);
        if(row<3 || row>=11)return -scroll_background_x(r,c,0,281*256);
        static const unsigned mul[]={1,3,4,5,6,7,8,9};
        return -scroll_floor((int64_t)(c*16/10)*mul[row-3],16);}
    case 11: {
        static const uint8_t heights[]={37,23,18,7,7,2,2,48,13,19,32,64,32,19,13,48,2,2,7,7,32,18,23,37};
        static const uint8_t mul[]={9,8,7,5,4,3,2,1,5,7,8,9,8,7,5,1,2,3,4,5,6,7,8,9};
        unsigned row=scroll_segment(by&511,heights,sizeof heights);
        return -scroll_floor((int64_t)(c*16/10)*mul[row],16);}
    case 12: /* CNZ's seven-step gradient, 1/16 ripple band, then 1/8. */
        if(by<112)return -(c+scroll_floor((int64_t)(by<0?0:by/16)*(scroll_floor(c,8)-c),8));
        return -scroll_floor(c,by<144?16:8);
    case 10: { /* OOZ emits from the bottom: factory, clouds, heat haze. */
        int bg=scroll_background_x(r,c,0,8192),y=223-line;
        int factory=r->frame_bg_y<80?48:r->frame_bg_y>=256?224:r->frame_bg_y-32;
        if(y<factory)return -bg;
        y-=factory;
        static const uint8_t heights[]={8,8,8,7,33,8,8,8,8,8,72};
        static const uint8_t shifts[]={3,4,2,4,255,3,4,2,4,3,0};
        unsigned row=scroll_segment(y,heights,sizeof heights);
        return shifts[row]==255?0:scroll_floor(-bg,1<<shifts[row]);}
    case 14: /* DEZ stars/Earth move by time; empty/sky rows follow the camera. */
        return by<128 || by>=384?-c:0;
    case 6: { /* WFZ's actual normal/transition segment table selects BG1. */
        unsigned at=r->frame_fg_x>=0x2700?0xC8CA:0xC916;
        int y=by&2047;
        for(unsigned i=0;i<38;++i,at+=2) {
            unsigned h=g_rom[at];if(!h)break;
            if(y<(int)h)return g_rom[at+1]<8?-c:0;
            y-=(int)h;
        }
        return by<512?-c:0;}
    case 16:return 0; /* SCZ advances its background clock independently. */
    default:return -c; /* Unused layouts retain their original viewport anchor. */
    }
}
static int background_view_base(const S2RenderContext *r,int native_camera,int view_left,int line,int by,int hs)
{
    return -hs+source_background_scroll(r,native_camera,line,by)-source_background_scroll(r,view_left,line,by);
}
static void capture_background_scroll(S2RenderContext *r)
{
    r->frame_bg_x[0]=(int32_t)(((uint32_t)ram16(0xEE68)<<16)|ram16(0xEE6A));
    r->frame_bg_x[1]=(int32_t)(((uint32_t)ram16(0xEE70)<<16)|ram16(0xEE72));
    r->frame_cloud=ram16(0xA822);r->frame_htz_shake=g_ram[0xEEBC];
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
/* The original512px cloud composition has clipped outer clouds. Reflect
 * those edges in the margins rather than joining two unrelated cut edges.
 * Its original0..511 samples remain exact; other parallax bands still loop. */
static int ehz_cloud_x(int x,unsigned *flip)
{
    unsigned phase=(unsigned)x&1023u;
    *flip=phase>=512?0x800u:0;
    return (int)(phase>=512?1023-phase:phase);
}
static int reflected_cloud_x(int x,int span,unsigned *flip)
{
    int phase=x%(span*2);if(phase<0)phase+=span*2;
    *flip=phase>=span?0x800u:0;
    return phase>=span?span*2-1-phase:phase;
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
const GVDP *s2_video_slots_overlay(const GVDP *v,int line,uint32_t *out,int width)
{
    if(enabled() || width!=320 || !slot_theme(slot_role(line)))return v;
    static GVDP copy;static unsigned replaced;static uint8_t rows[2][128];
    if(!line || ((v->reg[12]&6)==6 && line==224)) {
        copy=*v;replaced=slot_patterns(&copy,line);
        memset(rows,0,sizeof rows);
        if(replaced) {
            unsigned wt=(unsigned)plane_tiles(v->reg[16]&3),ht=(unsigned)plane_tiles((v->reg[16]>>4)&3);
            for(unsigned p=0;p<2;++p) {
                unsigned base=p?(v->reg[4]&7u)<<13:(v->reg[2]&56u)<<10;
                for(unsigned y=0;y<ht;++y)for(unsigned x=0;x<wt;++x) {
                    unsigned material=slot_material(&copy,vram16(&copy,base+(y*wt+x)*2));
                    if(material && (replaced&(1u<<(material-1)))){rows[p][y]=1;break;}
                }
            }
        }
    }
    if(!replaced)return v;
    memcpy(copy.reg,v->reg,sizeof copy.reg);memcpy(copy.cram,v->cram,sizeof copy.cram);
    memcpy(copy.vsram,v->vsram,sizeof copy.vsram);
    /* Recompose only rows which can display a replaced window, including
     * either interlace view and the final partial horizontal scroll cell. */
    int visible=0,im2=(v->reg[12]&6)==6;
    unsigned table=(v->reg[13]&63u)<<10,mode=v->reg[11]&3;
    unsigned raster=im2?(unsigned)line>>1:(unsigned)line;
    unsigned row=mode==0?0:mode==2?raster&~7u:raster;
    unsigned height_mask=(unsigned)plane_tiles((v->reg[16]>>4)&3)-1;
    int column_scroll=!!(v->reg[11]&4),vs_mask=im2?2047:1023;
    for(unsigned plane=0;!visible && plane<2;++plane) {
        unsigned base=plane?(v->reg[4]&7u)<<13:(v->reg[2]&56u)<<10;
        int hs=vram16(v,table+row*4+plane*2)&1023;
        int full_vs=v->vsram[plane]&vs_mask;
        if(!column_scroll && !rows[plane][((unsigned)(line+full_vs)>>(im2?4:3))&height_mask])continue;
        for(int x=0;!visible && x<=320;x+=8) {
            int screen=x<320?x:319;
            unsigned col=column_scroll?(unsigned)(screen>>4)*2:0;
            int vs=column_scroll?v->vsram[(col+plane)%GVDP_VSRAM_ENTRIES]&vs_mask:full_vs;
            unsigned tile_row=((unsigned)(line+vs)>>(im2?4:3))&height_mask;
            if(!rows[plane][tile_row])continue;
            unsigned material=slot_material(&copy,plane_attr(&copy,base,screen-hs,line+vs));
            if(material && (replaced&(1u<<(material-1))))visible=1;
        }
    }
    if(visible) {
        uint8_t indices[GVDP_MAX_WIDTH];int count=gvdp_render_scanline(&copy,line,indices);
        const uint32_t *host=gvdp_host_palette();
        for(int x=0;x<count && x<width;++x) {
            unsigned i=indices[x];
            out[x]=i<GVDP_WS_BAR_INDEX?genesis_dac_cram_to_argb(v->cram[i%64],
                i<64?GENESIS_DAC_NORMAL:i<128?GENESIS_DAC_SHADOW:GENESIS_DAC_HIGHLIGHT):
                i>=GVDP_HOST_PALETTE_BASE?(host?host[i-GVDP_HOST_PALETTE_BASE]:
                    genesis_dac_cram_to_argb(gvdp_ext_color(line,
                        1+(int)(i-GVDP_HOST_PALETTE_BASE)/16,(int)(i-GVDP_HOST_PALETTE_BASE)%16),
                        GENESIS_DAC_NORMAL)):0xFF000000;
        }
    }
    return &copy;
}

/* Enhanced scene data lives on the host: no SAT coordinate wrap, 80-piece
 * ceiling, or 20-sprites-per-line ceiling. Native BuildSprites still executes
 * for the hardware pass and publishes a signature used to align our list
 * with the exact VBlank upload displayed by the VDP. */
typedef struct { int x, y; uint16_t attr; uint8_t size, hud, art, actor; } SceneSprite;
enum { SCENE_SPRITES = 32768, SCENE_PLACEMENTS = 1024,
       ROW_CARD_COVERED = 8,
       SCENE_FOREGROUND_SCREEN = 4, SCENE_WATER_REPEAT = 16,
       SCENE_ART_SIGNPOST = 8, SCENE_ART_CAPSULE = 32,
       SCENE_ART_EHZ_CAR = 40, SCENE_ART_MASK = 40,
       SCENE_ART_MARGIN_VIEW = 64, SCENE_ART_NATIVE_VIEW = 128 };
typedef struct { uint8_t pixels[4096]; unsigned ready,bytes; } NativeSpriteArt;
typedef struct { unsigned asset,id,map,base,tiles;int zone; } NativeObjectArt;
static const NativeObjectArt s_native_object_art[]={
    {S2_NATIVE_EXPLOSION,0x27,0x21120,0x5A4,68,-1},
    {S2_NATIVE_FLICKY,0x28,0x11E1C,0x594,16,0},
    {S2_NATIVE_SQUIRREL,0x28,0x11E40,0x580,20,0}
};
static const NativeObjectArt *native_asset_binding(unsigned asset)
{
    for(unsigned i=0;i<sizeof s_native_object_art/sizeof *s_native_object_art;++i)
        if(s_native_object_art[i].asset==asset)return &s_native_object_art[i];
    return NULL;
}
static const NativeObjectArt *native_object_binding(unsigned o,unsigned map,unsigned gfx)
{
    if(o<0xB000 || o>=0xD800 || (o&63))return NULL;
    for(unsigned i=0;i<sizeof s_native_object_art/sizeof *s_native_object_art;++i) {
        const NativeObjectArt *b=&s_native_object_art[i];
        if(g_ram[o]!=b->id || map!=b->map || (gfx&2047)!=b->base)continue;
        /* Animal tile numbers are reused by other zone/species PLCs and
         * the stock ending. Only the native EHZ pair owns these bindings. */
        if(b->zone>=0 && (!presented_gameplay() || presented_competition() ||
                         (s2_video_level_zone()>>8)!=(unsigned)b->zone))continue;
        return b;
    }
    return NULL;
}
static int ooz_collapse_material(unsigned map,unsigned gfx,int static_mapping)
{
    if((gfx&2047)!=0x39D || (s2_video_level_zone()>>8)!=10)return 0;
    /* Obj1F replaces its mapping table with a direct pointer to each of
     * the seven original pieces when it breaks. All retain the same art. */
    return static_mapping?(map>=0x110CE && map<=0x110FE && !((map-0x110CE)&7u)):
        map==0x110C6;
}
static int dez_window_material(unsigned owner,unsigned map,unsigned gfx)
{
    if(map!=0x3A08C || (gfx&2047)!=0x378 || s2_video_level_zone()!=0xE00 ||
       !presented_gameplay() || presented_competition())return 0;
    if(owner==0xFFF60000u)return 1; /* The same source-bound margin preview. */
    unsigned o=owner>>16;
    return o>=0xB400 && o<0xD000 && !(o&63) && g_ram[o]==0xAF &&
        g_ram[o+0x24]>=0x1A && g_ram[o+0x24]<=0x22 && !(g_ram[o+0x24]&1);
}
static NativeSpriteArt s_native_sprite_art[S2_NATIVE_SPRITE_ART_COUNT];
static int native_art_ready(unsigned asset)
{
    if(asset>=S2_NATIVE_SPRITE_ART_COUNT)return 0;
    NativeSpriteArt *art=&s_native_sprite_art[asset];
    if(!art->ready) {
        /* Settings initialize the sprite banks before the runner loads ROM.
         * An absent image is not a failed immutable asset decode. */
        if(memcmp(g_rom+0x100,"SEGA",4))return 0;
        art->bytes=s2_native_sprite_art_decode(g_rom,0x100000,asset,art->pixels,sizeof art->pixels);
        art->ready=art->bytes?1:2;
    }
    return art->ready==1;
}
static uint8_t native_asset_pixel(const GVDP *v,unsigned asset,unsigned base,uint16_t attr,int x,int y,int width)
{
    if((!native_asset_binding(asset) && (!enabled() || width<=320)) ||
       (v->reg[12]&6)==6 || !native_art_ready(asset))
        return pattern_pixel(v,attr,x,y);
    NativeSpriteArt *art=&s_native_sprite_art[asset];
    unsigned tile=(attr&2047)-base;
    if(art->ready!=1 || tile>=art->bytes/32)return pattern_pixel(v,attr,x,y);
    unsigned fx=x&7,fy=y&7;
    if(attr&0x800)fx^=7;if(attr&0x1000)fy^=7;
    unsigned byte=art->pixels[tile*32+fy*4+fx/2],ink=fx&1?byte&15:byte>>4;
    return ink?(uint8_t)(((attr>>9)&48)|ink):0;
}
static uint8_t native_scene_pixel(const GVDP *v,unsigned material,uint16_t attr,int x,int y,int width)
{
    unsigned asset,base;
    switch(material&SCENE_ART_MASK) {
    case SCENE_ART_SIGNPOST:asset=S2_NATIVE_SIGNPOST;base=0x434;break;
    case SCENE_ART_CAPSULE:asset=S2_NATIVE_CAPSULE;base=0x680;break;
    case SCENE_ART_EHZ_CAR:
        if(s_frame_zone==13) {
            unsigned tile=attr&2047u;
            if(tile>=0x500){asset=S2_NATIVE_CPZ_PUMP;base=0x500;}
            else if(tile>=0x420){asset=S2_NATIVE_CPZ_POD;base=0x420;}
            else {asset=S2_NATIVE_CPZ_JETS;base=0x418;}
        } else {asset=S2_NATIVE_EHZ_CAR;base=0x400;}
        break;
    default:return pattern_pixel(v,attr,x,y);
    }
    return native_asset_pixel(v,asset,base,attr,x,y,width);
}
typedef struct {
    SceneSprite sprites[SCENE_SPRITES];
    unsigned count, serial;
    uint8_t sat[640];
    int scene; /* 1 level, 2 special, 3 native competition, 0 menu */
    int camera_x, camera_y; /* coordinates used when capturing world sprites */
    int native_hscroll,native_vscroll; /* native DMA publication, before VDP wrapping */
    int views[S2_MAX_PLAYERS][2]; /* all players' origins, identical on every peer */
    S2RuntimeActors actors; /* same BuildSprites publication as the native SAT */
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
/* The defeated DEZ body becomes a moving explosion controller. Retain only
 * its last genuine drawing, in world coordinates, while a wide view owns it. */
typedef struct {
    SceneSprite pieces[16];
    uint8_t priority[16];
    unsigned count,root,active;
} CorpsePresentation;
static CorpsePresentation s_dez_corpse;

/* Ending scenery can leave the hardware view before it leaves a wide view.
 * Keep only the terminal native poses; object slots and guest deletion remain
 * entirely native. The fixed pool exceeds the ending's authored cloud/bird
 * cohorts, including records whose native slots have already been reused. */
enum { ENDING_OBJECTS=144, ENDING_CONTINUATIONS=512 };
typedef struct { uint8_t object[64],active; } EndingScenery;
static struct {
    uint8_t previous[ENDING_OBJECTS][64];
    EndingScenery continued[ENDING_CONTINUATIONS];
    uint8_t armed;
} s_ending_scenery;
typedef struct { uint32_t position; uint16_t zone,tick,velocity; uint8_t seen,active; } Runoff;
static Runoff s_runoff[S2_MAX_PLAYERS];
int s2_video_runoff_velocity(unsigned role)
{ return role<S2_MAX_PLAYERS && s_runoff[role].active?s_runoff[role].velocity:0; }
/* The native goal clamps actors beyond its 320-pixel view. Continue only
 * their completed display positions; collision, AI and results stay native.
 * Called during object publication, never from fractional presentation. */
int s2_video_runoff_x(unsigned role,unsigned o,int x)
{
    if(role>=S2_MAX_PLAYERS || o<0xB000 || o>=0xD000 || (o&63))return x;
    Runoff *s=&s_runoff[role];unsigned zone=s2_video_level_zone(),tick=ram16(0xFE04);
    if(!enabled() || s_requested_width<=320 || !presented_gameplay() ||
       !g_ram[0xF711] || native_demo() || !ram16(0xFFC8)) {
        memset(s,0,sizeof *s);return x;
    }
    if(!s->seen || s->zone!=zone) {
        memset(s,0,sizeof *s);s->seen=1;s->zone=(uint16_t)zone;s->tick=(uint16_t)tick;
    }
    int velocity=(int16_t)ram16(o+0x10);
    if(!s->active && velocity>0)s->velocity=(uint16_t)velocity;
    unsigned dt=(uint16_t)(tick-s->tick);s->tick=(uint16_t)tick;
    if(g_ram[0xFE1E] || (!s->active && x<(int)ram16(0xEECA)+360) ||
       !g_ram[o] || g_ram[o+0x24]>=6) {
        s->active=0;s->position=(uint32_t)(uint16_t)x<<8;return x;
    }
    if(!s->active) {
        s->active=1;s->position=(uint32_t)(uint16_t)x<<8;
        if(s->velocity<0x600)s->velocity=0x600;
    } else {
        if(dt>4)dt=1; /* counter resets must not become a display teleport */
        uint64_t next=(uint64_t)s->position+(uint64_t)s->velocity*dt;
        s->position=next>0xFFFF00u?0xFFFF00u:(uint32_t)next;
        if(s->position<(uint32_t)(uint16_t)x<<8)s->position=(uint32_t)(uint16_t)x<<8;
    }
    return (int)(s->position>>8);
}

/* The native SAT remains authoritative for foreground sprites and UI. Keep only
 * source-object classifications beside its completed publication; no guest
 * marker, mapping, palette or sprite order is changed. */
typedef struct {
    uint8_t sat[2][640],kind[2][80];
    unsigned serial,ui_mask,world_mask;
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
    for(unsigned p=0;p<2;++p)for(unsigned i=0;i<80;++i) {
        if(s_front_build.kind[p][i])s_front_build.ui_mask|=1u<<p;
        if(s_front_build.kind[p][i]==4)s_front_build.world_mask|=1u<<p;
    }
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
    S2_STATE(io,s_runoff);
    if(io->mode) {
        if(io->pos>io->size || sizeof s_dez_corpse>io->size-io->pos) {io->ok=0;return;}
        CorpsePresentation saved_corpse;memcpy(&saved_corpse,io->data+io->pos,sizeof saved_corpse);
        if(saved_corpse.count>16 || saved_corpse.active>1 ||
           (saved_corpse.root && (saved_corpse.root<0xB400 || saved_corpse.root>=0xD000 || (saved_corpse.root&63))) ||
           (saved_corpse.active && (!saved_corpse.root || !saved_corpse.count))) {io->ok=0;return;}
    }
    S2_STATE(io,s_dez_corpse);
    S2_STATE(io,s_ending_scenery);
    S2_STATE(io,s_level_zone);S2_STATE(io,s_level_zone_valid);S2_STATE(io,s_boss_preview_complete);
    S2_STATE(io,s_donor_fade); S2_STATE(io,s_presented_fade);
    S2_STATE(io,s_menu_background_row);S2_STATE(io,s_menu_background_ready);
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
    return a < S2_KNUCKLES_ENDING_ROM+S2_KNUCKLES_ENDING_BYTES ? g_rom[a] : a >= 0xFF0000u ? g_ram[a & 65535u] : 0;
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
        memset(&s_ending_scenery,0,sizeof s_ending_scenery);
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
    /* The lock-on palette also colours the native spin faces and pole.
     * Only the two settled-face pieces use replacement pattern data. */
    return (tile>=0x400 && tile<0x456) || (tile>=0x46E && tile<0x482) ||
        (tile>=0x580 && tile<0x5A8);
}
static int amy_item_tile(unsigned tile)
{
    if(tile>=0x434 && tile<0x482)return 1;
    return (tile>=0x7D4 && tile<0x7D8) || (tile>=0x5F4 && tile<0x600);
}
#if OWN_BACKEND
static int shadow_sprite_tagged(const uint8_t *data,unsigned base,int interlace)
{
    unsigned slot=0;
    for (unsigned n=0;n<80;++n) {
        unsigned at=(base+slot*8)&65535u;
        unsigned tile=((data[(at+4)&65535u]<<8)|data[(at+5)&65535u])&2047u;
        if(interlace)tile*=2;
        if ((data[(at+2)&65535u]&0xC0)==0xC0 && tile>=0x4DE && tile<0x500) return 1;
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
    if (s_display) for (unsigned n=0;n<s_display->count;++n) {
        const SceneSprite *s=&s_display->sprites[n];
        unsigned tile=(s->attr&2047u)*(s_display->scene==3?2u:1u);
        if ((s->size&0xC0)==0xC0 && tile>=0x4DE && tile<0x500) return 1;
    }
    /* Retain the palette for both queued and currently displayed pieces:
     * an interrupt can precede completion/upload of the next native SAT. */
    return shadow_sprite_tagged(g_ram,0xF800,presented_competition()) ||
        (competition() && shadow_sprite_tagged(g_ram,0xDD00,1)) ||
        shadow_sprite_tagged(g_machine.vdp.vram,(g_machine.vdp.reg[5]&127u)<<9,
            (g_machine.vdp.reg[12]&6)==6);
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
static int s_item_water_enabled,s_item_water_selected=-1,s_item_sprite_palette;
static int s_slot_palette_enabled,s_slot_palette_row=-1,s_item_color_split;
static void item_sprite_palette(int wet)
{
    if (!s_item_sprite_palette) return;
    uint32_t colors[GVDP_EXT_LINES*16];
    const uint16_t *source=wet?s_item_bottom:s_item_top;
    for(unsigned c=0;c<GVDP_EXT_LINES*16;++c)
        colors[c]=genesis_dac_cram_to_argb(source[c],GENESIS_DAC_NORMAL);
    gvdp_set_host_palette(colors,GVDP_EXT_LINES*16);
}
static void item_palette_row(void *user,const GVDP *v,const GVDPSpriteLayer *layer)
{
    (void)user;
    if(s_slot_palette_enabled && layer && presented_competition()) {
        unsigned p=layer->line>=224;int changed=s_slot_palette_row!=(int)p;
        if(!slot_theme(p)) {
            uint16_t *colors=(p?s_item_bottom:s_item_top)+32;
            if(memcmp(colors,v->cram,16*sizeof *colors)) {
                memcpy(colors,v->cram,16*sizeof *colors);changed=1;
            }
        }
        if(changed) {
            gvdp_set_ext_colors(s_item_top,s_item_bottom,s_item_color_split);
            item_sprite_palette((int)p);s_slot_palette_row=(int)p;
        }
    }
    if (!s_item_water_enabled) return;
    int wet=s2_video_water_palette(v);
    if (wet!=s_item_water_selected) {
        gvdp_set_ext_colors(s_item_top,s_item_bottom,wet?0:1<<20);
        item_sprite_palette(wet);
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
    static const S2ItemArt *cached_amy_items;
    static unsigned cached_native;
    static uint8_t patterns[65536];
    static uint8_t shadow_patterns[65536],shadow_lines[2048],world_lines[2048],item_lines[2048];
    static int shadow_ready;
    const S2KnucklesArt *art=s2_resource_knuckles_art();
    const uint16_t *special_palette=NULL;
    const S2SpecialArtFrame *special=special_actor_art(&special_palette);
    const S2SpecialStageArt *amy=!strcmp(s2_party.roster.character[0],"amy")?s2_resource_amy_special():NULL;
    const S2ItemArt *amy_items=s2_resource_primary_items();
    const S2DonorBank *amy_bank=!amy_items?NULL:
        !strcmp(s2_party.roster.character[0],"shadow")?s2_resource_shadow_bank():
        s2_resource_bank(!strcmp(s2_party.roster.character[0],"amy")?S2_RESOURCE_AMY:S2_RESOURCE_MEGAMIX);
    if(!amy_bank)amy_items=NULL;
    unsigned native=0;
    for(unsigned i=0;i<sizeof s_native_object_art/sizeof *s_native_object_art;++i)
        if(native_art_ready(s_native_object_art[i].asset))native|=1u<<i;
    if (art!=cached || special!=cached_special || amy_items!=cached_amy_items || native!=cached_native) {
        memset(patterns,0,sizeof patterns);
        memset(item_lines,0,sizeof item_lines);
        /* Destruction pieces share their native atlas with title-card PLCs.
         * Marked SAT pieces retain original art at native width as well;
         * unmarked title glyphs still read live, unchanged guest VRAM. */
        for(unsigned i=0;i<sizeof s_native_object_art/sizeof *s_native_object_art;++i)if(native&(1u<<i)) {
            const NativeObjectArt *b=&s_native_object_art[i];
            memcpy(shadow_patterns+b->base*32,s_native_sprite_art[b->asset].pixels,
                   s_native_sprite_art[b->asset].bytes);
        }
        if(art) {
            memcpy(patterns+0x4BE*32,art->shield_stars,sizeof art->shield_stars);
            memcpy(patterns+0x6AC*32,art->monitor,sizeof art->monitor);
            memcpy(patterns+0x456*32,art->signpost,sizeof art->signpost);
            memcpy(patterns+0x7D4*32,art->life,4*32);
            memcpy(patterns+0x5F4*32,art->continue_icons,sizeof art->continue_icons);
            memcpy(patterns+SPECIAL_FONT_K*32,art->font_k,sizeof art->font_k);
            for (unsigned t=0x434;t<0x482;++t) item_lines[t]=2;
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
        if(amy_items) {
            if(amy_items->signpost_tiles && amy_items->signpost_tiles<=78) {
                memcpy(patterns+0x434*32,amy_items->signpost,amy_items->signpost_tiles*32);
                for(unsigned t=0x434;t<0x434+amy_items->signpost_tiles;++t)
                    item_lines[t]=GVDP_EXT_SPRITE_FORCE|2;
            }
            memcpy(patterns+0x7D4*32,amy_items->life,sizeof amy_items->life);
            for(unsigned t=0x7D4;t<0x7D8;++t)item_lines[t]=GVDP_EXT_SPRITE_FORCE|2;
            if(!strcmp(s2_party.roster.character[0],"amy")) {
                memcpy(patterns+0x5F4*32,amy_items->continue_icons,sizeof amy_items->continue_icons);
                for(unsigned t=0x5F4;t<0x600;++t)item_lines[t]=GVDP_EXT_SPRITE_FORCE|2;
            }
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
    cached=art;cached_special=special;cached_amy_items=amy_items;cached_native=native;
    /* Original grey items keep live CRAM. The signpost patch and tagged
     * world objects select a separate, native-timed Knuckles palette. */
    int have_patterns=art || special || amy_items;
    gvdp_set_sprite_patterns_bank(0,have_patterns?patterns:NULL,have_patterns?sizeof patterns:0,item_lines);
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
    int amy_world=amy_items && (g_ram[0xF600]&127)==12;
    int item_world=knux_world || amy_world;
    s_item_sprite_palette=amy_world && !shadow_active;
    if (!s_item_sprite_palette) gvdp_set_host_palette(NULL,0);
    int actor_special=special && (g_ram[0xF600]&127)==16;
    int slots=slot_theme(0) || (presented_competition() && slot_theme(1));
    s_slot_palette_enabled=slots;s_slot_palette_row=-1;
    memset(world_lines,0,sizeof world_lines);
    /* The active colour table is shared, but only object-tagged pieces use
     * its Knuckles line. Planes and unmarked sprites keep native CRAM. */
    s_item_water_enabled=item_world && g_ram[0xF64C] && !competition();
    s_item_water_selected=-1;
    gvdp_set_host_sprites(s_item_water_enabled || slots?item_palette_row:NULL,NULL);
    /* Bank0's Knuckles animal palette covers $580..$5A7. Keep native pieces
     * in bank1's disjoint strip so all their pieces retain live native CRAM,
     * including while imported item/special-stage resources are cached. */
    gvdp_set_sprite_patterns_bank(1,shadow_active || native?shadow_patterns:NULL,
        shadow_active || native?sizeof shadow_patterns:0,shadow_lines);
    if (!shadow_active && !item_world && !actor_special && !slots) {gvdp_set_ext_tiles(NULL);return;}
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
    if(amy_world) {
        unsigned zone=g_ram[0xFE10];
        const uint16_t *wet=amy_bank->palette;
        if(!competition() && g_ram[0xF64C] && amy_bank->water_palette_count)
            wet=zone==15 && amy_bank->water_palette_count>1?amy_bank->water_palette_alt:amy_bank->water_palette;
        for(unsigned c=0;c<16;++c) {
            top[16+c]=s2_video_donor_color(amy_bank->palette[c]);
            bottom[16+c]=s2_video_donor_color(wet[c]);
        }
    }
    if(actor_special)for(unsigned c=0;c<16;++c)
        top[32+c]=bottom[32+c]=s2_video_donor_color(special_palette[c]);
    if(slots) {
        int vs=presented_competition();unsigned base=vs?0x3A8:0x550,count=vs?24:48;
        for(unsigned t=base;t<base+count;++t)world_lines[t]=3;
        for(unsigned c=0;c<16;++c) {
            top[32+c]=slot_theme(0)?s2_video_donor_color(art->palette[c]):ram16(0xFB00+c*2);
            bottom[32+c]=slot_theme(vs?1:0)?s2_video_donor_color(art->palette[c]):ram16(0xFB00+c*2);
        }
    }
    int split=1<<20;
    if (!competition() && g_ram[0xF64C])
        split=g_ram[0xF64E]?0:g_ram[0xF625]+1;
    if(slots && presented_competition())split=224;
    s_item_color_split=split;
    /* Amy items and half-pipe colours belong only to marked sprites.
     * An all-zero plane table still selects private-plane composition. */
    gvdp_set_ext_tiles(knux_world || shadow_active || slots?world_lines:NULL);
    memcpy(s_item_top,top,sizeof s_item_top);memcpy(s_item_bottom,bottom,sizeof s_item_bottom);
    gvdp_set_ext_colors(top,bottom,split);
    item_sprite_palette(split==0);
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
static int special_primary_ui_sat(const GVDP *v,GVDP *copy,int line,int knuckles)
{
    if(!knuckles && (line<16 || line>=24 || strcmp(s2_party.roster.character[0],"amy")))return 0;
    unsigned table=(v->reg[5]&127u)<<9,slot=0;int changed=0;
    for(unsigned n=0;n<64 && slot<80;++n) {
        unsigned at=table+slot*8;uint8_t p[8];
        for(unsigned i=0;i<8;++i)p[i]=v->vram[(at+i)&65535];
        unsigned attr=(p[4]<<8)|p[5],tile=attr&2047;
        unsigned cells=(((p[2]>>2)&3)+1)*((p[2]&3)+1);
        int y=(((p[0]<<8)|p[1])&1023)-128;
        if(!(p[2]&0x80) && (attr&0xE000)==0xA000 && tile>=0x1FA &&
           tile+cells<=0x1FF && line>=16 && line<24 && line>=y && line<y+8) {
            if(copy) {
                if(knuckles) {
                    copy->vram[at&65535]=0;copy->vram[(at+1)&65535]=0;
                } else {
                    attr=(attr&0xF800)|(SPECIAL_HUD_NAME+tile-0x1FA);
                    copy->vram[(at+2)&65535]=p[2]|0x80;
                    copy->vram[(at+4)&65535]=(uint8_t)(attr>>8);
                    copy->vram[(at+5)&65535]=(uint8_t)attr;
                }
            }
            changed=1;
        } else if(knuckles && !(p[2]&0x80) && (attr&0x6000)==0x2000 &&
                  tile>=0x1BC && tile+cells<=0x1FA && line>=y && line<y+((p[2]&3)+1)*8) {
            if(copy)copy->vram[(at+2)&65535]=p[2]|GVDP_SPRITE_PALETTE_ONLY;
            changed=1;
        }
        slot=p[3]&127;if(!slot)break;
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
    int knuckles=!strcmp(s2_party.roster.character[0],"knuckles");
    /* Amy's name can only occupy these eight rows. Apply its single SAT
     * walk directly to the private copy; Knuckles needs a checkpoint probe. */
    int ui=knuckles?special_primary_ui_sat(v,NULL,line,1):
        (line>=16 && line<24 && !strcmp(s2_party.roster.character[0],"amy"));
    GVDP copy;
    int body=valid?special_actor_sat(v,&picture,art,line,&copy):0;
    if(!body && !ui)return;
    if(!body)copy=*v;
    if(ui)ui=special_primary_ui_sat(&copy,&copy,line,knuckles);
    if(body || ui)special_actor_compose(v,&copy,palette,line,out,width);
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
    const NativeObjectArt *binding=native_object_binding(owner>>16,map,gfx);
    if(!static_mapping) {
        p+=(int16_t)scene_read16(p+(frame&255u)*2u);
        count=scene_read16(p);p+=2;
    }
    if(count>1024)return;
    for(int n=0;n<count && dst->count<SCENE_SPRITES;++n,p+=8) {
        int dy=(int8_t)scene_read8(p),size=scene_read8(p+1)&15;
        int dx=(int16_t)scene_read16(p+6);
        int vs=presented_competition();
        uint16_t attr=(uint16_t)(scene_read16(p+(vs?4:2))+gfx);
        if(flags&1){dx=-dx-(((size>>2)&3)+1)*8;attr^=0x800;}
        if(flags&2){dy=-dy-((size&3)+1)*8;attr^=0x1000;}
        if(!(flags&0x4000) && !vs && !s2_runtime_campaign_online() &&
           (y+dy>=224 || y+dy+((size&3)+1)*8<=0))continue;
        if(keys) {
            /* Asteron's lit frame inserts an eye before the same two body
             * pieces. Keep their identities across the animation change. */
            unsigned piece=(unsigned)n;
            if(map==0x38A96 && (frame&255u)==1 && !static_mapping)
                piece=n?(unsigned)n-1u:2u;
            keys[dst->count]=owner|piece;
        }
        SceneSprite *q=&dst->sprites[dst->count++];
        memset(q,0,sizeof *q); /* Serialized padding must be deterministic. */
        q->x=x+dx;q->y=y+dy;q->attr=attr;q->size=(uint8_t)(vs?((size&12)|((size&3)>>1)):size);
        /* Native PLCs can reclaim these tiles once their old objects leave
         * the hardware viewport. A wider view still owns those mappings.
         * Keep the original object, collision and animation; only its art
         * source remains available before/after the native load window. */
        q->art=0;
        if(!vs) {
            if(map==0x250BA)q->art=(uint8_t)(1+((gfx&2047)==0x3FF?S2_NATIVE_OOZ_BLOCK_HORIZ:S2_NATIVE_OOZ_BLOCK_VERT));
            else if(map==0x2B07E)q->art=1+S2_NATIVE_CNZ_PLUNGER_VERT;
            else if(map==0x2B0EC)q->art=1+S2_NATIVE_CNZ_PLUNGER_DIAG;
            else if(ooz_collapse_material(map,gfx,static_mapping))q->art=1+S2_NATIVE_OOZ_COLLAPSE;
            if(binding)q->art=(uint8_t)(1+binding->asset);
            /* Robo's PLC reclaims only the original eight window tiles.
             * Other pieces in this map (0508..0517) keep their native art. */
            if(dez_window_material(owner,map,gfx) && (attr&2047)>=0x378 && (attr&2047)<0x380)
                q->art=1+S2_NATIVE_DEZ_WINDOW;
        }
        if(flags&0x20000)q->art=(uint8_t)(1+((q->attr&2047)<0x518?
            S2_NATIVE_ROBOTNIK_UPPER:S2_NATIVE_ROBOTNIK_LOWER));
        if(flags&0x8000)q->art|=SCENE_ART_NATIVE_VIEW;
        if(flags&0x10000)q->art|=SCENE_ART_MARGIN_VIEW;
        unsigned art=vs?0:map==0x195BE?SCENE_ART_SIGNPOST:map==0x3F436?SCENE_ART_CAPSULE:
            (map==0x2FA58 || map==0x2ED8C || map==0x2EADC || map==0x2EE88)?SCENE_ART_EHZ_CAR:0;
        q->hud=(uint8_t)(art|anchor|((flags&0x400)?128:0)|((flags&0x800)?64:0)|
                         ((flags&0x1000)?SCENE_WATER_REPEAT:0)|
                         ((flags&0x4000)?SCENE_FOREGROUND_SCREEN:0));
        if ((flags&0x100) && knuckles_item_tile((attr&2047)*(vs?2:1))) q->size|=0x80;
        if ((flags&0x2000) && amy_item_tile((attr&2047)*(vs?2:1))) q->size|=0x80;
        if ((flags&0x100) && knuckles_palette_tile((attr&2047)*(vs?2:1))) q->size|=GVDP_SPRITE_PALETTE_ONLY;
        if ((flags&0x200) && (attr&2047)*(vs?2u:1u)>=0x4DE &&
            (attr&2047)*(vs?2u:1u)<0x500) q->size|=0xC0;
        if(binding)q->size|=0xC0; /* Independent patterns and live native CRAM. */
    }
}

static void load_placements(void)
{
    unsigned zone=ram16(0xFE10),index=((zone>>8)*4u)+((zone&1u)*2u);
    unsigned base=0xE6800u+(int16_t)scene_read16(0xE6800u+index);
    if (competition() && g_ram[0xFE10]==12) base=(zone&1)?0x18492:0x1802A;
    unsigned campaign=s2_runtime_stage_layout(zone>>8,zone&255u);
    if(campaign) base=campaign;
    if(base==s_placement_base && s_placement_count)return;
    s_placement_base=base;s_placement_count=0;unsigned state=1;
    unsigned limit=campaign?S2_STAGE_LAYOUT_ROM+S2_STAGE_LAYOUT_BYTES:0x100000u;
    for(unsigned p=base;p+6<=limit && s_placement_count<SCENE_PLACEMENTS;p+=6) {
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
static int object_queued(unsigned object)
{
    for(unsigned priority=0;priority<8;++priority) {
        unsigned queue=0xAC00+priority*128,bytes=ram16(queue);if(bytes>126)bytes=126;
        for(unsigned k=2;k<=bytes;k+=2)if(ram16(queue+k)==object)return 1;
    }
    return 0;
}
static int dormant_wfz_laser(unsigned o)
{
    return s2_video_level_zone()==0x600 && g_ram[o]==0xB9 &&
        g_ram[o+0x24]==2 && scene_read32(0xFF0000u+o+4)==0x3BB18;
}
static unsigned ending_word(const uint8_t *p,unsigned at)
{ return ((unsigned)p[at]<<8)|p[at+1]; }
static uint32_t ending_long(const uint8_t *p,unsigned at)
{ return ((uint32_t)ending_word(p,at)<<16)|ending_word(p,at+2); }
static int ending_scenery_owner(void)
{
    return g_ram[0xF600]==0x20 && g_ram[0xB100]==0xCA &&
        g_ram[0xB134]==0xFF && ram16(0xF750)<=4 && !(ram16(0xF750)&1);
}
static int ending_terminal(const uint8_t *p)
{
    unsigned map=ending_long(p,4),tile=ending_word(p,2)&2047;
    if(p[0]==0xCB)return p[0x24]==4 && map==0x3B32C && tile==0x54F &&
        p[0x1A]<=2 && (int16_t)ending_word(p,0x12)==0 &&
        ((int16_t)ending_word(p,0x10)==-0x100 ||
         (int16_t)ending_word(p,0x10)==-0x200 || (int16_t)ending_word(p,0x10)==-0x300);
    return p[0]==0xCD && p[0x24]==2 && p[0x25]==4 &&
        map==0x11E1C && tile==0x594 && p[0x1A]<=1 &&
        (int16_t)ending_word(p,0x10)==-0x100 &&
        ((int16_t)ending_word(p,0x12)==-0x20 || (int16_t)ending_word(p,0x12)==0x20);
}
static void ending_scenery_step(uint8_t *p)
{
    for(unsigned at=8;at<=12;at+=4) {
        /* Native ObjectMove wraps a32-bit position; signed shifting is not portable. */
        unsigned velocity=0x10+(at-8)/2;
        uint32_t value=ending_long(p,at)+(uint32_t)((int32_t)(int16_t)ending_word(p,velocity)*256);
        p[at]=(uint8_t)(value>>24);p[at+1]=(uint8_t)(value>>16);
        p[at+2]=(uint8_t)(value>>8);p[at+3]=(uint8_t)value;
    }
    if(p[0]==0xCD && --p[0x1E]==255) {
        /* Ani_objCD $AD7E:05,00,01,FF; six native ticks per frame. */
        p[0x1E]=5;p[0x1A]^=1;
    }
}
static void prepare_ending_scenery(int width,int read_only)
{
    if(read_only)return;
    if(width<=320 || !ending_scenery_owner()) {
        if(s_ending_scenery.armed)memset(&s_ending_scenery,0,sizeof s_ending_scenery);
        return;
    }
    s_ending_scenery.armed=1;
    for(unsigned i=0;i<ENDING_CONTINUATIONS;++i)
        if(s_ending_scenery.continued[i].active)ending_scenery_step(s_ending_scenery.continued[i].object);
    for(unsigned i=0;i<ENDING_OBJECTS;++i) {
        unsigned o=0xB000+i*64;uint8_t *old=s_ending_scenery.previous[i];
        const uint8_t *now=g_ram+o;
        int same=ending_terminal(now) && now[0]==old[0] &&
            abs((int16_t)ending_word(now,8)-(int16_t)ending_word(old,8))<=3 &&
            abs((int16_t)ending_word(now,12)-(int16_t)ending_word(old,12))<=3;
        int retired=0;
        if(ending_terminal(old) && !same) {
            if(old[0]==0xCB) {
                uint32_t next=ending_long(old,8)+(uint32_t)((int32_t)(int16_t)ending_word(old,0x10)*256);
                retired=(int16_t)(next>>16)<-32; /* Move, then signed AA94 predicate. */
            } else retired=ending_word(old,0x3C)==0; /* AB8E expiry precedes Move/Animate. */
        }
        if(retired)for(unsigned n=0;n<ENDING_CONTINUATIONS;++n) {
            EndingScenery *q=&s_ending_scenery.continued[n];if(q->active)continue;
            memcpy(q->object,old,64);q->active=1;
            /* The cloud's deletion tick already moved. The bird's native
             * deletion skipped movement/animation: continue that terminal
             * trajectory in presentation only, exactly once per publication. */
            ending_scenery_step(q->object);break;
        }
        if(ending_terminal(now) && object_queued(o))memcpy(old,now,64);
        else memset(old,0,64);
    }
}
static void draw_ending_scenery(SceneFrame *dst,uint32_t *keys,unsigned priority,int width,int read_only)
{
    if(width<=320 || !ending_scenery_owner())return;
    int left=-(width-320)/2;
    for(unsigned i=0;i<ENDING_CONTINUATIONS;++i) {
        EndingScenery *q=&s_ending_scenery.continued[i];const uint8_t *p=q->object;
        if(!q->active || (p[0x18]&7)!=priority)continue;
        unsigned map=ending_long(p,4),at=map+(int16_t)scene_read16(map+p[0x1A]*2u);
        unsigned count=scene_read16(at);int x=(int16_t)ending_word(p,8),right=INT_MIN;
        if(count>16){if(!read_only)q->active=0;continue;}
        for(unsigned n=0;n<count;++n) {
            unsigned piece=at+2+n*8;int dx=(int16_t)scene_read16(piece+6);
            int size=(((scene_read8(piece+1)>>2)&3)+1)*8;
            if(p[1]&1)dx=-dx-size;
            if(x+dx+size>right)right=x+dx+size;
        }
        if(right<=left){if(!read_only)q->active=0;continue;}
        /* Native Clouds/Animal2 patterns stay owned by this ending until
         * its credits clear. Copies retain both bird frames' map/art identity;
         * no deleted/reused object slot or future scene material is consulted. */
        add_mapping(dst,keys,0xF0000000u+(i<<16),map,p[0x1A],ending_word(p,2),
            (p[1]&3)|0x10004,x,(int16_t)ending_word(p,12),0,0);
    }
}
static void ending_birth_preview(SceneFrame *dst,uint32_t *keys,unsigned priority,
                                 const S2EndingBirth *births,unsigned count,int width)
{
    int left=-(width-320)/2,right=left+width;
    for(unsigned i=0;i<count;++i) {
        const S2EndingBirth *p=births+i;
        if(priority!=(p->id==0xCB?6u:2u))continue;
        int x=p->x-scroll_floor((int64_t)p->vx*p->tick,256);
        int y=scroll_floor((int64_t)p->y*256-(int64_t)p->vy*p->tick,256);
        if(x+64<=left || x-64>=right)continue;
        unsigned frame=p->frame;
        if(p->id==0xCD)frame=(unsigned)scroll_floor(-(int64_t)p->tick-1,6)&1u;
        /* Presentation-only scenery is clipped to the added margins. Native
         * positions, animation, allocation and random sequence stay untouched.
         * A birth-clock key remains stable as the lookahead distance shrinks. */
        uint32_t key=0xE0000000u|(((ram16(0xB132)+p->tick)&4095u)<<16)|
                     (p->id==0xCD?0x8000u:0);
        add_mapping(dst,keys,key,p->id==0xCB?0x3B32C:0x11E1C,frame,
                    p->id==0xCB?0x654F:0x594,0x10004,x,y,0,0);
    }
}
static void ending_plane_preview(SceneFrame *dst,uint32_t *keys,int width)
{
    if(width<=320 || g_ram[0xF600]!=0x20 || g_ram[0xB100]!=0xCA ||
       g_ram[0xB124]!=14 || ram16(0xB13C)>96)return;
    for(unsigned o=0xB000;o<0xD400;o+=64)
        if(g_ram[o]==0xCC && object_queued(o))return;
    /* ObjCC_Init starts at(-16,192), moving right1/up0.5 per tick.
     * Extend that very approach backwards from the native CA countdown.
     * The allocation tick(countdown0) is covered until CC actually publishes. */
    int ticks=ram16(0xB13C);
    add_mapping(dst,keys,0xFFF80000u,0x3AFF2,ram16(0xFF70)==2?4:0,0x8500,0x10004,
        -16-ticks,192+ticks/2,0,0);
}
static void wfz_robotnik_preview(SceneFrame *dst,uint32_t *keys,int width,int cam,int cy,int read_only)
{
    if(width<=320 || (g_ram[0xF600]&127)!=12 || !gameplay() || native_demo() || competition() ||
       !g_ram[0xF711] || s2_video_level_zone()!=0x600 || ram16(0xF660)!=2 ||
       (s_boss_preview_complete&4))return;
    unsigned root=0;
    for(unsigned o=0xB400;o<0xD000;o+=64)
        if(g_ram[o]==0xC5 && g_ram[o+0x24]==2 && (g_ram[o+0x25]==2 || g_ram[o+0x25]==4) &&
           g_ram[o+0x21] && ram16(o+8)==0x2B80 && scene_read32(0xFF0000u+o+4)==0x3CCD8){root=o;break;}
    if(!root)return;
    unsigned egg=ram16(root+0x38),platform=0;int egg_queued=0,platform_queued=0;
    if(egg>=0xB400 && egg<0xD000 && !(egg&63) && g_ram[egg]==0xC5 &&
       g_ram[egg+0x24]==16 && ram16(egg+0x2C)==root && scene_read32(0xFF0000u+egg+4)==0x3D0EE) {
        egg_queued=object_queued(egg);platform=ram16(egg+0x3E);
        if(platform>=0xB400 && platform<0xD000 && !(platform&63) && g_ram[platform]==0xC5 &&
           g_ram[platform+0x24]==18 && ram16(platform+0x2C)==egg &&
           scene_read32(0xFF0000u+platform+4)==0x3CEBC)platform_queued=object_queued(platform);
    }
    if(egg_queued && platform_queued) {if(!read_only)s_boss_preview_complete|=4;return;}
    /* Native CaseStart creates these children only at player proximity.
     * Their initial world poses already belong to a wide margin. Use exact
     * immutable owner-ROM Eggman art before its native boss PLC; a missing
     * decode skips the preview rather than exposing unrelated live tiles. */
    if(!native_art_ready(S2_NATIVE_ROBOTNIK_UPPER) || !native_art_ready(S2_NATIVE_ROBOTNIK_LOWER))return;
    if(!egg_queued)add_mapping(dst,keys,0xFFF90000u,0x3D0EE,0,0,0x30004,0x2C60-cam,0x4E6-cy,0,0);
    if(!platform_queued)add_mapping(dst,keys,0xFFFA0000u,0x3CEBC,0,0xA46D,0x10004,0x2C60-cam,0x50C-cy,0,0);
}
static void prepare_dez_corpse(SceneFrame *dst,uint32_t *keys,int width,int cam,int cy,int read_only)
{
    if(width<=320 || read_only)return;
    if((g_ram[0xF600]&127)!=12 || !gameplay() || competition() || native_demo() ||
       !g_ram[0xF711] || s2_video_level_zone()!=0xE00) {
        memset(&s_dez_corpse,0,sizeof s_dez_corpse);return;
    }
    unsigned root=s_dez_corpse.root;
    if(root && (g_ram[root]!=0xC7 || scene_read32(0xFF0000u+root+4)!=0x3E5F8))
        memset(&s_dez_corpse,0,sizeof s_dez_corpse);
    root=s_dez_corpse.root;
    if(root && g_ram[root+0x24]==0x20) {
        s_dez_corpse.active=1;return; /* Never read the deleted/reused head. */
    }
    for(unsigned o=0xB400;o<0xD000;o+=64) {
        if(g_ram[o]!=0xC7 || g_ram[o+0x24]!=2 || g_ram[o+0x25]!=14 ||
           g_ram[o+0x21] || g_ram[o+0x1A]!=3 || ram16(o+12)!=0x15C ||
           scene_read32(0xFF0000u+o+4)!=0x3E5F8 || !object_queued(o))continue;
        unsigned head=ram16(o+0x36);
        if(head<0xB400 || head>=0xD000 || (head&63) || g_ram[head]!=0xC7 ||
           g_ram[head+0x24]!=14 || g_ram[head+0x1A]!=2 || ram16(head+0x2C)!=o ||
           scene_read32(0xFF0000u+head+4)!=0x3E5F8 || !object_queued(head))continue;
        CorpsePresentation saved={0};saved.root=o;
        unsigned actors[]={o,head},first=dst->count;
        for(unsigned a=0;a<2;++a) {
            unsigned actor=actors[a],start=dst->count;
            add_mapping(dst,keys,0,0x3E5F8,g_ram[actor+0x1A],ram16(actor+2),g_ram[actor+1],
                (int16_t)(ram16(actor+8)-cam),(int16_t)(ram16(actor+12)-cy),0,0);
            for(unsigned n=start;n<dst->count && saved.count<16;++n) {
                saved.pieces[saved.count]=dst->sprites[n];
                saved.pieces[saved.count].x+=cam;saved.pieces[saved.count].y+=cy;
                saved.priority[saved.count++]=g_ram[actor+0x18]&7;
            }
        }
        if(dst->count-first<=16 && saved.count)s_dez_corpse=saved;
        dst->count=first;return;
    }
}
static void draw_dez_corpse(SceneFrame *dst,uint32_t *keys,unsigned priority,int width,int cam,int cy,int read_only)
{
    if(width<=320 || !s_dez_corpse.active || (g_ram[0xF600]&127)!=12 ||
       !g_ram[0xF711] || s2_video_level_zone()!=0xE00 || presented_competition())return;
    int left=view_left(cam,width),visible=0;
    for(unsigned n=0;n<s_dez_corpse.count;++n) {
        const SceneSprite *p=&s_dez_corpse.pieces[n];
        if(p->x+(((p->size>>2)&3)+1)*8>left && p->x<left+width &&
           p->y+((p->size&3)+1)*8>cy && p->y<cy+224)visible=1;
    }
    if(!visible) {if(!read_only)memset(&s_dez_corpse,0,sizeof s_dez_corpse);return;}
    for(unsigned n=0;n<s_dez_corpse.count && dst->count<SCENE_SPRITES;++n) {
        if(s_dez_corpse.priority[n]!=priority)continue;
        if(keys)keys[dst->count]=0xFFF70000u|n;
        dst->sprites[dst->count]=s_dez_corpse.pieces[n];
        dst->sprites[dst->count].x-=cam;dst->sprites[dst->count++].y-=cy;
    }
}
static void dez_window_preview(SceneFrame *dst,uint32_t *keys,int render_width,int cam,int cy)
{
    /* Loc39C12 initializes the monitor's closed shutter after the native
     * camera reaches320. Its level PLC art is already resident. Publish
     * that same frame only in margins until the real child is submitted;
     * allocation alone precedes DisplaySprite by one native frame. */
    if(render_width<=320 || (g_ram[0xF600]&127)!=12 || !gameplay() || native_demo() || presented_competition() ||
       !g_ram[0xF711] || !g_ram[0xB000] || g_ram[0xB024]>=6 ||
       s2_video_level_zone()!=0xE00 || g_ram[0xEEDF]>2 || cam>0x224)return;
    for(unsigned o=0xB400;o<0xD000;o+=64)
        if(g_ram[o]==0xAF && scene_read32(0xFF0000u+o+4)==0x3A08C && object_queued(o))return;
    add_mapping(dst,keys,0xFFF60000u,0x3A08C,4,0x378,0x10004,
        0x2C0-cam,0x139-cy,0,0);
}
static void parked_boss(SceneFrame *dst,uint32_t *keys,unsigned priority,
                        int render_width,int cam,int cy,int read_only)
{
    unsigned zone=ram16(0xFE10),cpz=zone==0xD01,bit=cpz?2:1;
    if(render_width<=320 || !gameplay() || native_demo() || (zone!=1 && !cpz) ||
       !g_ram[0xF711] || g_ram[0xEEDF]>6 || (s_boss_preview_complete&bit))return;
    unsigned root=0,id=cpz?0x5D:0x56,main_map=cpz?0x2ED8C:0x2FA58;
    for(unsigned o=0xB400;o<0xD000;o+=64)
        if(g_ram[o]==id && g_ram[o+0x24]==(cpz?2:6) && scene_read32(0xFF0000u+o+4)==main_map){root=o;break;}
    /* Allocation precedes DisplaySprite. Retire only when the real body is
     * submitted, so the init frame cannot punch a hole in a visible preview. */
    if(root && object_queued(root)) {
        if(!read_only)s_boss_preview_complete|=bit;
        return;
    }
    if(root) {
        for(unsigned o=0xB400;o<0xD000;o+=64) {
            unsigned map=scene_read32(0xFF0000u+o+4);
            if(g_ram[o]!=id || g_ram[o+0x18]!=priority || object_queued(o) ||
               !(map==main_map || (cpz && (map==0x2EADC || map==0x2EE88))))continue;
            add_mapping(dst,keys,(o<<16)|0x800u,map,g_ram[o+0x1A],ram16(o+2),g_ram[o+1],
                (int16_t)(ram16(o+8)-cam),(int16_t)(ram16(o+12)-cy),0,0);
        }
        return;
    }
    int x=(cpz?0x2B80:0x2AF0)-cam,y=(cpz?0x4B0:0x426)-cy;
    int left=view_left(cam,render_width)-cam;
    if(x+64<left || x-80>=left+render_width)return;
    if(cpz) {
        if(priority==2)add_mapping(dst,keys,0xFFF00000u,0x2EADC,0,0x2500,4,x,y,0,0);
        if(priority==3) {
            add_mapping(dst,keys,0xFFF10000u,0x2ED8C,0,0x2420,4,x,y,0,0);
            add_mapping(dst,keys,0xFFF20000u,0x2ED8C,1,0x420,4,x,y,0,0);
            add_mapping(dst,keys,0xFFF30000u,0x2EE88,0,0x418,4,x,y,0,0);
        }
        if(priority==4) {
            add_mapping(dst,keys,0xFFF40000u,0x2EADC,10,0x2500,4,x-16,y-56,0,0);
            add_mapping(dst,keys,0xFFF50000u,0x2EADC,11,0x2500,4,x-16,y-56,0,0);
            add_mapping(dst,keys,0xFFF60000u,0x2EADC,0,0x2500,4,x,y,0,0);
        }
    } else if(priority==2) {
        /* The native first suspension probes settle these wheels on the
         * parked terrain at y+9 and y+13; the chassis centre stays at$426. */
        add_mapping(dst,keys,0xFFF00000u,0x2FA58,4,0x2400,4,x+28,y+9,0,0);
        add_mapping(dst,keys,0xFFF10000u,0x2FA58,4,0x2400,4,x-12,y+13,0,0);
        add_mapping(dst,keys,0xFFF20000u,0x2FA58,1,0x2400,4,x-54,y+8,0,0);
    } else if(priority==3) {
        add_mapping(dst,keys,0xFFF30000u,0x2FA58,0,0x400,4,x,y,0,0);
        add_mapping(dst,keys,0xFFF40000u,0x2FA58,6,0x2400,4,x-44,y+13,0,0);
    }
}
static int ending_parked_player(unsigned o)
{
    if(o!=0xB000 || g_ram[0xF600]!=0x20 || g_ram[0xB100]!=0xCA ||
       ram16(0xF750)==2 || ram16(o+8)!=512 || ram16(o+12)!=0)return 0;
    /* sub_A58C parks the ordinary player outside H40 while the native
     * ending picture owns Sonic/Tails. That staging pose is not scenery. */
    for(unsigned p=0xB140;p<0xD000;p+=64)
        if(g_ram[p]==0xCC && g_ram[p+0x24]==2 &&
           g_ram[p+0x25]>=4 && g_ram[p+0x25]<=10)return 1;
    return 0;
}
static void capture_objects_into(SceneFrame *dst,int render_width,int read_only,uint32_t *keys)
{
    dst->count=0;if(!read_only){s_visible_count=0;++s_scene_tick;}
    dst->scene=presented_competition() && (g_ram[0xF711] || ram16(0xB0BE))?3:presented_gameplay() && g_ram[0xF711]?1:(g_ram[0xF600]&127)==16?2:0;
    int vs=presented_competition();
    if (vs) activation_bounds(ram16(0xEE20),render_width,&s_vs_other_lo,&s_vs_other_hi);
    int cam=ram16(vs?0xEE00:0xEEF0),cy=ram16(vs?0xEE04:0xEEF4);
    dst->camera_x=cam;dst->camera_y=cy;
    dst->native_hscroll=(int16_t)ram16(0xE000);dst->native_vscroll=ram16(0xF616);
    s2_runtime_published_actors(&dst->actors);
    for (unsigned p=0;p<S2_MAX_PLAYERS;++p) player_camera(p,&dst->views[p][0],&dst->views[p][1]);
    int campaign=s2_runtime_campaign_online();
    int runoff[2]={0,0};
    if(!vs && render_width>320)for(unsigned p=0;p<2;++p) {
        int x=ram16(0xB008+p*64);
        if(!read_only)runoff[p]=s2_video_runoff_x(p,0xB000+p*64,x)-x;
        else if(!native_demo() && s_runoff[p].active && s_runoff[p].zone==ram16(0xFE10))
            runoff[p]=(int)(s_runoff[p].position>>8)>x?(int)(s_runoff[p].position>>8)-x:0;
    }
    int pylon=!vs && g_ram[0xFE10]==13 &&
        g_ram[0xB340]==0x7C && g_ram[0xB364]>=2 && scene_read32(0xFFB344)==0x2103C;
    int vs_left[2]={0,0};
    if (vs) {vs_left[0]=view_left(cam,render_width);vs_left[1]=view_left(ram16(0xEE20),render_width);}
    int left=presented_gameplay()?view_left(cam,render_width):cam-(render_width-320)/2;
    if(presented_gameplay() && g_ram[0xF711]) {
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
    prepare_dez_corpse(dst,keys,render_width,cam,cy,read_only);
    prepare_ending_scenery(render_width,read_only);
    S2EndingBirth births[256];unsigned birth_count=0;
    if(render_width>320 && g_ram[0xF600]==0x20 && g_ram[0xB100]==0xCA &&
       ram16(0xF750)<=4 && !(ram16(0xF750)&1)) {
        birth_count=s2_ending_births(g_ram,(unsigned)(render_width-320)/2+64,births,256);
        /* CB/CD initialization intentionally does not submit DisplaySprite.
         * Bridge that one tick until their native first visible update. */
        if(g_ram[0xB134])for(unsigned o=0xB140;o<0xD000 && birth_count<256;o+=64) {
            const uint8_t *p=g_ram+o;
            if(object_queued(o) || p[0x24]!=2)continue;
            int cloud=p[0]==0xCB && ram16(o+8)==336 && !ram16(o+0x10);
            int bird=p[0]==0xCD && !p[0x25] && ram16(o+0x3C)==192;
            if(!cloud && !bird)continue;
            births[birth_count++]=(S2EndingBirth){0,o,p[0],p[0x1A],
                (int16_t)ram16(o+8),(int16_t)ram16(o+12),
                cloud?(int16_t)ram16(o+0x12):(int16_t)ram16(o+0x10),
                cloud?0:(int16_t)ram16(o+0x12)};
        }
    }
    for(unsigned pri=0;pri<8;++pri) {
        if(pri==3 && !vs)ending_plane_preview(dst,keys,render_width);
        if(pri==5 && !vs)wfz_robotnik_preview(dst,keys,render_width,cam,cy,read_only);
        draw_dez_corpse(dst,keys,pri,render_width,cam,cy,read_only);
        draw_ending_scenery(dst,keys,pri,render_width,read_only);
        ending_birth_preview(dst,keys,pri,births,birth_count,render_width);
        if(!vs)parked_boss(dst,keys,pri,render_width,cam,cy,read_only);
        if(pri==6 && !vs)dez_window_preview(dst,keys,render_width,cam,cy);
        /* Obj7C is a screen-space foreground strip, not a centered menu.
         * Its native 1024px camera cycle moves 768px. Keep the original
         * nine-piece map even during its native off-screen submission gap. */
        if(pri==7 && pylon)
            add_mapping(dst,keys,0xB3400000u,0x2103C,0,ram16(0xB342),0x4000,384,128,0,0);
        unsigned queue=0xAC00+pri*128,bytes=ram16(queue);if(bytes>126)bytes=126;
        for(unsigned k=2;k<=bytes;k+=2) {
            unsigned o=ram16(queue+k);if(o<0xB000 || o>=0xD400 || !g_ram[o])continue;
            if(pylon && o==0xB340)continue;
            unsigned flags=g_ram[o+1],multi=flags&64,space=flags&12;
            if(ending_parked_player(o))flags|=0x8000;
            if(g_ram[o]==0x3A && g_ram[o+0x1A]<=2 && custom_result_name())continue;
            if (s2_runtime_knuckles_item(o)) flags|=0x100;
            if (s2_runtime_amy_item(o)) flags|=0x2000;
            if (s2_runtime_shadow_star(o)) flags|=0x200;
            int x,y,anchor=0;
            if(space || multi) {
                x=(int16_t)(ram16(o+8)-cam);
                int visible_x=x;
                if(!vs) {
                    if(o==0xB000 || o==0xB040)x+=runoff[o==0xB040];
                    else if((g_ram[o]==5 || g_ram[o]==0x38 || g_ram[o]==0x35) &&
                            (ram16(o+0x3E)==0xB000 || ram16(o+0x3E)==0xB040))
                        x+=runoff[ram16(o+0x3E)==0xB040];
                }
                y=(int16_t)(ram16(o+12)-cy);
                if(!vs && (g_ram[o]==0x38 || g_ram[o]==0x35)) {
                    unsigned parent=ram16(o+0x3E),role=parent==0xB040;
                    if((parent==0xB000 || parent==0xB040) && s_runoff[role].active &&
                       g_ram[parent+0x16]<19)y-=19-g_ram[parent+0x16];
                }
                int radius=g_ram[o+(multi?0xE:0x19)],visible_y=1,visible_horizontal=1;
                if(!campaign && !vs) {
                    /* Cosmetic runoff must not change the native on-screen
                     * bit or AI activation when its drawing leaves the view. */
                    visible_horizontal=visible_x+radius>=left-cam && visible_x-radius<left-cam+render_width;
                    if(!visible_horizontal && render_width<=320)continue;
                    if(flags&16){int h=g_ram[o+(multi?0x14:0x16)];visible_y=y+h>=0 && y-h<224;}
                    else {y=((y+128)&2047)-128;visible_y=y>=-32 && y<256;}
                    /* Native root bounds are approximate (a starpost reaches
                     *40px above its origin; a jet reaches44px sideways).
                     *In a wider presentation retain
                     *actual queued pieces until their own bounds leave the
                     *picture. The native on-screen/AI flag still follows the
                     *original root check; no object is loaded or activated. */
                    if(!visible_y && render_width<=320)continue;
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
                /* Dormant WFZ lasers use on_screen as their activation
                 * trigger (loc3BAD2). A margin preview must not start their
                 * 400px entrance before the original viewport reaches them. */
                int native_trigger=dormant_wfz_laser(o);
                if(!read_only && visible_horizontal && visible_y && (!native_trigger ||
                   (visible_x+radius>=0 && visible_x-radius<320)) && s_visible_count<144)
                    s_visible_objects[s_visible_count++]=o;
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
                unsigned routine=g_ram[o+0x24];
                int result_entry=g_ram[o]==0x3A && (routine==2 || routine==4 ||
                    routine==6 || routine==8 || routine==0x16);
                if (g_ram[o]==0x34 || result_entry) {
                    if (g_ram[o]==0x34 && g_ram[o+0x1A]==0x16)
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
            unsigned actor=0;
            for(unsigned p=0;p<S2_MAX_PLAYERS;++p) {
                unsigned root=dst->actors.object[p];
                if(root && (o==root ||
                   ((g_ram[o]==5 || g_ram[o]==0x38 || g_ram[o]==0x35) && ram16(o+0x3E)==root))) {
                    actor=p+1;break;
                }
            }
            if(dormant_wfz_laser(o))flags|=0x8000;
            /* This is the waiting Eggman in DEZ's next room, not the
             * monitor actor or his door. Keep his native pixels and all
             * visibility/AI bookkeeping above, but do not preview him in
             * margins before his original proximity routine starts him. */
            if(render_width>320 && !vs && (g_ram[0xF600]&127)==12 && s2_video_level_zone()==0xE00 &&
               g_ram[o]==0xC6 && g_ram[o+0x24]==2 && g_ram[o+0x25]==2 &&
               g_ram[o+0x1A]==0 && map==0x3D0EE && ram16(o+8)==0x440 && ram16(o+12)==0x168)
                flags|=0x8000;
            /* Chemical Plant and Aquatic Ruin use separate Obj04 maps.
             * Both use two surface objects to form a 384-pixel cycle. */
            if(!vs && g_ram[o]==4 && (map==0x20A0E || map==0x20AFE))flags|=0x1000;
            if(g_ram[o]==0x3A || g_ram[o]==0x34 || g_ram[o]==0x0A)flags|=0x400;
            /* Playable actors use priority 2. Native world objects in the
             * earlier queues, including Grabber legs, must stay in front. */
            if(pri<2 && (space || multi))flags|=0x800;
            /* Keep native visibility bookkeeping above, but draw continued
             * actors from decoded art: offscreen DPLCs no longer animate. */
            if(!vs && ((o==0xB000 && s_runoff[0].active) ||
                       (o==0xB040 && s_runoff[1].active)))continue;
            unsigned first=dst->count;
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
            for(unsigned n=first;n<dst->count;++n)dst->sprites[n].actor=(uint8_t)actor;
        }
    }
}

static void capture_objects(void)
{
    /* Expanded presentation must not mark offscreen demo objects visible:
     * that bit also controls native AI and the outgoing display queue. */
    int read_only=native_demo() || outgoing_level();
    if(read_only)s_visible_count=0;
    capture_objects_into(&s_build,s_requested_width,read_only,
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
    f->actors=s_native_build.actors;
    f->native_hscroll=s_native_build.native_hscroll;f->native_vscroll=s_native_build.native_vscroll;
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
    dst->actors=s_build.actors;
    dst->native_hscroll=s_build.native_hscroll;dst->native_vscroll=s_build.native_vscroll;
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
    /* Credits clear the object owner before replacing its patterns and may
     * stop BuildSprites. Retire that presentation at the next existing guest
     * hook, rather than waiting for another object publication or scanline. */
    if(s_ending_scenery.armed && !ending_scenery_owner())
        memset(&s_ending_scenery,0,sizeof s_ending_scenery);
    int demo=(g_ram[0xF600]&127)==8;
    switch (pc) {
    case 0x8C92:
        /* MenuScreen has decoded MapEng_MenuBack, before its temporary RAM
         * is reused for foreground cards. Retain an identity for that plane. */
        for(unsigned i=0;i<40;++i)s_menu_background_row[i]=ram16(i*2);
        s_menu_background_ready=1;
        break;
    case 0x5300:
        /* Half-pipe results reuse the level V-int, including its tile
         * streaming. Retire pending world redraws before ClearScreen, so
         * that first results V-int cannot repopulate the cleared planes. */
        if ((g_ram[0xF600]&127)==16) {
            memset(g_ram+0xEE50,0,16); /* both players' source scroll flags */
            memset(g_ram+0xEEA0,0,16); /* pending V-int/H-int copies */
        }
        break;
    case 0x4450: s_donor_fade=(DonorFade){0}; /* fresh level retires SS publications */
        /* Results select the next zone before the old palette fades out.
         * Its scenery and cosmetic continuations still belong to this level. */
        s_level_zone=ram16(0xFE10);s_level_zone_valid=1;s_boss_preview_complete=0;
        memset(&s_dez_corpse,0,sizeof s_dez_corpse);
        memset(&s_ending_scenery,0,sizeof s_ending_scenery);
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
            /* BuildSprites counts queues 0..7 with D7=7..0 in both views.
             * DisplaySprite3 overrides the nominal object priority; A4 is
             * temporarily a child mapping pointer for multipart objects. */
            else if(o>=0xB000 && o<0xD800 && !(o&63) && slot<80)
                s_front_build.kind[p][slot]=g_ram[o]==0x3A?2:
                    (g_ram[o]==0x34 || g_ram[o]==0x0A)?3:
                    ((uint16_t)g_cpu.D[7]>=6 && (uint16_t)g_cpu.D[7]<=7 &&
                     (g_ram[o+1]&0x4C))?4:0;
            /* Mode2 cells are8x16: each index addresses two ordinary tiles. */
            if (competition) tile*=2;
            const NativeObjectArt *binding=NULL;
            if(o>=0xB000 && o<0xD800 && !(o&63))
                binding=native_object_binding(o,scene_read32(0xFF0000u+o+4),ram16(o+2));
            if(binding && tile>=binding->base && tile<binding->base+binding->tiles)
                g_ram[next-6]|=0xC0;
            if (s2_runtime_knuckles_item(o) && knuckles_item_tile(tile)) g_ram[next-6]|=0x80;
            if (amy_item_tile(tile) && s2_runtime_amy_item(o)) g_ram[next-6]|=0x80;
            if (s2_runtime_knuckles_item(o) && knuckles_palette_tile(tile)) g_ram[next-6]|=GVDP_SPRITE_PALETTE_ONLY;
            if (s2_runtime_shadow_star(o) && tile>=0x4DE && tile<0x500) g_ram[next-6]|=0xC0;
        }
        return 0;
    }
    if(pc==0x17AA4 && !g_ram[0xF76C]) {
        s_placement_base=s_placement_count=0;s_loader_active=0;
        memset(s_active_cells,0,sizeof s_active_cells);s_streaming_pressure=0;
    }
    /* The native demo fade changes mode before continuing RunObjects and
     * BuildSprites for sixty ticks. Keep its displayed mapping format and
     * publication alive, while all loading and CPU timing remain native. */
    if(enabled() && outgoing_level()) {
        if(pc==0x16604)capture_objects();
        if(presented_competition()?(pc==0x16B90 || pc==0x16B98):
                                  (pc==0x16712 || pc==0x1671A))publish_sprites();
        return 0;
    }
    /* Attract inputs target the original object timing and fixed 2P pools.
     * Keep wide presentation, but let the native loaders/cullers run. */
    if(!demo && pc==0x17B84 && gameplay() && (enabled() || s_loader_active)) {spawn_scene();return 1;}
    if (!demo && competition() && s_loader_active && pc==0x17CCC) {spawn_scene();return 1;}
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
            if(demo)return 0;
            write8(0xF76C,4);spawn_scene();return 1;
        case 0x16604:capture_objects();return 0;
        case 0x16B90:case 0x16B98:publish_sprites();return 0;
        case 0x170D0:
            if(!demo){g_cpu.A[1]=0xFFFFE806u;g_cpu.A[2]=0xFFFF0000u|rings_end();}
            return 0;
        default:return 0;
        }
    }
    switch(pc) {
    case 0x17AA4:return 0;
    case 0x16604:capture_objects();return 0;
    case 0x16712:case 0x1671A:publish_sprites();return 0;
    case 0x367A0: {
        /* Obj_DeleteBehindScreen uses BMI on its own original left-cell
         * subtraction. SCZ enemies may still be visible in the wider left
         * margin. Change only the N flag that this branch consumes. */
        if(!gameplay() || demo || competition())return 0;
        unsigned o=g_cpu.A[0]&65535u;
        int lo,hi;activation_bounds(ram16(0xEE00),s_requested_width,&lo,&hi);
        int behind=(ram16(o+8)&~127)<lo;
        if(s2_runtime_campaign_online())behind=!active_x(ram16(o+8));
        g_cpu.SR=(uint16_t)((g_cpu.SR&~8u)|(behind?8u:0u));
        return 0;
    }
    case 0x170D0:
        /* Use the game's original collision/reward/consumption routines for
         * both Sonic and Tails. Only lift the old camera's ring search gate. */
        if(gameplay() && !demo){g_cpu.A[1]=0xFFFFE806u;g_cpu.A[2]=0xFFFF0000u|rings_end();}
        return 0;
    case 0x2ABAC: {
        /* Obj85 keeps its culling distance in D1. D0 remains the native
         * display parameter used after this comparison. */
        if(!gameplay() || demo)return 0;
        int x=(uint16_t)((uint16_t)g_cpu.D[1]+ram16(0xF7DA));
        int lo,hi;activation_bounds(ram16(0xEE00),s_requested_width,&lo,&hi);
        g_cpu.D[1]=(g_cpu.D[1]&0xFFFF0000u)|(scene_contains(x,lo,hi)?320u:641u);
        return 0;
    }
    case 0x16F16:case 0x16F3E: {
        if(!gameplay() || demo)return 0;
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
            if(gameplay() && !demo) {
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
void s2_video_preserve_foreground_row(const uint32_t *out,int width)
{
    if(enabled() || !out || width<=0)return;
    unsigned world=s_front_build.world_mask;
    for(unsigned n=0;n<3;++n)world|=s_front_history[n].world_mask;
    for(unsigned p=0;p<2;++p)world|=s_front_display[p].world_mask;
    /* Preserve the composed native colour, including high planes, private
     * palettes and shading; an occluded foreground sprite still prevents
     * the sprite behind it from becoming visible through that plane. */
    if(world && reserve_sprite_row(width))
        memcpy(s_front_rgb,out,(size_t)width*sizeof *out);
}
static int scene_scroll_distance(const GVDP *v,const SceneFrame *f)
{
    if(f->scene!=1)return 0;
    unsigned base=(v->reg[13]&63u)<<10;
    int dx=(int16_t)(f->native_hscroll-vram16(v,base));
    int dy=((f->native_vscroll-v->vsram[0]+512)&1023)-512;
    return abs(dx)+abs(dy);
}
static void select_scene(const GVDP *v)
{
    s_display=NULL;
    int distance=INT_MAX;
    unsigned sat=(v->reg[5]&127u)<<9;
    /* Keep the front buffer's publication alive even if the producer cycles
     * through every history slot before the next native sprite DMA. */
    for(unsigned n=0;n<4;++n) {
        const SceneFrame *p=n==3?&s_display_frame:&s_history[n];
        if(!p->serial)continue;
        int matches=1;
        for(unsigned k=0;k<640;++k)
            if(p->sat[k]!=v->vram[(sat+k)&65535u]) {matches=0;break;}
        if(matches) {
            int score=scene_scroll_distance(v,p);
            if(score<distance || (score==distance && (!s_display || p->serial>s_display->serial))) {
                s_display=p;distance=score;
            }
        }
    }
    if(!s_display) {
        ++s_scene_misses;
        int scene=presented_competition() && (g_ram[0xF711] || ram16(0xB0BE))?3:presented_gameplay() && g_ram[0xF711]?1:(g_ram[0xF600]&127)==16?2:0;
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
        s_display_frame.actors=s_display->actors;
        s_display_frame.native_hscroll=s_display->native_hscroll;s_display_frame.native_vscroll=s_display->native_vscroll;
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
        int foreground=!!(s->hud&SCENE_FOREGROUND_SCREEN);
        int world=!foreground && !anchor && (s_display->scene==1 || s_display->scene==3);
        int repeat=foreground || ((s->hud&SCENE_WATER_REPEAT) && world &&
            (width>320 || s_frame_campaign_view));
        int period=foreground?768:384;
        int cell_height=(v->reg[12]&6)==6?16:8;
        int sx=s->x,sy=s->y;
        if(world && s_interpolating)interpolation_sprite(n,&sx,&sy);
        int y=foreground?sy-(int)(((unsigned)s_camera_y>>1)&63):
            sy+(world?s_display->camera_y-s_camera_y:0);
        if(line<y || line>=y+ch*cell_height)continue;
        int base=sx+(anchor==1?0:anchor==2?(width-320)/2:origin);
        if(foreground) {
            unsigned camera=(unsigned)(s_left+(width-320)/2)&1023;
            base=sx+(width-320)/2-(int)((camera>>1)+(camera>>2));
        }
        /* Native camera scroll may advance while a completed host frame is
         * retained. Reproject its world coordinates, not its old screen
         * coordinates; otherwise stationary rings jitter with the camera. */
        if(world)base+=s_display->camera_x-s_camera;
        int iy=line-y;
        if(s->attr&0x1000)iy=ch*cell_height-1-iy;
        for(int i=0;i<cw*8;++i) {
            int x=base+i;
            /* Clip only the dormant story actor's added margins. The live
             * viewport origin includes clamping and each campaign view, so
             * even a partially entering body preserves its native center. */
            if((s->art&SCENE_ART_NATIVE_VIEW) && (x<origin || x>=origin+320))continue;
            if((s->art&SCENE_ART_MARGIN_VIEW) && (width<=320 || (x>=origin && x<origin+320)))continue;
            if(repeat) {x%=period;if(x<0)x+=period;}
            else if(x<0 || x>=width || (s_priority[x]&2))continue;
            /* Stage objects may be queued before their title PLC finishes.
             * Their native offscreen positions become visible only in added
             * margins. Respect the displayed opaque card there, including
             * high-priority Tornado pieces, without changing native pixels,
             * screen-space card glyphs/HUD, or the revealed stage. */
            int card_origin=(width-320)/2;
            if(width>320 && !anchor && !foreground &&
               (x<card_origin || x>=card_origin+320) && (s_priority[x]&ROW_CARD_COVERED))continue;
            int ix=(s->attr&0x800)?cw*8-1-i:i;
            uint16_t cell=(uint16_t)((s->attr&0xE000u)|
                (((s->attr&2047u)+(unsigned)(ix/8*ch+iy/cell_height))&2047u));
            uint32_t custom=0;
            int knux=anchor==1 && s2_video_knuckles_hud_pixel(v,cell,ix,iy,&custom);
            uint8_t p=knux?(custom!=0):native_scene_pixel(v,s->hud,cell,ix,iy,width);
            unsigned object_art=s->art&~(SCENE_ART_NATIVE_VIEW|SCENE_ART_MARGIN_VIEW);
            if(!knux && object_art) {
                unsigned asset=object_art-1;
                const NativeObjectArt *binding=native_asset_binding(asset);
                unsigned base=binding?binding->base:asset==S2_NATIVE_OOZ_BLOCK_VERT?0x332:
                    asset==S2_NATIVE_OOZ_BLOCK_HORIZ?0x3FF:asset==S2_NATIVE_CNZ_PLUNGER_VERT?0x422:
                    asset==S2_NATIVE_CNZ_PLUNGER_DIAG?0x402:asset==S2_NATIVE_ROBOTNIK_UPPER?0x500:
                    asset==S2_NATIVE_ROBOTNIK_LOWER?0x564:asset==S2_NATIVE_DEZ_WINDOW?0x378:0x39D;
                p=native_asset_pixel(v,asset,base,cell,ix,iy,width);
            }
            if (s->size&0x80) {
                unsigned bytes=0;const uint8_t *patterns=gvdp_sprite_patterns(s->size,&bytes);
                unsigned at=(cell&2047)*((v->reg[12]&6)==6?64:32)+(iy&(((v->reg[12]&6)==6)?15:7))*4+(ix&7)/2;
                if (patterns && at<bytes) p=(ix&1)?patterns[at]&15:patterns[at]>>4;
                p=(uint8_t)((cell>>9&48)|p); /* native palette, including water/fades */
                if (!(p&15)) p=0;
            }
            if(!p)continue;
            /* This immutable native window is independent of character and
             * special-stage pattern banks, including their palette maps. */
            int extra=object_art==1+S2_NATIVE_DEZ_WINDOW?0:
                gvdp_sprite_palette_line(s->size,cell&2047,(cell>>13)&3);
            if (extra) {
                custom=genesis_dac_cram_to_argb(gvdp_ext_color(line,extra,p&15),GENESIS_DAC_NORMAL);
                knux=1;
            }
            /* Native boss hits toggle palette1 ink1. Its capsule is normally
             * outside that view; wider presentation retains its black ink. */
            if((s->hud&SCENE_ART_MASK)==SCENE_ART_CAPSULE && p==17 && v->cram[17]==0xEEE) {
                custom=genesis_dac_cram_to_argb(s2_video_donor_color(0),GENESIS_DAC_NORMAL);
                knux=1;
            }
            for(;x<width;x+=period) {
                if(!(s_priority[x]&2)) {
                    s_priority[x]|=2;
                    if(anchor || (s->attr&0x8000) || !(s_priority[x]&1))
                        out[x]=knux?custom:palette[p];
                    if((s->hud&192) && s_front_rgb){s_priority[x]|=4;s_front_rgb[x]=out[x];}
                }
                if(!repeat)break;
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
        int replace=!native_demo() && strcmp(s2_party.roster.character[p],p?"tails":"sonic")!=0;
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
    int world=s_vs_world,slot_skin=slot_theme(p);
    const uint32_t *slot_palette=slot_skin?slot_colors():NULL;
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
    uint16_t card_ink=s_frame_card?s2_runtime_title_card_ink():0;
    int bg_base=-origin-hs_b;
    int cloud_band=world && s_frame_zone==0 && y>=22 && y<102;
    for(int x=0;x<width;++x) {
        int wx=left+x,wy=plane_y,bx=x+bg_base;
        if(bg_width){bx%=bg_width;if(bx<0)bx+=bg_width;}
        uint16_t a,b;unsigned ap,bp;
        if(world) {
            a=world_attr_2p(s_world_frame,wx,wy,0);ap=pattern_pixel(&s_video_frame,a,wx,wy);
            unsigned flip=0;int source=cloud_band?ehz_cloud_x(bx,&flip):bx;
            b=world_attr_2p(s_world_frame,source,by,1)^(uint16_t)flip;bp=pattern_pixel(&s_video_frame,b,bx,by);
        } else {
            int ax=x-margin-hs_a,ay=line+v->vsram[0],py=line+v->vsram[1];
            a=plane_attr(v,base_a,ax,ay);ap=pattern_pixel(v,a,ax,ay);
            if(x<margin || x>=margin+nw)ap=0;
            b=plane_attr(v,base_b,x-margin-hs_b,py);bp=pattern_pixel(v,b,x-margin-hs_b,py);
        }
        if(!ready) {
            int ax=(int)((int64_t)x*nw/width)-hs_a,ay=line+v->vsram[0];
            uint16_t card=plane_attr(v,base_a,ax,ay);
            if(card_fill((card&0xF800u)|((card&2047u)*2u))) {a=card;ap=pattern_pixel(v,a,ax,ay);}
        }
        int front=ap && ((a&0x8000) || !bp || !(b&0x8000));
        unsigned index=front?ap:bp?bp:v->reg[7]&63;
        out[x]=palette[index];
        unsigned material=front?a:b;
        if(slot_palette && (ap || bp) && !(material&0x6000) &&
           (unsigned)((material&2047u)-0x3A8u)<24)out[x]=slot_palette[index&15];
        s_priority[x]=(uint8_t)((ap && (a&0x8000)) || (bp && (b&0x8000)));
    }
    s_camera=camera;s_camera_y=cy;s_left=left;s_native_x=origin;
    if(s_display)draw_scene_sprites(&s_video_frame,y,out,width,world?origin:margin,palette);
    if(!ready && !s_display) {
        if(!native)native=native_row(native_ctx);
        memcpy(out+margin,native,(size_t)nw*sizeof *out);
    } else if(ready)competition_hud_sprites(v,line,out,width,palette);
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
        slot_patterns(&s_video_frame,line);
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
        s_frame_zone=s2_video_level_zone()>>8;
        s_frame_act=s2_video_level_zone()&255;
        s_frame_resize=g_ram[0xEEDF];
        s_frame_card=0;s_frame_card_leaving=ram16(0xB0BE)!=0;
        /* Title mode is selected before the outgoing SEGA raster fades.
         * Keep its composition until the displayed VDP setup is replaced. */
        s_frame_sega=width>nw && (g_ram[0xF600]==0 || g_ram[0xF600]==4) && g_ram[0xB040]==0xB0 &&
            v->reg[2]==0x30 && v->reg[4]==5;
        if (width>nw && ((g_ram[0xF600]&127)==12 || (g_ram[0xF600]&127)==8) && !ram16(0xFFD8) &&
            (v->vram[0xBB40]==0x33 || v->vram[0xBB40]==0x44) &&
            v->vram[0xBB7F]==v->vram[0xBB40])
            for (unsigned o=0xB080;o<=0xB1C0;o+=64)
                if (g_ram[o]==0x34) s_frame_card=1;
        s_frame_fg_x=ram16(0xEE60);s_frame_fg_y=ram16(0xEE64);s_frame_bg_y=ram16(0xEE6C);
        capture_background_scroll(s_render);
        /* Game_Mode changes before departure fades. The displayed name table
         * owns the backdrop until the incoming scene actually replaces it. */
        s_render->frame_menu_background=s_menu_background_ready &&
            ((v->reg[4]&7u)<<13)==0xE000 && (v->reg[16]&3)==1;
        for(unsigned i=0;s_render->frame_menu_background && i<40;++i)
            if(vram16(v,0xE000+i*2)!=s_menu_background_row[i])
                s_render->frame_menu_background=0;
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
    int slot_views=slot_theme(0) || (presented_competition() && slot_theme(1));
    if(slot_views && s_frame_vs && line==224 && !s_interpolating) {
        memcpy(&s_video_frame,v,sizeof s_video_frame);
        slot_patterns(&s_video_frame,line);
    }
    if(competition_scanline(slot_views && s_frame_vs?&s_video_frame:v,line,native,nw,out,width,native_row,native_ctx,palette))return;
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
    /* Normal SwScrl_HTZ never streams its background: DrawInitialBG owns
     * the authored512x256 PlaneB domain at source(0,0). Distant quake cells
     * share the layout but are not part of that repeating normal scene.
     * EEBC selects the source scroll branch; resize8 occurs in both scenes. */
    int normal_htz=bg_world && s_frame_zone==7 && !s_render->frame_htz_shake;
    if(normal_htz)by=(int)((unsigned)by&255u);
    int bg_width=0;
    if(bg_world) {
        if(normal_htz)bg_width=512;
        else {
            int row=((by>>7)&15)*256;
            for(int col=0;col<128;++col)if(s_world_frame[0x8080+row+col])bg_width=(col+1)*128;
        }
    }
    uint16_t card_ink=s_frame_card?s2_runtime_title_card_ink():0;
    int bg_base=-origin-hs_b;
    /* Native SwScrl supplies each row's actual motion and animation. Rebase
     * only its camera-dependent term to this viewport's foreground origin.
     * A clamped wide camera then keeps its own parallax instead of inheriting
     * the moving native viewport's foreground-speed translation. */
    if(bg_world && width>nw) {
        bg_base=background_view_base(s_render,native_camera,s_left,line,by,hs_b);
        if(s_interpolating)bg_base=interpolation_background_base(line);
    }
    int background_x=bg_base;
    if(bg_width>0) {
        background_x%=bg_width;
        if(background_x<0)background_x+=bg_width;
    }
    GenesisTileRow fg = {0}, bg = {0};
    int slot_skin=slot_theme(0);
    const uint32_t *slot_palette=slot_skin?slot_colors():NULL;
    int fg_slot=0,bg_slot=0;
    int cloud_band=bg_world && s_frame_zone==0 && line>=22 && line<101;
    for(int x=0;x<width;++x) {
        int nx=x-origin,wx=level?nx+camera:nx-hs_a;
        int reference_bx=nx-hs_b,bx=bg_base+x;
        /* MenuScreen loads 40 columns, ending partway through its six-column
         * lettering pattern. Continue that pattern beyond the populated map;
         * repeating the 320-pixel crop would join two incomplete words. Keep
         * every original viewport cell, including its palette attributes. */
        if (!level && s_render->frame_menu_background) {
            if (bx<0 || bx>=320) { bx%=48; if (bx<0) bx+=48; bx+=48; }
        }
        /* A margin-only chunk must not carry its tile row into native pixels
         * when a clamped viewport origin is not aligned to an8px tile. */
        if(level && width>320 && s_frame_zone==12 && (nx==0 || nx==nw))fg.remaining=0;
        if(!fg.remaining) {
            uint16_t attr=level?foreground_attr(s_world_frame,wx,wy,width>320 && (nx<0 || nx>=nw),s_frame_zone,s_frame_act):
                plane_attr(v,base_a,nx-hs_a,line+(v->vsram[0]&1023));
            gtile_row_begin(&fg,v->vram,attr,level?wx:nx-hs_a,level?wy:line+(v->vsram[0]&1023));
            fg_slot=slot_skin && slot_material(v,attr);
        }
        if(bg_world && (s_frame_zone==16 || s_frame_zone==6) && (nx==0 || nx==nw))bg.remaining=0;
        if(!bg.remaining) {
            unsigned flip=0;int source=cloud_band?ehz_cloud_x(width>nw?bg_base+x:background_x,&flip):background_x;
            /* Reflect SCZ's clipped composition as one continuous wide
             * background. Switching between wrapping and reflection at the
             * moving native subviewport would introduce another cloud seam.
             * Native-width output retains its original sampling. */
            if(bg_world && width>nw && s_frame_zone==16 && bg_width>0)
                source=reflected_cloud_x(bg_base+x,bg_width,&flip);
            if(bg_world)
                source=wfz_background_source(bg_base+x,source,by,-hs_b,nw,
                    width>nw && (nx<0 || nx>=nw),s_frame_zone);
            uint16_t attr=bg_world?(background_attr(s_world_frame,source,by,bg_base+x,
                width>nw && (nx<0 || nx>=nw),s_frame_zone,s_frame_resize)^(uint16_t)flip):plane_attr(v,base_b,bx,by);
            gtile_row_begin(&bg,v->vram,attr,bg_world?background_x:bx,by);
            bg_slot=slot_skin && slot_material(v,attr);
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
        if(!level && (g_ram[0xF600]&127)==0x20 && (nx<0 || nx>=nw)) {
            /* Ending maps populate one viewport. Reflect its background
             * composition into the margins: stars/cloud bands keep their
             * live scrolling instead of stretching one edge into stripes.
             * Foreground photographs and credit text stay centered. */
            int sample=nx%(nw*2);if(sample<0)sample+=nw*2;
            if(sample>=nw)sample=nw*2-1-sample;
            int source=sample-hs_b;
            b=plane_attr(v,base_b,source,by);bp=pattern_pixel(v,b,source,by);
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
        int front=ap && ((a&0x8000) || !bp || !(b&0x8000));
        /* This is the actual composed card coverage, with the selected row's
         * scroll, resampling and publication phase already applied. Its
         * native interval remains centered even when the incoming gameplay
         * camera has started clamping at the left edge. */
        s_priority[x]=(uint8_t)((((a&0x8000)&&ap)||((b&0x8000)&&bp))|
            (s_frame_card && front && card_fill(a)?ROW_CARD_COVERED:0));
        uint8_t p=front?ap:bp;
        out[x]=p?palette[p]:backdrop;
        if(slot_palette && p && (front?(fg_slot && a==fg.attr):bg_slot))out[x]=slot_palette[p&15];
        if (card_ink && s_frame_card && (a&2047)>=0x5DA && (a&2047)<=0x5DB && (ap==3 || ap==4)) {
            uint32_t color=genesis_dac_cram_to_argb(card_ink,GENESIS_DAC_NORMAL);
            unsigned gain=palette[ap]&255;
            out[x]=0xFF000000u|((((color>>16)&255)*gain/255)<<16)|
                ((((color>>8)&255)*gain/255)<<8)|((color&255)*gain/255);
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
        if((g_ram[0xF600]&127)==0x20 && s_display)
            draw_scene_sprites(v,line,out,width,origin,palette);
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
    S2RuntimePresentation party;
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
static int s_lerp_sprite[SCENE_SPRITES];

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
    int distance=INT_MAX;
    unsigned sat=(v->reg[5]&127u)<<9;
    for(unsigned i=0;i<4;++i) {
        const SceneFrame *p=i==3?&s_native_display:&s_native_history[i];
        if(!p->serial)continue;
        unsigned k=0;
        for(;k<640 && p->sat[k]==v->vram[(sat+k)&65535u];++k){}
        if(k==640) {
            int score=scene_scroll_distance(v,p);
            if(score<distance || (score==distance && (!best || p->serial>best->serial))) {
                best=p;distance=score;
            }
        }
    }
    if(best && best!=&s_native_display) {
        unsigned count=best->count;
        memcpy(s_native_display_keys,s_native_keys[best->serial%3],count*sizeof(uint32_t));
        s_native_display.count=count;s_native_display.serial=best->serial;
        s_native_display.scene=best->scene;s_native_display.camera_x=best->camera_x;
        s_native_display.camera_y=best->camera_y;
        s_native_display.actors=best->actors;
        s_native_display.native_hscroll=best->native_hscroll;s_native_display.native_vscroll=best->native_vscroll;
        memcpy(s_native_display.views,best->views,sizeof best->views);
        memcpy(s_native_display.sat,best->sat,sizeof best->sat);
        memcpy(s_native_display.sprites,best->sprites,count*sizeof(SceneSprite));
        best=&s_native_display;
    }
    return best;
}
int s2_video_presented_actors(const GVDP *v,S2RuntimeSprite *sprites,S2RuntimeSprite *dust)
{
    /* The body and its native shield must follow the same completed SAT,
     * even if the object producer has already advanced to the next tick. */
    const SceneFrame *f=enabled()?s_display:native_select_scene(v);
    if(!f || !f->serial || f->scene!=1)return 0;
    memcpy(sprites,f->actors.sprites,sizeof f->actors.sprites);
    memcpy(dust,f->actors.dust,sizeof f->actors.dust);
    return 1;
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
        /* Native interlace, online campaigns and transition screens
         * retain ordinary completed pictures until an adapter is qualified. */
        if(width<native_w || native_w!=320 || g_ram[0xF600]!=12 ||
           !g_ram[0xF711] || ram16(0xF63A) || competition() ||
           (v->reg[12]&14) || !(v->reg[1]&64) ||
           s2_runtime_campaign_online())return;
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
            capture_background_scroll(&f->render);
            f->render.frame_zone=s2_video_level_zone()>>8;f->render.frame_act=s2_video_level_zone()&255;
            f->render.frame_resize=g_ram[0xEEDF];
            f->render.stage_width=stage_width();
            f->render.width=width;
        }
        f->scene.count=scene->count;f->scene.scene=scene->scene;
        s2_runtime_presentation_capture(&f->party);
        f->scene.camera_x=scene->camera_x;f->scene.camera_y=scene->camera_y;
        f->scene.actors=scene->actors;
        f->scene.native_hscroll=scene->native_hscroll;f->scene.native_vscroll=scene->native_vscroll;
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
static int captured_background_base(const PresentationFrame *f,int line)
{
    const S2RenderContext *r=&f->render;
    int row=r->frame_zone==0 && line>=222?221:line;
    int camera=unwrap(-(int16_t)f->hscroll[row][0],r->frame_fg_x);
    int by=unwrap(f->vscroll[line][1],r->frame_bg_y)+line;
    if(r->frame_campaign_view)
        by+=(r->frame_view_y-unwrap(f->vscroll[line][0],r->frame_fg_y))/4;
    if(r->frame_zone==7 && !r->frame_htz_shake)by=(int)((unsigned)by&255u);
    return background_view_base(r,camera,r->left,line,by,(int16_t)f->hscroll[row][1]);
}
static int interpolation_background_base(int line)
{
    /* Remove native camera motion at each completed endpoint before
     * interpolating. Re-evaluating quantized parallax at a fractional camera
     * can disagree by a pixel with interpolated native scroll words, making
     * a clamped, stationary wide background shake between identical frames.
     * Endpoint rebasing also retains native animation and event offsets. */
    return lerp_position(captured_background_base(s_lerp_old,line),
                         captured_background_base(s_lerp_new,line));
}
static void interpolation_sprite(unsigned index,int *x,int *y)
{
    const SceneFrame *a=&s_lerp_old->scene,*b=&s_lerp_new->scene;
    unsigned role=b->sprites[index].actor;
    if(role && role<=S2_MAX_PLAYERS && s_lerp_sprite[index]==-2) {
        --role;
        *x+=lerp_position(a->actors.x[role],b->actors.x[role])-b->actors.x[role];
        int dy=((a->actors.y[role]-b->actors.y[role]+1024)&2047)-1024;
        *y+=lerp_position(dy,0);
        return;
    }
    int previous=s_lerp_sprite[index];
    if(previous<0)return;
    *x=lerp_position(a->sprites[previous].x+a->camera_x,b->sprites[index].x+b->camera_x)-b->camera_x;
    *y=lerp_position(a->sprites[previous].y+a->camera_y,b->sprites[index].y+b->camera_y)-b->camera_y;
}
static int interpolate(double phase,uint32_t *image,int stride,int height)
{
    const PresentationFrame *b=&s_present[s_present_front],*a=&s_present[s_present_front^1];
    if(!s_interpolation_enabled || !image || height!=224 || !(phase>=0 && phase<=1) ||
       !a->valid || !b->valid || a->lines!=224 || b->lines!=224 ||
       a->width!=b->width || stride<b->width || a->zone!=b->zone ||
       (uint16_t)(b->tick-a->tick)>2 ||
       abs(a->render.frame_fg_x-b->render.frame_fg_x)>128 ||
       abs(a->render.frame_fg_y-b->render.frame_fg_y)>128 ||
       !s2_runtime_presentation_matches(&a->party,&b->party))return 0;
    /* Mapping pieces keep their object/piece identity when neighboring objects
     * enter, retire or change animation. Unmatched pieces use their completed
     * position while stable pieces and the camera continue to interpolate. */
    enum { MATCH_SLOTS=SCENE_SPRITES*2 };
    static uint32_t match_keys[MATCH_SLOTS];
    static int match_indices[MATCH_SLOTS];
    memset(match_indices,0xFF,sizeof match_indices);
    for(unsigned i=0;i<a->scene.count;++i) {
        uint32_t key=a->keys[i];
        unsigned slot=(key*2654435761u)>>(32-16);
        while(match_indices[slot]!=-1 && match_keys[slot]!=key)
            slot=(slot+1)&(MATCH_SLOTS-1);
        if(match_indices[slot]==-1) {
            match_keys[slot]=key;match_indices[slot]=(int)i;
        } else match_indices[slot]=-2; /* Ambiguous sources retain their picture. */
    }
    for(unsigned i=0;i<b->scene.count;++i) {
        uint32_t key=b->keys[i];
        unsigned slot=(key*2654435761u)>>(32-16);
        while(match_indices[slot]!=-1 && match_keys[slot]!=key)
            slot=(slot+1)&(MATCH_SLOTS-1);
        int previous=match_indices[slot];
        const SceneSprite *q=&b->scene.sprites[i];
        if(q->actor && q->actor<=S2_MAX_PLAYERS) {
            unsigned role=q->actor-1;
            int dy=((a->scene.actors.y[role]-b->scene.actors.y[role]+1024)&2047)-1024;
            if(a->scene.actors.object[role]!=b->scene.actors.object[role] ||
               abs(a->scene.actors.x[role]-b->scene.actors.x[role])>48 || abs(dy)>48)return 0;
            /* Keep the new native pose, interpolate its shared actor root.
             * Animation may add/remove/rearrange mapping pieces freely. */
            s_lerp_sprite[i]=-2;continue;
        }
        int player=(key>>16)==0xB000 || (key>>16)==0xB040;
        if(previous<0) {
            if(player)return 0;
            s_lerp_sprite[i]=-1;continue;
        }
        const SceneSprite *p=&a->scene.sprites[previous];
        int jump=!(q->hud&3) &&
            (abs(p->x+a->scene.camera_x-q->x-b->scene.camera_x)>48 ||
             abs(p->y+a->scene.camera_y-q->y-b->scene.camera_y)>48);
        if(player && jump)return 0;
        s_lerp_sprite[i]=!jump && p->size==q->size && p->hud==q->hud && p->art==q->art?previous:-1;
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
    for(unsigned i=0;i<2;++i)
        fractional.frame_bg_x[i]=a->render.frame_bg_x[i]+(int32_t)((b->render.frame_bg_x[i]-(int64_t)a->render.frame_bg_x[i])*phase);
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
        s2_runtime_presentation_row(&a->party,&b->party,phase,&fractional.video_frame,
                                    y,image+(size_t)y*stride,b->width);
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
     * Keep both planes and host/native actors out of the actual gap.
     * Demo exit selects Sega/title before the outgoing IM2 image fades;
     * the divider retires when that display mode is replaced. */
    if (!ram16(0xFFD8) || (mode!=0 && mode!=4 && mode!=8 && mode!=12) || (v->reg[12]&6)!=6 ||
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
    if(enabled() && s_priority_capacity>=width && s_front_rgb &&
       (s_frame_level || s_frame_vs || s_frame_card)) {
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
    uint32_t hud_probe;
    int donor_hud=!vs && s2_video_knuckles_hud_pixel(v,0x7D4,0,0,&hud_probe);
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
                            if(kind==4) {
                                if(s_front_rgb && s_priority_capacity>=width)out[target]=s_front_rgb[target];
                                continue;
                            }
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
static unsigned result_letter_width(char letter)
{
    return letter=='M' || letter=='W'?24:letter=='I'?8:16;
}
static unsigned result_letter_ink(const GVDP *v,char letter,int x,int y)
{
    /* TitleCardLetters indexes the original large broken-letter atlas in
     * two-row columns. ENOZ remain in the native common title-card bank. */
    static const uint8_t tiles[]={0,4,8,12,4,16,20,24,28,30,34,38,42,4,4,48,52,56,60,64,68,72,76,82,86,4};
    unsigned common=letter=='E'?0x580:letter=='N'?0x584:letter=='O'?0x588:letter=='Z'?0x58C:0;
    if(common)return results_ink(v,common,x,y,0);
    const uint8_t *font=s2_runtime_title_font();
    if(!font || letter<'A' || letter>'Z')return 0;
    unsigned tile=tiles[letter-'A']+(unsigned)(x/8*2+y/8);
    unsigned byte=font[tile*32+(unsigned)(y&7)*4+(unsigned)(x&7)/2];
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
    /* Keep the complete name/GOT row centered around its native five-letter
     * footprint. KiS2's eight-letter row advances its object by 24 pixels. */
    int knuckles=!strcmp(name,"KNUCKLES") && s2_resource_knuckles_art()!=NULL;
    int advance=0;
    for(unsigned c=0;c<length;++c)advance+=knuckles?16:(int)result_letter_width(name[c]);
    int name_origin=origin+s_result_name.x+(knuckles?((int)length-5)*8:(advance-72)/2);
    int x0=name_origin+8-advance;
    for(unsigned c=0;c<length;++c) {
      int glyph_width=knuckles?16:(int)result_letter_width(name[c]);
      for(int x=0;x<glyph_width;++x) {
        int dx=x0+x;if(dx<0 || dx>=width)continue;
        unsigned ink=0;
        if(knuckles) {
            static const unsigned tiles[]={0x5C6,0x584,0x5D8,0x5B4,0x5C6,0x5C2,0x580,0x5D0};
            ink=results_ink(v,tiles[c],x,y,name[c]=='K');
        } else ink=result_letter_ink(v,name[c],x,y);
        if(ink)out[dx]=genesis_dac_cram_to_argb(v->cram[ink],GENESIS_DAC_NORMAL);
      }
      x0+=glyph_width;
    }
    static const unsigned tiles[]={0x5B8,0x588,0x5D4};
    static const int offsets[]={16,32,47};
    for(unsigned c=0;c<3;++c)for(int x=0;x<16;++x) {
        int dx=name_origin+offsets[c]+x;if(dx<0 || dx>=width)continue;
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
    unsigned tile=attr&2047;
    if (native_demo() || tile<0x7D4 || tile>=0x7E0) return 0;
    const S2KnucklesArt *a=NULL;
    const S2ItemArt *items=NULL;
    const S2DonorBank *bank=NULL;
    const char *id=s2_party.roster.character[0];
    if (!strcmp(id,"knuckles") && s2_party.s3k_enabled) a=s2_resource_knuckles_art();
    else {
        items=s2_resource_primary_items();
        bank=!strcmp(id,"shadow")?s2_resource_shadow_bank():
            s2_resource_bank(!strcmp(id,"amy")?S2_RESOURCE_AMY:S2_RESOURCE_MEGAMIX);
        if (!items || !bank) return 0;
    }
    if (!a && !items) return 0;
    x&=7; y&=7; if (attr&0x800) x^=7; if (attr&0x1000) y^=7;
    /* Column-major 2-row pieces: the live digits are tiles 9 and 11.
     * Tile 10 is the final name column, between the two digit tiles in VRAM.
     * Keep KiS2's complete K.T.E. name instead of borrowing Sonic's last glyph. */
    unsigned offset=tile-0x7D4;
    const uint8_t *life=a?a->life:items->life;
    const uint8_t *pixels=offset==9 || offset==11?v->vram+tile*32:life+offset*32;
    unsigned byte=pixels[y*4+x/2],ink=x&1?byte&15:byte>>4;
    *color=0; if (!ink) return 1;
    unsigned zone=g_ram[0xFE10];
    int wet=s2_video_water_palette(v) && (zone==13 || zone==15);
    const uint16_t *palette=a?(wet?a->water_palette[zone==15]:a->palette):
        wet && bank->water_palette_count?(zone==15 && bank->water_palette_count>1?
            bank->water_palette_alt:bank->water_palette):bank->palette;
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
