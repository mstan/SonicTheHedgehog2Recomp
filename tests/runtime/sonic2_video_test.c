/* Exercise the actual per-game helpers, including shared spawn/cull cells. */
#include "sonic2_video.c"

uint8_t g_ram[65536],g_rom[0x400000];
M68KState g_cpu;
static S2StagePlacementView campaign_test_layout;
S2StagePlacementView s2_runtime_stage_layout(unsigned zone,unsigned act)
{ return zone==12 && act==0?campaign_test_layout:(S2StagePlacementView){0}; }
const S2EndingFrame *s2_runtime_ending_frame(unsigned map,unsigned frame)
{ (void)map;(void)frame;return NULL; }
#if OWN_BACKEND
GenesisMachine g_machine;
#endif
S2PartyConfig s2_party;
static S2KnucklesArt hud_art;
static S2SpecialStageArt amy_special;
static int amy_special_available;
static S2ItemArt amy_items;
static S2DonorBank amy_item_bank;
static int amy_items_available,amy_item_owner;
static int imported_hud_mock;
const S2SpecialStageArt *s2_resource_amy_special(void) {return amy_special_available?&amy_special:NULL;}
const S2ItemArt *s2_resource_amy_items(void) {return amy_items_available?&amy_items:NULL;}
const S2ItemArt *s2_resource_primary_items(void) {return imported_hud_mock || (amy_items_available && !strcmp(s2_party.roster.character[0],"amy"))?&amy_items:NULL;}
const S2DonorBank *s2_resource_bank(unsigned id) {return imported_hud_mock || (amy_items_available && id==S2_RESOURCE_AMY)?&amy_item_bank:NULL;}
const S2DonorBank *s2_resource_shadow_bank(void) { return imported_hud_mock?&amy_item_bank:NULL; }
int s2_native_stars_decode(const uint8_t *r,size_t size,uint8_t out[34*32])
{ (void)r;(void)size;(void)out;return 0; }
static unsigned native_decode_calls,native_decode_bytes,native_decode_asset=S2_NATIVE_EXPLOSION;
unsigned s2_native_sprite_art_decode(const uint8_t *r,size_t size,unsigned asset,uint8_t *out,size_t cap)
{
    (void)r;(void)size;++native_decode_calls;
    if(asset!=native_decode_asset || !native_decode_bytes || native_decode_bytes>cap)return 0;
    memset(out,0x66,native_decode_bytes);return native_decode_bytes;
}
static int slot_mock;
int s2_knuckles_slot_patterns(const uint8_t *r,size_t size,const S2KnucklesArt *art,unsigned slot,const uint8_t source[512],uint8_t out[512])
{(void)r;(void)size;(void)art;(void)slot;if(!slot_mock || source[0]!=0x11)return 0;memset(out,0x22,512);return 1;}
uint16_t s2_shadow_color(const S2DonorBank *b,unsigned i,int wet,unsigned phase,unsigned frame)
{ (void)b;(void)i;(void)wet;(void)phase;(void)frame;return 0; }
static int hud_available;
static uint8_t ui_font[88*32];
static uint8_t title_font[92*32];
const uint8_t *s2_runtime_ui_font(void) {return ui_font;}
const uint8_t *s2_runtime_title_font(void) {return title_font;}
const S2KnucklesArt *s2_resource_knuckles_art(void) { return hud_available?&hud_art:NULL; }
static int online,local_player,item_owner;
static unsigned local_view_players;
static int shadow_item_owner;
int s2_runtime_knuckles_item(unsigned o) { return item_owner && o==0xD180; }
int s2_runtime_amy_item(unsigned o) { return amy_item_owner && o==0xD180; }
int s2_runtime_shadow_star(unsigned o) { return shadow_item_owner && o==0xD200; }
static int positions[4][2];
int genesis_netplay_active(void){return online;}
int genesis_netplay_local_player(void){return local_player;}
int s2_runtime_campaign_online(void){return online && !ram16(0xFFD8) && (g_ram[0xF600]&127)==12;}
int s2_runtime_campaign_independent(void){return s2_runtime_campaign_online() || local_view_players>1;}
unsigned s2_runtime_local_view_players(void)
{return !online && g_ram[0xF600]==12 && !ram16(0xFFD8)?local_view_players:0;}
int s2_runtime_player_position(unsigned p,int *x,int *y)
{if(p>=4)return 0;*x=positions[p][0];*y=positions[p][1];return 1;}
void s2_runtime_published_actors(S2RuntimeActors *out)
{
    memset(out,0,sizeof *out);
    for(unsigned p=0;p<2;++p) {
        unsigned o=0xB000+p*64;if(!g_ram[o])continue;
        out->object[p]=(uint16_t)o;out->x[p]=(int16_t)ram16(o+8);out->y[p]=(int16_t)ram16(o+12);
        out->sprites[p].x=out->x[p];out->sprites[p].y=out->y[p];out->sprites[p].visible=1;
    }
}
void s2_options_overlay(const GVDP *v,int line,uint32_t *out,int width)
{ (void)v; (void)line; (void)out; (void)width; }
void s2_runtime_overlay(const GVDP *v,int line,uint32_t *out,int width)
{ (void)v; (void)line; (void)out; (void)width; }
void s2_runtime_overlay_view(const GVDP *v,int line,uint32_t *out,int width)
{ (void)v; (void)line; (void)out; (void)width; }
static const uint32_t *test_native_row(void *ctx) { return ctx; }
void s2_runtime_presentation_capture(S2RuntimePresentation *out)
{ memset(out,0,sizeof *out); }
void s2_runtime_presentation_row(const S2RuntimePresentation *a,const S2RuntimePresentation *b,
                                unsigned actors,double phase,const GVDP *v,int line,uint32_t *out,int width)
{ (void)a;(void)b;(void)actors;(void)phase;(void)v;(void)line;(void)out;(void)width; }
void glue_poke8(uint32_t a,uint8_t v){g_ram[a&65535]=v;}
void glue_poke16(uint32_t a,uint16_t v){glue_poke8(a,v>>8);glue_poke8(a+1,v);}
void glue_poke32(uint32_t a,uint32_t v){glue_poke16(a,v>>16);glue_poke16(a+2,v);}
void cmd_send_response(const char *json){(void)json;}
#define CHECK(c) do{if(!(c)){fprintf(stderr,"line %d: %s\n",__LINE__,#c);exit(1);}}while(0)
static void word(uint8_t *p,unsigned a,unsigned v){p[a]=(uint8_t)(v>>8);p[a+1]=(uint8_t)v;}
/* Synthetic stage fixtures replace RAM without running Level_SetPlayerMode. */
static void clear_guest(void)
{ memset(g_ram,0,sizeof g_ram);s_level_zone_valid=0;s_boss_preview_complete=0;memset(&s_dez_corpse,0,sizeof s_dez_corpse);memset(&s_ending_scenery,0,sizeof s_ending_scenery); }
static void clamped_background_alignment_test(void)
{
    static GVDP v;uint32_t out[796],native[320]={0};
    const unsigned zones[]={0,4,7,10,11,12,13,14,15,16};
    const unsigned cameras[]={0,512,1700};
    /* Source SwScrl terms at line40, C=0/512/1700 and wide origins0/274/1252.
     * The extra11px is a live native animation offset, retained verbatim. */
    static const unsigned native_terms[][3]={{0,8,27},{0,64,212},{0,64,213},{0,64,212},
        {0,409,1360},{0,400,1328},{0,64,212},{0,512,1700},{0,562,1866},{0,0,0}};
    static const unsigned wide_terms[][3]={{0,5,20},{0,34,156},{0,35,157},{0,34,156},
        {0,219,1001},{0,214,978},{0,34,156},{0,260,1252},{0,300,1374},{0,0,0}};
    presentation_configure(0);online=0;
    CHECK(configure("32:9"));CHECK(width(1,1,320,224)==796);
    for(unsigned z=0;z<sizeof zones/sizeof *zones;++z)
        for(unsigned c=0;c<sizeof cameras/sizeof *cameras;++c) {
            clear_guest();memset(&v,0,sizeof v);memset(&s2_party,0,sizeof s2_party);
            memset(s_history,0,sizeof s_history);memset(&s_display_frame,0,sizeof s_display_frame);
            s_history[0].serial=1;s_history[0].scene=1;
            g_ram[0xF600]=12;g_ram[0xF711]=1;g_ram[0xB000]=1;g_ram[0xFE10]=(uint8_t)zones[z];
            g_ram[0x800F]=3; /* 2048px stage, transparent foreground. */
            g_ram[0x8080]=g_ram[0x8082]=1;g_ram[0x8081]=g_ram[0x8083]=2;
            for(unsigned n=0;n<64;++n){word(g_ram,128+n*2,1);word(g_ram,256+n*2,2);}
            for(unsigned n=0;n<4;++n){word(g_ram,0x9008+n*2,1);word(g_ram,0x9010+n*2,2);}
            unsigned camera=cameras[c];word(g_ram,0xEE00,camera);word(g_ram,0xEE60,camera);
            uint32_t bg=camera*(zones[z]==15?281u*256u:8192u);
            word(g_ram,0xEE68,bg>>16);word(g_ram,0xEE6A,bg);
            write32(0xF672,bg);
            v.reg[1]=64;v.reg[2]=0x30;v.reg[4]=7;v.reg[5]=0x7C;
            v.reg[12]=1;v.reg[13]=0x3F;v.reg[16]=1;v.cram[1]=0xE;v.cram[2]=0xE00;
            memset(v.vram+32,0x11,32);memset(v.vram+64,0x22,32);
            for(unsigned y=0;y<224;++y){word(v.vram,0xFC00+y*4,-camera);word(v.vram,0xFC02+y*4,-(int)(native_terms[z][c]+11));}
            render_source_scanline(&v,0,native,320,out,796,NULL,NULL,-1);
            render_source_scanline(&v,40,native,320,out,796,NULL,NULL,-1);
            CHECK(s_display && s_native_x==(int)camera-view_left((int)camera,796));
            /* The expanded foreground viewport owns its background origin.
             * At a clamp it no longer inherits native-camera translation. */
            for(unsigned nx=0;nx<796;++nx) {
                unsigned source=nx+wide_terms[z][c]+11;
                if(zones[z]==0 || zones[z]==16) {
                    source%=1024;if(source>=512)source=1023-source;
                } else source%=512;
                unsigned ink=source/128%2?2:1;
                if(out[nx]!=genesis_dac_cram_to_argb(v.cram[ink],GENESIS_DAC_NORMAL))
                    fprintf(stderr,"BG zone=%u camera=%u left=%d x=%u source=%u actual=%08X\n",
                            zones[z],camera,s_left,nx,source,out[nx]);
                CHECK(out[nx]==genesis_dac_cram_to_argb(v.cram[ink],GENESIS_DAC_NORMAL));
            }
            render_source_scanline(&v,0,native,320,out,320,NULL,NULL,-1);
            render_source_scanline(&v,40,native,320,out,320,NULL,NULL,-1);
            for(unsigned nx=0;nx<320;++nx) {
                unsigned source=(nx+native_terms[z][c]+11)%512,ink=source/128%2?2:1;
                CHECK(out[nx]==genesis_dac_cram_to_argb(v.cram[ink],GENESIS_DAC_NORMAL));
            }
        }
    CHECK(configure("off"));
}
static void ending_background_extension_test(void)
{
    static GVDP v;uint32_t native[320],out[796];
    clear_guest();memset(&v,0,sizeof v);memset(s_history,0,sizeof s_history);
    memset(&s_display_frame,0,sizeof s_display_frame);g_ram[0xF600]=0x20;
    CHECK(configure("32:9"));CHECK(width(1,1,320,224)==796);
    v.reg[1]=64;v.reg[4]=7;v.reg[12]=1;v.reg[16]=1;
    v.cram[1]=0xE;v.cram[2]=0xE00;
    memset(v.vram+32,0x11,32);memset(v.vram+64,0x22,32);
    for(unsigned n=0;n<64;++n)word(v.vram,0xE000+n*2,n%5?1:2);
    for(unsigned x=0;x<320;++x)native[x]=0xFF123456;
    render_source_scanline(&v,0,native,320,out,796,NULL,NULL,-1);
    for(int x=0;x<796;++x) {
        int nx=x-238;
        if(nx>=0 && nx<320)CHECK(out[x]==native[nx]);
        else {
            int sample=nx<0?-nx-1:639-nx;
            unsigned ink=(sample/8)%5?1:2;
            CHECK(out[x]==genesis_dac_cram_to_argb(v.cram[ink],GENESIS_DAC_NORMAL));
        }
    }
    CHECK(configure("off"));
}
static void act_runoff_test(void)
{
    clear_guest();memset(s_runoff,0,sizeof s_runoff);
    g_ram[0xF600]=12;g_ram[0xF711]=1;g_ram[0xB000]=1;g_ram[0xB040]=2;
    word(g_ram,0xFFC8,1);word(g_ram,0xEECA,1000);word(g_ram,0xB010,0x680);
    CHECK(sonic2_video.configure("32:9"));CHECK(sonic2_video.width(1,1,320,224)==796);
    g_ram[0xFE1E]=1;word(g_ram,0xFE04,100);
    CHECK(s2_video_runoff_x(0,0xB000,1354)==1354);
    g_ram[0xFE1E]=0;word(g_ram,0xFE04,101);word(g_ram,0xB010,0);
    CHECK(s2_video_runoff_x(0,0xB000,1360)==1360);
    word(g_ram,0xFE04,102);CHECK(s2_video_runoff_x(0,0xB000,1360)==1366);
    CHECK(s2_video_runoff_x(0,0xB000,1360)==1366); /* repeated publication */
    word(g_ram,0xFE04,103);CHECK(s2_video_runoff_x(0,0xB000,1360)==1373);
    CHECK(s2_video_runoff_x(1,0xB040,1340)==1340); /* companion has not left */
    S2StateIO length={0};s2_video_rb_state(&length);uint8_t *bytes=malloc(length.pos);CHECK(bytes);
    S2StateIO state={bytes,length.pos,0,0,1};s2_video_rb_state(&state);CHECK(state.ok);
    word(g_ram,0xFE04,104);CHECK(s2_video_runoff_x(0,0xB000,1360)==1379);
    state.pos=0;state.mode=2;s2_video_rb_state(&state);CHECK(state.ok);
    CHECK(s2_video_runoff_x(0,0xB000,1360)==1379);
    /* Its display can leave the wide canvas while the guest's original
     * visibility/AI decision and its outgoing fade remain unchanged. */
    uint8_t saved_map[16];memcpy(saved_map,g_rom+0x700,sizeof saved_map);
    memset(g_rom+0x700,0,sizeof saved_map);word(g_rom,0x700,2);word(g_rom,0x702,1);
    g_ram[0xB001]=4;g_ram[0xB019]=24;word(g_ram,0xB004,0);word(g_ram,0xB006,0x700);
    word(g_ram,0xB008,1360);word(g_ram,0xB00C,100);word(g_ram,0xEEF0,1000);
    word(g_ram,0xAD00,2);word(g_ram,0xAD02,0xB000);word(g_ram,0xE808,0xFFFF);g_ram[0x807F]=1;
    s_runoff[0].position=2000u<<8;
    capture_objects_into(&s_build,796,0,NULL);
    CHECK(s_visible_count==1 && s_visible_objects[0]==0xB000);
    int found=0;for(unsigned n=0;n<s_build.count;++n)found|=s_build.sprites[n].x==1000 && s_build.sprites[n].y==100;
    CHECK(!found);publish_sprites();CHECK(g_ram[0xB001]==132);
    /* An attached shield follows the same continuation and standing centre. */
    word(g_ram,0xAD00,4);word(g_ram,0xAD04,0xD180);
    memcpy(g_ram+0xD180,g_ram+0xB000,64);g_ram[0xD180]=0x38;
    word(g_ram,0xD1BE,0xB000);g_ram[0xB016]=14;
    capture_objects_into(&s_build,796,1,NULL);
    found=0;for(unsigned n=0;n<s_build.count;++n)
        found|=s_build.sprites[n].x==1000 && s_build.sprites[n].y==95;
    CHECK(found);g_ram[0xD180]=0;word(g_ram,0xAD00,2);
    Runoff old_runoff=s_runoff[0];g_ram[0xF600]=4;
    capture_objects_into(&s_build,796,1,NULL);
    CHECK(!memcmp(&old_runoff,&s_runoff[0],sizeof old_runoff));
    found=0;for(unsigned n=0;n<s_build.count;++n)found|=s_build.sprites[n].x==1000 && s_build.sprites[n].y==100;
    CHECK(!found);g_ram[0xF600]=12;
    memcpy(g_rom+0x700,saved_map,sizeof saved_map);
    state.pos=0;state.mode=2;s2_video_rb_state(&state);CHECK(state.ok);free(bytes);
    uint8_t original[65536];memcpy(original,g_ram,sizeof original);
    CHECK(sonic2_video.configure("off"));CHECK(s2_video_runoff_x(0,0xB000,1360)==1360);
    CHECK(!memcmp(original,g_ram,sizeof original));
    CHECK(sonic2_video.configure("32:9"));g_ram[0xF600]=8;
    CHECK(s2_video_runoff_x(0,0xB000,1360)==1360);
    g_ram[0xF600]=12;word(g_ram,0xFFD8,1);
    CHECK(s2_video_runoff_x(0,0xB000,1360)==1360);
    clear_guest();memset(s_runoff,0,sizeof s_runoff);
}
static void parked_car_test(void)
{
    uint8_t saved_map[256];memcpy(saved_map,g_rom+0x2FA58,sizeof saved_map);
    clear_guest();g_ram[0xF600]=12;g_ram[0xF711]=1;g_ram[0xB000]=1;
    word(g_ram,0xFE10,1);g_ram[0xEEDF]=4;g_ram[0x807F]=1;
    memset(g_rom+0x2FA58,0,sizeof saved_map);
    for(unsigned f=0;f<7;++f)word(g_rom,0x2FA58+f*2,128);
    word(g_rom,0x2FAD8,1);
    static SceneFrame scene;memset(&scene,0,sizeof scene);
    uint8_t original[65536];memcpy(original,g_ram,sizeof original);
    parked_boss(&scene,NULL,2,320,0x2940,0x388,0);CHECK(!scene.count);
    parked_boss(&scene,NULL,2,796,0x2940,0x388,0);CHECK(scene.count==3);
    parked_boss(&scene,NULL,3,796,0x2940,0x388,0);CHECK(scene.count==5);
    CHECK(scene.sprites[0].x==0x2AF0-0x2940+28 && scene.sprites[0].y==0x426-0x388+9);
    CHECK(scene.sprites[3].x==0x2AF0-0x2940 && (scene.sprites[3].hud&SCENE_ART_MASK)==SCENE_ART_EHZ_CAR);
    CHECK(!memcmp(original,g_ram,sizeof original));
    scene.count=0;g_ram[0xB400]=0x56;g_ram[0xB424]=6;word(g_ram,0xB404,2);word(g_ram,0xB406,0xFA58);
    word(g_ram,0xB408,0x2AEF);word(g_ram,0xB40C,0x426);g_ram[0xB418]=3;g_ram[0xB401]=4;
    g_ram[0xEEDF]=6;
    parked_boss(&scene,NULL,3,796,0x2940,0x388,0);
    CHECK(scene.count==1 && scene.sprites[0].x==0x2AEF-0x2940 && !s_boss_preview_complete);
    /* The allocation frame is still drawable. Only completed native queue
     * ownership retires the preview, and deletion cannot resurrect it. */
    scene.count=0;word(g_ram,0xAD80,2);word(g_ram,0xAD82,0xB400);
    parked_boss(&scene,NULL,3,796,0x2940,0x388,0);CHECK(!scene.count && s_boss_preview_complete==1);
    g_ram[0xB400]=0;word(g_ram,0xAD80,0);
    parked_boss(&scene,NULL,2,796,0x2940,0x388,0);CHECK(!scene.count);
    g_ram[0xEEDF]=4;word(g_ram,0xFFD8,1);
    parked_boss(&scene,NULL,2,796,0x2940,0x388,0);CHECK(!scene.count);
    memcpy(g_rom+0x2FA58,saved_map,sizeof saved_map);clear_guest();
}
static void cloud_edge_test(void)
{
    GVDP v={0};v.reg[12]=1;
    v.vram[32]=0x12;v.vram[33]=0x34;v.vram[34]=0x56;v.vram[35]=0x78;
    for(int x=-16;x<1040;++x) {
        unsigned flip;int source=ehz_cloud_x(x,&flip);
        CHECK(source>=0 && source<512);
        if(x>=0 && x<512)CHECK(source==x && !flip);
        CHECK(pattern_pixel(&v,(uint16_t)(1|flip),x,0)==pattern_pixel(&v,1,source,0));
    }
    unsigned a,b;CHECK(ehz_cloud_x(-1,&a)==ehz_cloud_x(0,&b));
    CHECK(ehz_cloud_x(511,&a)==ehz_cloud_x(512,&b));
    for(int span=128;span<=3200;span+=128)for(int x=-17;x<span+17;++x) {
        unsigned flip;int source=reflected_cloud_x(x,span,&flip);
        CHECK(source>=0 && source<span);
        if(x>=0 && x<span)CHECK(source==x && !flip);
        CHECK(pattern_pixel(&v,(uint16_t)(1|flip),x,0)==pattern_pixel(&v,1,source,0));
    }
}
static void ending_background_world_test(void)
{
    /* The opening ending uses the scrolling DEZ sky layout, not a reflected
     * crop of the current H40 image. A moving star crosses the old viewport
     * edge in the same direction and phase as the rest of its world. */
    clear_guest();presentation_configure(0);online=0;CHECK(configure("32:9"));
    CHECK(width(1,1,320,224)==796);g_ram[0xF600]=0x20;g_ram[0xB100]=0xCA;g_ram[0xB124]=0x0C;
    word(g_ram,0xFE10,0xE00);
    for(unsigned row=0;row<16;++row)for(unsigned col=0;col<4;++col)
        g_ram[0x8080+row*256+col]=(uint8_t)(1+(col&1));
    /* The ending reuses chunk zero for its picture decompression. Unused
     * layout columns must never sample those now-unrelated blocks. */
    for(unsigned n=0;n<64;++n)word(g_ram,n*2,3);
    for(unsigned n=0;n<64;++n){word(g_ram,128+n*2,1);word(g_ram,256+n*2,2);}
    for(unsigned n=0;n<4;++n){word(g_ram,0x9008+n*2,1);word(g_ram,0x9010+n*2,2);}
    for(unsigned n=0;n<4;++n)word(g_ram,0x9018+n*2,3);
    static GVDP v;memset(&v,0,sizeof v);
    v.reg[1]=64;v.reg[2]=0x30;v.reg[4]=7;v.reg[5]=0x7C;
    v.reg[12]=1;v.reg[13]=0x3F;v.reg[16]=1;v.cram[1]=0xE;v.cram[2]=0xE00;
    memset(v.vram+32,0x11,32);memset(v.vram+64,0x22,32);
    memset(v.vram+96,0x33,32);v.cram[3]=0xE0;
    for(unsigned y=0;y<32;++y)for(unsigned x=0;x<64;++x)
        word(v.vram,0xE000+(y*64+x)*2,world_attr(g_ram,x*8,y*8,1));
    memset(s_history,0,sizeof s_history);memset(&s_display_frame,0,sizeof s_display_frame);
    s_history[0].serial=1;s_history[0].scene=0;
    uint32_t out[796],native[320]={0};
    for(unsigned scroll=0;scroll<=11;scroll+=11) {
        for(unsigned y=0;y<224;++y)word(v.vram,0xFC02+y*4,-(int)scroll);
        render_source_scanline(&v,0,native,320,out,796,NULL,NULL,-1);
        for(unsigned x=0;x<796;++x) {
            unsigned source=(unsigned)((int)x-238+(int)scroll)&16383u;
            unsigned ink=(source/128&1)?2:1;
            if(out[x]!=genesis_dac_cram_to_argb(v.cram[ink],GENESIS_DAC_NORMAL))
                fprintf(stderr,"ending x=%u scroll=%u source=%u actual=%08X expected=%08X phase=%u mode=%u left=%d\n",
                    x,scroll,source,out[x],genesis_dac_cram_to_argb(v.cram[ink],GENESIS_DAC_NORMAL),
                    g_ram[0xB124],g_ram[0xF600],s_left);
            CHECK(out[x]==genesis_dac_cram_to_argb(v.cram[ink],GENESIS_DAC_NORMAL));
        }
    }
    for(unsigned x=0;x<320;++x)native[x]=0xFF010203u+x;
    render_source_scanline(&v,0,native,320,out,320,NULL,NULL,-1);
    CHECK(!memcmp(out,native,sizeof native));CHECK(configure("off"));clear_guest();
}
static void behind_screen_lifetime_test(void)
{
    clear_guest();g_ram[0xF600]=12;g_ram[0xF711]=1;
    g_ram[0xB000]=1;g_ram[0xB400]=0x9A;word(g_ram,0x800F,1);
    CHECK(configure("32:9"));CHECK(width(1,1,320,224)==796);
    word(g_ram,0xEE00,1024);g_cpu.A[0]=0xFFFFB400;
    int lo,hi;activation_bounds(1024,796,&lo,&hi);
    for(int x=lo-128;x<=lo+128;x+=128) {
        word(g_ram,0xB408,x);g_cpu.SR=0x2717;
        CHECK(!s2_video_hook(0x367A0));
        CHECK((g_cpu.SR&~8u)==0x2717);
        CHECK(!!(g_cpu.SR&8)==(x<lo));
    }
    CHECK(configure("off"));g_cpu.SR=0x271F;
    CHECK(!s2_video_hook(0x367A0) && g_cpu.SR==0x271F);
    CHECK(configure("32:9"));g_ram[0xF600]=8;g_cpu.SR=0x271F;
    CHECK(!s2_video_hook(0x367A0) && g_cpu.SR==0x271F);
    CHECK(configure("off"));clear_guest();
}
static void mapping_vertical_bounds_test(void)
{
    clear_guest();g_ram[0xF600]=12;g_ram[0xF711]=1;g_ram[0xB000]=1;
    g_ram[0x800F]=1;word(g_ram,0xE808,0xFFFF);
    for(unsigned f=0;f<4;++f)word(g_rom,0x40A9A+f*2,8);
    word(g_rom,0x40AA2,0); /* Empty HUD fixture. */
    g_ram[0xB400]=0x79;g_ram[0xB401]=4;g_ram[0xB419]=8;
    word(g_ram,0xB408,160);word(g_ram,0xB40C,260);
    glue_poke32(0xFFB404,0x1F424);word(g_ram,0xB402,0x47C);
    word(g_ram,0xAE80,2);word(g_ram,0xAE82,0xB400);
    /* Actual starpost frame0 upper ball:16x16 at y=-40. The root's
     *native32px approximation drops its final4visible rows at y=260. */
    word(g_rom,0x1F424,2);word(g_rom,0x1F426,1);
    g_rom[0x1F428]=(uint8_t)-40;g_rom[0x1F429]=5;
    word(g_rom,0x1F42A,0);word(g_rom,0x1F42C,0);word(g_rom,0x1F42E,-8);
    static SceneFrame scene;
    CHECK(configure("32:9"));CHECK(width(1,1,320,224)==796);
    capture_objects_into(&scene,796,0,NULL);
    CHECK(scene.count==1 && scene.sprites[0].y==220);
    CHECK(s_visible_count==0 && !(g_ram[0xB401]&128));
    capture_objects_into(&scene,320,0,NULL);CHECK(!scene.count && !s_visible_count);
    word(g_ram,0xB40C,264);capture_objects_into(&scene,796,0,NULL);
    CHECK(!scene.count && !s_visible_count); /* Every piece has left. */
    word(g_ram,0xB40C,250);capture_objects_into(&scene,796,0,NULL);
    CHECK(scene.count==1 && s_visible_count==1); /* Native AI boundary. */
    CHECK(configure("off"));clear_guest();
}
static void mapping_horizontal_bounds_test(void)
{
    static SceneFrame scene;static GVDP v;uint32_t out[796],palette[64]={0};
    uint8_t before[65536];
    clear_guest();memset(&v,0,sizeof v);g_ram[0xF600]=12;g_ram[0xF711]=1;
    g_ram[0xB000]=1;g_ram[0x800F]=1;word(g_ram,0xE808,0xFFFF);
    for(unsigned f=0;f<4;++f)word(g_rom,0x40A9A+f*2,8);
    word(g_rom,0x40AA2,0);
    /* Actual Turtloid jet frame6:16x8 at(+28,+6), root radius8.
     * Its last opaque pixels must survive both wide edges and flips. */
    g_ram[0xB400]=0x9C;g_ram[0xB401]=4;g_ram[0xB419]=8;g_ram[0xB41A]=6;
    glue_poke32(0xFFB404,0x37B62);word(g_ram,0xB402,0x38A);
    word(g_rom,0x37B62+12,20);word(g_rom,0x37B62+20,1);
    unsigned p=0x37B62+22;g_rom[p]=6;g_rom[p+1]=4;
    word(g_rom,p+2,0x8036);word(g_rom,p+4,0x801B);word(g_rom,p+6,28);
    word(g_ram,0xAE80,2);word(g_ram,0xAE82,0xB400);word(g_ram,0xB40C,100);
    memset(v.vram+0x3C0*32,0x11,64);palette[1]=0xFFFFFFFF;
    CHECK(configure("32:9"));CHECK(width(1,1,320,224)==796);CHECK(reserve_sprite_row(796));
    s_frame_level=1;s_frame_vs=0;s_camera=1024;s_camera_y=0;
    word(g_ram,0xEE00,1024);word(g_ram,0xEEF0,1024);
    for(unsigned side=0;side<2;++side) {
        word(g_ram,0xB408,1024+(side?568:-248));g_ram[0xB401]=(uint8_t)(4|side);
        memcpy(before,g_ram,sizeof before);capture_objects_into(&scene,796,0,NULL);
        CHECK(scene.count==1 && !s_visible_count && !memcmp(before,g_ram,sizeof before));
        CHECK(scene.sprites[0].x==(side?524:-220));s_display=&scene;
        memset(s_priority,0,796);memset(out,0,sizeof out);
        draw_scene_sprites(&v,106,out,796,238,palette);
        unsigned left=side?762:18;
        CHECK(out[left]==0xFFFFFFFF && out[left+15]==0xFFFFFFFF);
        CHECK(!out[left-1] && !out[left+16]);
        capture_objects_into(&scene,320,0,NULL);CHECK(!scene.count && !s_visible_count);
    }
    /* Native visibility retains its original coarse root boundary. */
    g_ram[0xB401]=4;word(g_ram,0xB408,1024-246);
    capture_objects_into(&scene,796,0,NULL);CHECK(scene.count==1 && s_visible_count==1);
    /* Retaining pieces never invents an unqueued object. */
    word(g_ram,0xAE80,0);capture_objects_into(&scene,796,0,NULL);CHECK(!scene.count);
    s_display=NULL;CHECK(configure("off"));clear_guest();
}
static void dormant_dez_margin_test(void)
{
    static SceneFrame scene;static uint8_t before[65536];
    uint8_t hud[10],maps[2][12];unsigned map_addresses[]={0x3D0EE,0x3D1DE};
    memcpy(hud,g_rom+0x40A9A,sizeof hud);
    for(unsigned n=0;n<2;++n) {
        unsigned map=map_addresses[n];memcpy(maps[n],g_rom+map,12);
        word(g_rom,map,2);word(g_rom,map+2,1);
        g_rom[map+4]=(uint8_t)-26;g_rom[map+5]=13;
        word(g_rom,map+6,0x500);word(g_rom,map+8,0);word(g_rom,map+10,-16);
    }
    clear_guest();g_ram[0xF600]=12;g_ram[0xF711]=1;g_ram[0xB000]=1;
    word(g_ram,0xFE10,0xE00);g_ram[0x801F]=1;word(g_ram,0xE808,0xFFFF);
    g_ram[0xEEDF]=4; /* The separate monitor preview has already retired. */
    for(unsigned f=0;f<4;++f)word(g_rom,0x40A9A+f*2,8);word(g_rom,0x40AA2,0);
    g_ram[0xB400]=0xC6;g_ram[0xB401]=0x84;g_ram[0xB419]=24;
    g_ram[0xB424]=g_ram[0xB425]=2;
    word(g_ram,0xB408,0x440);word(g_ram,0xB40C,0x168);
    glue_poke32(0xFFB404,0x3D0EE);word(g_ram,0xAE80,2);word(g_ram,0xAE82,0xB400);
    word(g_ram,0xEE00,0x224);word(g_ram,0xEEF0,0x224);word(g_ram,0xEEF4,0xC8);
    CHECK(configure("32:9"));CHECK(width(1,1,320,224)==796);
    memcpy(before,g_ram,sizeof before);capture_objects_into(&scene,796,0,NULL);
    CHECK(!memcmp(before,g_ram,sizeof before));
    CHECK(scene.count==1 && (scene.sprites[0].art&SCENE_ART_NATIVE_VIEW));
    CHECK(!s_visible_count); /* First-room framing does not activate the next room. */
    /* Source frame0's32px head can straddle the native right edge. Clip
     * only its6added pixels: all26native pixels must remain identical. */
    word(g_ram,0xEE00,0x440-310);word(g_ram,0xEEF0,0x440-310);
    capture_objects_into(&scene,796,0,NULL);s_display=&scene;
    s_camera=scene.camera_x;s_camera_y=scene.camera_y;s_left=s_camera-238;
    static GVDP v;memset(&v,0,sizeof v);v.reg[12]=1;memset(v.vram+0x500*32,0x11,8*32);
    uint32_t native[320]={0},wide[796]={0},palette[64]={0};palette[1]=0xFFFFFFFF;
    CHECK(reserve_sprite_row(796));memset(s_priority,0,796);
    draw_scene_sprites(&v,134,native,320,0,palette);
    memset(s_priority,0,796);draw_scene_sprites(&v,134,wide,796,238,palette);
    CHECK(!memcmp(native,wide+238,sizeof native));
    CHECK(wide[532]==palette[1] && wide[557]==palette[1] && !wide[558]);
    /* Active actor, another actor, door map, zone, or native width retain
     * their original publication. No simulation field selects the guard. */
    for(unsigned kind=0;kind<5;++kind) {
        g_ram[0xB400]=0xC6;g_ram[0xB425]=2;
        word(g_ram,0xFE10,0xE00);glue_poke32(0xFFB404,0x3D0EE);
        if(kind==0)g_ram[0xB425]=4;
        if(kind==1)g_ram[0xB400]=0xC8;
        if(kind==2)glue_poke32(0xFFB404,0x3D1DE);
        if(kind==3)word(g_ram,0xFE10,0x600);
        capture_objects_into(&scene,kind==4?320:796,0,NULL);
        CHECK(scene.count==1 && !(scene.sprites[0].art&SCENE_ART_NATIVE_VIEW));
    }
    for(unsigned n=0;n<2;++n)memcpy(g_rom+map_addresses[n],maps[n],12);
    memcpy(g_rom+0x40A9A,hud,sizeof hud);s_display=NULL;
    CHECK(configure("off"));clear_guest();
}
static void dez_eggman_preview_test(void)
{
    /* The first room's extra view must stop before the waiting story actor.
     * Opening the next room follows camera movement after Mecha's defeat;
     * a resize-state change alone must not move the displayed scene. */
    clear_guest();g_ram[0xF600]=12;g_ram[0xF711]=1;
    word(g_ram,0xFE10,0xE00);g_ram[0xEEDF]=2;g_ram[0x800F]=1;
    CHECK(configure("32:9"));CHECK(width(1,1,320,224)==796);
    CHECK(view_left(548,796)==0x420-796);
    g_ram[0xEEDF]=4;CHECK(view_left(548,796)==0x420-796);
    g_ram[0xEEDF]=6;CHECK(view_left(598,796)==598-238);
    CHECK(view_left(548,320)==548);
    clear_guest();g_ram[0xF600]=12;g_ram[0xF711]=1;g_ram[0xB000]=1;
    word(g_ram,0xFE10,0xE00);g_ram[0xEEDF]=2;
    NativeSpriteArt upper=s_native_sprite_art[S2_NATIVE_ROBOTNIK_UPPER];
    NativeSpriteArt lower=s_native_sprite_art[S2_NATIVE_ROBOTNIK_LOWER];
    s_native_sprite_art[S2_NATIVE_ROBOTNIK_UPPER].ready=1;
    s_native_sprite_art[S2_NATIVE_ROBOTNIK_LOWER].ready=1;
    uint8_t map[12];memcpy(map,g_rom+0x3D0EE,12);
    word(g_rom,0x3D0EE,2);word(g_rom,0x3D0F0,1);
    memset(g_rom+0x3D0F2,0,8);word(g_rom,0x3D0F4,0x500);
    static uint8_t before[65536];memcpy(before,g_ram,sizeof before);
    static SceneFrame scene;scene.count=0;dez_eggman_preview(&scene,NULL,796,548,224);
    CHECK(scene.count==1 && (scene.sprites[0].art&SCENE_ART_MARGIN_VIEW));
    CHECK(scene.sprites[0].x==540 && scene.sprites[0].y==136);
    CHECK(!memcmp(before,g_ram,sizeof before));
    /* Allocation is not a handoff; the native waiting actor still clips to
     * H40. The margin pose retires when its native proximity routine starts. */
    g_ram[0xB400]=0xC6;g_ram[0xB424]=2;g_ram[0xB425]=2;
    glue_poke32(0xFFB404,0x3D0EE);word(g_ram,0xB408,0x440);word(g_ram,0xB40C,0x168);
    scene.count=0;dez_eggman_preview(&scene,NULL,796,548,224);CHECK(scene.count==1);
    g_ram[0xB425]=4;scene.count=0;dez_eggman_preview(&scene,NULL,796,548,224);CHECK(!scene.count);
    g_ram[0xB425]=2;dez_eggman_preview(&scene,NULL,320,548,224);CHECK(!scene.count);
    g_ram[0xEEDF]=0;dez_eggman_preview(&scene,NULL,796,548,224);CHECK(!scene.count);
    memcpy(g_rom+0x3D0EE,map,12);
    s_native_sprite_art[S2_NATIVE_ROBOTNIK_UPPER]=upper;
    s_native_sprite_art[S2_NATIVE_ROBOTNIK_LOWER]=lower;clear_guest();
}
static void dez_window_preview_test(void)
{
    static SceneFrame scene;static uint8_t before[65536];
    uint8_t hud[10],map[20];memcpy(hud,g_rom+0x40A9A,sizeof hud);
    memcpy(map,g_rom+0x3A08C,sizeof map);
    clear_guest();g_ram[0xF600]=12;g_ram[0xF711]=1;g_ram[0xB000]=1;
    word(g_ram,0xFE10,0xE00);g_ram[0x801F]=1;word(g_ram,0xE808,0xFFFF);
    for(unsigned f=0;f<4;++f)word(g_rom,0x40A9A+f*2,8);word(g_rom,0x40AA2,0);
    /* Closed frame4's real mapping may cross the native right edge.
     * A compact16px fixture isolates its margin-only pixel contract. */
    word(g_rom,0x3A094,10);word(g_rom,0x3A096,1);
    g_rom[0x3A098]=0;g_rom[0x3A099]=4;
    word(g_rom,0x3A09A,0);word(g_rom,0x3A09C,0);word(g_rom,0x3A09E,-8);
    word(g_ram,0xEE00,0x180);word(g_ram,0xEEF0,0x180);word(g_ram,0xEEF4,0xC8);
    CHECK(configure("32:9"));CHECK(width(1,1,320,224)==796);
    memcpy(before,g_ram,sizeof before);capture_objects_into(&scene,796,0,NULL);
    CHECK(!memcmp(before,g_ram,sizeof before) && !s_visible_count);
    CHECK(scene.count==1 && scene.sprites[0].art==(SCENE_ART_MARGIN_VIEW|1+S2_NATIVE_DEZ_WINDOW));
    s_display=&scene;s_camera=scene.camera_x;s_camera_y=scene.camera_y;s_left=s_camera-238;
    static GVDP v;memset(&v,0,sizeof v);v.reg[12]=1;memset(v.vram+0x378*32,0x11,64);
    uint32_t native[320]={0},wide[796]={0},palette[64]={0};palette[1]=0xFFFFFFFF;
    CHECK(reserve_sprite_row(796));memset(s_priority,0,796);
    draw_scene_sprites(&v,113,native,320,0,palette);
    memset(s_priority,0,796);draw_scene_sprites(&v,113,wide,796,238,palette);
    CHECK(!memcmp(native,wide+238,sizeof native));
    CHECK(!wide[557] && wide[558]==palette[1] && wide[565]==palette[1] && !wide[566]);
    /* A native allocation does not retire the preview until publication.
     * Then exactly the real shutter is retained, with its live frame. */
    g_ram[0xEEDF]=2;g_ram[0xB400]=0xAF;g_ram[0xB401]=4;
    g_ram[0xB419]=16;g_ram[0xB424]=0x1C;g_ram[0xB41A]=4;
    word(g_ram,0xB408,0x2C0);word(g_ram,0xB40C,0x139);word(g_ram,0xB402,0x378);
    glue_poke32(0xFFB404,0x3A08C);
    capture_objects_into(&scene,796,0,NULL);
    CHECK(scene.count==1 && scene.sprites[0].art==(SCENE_ART_MARGIN_VIEW|1+S2_NATIVE_DEZ_WINDOW));
    word(g_ram,0xAF00,2);word(g_ram,0xAF02,0xB400);
    capture_objects_into(&scene,796,0,NULL);
    CHECK(scene.count==1 && scene.sprites[0].art==1+S2_NATIVE_DEZ_WINDOW);
    word(g_ram,0xAF00,0);
    g_ram[0xF600]=0x8C;g_ram[0xF711]=0;word(g_ram,0xB0BE,0xFFFF);
    capture_objects_into(&scene,796,0,NULL);CHECK(scene.count==1);
    word(g_ram,0xB0BE,0);capture_objects_into(&scene,796,0,NULL);CHECK(!scene.count);
    g_ram[0xF600]=12;
    for(unsigned kind=0;kind<5;++kind) {
        g_ram[0xEEDF]=0;g_ram[0xF711]=1;g_ram[0xB024]=2;word(g_ram,0xFE10,0xE00);
        if(kind==0)g_ram[0xEEDF]=4;
        if(kind==1)g_ram[0xF711]=0;
        if(kind==2)g_ram[0xB024]=6;
        if(kind==3)word(g_ram,0xFE10,0x600);
        capture_objects_into(&scene,kind==4?320:796,0,NULL);CHECK(!scene.count);
    }
    memcpy(g_rom+0x40A9A,hud,sizeof hud);memcpy(g_rom+0x3A08C,map,sizeof map);
    s_display=NULL;CHECK(configure("off"));clear_guest();
}
static void dormant_wfz_laser_test(void)
{
    static SceneFrame scene;static uint8_t before[65536];uint8_t hud[10],map[12];
    memcpy(hud,g_rom+0x40A9A,sizeof hud);memcpy(map,g_rom+0x3BB18,sizeof map);
    clear_guest();g_ram[0xF600]=12;g_ram[0xF711]=1;g_ram[0xB000]=1;
    word(g_ram,0xFE10,0x600);g_ram[0x801F]=1;word(g_ram,0xE808,0xFFFF);
    for(unsigned f=0;f<4;++f)word(g_rom,0x40A9A+f*2,8);word(g_rom,0x40AA2,0);
    word(g_rom,0x3BB18,2);word(g_rom,0x3BB1A,1);g_rom[0x3BB1C]=0;g_rom[0x3BB1D]=5;
    word(g_rom,0x3BB1E,0);word(g_rom,0x3BB20,0);word(g_rom,0x3BB22,-8);
    g_ram[0xB400]=0xB9;g_ram[0xB401]=4;g_ram[0xB419]=24;g_ram[0xB424]=2;
    word(g_ram,0xB408,416);word(g_ram,0xB40C,100);glue_poke32(0xFFB404,0x3BB18);
    word(g_ram,0xAE00,2);word(g_ram,0xAE02,0xB400);
    CHECK(configure("32:9"));CHECK(width(1,1,320,224)==796);
    memcpy(before,g_ram,sizeof before);capture_objects_into(&scene,796,0,NULL);
    CHECK(!memcmp(before,g_ram,sizeof before) && !s_visible_count);
    CHECK(scene.count==1 && (scene.sprites[0].art&SCENE_ART_NATIVE_VIEW));
    word(g_ram,0xB408,300);capture_objects_into(&scene,796,0,NULL);
    CHECK(s_visible_count==1 && (scene.sprites[0].art&SCENE_ART_NATIVE_VIEW));
    word(g_ram,0xB408,416);g_ram[0xB424]=4;capture_objects_into(&scene,796,0,NULL);
    CHECK(s_visible_count==1 && !(scene.sprites[0].art&SCENE_ART_NATIVE_VIEW));
    g_ram[0xB424]=2;word(g_ram,0xFE10,0x700);capture_objects_into(&scene,796,0,NULL);
    CHECK(s_visible_count==1 && !(scene.sprites[0].art&SCENE_ART_NATIVE_VIEW));
    memcpy(g_rom+0x40A9A,hud,sizeof hud);memcpy(g_rom+0x3BB18,map,sizeof map);
    CHECK(configure("off"));clear_guest();
}
static void dez_corpse_presentation_test(void)
{
    static SceneFrame scene;static uint8_t before[65536];uint8_t hud[10],map[32];
    memcpy(hud,g_rom+0x40A9A,sizeof hud);memcpy(map,g_rom+0x3E5F8,sizeof map);
    clear_guest();g_ram[0xF600]=12;g_ram[0xF711]=1;g_ram[0xB000]=1;
    word(g_ram,0xFE10,0xE00);g_ram[0x801F]=1;word(g_ram,0xE808,0xFFFF);
    g_ram[0xEEDF]=4;
    for(unsigned f=0;f<4;++f)word(g_rom,0x40A9A+f*2,8);word(g_rom,0x40AA2,0);
    word(g_rom,0x3E5FE,8);word(g_rom,0x3E600,1);g_rom[0x3E602]=(uint8_t)-8;g_rom[0x3E603]=5;
    word(g_rom,0x3E604,0x100);word(g_rom,0x3E606,0);word(g_rom,0x3E608,-8);
    word(g_rom,0x3E5FC,18);word(g_rom,0x3E60A,1);g_rom[0x3E60C]=(uint8_t)-4;g_rom[0x3E60D]=4;
    word(g_rom,0x3E60E,0x120);word(g_rom,0x3E610,0);word(g_rom,0x3E612,-8);
    for(unsigned o=0xB400;o<=0xB440;o+=64) {
        g_ram[o]=0xC7;g_ram[o+1]=0x84;word(g_ram,o+2,0x330);
        glue_poke32(0xFF0000u+o+4,0x3E5F8);word(g_ram,o+8,0x840);g_ram[o+0x19]=64;
    }
    g_ram[0xB424]=2;g_ram[0xB425]=14;g_ram[0xB41A]=3;g_ram[0xB418]=5;
    word(g_ram,0xB40C,0x15C);word(g_ram,0xB436,0xB440);
    g_ram[0xB464]=14;g_ram[0xB45A]=2;g_ram[0xB458]=4;
    word(g_ram,0xB44C,0x128);word(g_ram,0xB46C,0xB400);
    word(g_ram,0xAE80,2);word(g_ram,0xAE82,0xB400);word(g_ram,0xAE00,2);word(g_ram,0xAE02,0xB440);
    word(g_ram,0xEE00,0x830);word(g_ram,0xEEF0,0x830);word(g_ram,0xEEF4,0xC8);
    CHECK(configure("32:9"));CHECK(width(1,1,320,224)==796);
    memcpy(before,g_ram,sizeof before);capture_objects_into(&scene,796,0,NULL);
    CHECK(!memcmp(before,g_ram,sizeof before) && scene.count==2);
    CHECK(s_dez_corpse.count==2 && !s_dez_corpse.active);
    /* Native role20 now follows the player; the head slot is reused. Neither
     * position may replace the last genuine body/head's world coordinates. */
    g_ram[0xB424]=0x20;word(g_ram,0xB408,0x999);word(g_ram,0xB40C,0x999);
    g_ram[0xB440]=0x58;word(g_ram,0xB448,0x777);word(g_ram,0xB44C,0x777);
    word(g_ram,0xAE80,0);word(g_ram,0xAE00,0);
    word(g_ram,0xEE00,0x844);word(g_ram,0xEEF0,0x844);
    memcpy(before,g_ram,sizeof before);capture_objects_into(&scene,796,0,NULL);
    CHECK(!memcmp(before,g_ram,sizeof before) && scene.count==2 && s_dez_corpse.active);
    CHECK(scene.sprites[1].x+scene.camera_x==0x838 && scene.sprites[1].y+scene.camera_y==0x154);
    CHECK(scene.sprites[0].x+scene.camera_x==0x838 && scene.sprites[0].y+scene.camera_y==0x124);
    S2StateIO size={0};s2_video_state(&size);uint8_t *saved=malloc(size.pos);CHECK(saved);
    S2StateIO io={saved,size.pos,0,0,1};s2_video_state(&io);CHECK(io.ok);
    size_t corpse_at=0;unsigned matches=0;
    for(size_t at=0;at+sizeof s_dez_corpse<=size.pos;++at)
        if(!memcmp(saved+at,&s_dez_corpse,sizeof s_dez_corpse)){corpse_at=at;++matches;}
    CHECK(matches==1);
    CorpsePresentation malformed=s_dez_corpse;
    malformed.count=17;memcpy(saved+corpse_at,&malformed,sizeof malformed);
    io=(S2StateIO){saved,size.pos,0,1,1};s2_video_state(&io);CHECK(!io.ok && s_dez_corpse.count==2);
    malformed=s_dez_corpse;malformed.root=0xB401;memcpy(saved+corpse_at,&malformed,sizeof malformed);
    io=(S2StateIO){saved,size.pos,0,1,1};s2_video_state(&io);CHECK(!io.ok && s_dez_corpse.root==0xB400);
    memcpy(saved+corpse_at,&s_dez_corpse,sizeof s_dez_corpse);
    memset(&s_dez_corpse,0,sizeof s_dez_corpse);io.pos=0;io.mode=2;io.ok=1;s2_video_state(&io);
    CHECK(io.ok && s_dez_corpse.count==2 && s_dez_corpse.active);free(saved);
    /* Wide continuation can smoothly overlap the original view; selecting
     * native aspect must still show the original retirement immediately. */
    capture_objects_into(&scene,320,0,NULL);CHECK(!scene.count);
    capture_objects_into(&scene,796,0,NULL);s_display=&scene;
    s_camera=scene.camera_x;s_camera_y=scene.camera_y;s_left=s_camera-238;
    static GVDP v;memset(&v,0,sizeof v);v.reg[12]=1;memset(v.vram+0x430*32,0x11,128);
    uint32_t native[320]={0},wide[796]={0},palette[64]={0};palette[1]=0xFFFFFFFF;
    CHECK(reserve_sprite_row(796));memset(s_priority,0,796);draw_scene_sprites(&v,148,wide,796,238,palette);
    CHECK(wide[226]==palette[1] && wide[241]==palette[1] && !wide[242]);
    capture_objects_into(&scene,320,0,NULL);s_display=&scene;
    memset(s_priority,0,796);draw_scene_sprites(&v,148,native,320,0,palette);
    for(unsigned x=0;x<320;++x)CHECK(!native[x]);
    word(g_ram,0xEEF0,0xD00);capture_objects_into(&scene,796,0,NULL);
    CHECK(!scene.count && !s_dez_corpse.count); /* Last world piece has left. */
    memcpy(g_rom+0x40A9A,hud,sizeof hud);memcpy(g_rom+0x3E5F8,map,sizeof map);
    s_display=NULL;CHECK(configure("off"));clear_guest();
}
static void ending_plane_preview_test(void)
{
    static SceneFrame scene;static uint8_t before[65536];uint8_t map[32];
    memcpy(map,g_rom+0x3AFF2,sizeof map);clear_guest();g_ram[0xF600]=0x20;
    g_ram[0xB100]=0xCA;g_ram[0xB124]=14;g_ram[0x801F]=1;
    for(unsigned f=0;f<2;++f) {
        unsigned at=0x3AFF2+10+f*10;word(g_rom,0x3AFF2+f*8,10+f*10);
        word(g_rom,at,1);g_rom[at+2]=(uint8_t)-8;g_rom[at+3]=13;
        word(g_rom,at+4,f*16);word(g_rom,at+6,0);word(g_rom,at+8,-8);
    }
    CHECK(configure("32:9"));CHECK(width(1,1,320,224)==796);
    word(g_ram,0xB13C,8);memcpy(before,g_ram,sizeof before);
    capture_objects_into(&scene,796,0,NULL);CHECK(!memcmp(before,g_ram,sizeof before));
    CHECK(scene.count==1 && scene.sprites[0].x==-32 && scene.sprites[0].y==188);
    CHECK(scene.sprites[0].art==SCENE_ART_MARGIN_VIEW && !(scene.sprites[0].attr&0x800));
    word(g_rom,0x3AFF2+14,0x2A0);
    capture_objects_into(&scene,796,0,NULL);
    CHECK(scene.count==1 && scene.sprites[0].art==(SCENE_ART_MARGIN_VIEW|(1+S2_NATIVE_ENDING_TAILS_PILOT)));
    word(g_rom,0x3AFF2+14,0);
    word(g_rom,0x3AFF2+2,20);
    word(g_ram,0xB13C,7);capture_objects_into(&scene,796,0,NULL);
    CHECK(scene.count==1 && (scene.sprites[0].attr&2047)==0x510);
    word(g_ram,0xB13C,0);capture_objects_into(&scene,796,0,NULL);
    CHECK(scene.count==1 && scene.sprites[0].x==-24 && scene.sprites[0].y==184);
    s_display=&scene;s_camera=s_camera_y=0;s_left=-238;
    static GVDP v;memset(&v,0,sizeof v);v.reg[12]=1;memset(v.vram+0x500*32,0x11,256);
    uint32_t out[796]={0},palette[64]={0};palette[1]=0xFFFFFFFF;
    CHECK(reserve_sprite_row(796));memset(s_priority,0,796);draw_scene_sprites(&v,192,out,796,238,palette);
    CHECK(out[214]==palette[1] && out[237]==palette[1] && !out[238]);
    word(g_ram,0xFF70,2);capture_objects_into(&scene,796,0,NULL);
    CHECK(scene.count==1 && (scene.sprites[0].attr&2047)==0x510); /* Native Tails frame4. */
    word(g_ram,0xFF70,0);g_ram[0xB400]=0xCC;g_ram[0xB401]=4;g_ram[0xB424]=2;
    glue_poke32(0xFFB404,0x3AFF2);word(g_ram,0xB402,0x8500);
    word(g_ram,0xB408,0xFFF0);word(g_ram,0xB40C,192);g_ram[0xB419]=96;
    capture_objects_into(&scene,796,0,NULL);CHECK(scene.count==1); /* Allocation is not publication. */
    word(g_ram,0xAD80,2);word(g_ram,0xAD82,0xB400);
    capture_objects_into(&scene,796,0,NULL);
    CHECK(scene.count==1 && !(scene.sprites[0].art&SCENE_ART_MARGIN_VIEW));
    word(g_ram,0xAD80,0);
    capture_objects_into(&scene,320,0,NULL);CHECK(!scene.count);
    word(g_ram,0xB13C,97);capture_objects_into(&scene,796,0,NULL);CHECK(!scene.count);
    word(g_ram,0xB13C,8);g_ram[0xB124]=12;capture_objects_into(&scene,796,0,NULL);CHECK(!scene.count);
    memcpy(g_rom+0x3AFF2,map,sizeof map);s_display=NULL;CHECK(configure("off"));clear_guest();
}
static void ending_scenery_birth_test(void)
{
    clear_guest();g_ram[0xF600]=0x20;g_ram[0xB100]=0xCA;
    word(g_ram,0xB008,512);g_ram[0xB2C0]=0xCC;g_ram[0xB2E4]=2;
    g_ram[0xB2E5]=2;CHECK(!ending_parked_player(0xB000));
    g_ram[0xB2E5]=4;CHECK(ending_parked_player(0xB000));
    CHECK(!ending_parked_player(0xB040));
    word(g_ram,0xB00C,1);CHECK(!ending_parked_player(0xB000));
    word(g_ram,0xB00C,0);word(g_ram,0xF750,2);CHECK(ending_parked_player(0xB000));
    clear_guest();g_ram[0xF600]=0x20;g_ram[0xB100]=0xCA;
    g_ram[0xB124]=14;g_ram[0xB134]=255;word(g_ram,0xB130,4);
    word(g_ram,0xB132,2857);word(g_ram,0xB13C,0xFF4F);
    write32(0xF636,0x09E3488Eu);
    const unsigned clouds[]={0xB140,0xB180,0xB200,0xB240,0xB280,0xB300,0xB340,0xB380,0xB3C0};
    for(unsigned i=0;i<sizeof clouds/sizeof *clouds;++i) {
        g_ram[clouds[i]]=0xCB;g_ram[clouds[i]+0x24]=4;
    }
    g_ram[0xB2C0]=0xCC;g_ram[0xB2E4]=2;g_ram[0xB2E5]=2;
    g_ram[0xB2F5]=20;word(g_ram,0xB2FC,1152);
    static uint8_t before[65536];memcpy(before,g_ram,sizeof before);
    S2EndingBirth births[16],again[16];
    unsigned n=s2_ending_births(g_ram,5,births,16);
    /* Native ending observations: the bird initializes on the next tick,
     * then a cloud uses an earlier free slot four ticks later. */
    CHECK(n==2 && births[0].tick==1 && births[0].slot==0xB400);
    CHECK(births[0].id==0xCD && births[0].x==-73 && births[0].y==130);
    CHECK(births[1].tick==5 && births[1].slot==0xB1C0 && births[1].id==0xCB);
    CHECK(births[1].x==336 && births[1].y==91 && births[1].frame==0);
    CHECK(!memcmp(before,g_ram,sizeof before));
    CHECK(s2_ending_births(g_ram,5,again,16)==n);
    CHECK(!memcmp(births,again,n*sizeof *births));
    CHECK(s2_ending_births(g_ram,5,again,1)==1);
    /* Both bird frames and the cloud use a one-piece map in this fixture. */
    word(g_rom,0x11E1C,4);word(g_rom,0x11E1E,4);word(g_rom,0x11E20,1);
    memset(g_rom+0x11E22,0,8);
    word(g_rom,0x3B32C,6);word(g_rom,0x3B332,1);memset(g_rom+0x3B334,0,8);
    g_rom[0x3B335]=12;word(g_rom,0x3B33A,-40);
    static SceneFrame scene;scene.count=0;
    ending_birth_preview(&scene,NULL,2,births,n,796);
    CHECK(scene.count==1 && scene.sprites[0].x==-74 && scene.sprites[0].y==130);
    CHECK(scene.sprites[0].art&SCENE_ART_MARGIN_VIEW);
    scene.count=0;ending_birth_preview(&scene,NULL,6,births,n,796);
    CHECK(scene.count==1 && scene.sprites[0].x==311 && scene.sprites[0].y==91);
    /* This 32px cloud crosses H40's right edge before native allocation.
     * Its whole mapping belongs to the wide view during that handoff. */
    CHECK(!(scene.sprites[0].art&SCENE_ART_MARGIN_VIEW));
    g_ram[0xB124]=16;CHECK(!s2_ending_births(g_ram,5,again,16));
    g_ram[0xB124]=14;g_ram[0xF600]=12;CHECK(!s2_ending_births(g_ram,5,again,16));
}
static void ending_scenery_retirement_test(void)
{
    /* Exact REV01 cloud and Animal2 mapping bytes, including both bird poses. */
    static const uint8_t cloud[]={0,8,0,34,0,52,0,8,0,3,252,12,0,0,0,0,255,216,252,12,0,4,0,2,255,248,252,4,0,8,0,4,0,24,0,2,252,12,0,10,0,5,255,232,252,4,0,14,0,7,0,8,0,1,252,4,0,16,0,8,255,248};
    static const uint8_t bird[]={0,16,0,26,0,6,0,1,236,7,0,0,0,0,255,248,0,1,248,5,0,8,0,4,255,248,0,1,248,5,0,12,0,6,255,248};
    uint8_t saved_cloud[sizeof cloud],saved_bird[sizeof bird];static uint8_t before[65536];
    memcpy(saved_cloud,g_rom+0x3B32C,sizeof cloud);memcpy(saved_bird,g_rom+0x11E1C,sizeof bird);
    memcpy(g_rom+0x3B32C,cloud,sizeof cloud);memcpy(g_rom+0x11E1C,bird,sizeof bird);
    clear_guest();CHECK(configure("32:9"));CHECK(width(1,1,320,224)==796);
    g_ram[0xF600]=0x20;g_ram[0xB100]=0xCA;g_ram[0xB134]=0xFF;
    unsigned o=0xB140;g_ram[o]=0xCB;g_ram[o+1]=0x84;g_ram[o+0x24]=4;g_ram[o+0x18]=5;
    word(g_ram,o+2,0x654F);glue_poke32(0xFF0000+o+4,0x3B32C);
    glue_poke32(0xFF0000+o+8,(uint32_t)-30*65536u);word(g_ram,o+12,100);word(g_ram,o+0x10,-0x300);
    word(g_ram,0xAE80,2);word(g_ram,0xAE82,o);
    prepare_ending_scenery(796,0);CHECK(!s_ending_scenery.continued[0].active);
    /* A genuinely published native cloud crosses AA94; its slot is reused.
     * The retained drawing must neither follow the new object nor alter RAM. */
    g_ram[o]=0x58;word(g_ram,o+8,500);word(g_ram,0xAE80,0);
    memcpy(before,g_ram,sizeof before);prepare_ending_scenery(796,0);
    CHECK(!memcmp(before,g_ram,sizeof before));EndingScenery *c=&s_ending_scenery.continued[0];
    CHECK(c->active && (int16_t)ending_word(c->object,8)==-33 && ending_word(c->object,12)==100);
    static SceneFrame scene;scene.count=0;draw_ending_scenery(&scene,NULL,5,796,0);
    CHECK(scene.count==3 && !(scene.sprites[0].art&SCENE_ART_MARGIN_VIEW));
    static GVDP v;memset(&v,0,sizeof v);v.reg[12]=1;memset(v.vram+0x54F*32,0x11,69*32);
    uint32_t out[796]={0},palette[64]={0};palette[49]=0xFFFFFFFF;
    s_display=&scene;s_camera=s_camera_y=0;s_left=-238;
    CHECK(reserve_sprite_row(796));memset(s_priority,0,796);draw_scene_sprites(&v,100,out,796,238,palette);
    /* The cloud's last piece still extends six pixels into the original
     * viewport after its native root retires. Keep that piece whole in the
     * wide scene, rather than cutting a visible notch at the old boundary. */
    CHECK(out[237]==palette[49] && out[238]==palette[49] && out[244]==palette[49]);
    for(unsigned x=245;x<558;++x)CHECK(!out[x]);
    v.reg[1]=64;v.reg[2]=0x30;v.reg[4]=7;v.reg[5]=0x7C;v.reg[13]=0x3F;v.reg[16]=1;
    v.cram[49]=0xEEE;s_video_frame=v;s_frame_level=s_frame_card=s_frame_sega=0;
    uint32_t native[320]={0};memset(out,0,sizeof out);
    render_scanline(&v,100,native,320,out,796,NULL,NULL);
    CHECK(out[238]==0xFFFFFFFF && out[244]==0xFFFFFFFF);
    /* Vertical falling clouds also fill the left margin using their real
     * nine-bit X domain. Every added piece is whole and outside H40, and
     * the native objects/RNG remain byte-identical. */
    unsigned extra=0xB1C0;g_ram[extra]=0xCB;g_ram[extra+1]=0x84;
    g_ram[extra+0x24]=2;g_ram[extra+0x18]=5;word(g_ram,extra+2,0x654F);
    glue_poke32(0xFF0000+extra+4,0x3B32C);word(g_ram,extra+8,400);word(g_ram,extra+12,100);
    word(g_ram,0xAE80,2);word(g_ram,0xAE82,extra);g_ram[0xB124]=14;g_ram[0xB134]=0;
    memcpy(before,g_ram,sizeof before);scene.count=0;ending_fall_clouds(&scene,NULL,5,796);
    CHECK(!memcmp(before,g_ram,sizeof before) && scene.count==3);
    for(unsigned n=0;n<scene.count;++n)
        CHECK(scene.sprites[n].x+((((scene.sprites[n].size>>2)&3)+1)*8)<=0);
    scene.count=0;ending_fall_clouds(&scene,NULL,5,320);CHECK(!scene.count);
    g_ram[0xB134]=0xFF;ending_fall_clouds(&scene,NULL,5,796);CHECK(!scene.count);
    g_ram[0xB124]=0;g_ram[extra]=0;word(g_ram,0xAE80,0);
    /* Read-only/native-width capture must not advance or discard continuations. */
    uint8_t pose[64];memcpy(pose,c->object,64);prepare_ending_scenery(796,1);
    CHECK(!memcmp(pose,c->object,64));scene.count=0;draw_ending_scenery(&scene,NULL,5,320,1);CHECK(!scene.count);
    /* CD's terminal expiry is before Move/Animate. Continue once at -1px,
     * -1/8px and the exact six-tick animation, independent of presentation rate. */
    o=0xB180;g_ram[o]=0xCD;g_ram[o+1]=0x84;g_ram[o+0x24]=2;g_ram[o+0x25]=4;g_ram[o+0x18]=2;
    word(g_ram,o+2,0x594);glue_poke32(0xFF0000+o+4,0x11E1C);
    glue_poke32(0xFF0000+o+8,(uint32_t)-74*65536u);word(g_ram,o+12,77);
    word(g_ram,o+0x10,-0x100);word(g_ram,o+0x12,-0x20);word(g_ram,o+0x3C,0);
    word(g_ram,0xAD00,2);word(g_ram,0xAD02,o);prepare_ending_scenery(796,0);
    g_ram[o]=0;word(g_ram,0xAD00,0);prepare_ending_scenery(796,0);
    EndingScenery *b=&s_ending_scenery.continued[1];CHECK(b->active);
    CHECK((int16_t)ending_word(b->object,8)==-75 && ending_long(b->object,12)==0x004CE000);
    CHECK(b->object[0x1A]==1 && b->object[0x1E]==5);
    for(unsigned n=0;n<6;++n)prepare_ending_scenery(796,0);
    CHECK(b->object[0x1A]==0 && b->object[0x1E]==5);
    for(unsigned rollback=0;rollback<2;++rollback) {
        S2StateIO length={0};video_state(&length,rollback);uint8_t *bytes=malloc(length.pos);CHECK(bytes);
        S2StateIO io={bytes,length.pos,0,0,1};video_state(&io,rollback);CHECK(io.ok);
        memcpy(pose,b->object,64);memset(&s_ending_scenery,0,sizeof s_ending_scenery);
        io=(S2StateIO){bytes,length.pos,0,2,1};video_state(&io,rollback);
        CHECK(io.ok && b->active && !memcmp(pose,b->object,64));free(bytes);
    }
    /* Full mapped extent, rather than a root radius, owns the last column. */
    b->active=0;c->object[8]=0xFE;c->object[9]=0xEB; /* -277: rightmost column remains. */
    scene.count=0;draw_ending_scenery(&scene,NULL,5,796,0);CHECK(c->active && scene.count==3);
    c->object[9]=0xEA;scene.count=0;draw_ending_scenery(&scene,NULL,5,796,0);CHECK(!c->active && !scene.count);
    /* Arbitrary deletion, nonterminal cloud/bird phases and future scenes
     * cannot create or retain ending art. Credits clear CA before new PLCs. */
    g_ram[0xB100]=0;s2_video_hook(0x23C6); /* Existing credits fade hook, no BuildSprites needed. */
    for(unsigned n=0;n<ENDING_CONTINUATIONS;++n)CHECK(!s_ending_scenery.continued[n].active);
    g_ram[0xB100]=0xCA;g_ram[0xB134]=0;prepare_ending_scenery(796,0);CHECK(!ending_scenery_owner());
    g_ram[0xB134]=0xFF;word(g_ram,0xF750,4);
    uint8_t terminal[64]={0};terminal[0]=0xCD;terminal[0x24]=2;terminal[0x25]=4;
    terminal[4]=0;terminal[5]=1;terminal[6]=0x1E;terminal[7]=0x1C;
    word(terminal,2,0x594);word(terminal,0x10,-0x100);word(terminal,0x12,0x20);
    CHECK(ending_terminal(terminal)); /* Tails Animal2 shares the mapping, not the art species. */
    terminal[0x25]=2;CHECK(!ending_terminal(terminal));terminal[0x25]=4;
    word(terminal,2,0x595);CHECK(!ending_terminal(terminal));word(terminal,2,0x594);
    terminal[7]=0x1E;CHECK(!ending_terminal(terminal));
    /* Deletion away from the exact source retirement predicate is unrelated. */
    memset(terminal,0,sizeof terminal);terminal[0]=0xCB;terminal[0x24]=4;
    terminal[5]=3;terminal[6]=0xB3;terminal[7]=0x2C;
    word(terminal,2,0x654F);word(terminal,0x10,-0x300);word(terminal,8,100);
    CHECK(ending_terminal(terminal));memcpy(s_ending_scenery.previous[2],terminal,64);
    s_ending_scenery.armed=1;
    prepare_ending_scenery(796,0);
    for(unsigned n=0;n<ENDING_CONTINUATIONS;++n)CHECK(!s_ending_scenery.continued[n].active);
    memcpy(g_rom+0x3B32C,saved_cloud,sizeof cloud);memcpy(g_rom+0x11E1C,saved_bird,sizeof bird);
    s_display=NULL;CHECK(configure("off"));clear_guest();
}
static void title_card_world_margin_test(void)
{
    /* The incoming gameplay camera is already left-clamped (origin0), while
     * the displayed title/native UI still occupies the centered320 interval.
     * Exercise the actual compositor, not a second plane-sampling helper. */
    clear_guest();online=0;presentation_configure(0);CHECK(configure("32:9"));CHECK(width(1,1,320,224)==796);
    static GVDP v;memset(&v,0,sizeof v);
    memset(s_history,0,sizeof s_history);memset(&s_display_frame,0,sizeof s_display_frame);
    v.reg[1]=0x40;v.reg[2]=0x30;v.reg[4]=7;v.reg[5]=0x7C;v.reg[12]=1;v.reg[13]=0x3F;v.reg[16]=1;
    memset(v.vram+0x5D8*32,0xCC,32);memset(v.vram+0x5D9*32,0xCC,32);
    memset(v.vram+0x5DA*32,0x33,64);memset(v.vram+0x5DC*32,0xCC,64);
    memset(v.vram+32,0x22,32);v.cram[2]=0xE00;
    v.cram[12]=v.cram[3]=v.cram[28]=v.cram[19]=0xE;
    g_ram[0xF600]=12;g_ram[0xF711]=1;g_ram[0xB000]=1;g_ram[0xB080]=0x34;g_ram[0x801F]=1;
    s_history[0].serial=1;s_history[0].scene=1;s_history[0].count=2;
    s_history[0].sprites[0]=(SceneSprite){.x=0,.y=0,.attr=0x8001};
    s_history[0].sprites[1]=(SceneSprite){.x=250,.y=0,.attr=0x8001};
    uint32_t out[796],native[320]={0};
    uint32_t red=genesis_dac_cram_to_argb(0xE,GENESIS_DAC_NORMAL);
    uint32_t blue=genesis_dac_cram_to_argb(0xE00,GENESIS_DAC_NORMAL);
    for(unsigned tile=0x5D8;tile<=0x5DD;++tile) {
        unsigned attr=tile|(tile>=0x5DC?0x2000u:0);
        /* Include exact measured WFZ85D8/high ink12 and genuine low fills. */
        for(unsigned high=0;high<2;++high) {
            for(unsigned i=0;i<64*32;++i)word(v.vram,0xC000+i*2,attr|(high?0x8000u:0));
            scanline(&v,0,native,320,out,796);
            CHECK(s_frame_card && s_frame_level && s_native_x==0);
            CHECK((s_priority[0]&ROW_CARD_COVERED) && out[0]==red);
            CHECK(out[250]==blue); /* Actual centered native title interval is unchanged. */
        }
    }
    /* Gameplay origin0 must not be mistaken for the displayed title's native
     * interval238..557. Original-width output retains its native submission. */
    for(unsigned x=0;x<320;++x)
        native[x]=(x<8 || (x>=250 && x<258))?blue:0xFF000000u;
    scanline(&v,0,native,320,out,320);
    CHECK(!memcmp(out,native,sizeof native));
    for(unsigned i=0;i<64*32;++i)word(v.vram,0xC000+i*2,0);
    scanline(&v,0,native,320,out,796);
    CHECK(!(s_priority[0]&ROW_CARD_COVERED) && out[0]==blue); /* Actual geometric reveal. */
    for(unsigned i=0;i<64*32;++i)word(v.vram,0xC000+i*2,0x85D8);
    /* Card glyphs and HUD are screen-space UI, never queued world scenery. */
    s_history[0].serial++;s_history[0].count=1;
    s_history[0].sprites[0]=(SceneSprite){.x=-230,.y=0,.attr=0x8001,.hud=2};
    scanline(&v,0,native,320,out,796);CHECK(out[8]==blue);
    s_history[0].serial++;s_history[0].sprites[0].hud=1;s_history[0].sprites[0].x=8;
    scanline(&v,0,native,320,out,796);CHECK(out[8]==blue);
    /* Public actor visibility keeps its original priority contract. */
    CHECK(s2_video_intro_pixel_visible(&v,0,0,1));
    s_display=NULL;s_frame_card=0;s_width=320;CHECK(configure("off"));clear_guest();
}
static void title_card_publication_handoff_test(void)
{
    clear_guest();presentation_configure(0);online=0;CHECK(configure("32:9"));
    CHECK(width(1,1,320,224)==796);g_ram[0x800F]=1;
    g_ram[0xF600]=0x8C;g_ram[0xB000]=1;g_ram[0xB080]=0x34;word(g_ram,0xB0BE,0xFFFF);
    static GVDP v;memset(&v,0,sizeof v);
    v.reg[1]=64;v.reg[2]=0x30;v.reg[4]=7;v.reg[5]=0x7C;
    v.reg[12]=1;v.reg[13]=0x3F;v.reg[16]=1;v.cram[1]=0xEEE;
    memset(v.vram+32,0x11,32);memset(v.vram+0xBB40,0x33,64);
    memset(s_history,0,sizeof s_history);memset(&s_display_frame,0,sizeof s_display_frame);
    s_history[0].serial=1;s_history[0].scene=0;s_history[0].count=1;
    s_history[0].sprites[0]=(SceneSprite){.x=160,.y=56,.attr=0x8001,.hud=2};
    uint32_t native[320]={0},out[796];
    for(unsigned x=160;x<168;++x)native[x]=0xFFFFFFFF;
    render_source_scanline(&v,0,native,320,out,796,NULL,NULL,-1);
    render_source_scanline(&v,56,native,320,out,796,NULL,NULL,-1);
    CHECK(out[398]==0xFFFFFFFF);
    /* A partial next SAT can overlap the mode/Level_started publication.
     * Preserve the completed title text until a matching list reaches DMA,
     * rather than relocating the native H40 glyphs to the terrain origin. */
    g_ram[0xF600]=12;g_ram[0xF711]=1;v.vram[0xF800]=1;
    render_source_scanline(&v,0,native,320,out,796,NULL,NULL,-1);
    render_source_scanline(&v,56,native,320,out,796,NULL,NULL,-1);
    CHECK(s_display && s_display->serial==1 && out[398]==0xFFFFFFFF);
    presentation_configure(1);
    /* Simulation can start while the title pieces are still leaving. A
     * completed gameplay list does not make the transition interpolatable. */
    s_display_frame.scene=1;s_display=&s_display_frame;
    s_display_key_serial=s_display_frame.serial;
    presentation_capture(&v,0,out,796,320,NULL);
    CHECK(!s_present[s_present_front].valid);
    g_ram[0xB080]=0;
    presentation_capture(&v,0,out,796,320,NULL);
    CHECK(s_present[s_present_front].valid);
    presentation_configure(0);
    CHECK(configure("off"));clear_guest();
}
static void htz_background_domain_test(void)
{
    static GVDP v;memset(&v,0,sizeof v);clear_guest();online=0;presentation_configure(0);
    CHECK(configure("32:9"));CHECK(width(1,1,320,224)==796);
    memset(s_history,0,sizeof s_history);memset(&s_display_frame,0,sizeof s_display_frame);
    s_history[0].serial=1;s_history[0].scene=1;
    v.reg[1]=0x40;v.reg[2]=0x30;v.reg[4]=7;v.reg[5]=0x7C;v.reg[12]=1;v.reg[13]=0x3F;v.reg[16]=1;
    const unsigned chunks[]={0xD6,0xDF,0xEA,0xEC,0xF7,1};
    const unsigned colors[]={0,0xE,0xE0,0xE00,0xEE,0xEEE,0xE0E};
    for(unsigned i=0;i<6;++i) {
        for(unsigned j=0;j<64;++j)word(g_ram,chunks[i]*128+j*2,i+1);
        for(unsigned j=0;j<4;++j)word(g_ram,0x9000+(i+1)*8+j*2,i+1);
        memset(v.vram+(i+1)*32,(int)((i+1)*17),32);v.cram[i+1]=(uint16_t)colors[i+1];
    }
    for(unsigned i=0;i<8;++i) {
        g_ram[0x8080+i]=(uint8_t)chunks[i&3];g_ram[0x8180+i]=0xF7;
    }
    /* Real normal layout has two authored512px copies, followed by empty
     * cells and distant quake material. Those cells must not set its period. */
    g_ram[0x8080+51]=1;g_ram[0x8180+49]=1;
    for(unsigned i=70;i<80;++i)g_ram[0x8380+i]=1;
    g_ram[0x807F]=1;g_ram[0xF600]=12;g_ram[0xF711]=1;g_ram[0xB000]=1;
    word(g_ram,0xFE10,0x700);g_ram[0xEEDF]=8;
    for(unsigned y=0;y<32;++y)for(unsigned x=0;x<64;++x)
        word(v.vram,0xE000+(y*64+x)*2,world_attr(g_ram,(int)x*8,(int)y*8,1));
    const unsigned camera[]={0x29A0,0x3100},scroll[]={1332,1568},wide_base[]={1303,1539};
    uint32_t out[796],native[320]={0};
    for(unsigned c=0;c<2;++c)for(unsigned animation=0;animation<=3;animation+=3) {
        word(g_ram,0xEE60,camera[c]);word(g_ram,0xEE00,camera[c]);word(g_ram,0xEEF0,camera[c]);
        v.vsram[1]=512; /* Same native PlaneB row after positive/negative wrapping. */
        for(unsigned y=0;y<224;++y) {
            word(v.vram,0xFC00+y*4,-(int)camera[c]);
            word(v.vram,0xFC02+y*4,-(int)(scroll[c]+animation));
        }
        scanline(&v,0,native,320,out,796);scanline(&v,40,native,320,out,796);
        for(unsigned x=0;x<796;++x) {
            unsigned tile=((wide_base[c]+animation+x)&511)/128+1;
            CHECK(out[x]==genesis_dac_cram_to_argb((uint16_t)colors[tile],GENESIS_DAC_NORMAL));
        }
        /* Native-width sampling still agrees with the original512x256 VDP
         * table, including live scroll/animation rather than a frozen image. */
        scanline(&v,0,native,320,out,320);scanline(&v,40,native,320,out,320);
        for(unsigned x=0;x<320;++x) {
            uint16_t attr=plane_attr(&v,0xE000,(int)x+(int)scroll[c]+(int)animation,552);
            unsigned ink=pattern_pixel(&v,attr,(int)x+(int)scroll[c]+(int)animation,552);
            CHECK(out[x]==genesis_dac_cram_to_argb(v.cram[ink],GENESIS_DAC_NORMAL));
        }
    }
    /* The wider left edge stays at zero while the native camera crosses
     * several phase boundaries. Source art follows the viewport in this case. */
    uint16_t source_before=scene_read16(0x3FD9C);
    word(g_rom,0x3FD9C,0x1000);
    memset(g_ram+0x1000,0x11,32);
    memset(v.vram+0x500*32,0x22,32);
    for(unsigned j=0;j<4;++j)word(g_ram,0x9008+j*2,0x500);
    v.vsram[1]=0;
    const unsigned native_x[]={8,9,16,17};
    for(unsigned i=0;i<sizeof native_x/sizeof *native_x;++i) {
        word(g_ram,0xEE00,native_x[i]);word(g_ram,0xEE60,native_x[i]);word(g_ram,0xEEF0,native_x[i]);
        word(v.vram,0xFC00,-(int)native_x[i]);word(v.vram,0xFC02,-(int)((native_x[i]+7)/8));
        scanline(&v,0,native,320,out,796);
        CHECK(s_left==0 && out[0]==genesis_dac_cram_to_argb(v.cram[1],GENESIS_DAC_NORMAL));
    }
    for(unsigned j=0;j<4;++j)word(g_ram,0x9008+j*2,1);
    word(g_rom,0x3FD9C,source_before);
    /* The actual cave/lift also has resize8, but EEBC1 selects streamed
     * quake coordinates24E0/016D. Preserve its distant row/domain. */
    g_ram[0xEEBC]=1;word(g_ram,0xEE60,0x23C3);word(g_ram,0xEE68,0x24E0);word(g_ram,0xEE6C,0x16D);
    v.vsram[1]=0x16D;
    for(unsigned y=0;y<224;++y) {
        word(v.vram,0xFC00+y*4,-0x23C3);word(v.vram,0xFC02+y*4,-0x24E0);
    }
    scanline(&v,0,native,320,out,796);g_ram[0xEEBC]=0;
    scanline(&v,40,native,320,out,796);
    CHECK(s_render->frame_htz_shake==1 && out[0]==genesis_dac_cram_to_argb(0xE0E,GENESIS_DAC_NORMAL));
    word(g_ram,0xFE10,0x500);scanline(&v,0,native,320,out,796);scanline(&v,40,native,320,out,796);
    CHECK(out[0]==genesis_dac_cram_to_argb(0xE0E,GENESIS_DAC_NORMAL)); /* Other zones unchanged. */
    s_display=NULL;CHECK(configure("off"));clear_guest();
}
static void dez_window_native_material_test(void)
{
    static const uint8_t frame4[]={0,7,244,6,32,0,32,0,255,240,244,6,40,0,40,0,0,0,244,4,32,6,32,3,255,240,244,4,32,6,32,3,0,0,252,4,32,6,32,3,255,240,252,4,32,6,32,3,0,0,248,13,1,144,0,200,255,240};
    static const uint8_t frame7[]={0,3,244,6,32,0,32,0,255,240,244,6,40,0,40,0,0,0,248,13,1,152,0,204,255,240};
    uint8_t map[128],header[4];memcpy(map,g_rom+0x3A08C,sizeof map);memcpy(header,g_rom+0x100,4);
    NativeSpriteArt saved=s_native_sprite_art[S2_NATIVE_DEZ_WINDOW];
    unsigned calls=native_decode_calls,bytes=native_decode_bytes,asset=native_decode_asset;
    memset(&s_native_sprite_art[S2_NATIVE_DEZ_WINDOW],0,sizeof saved);memset(g_rom+0x100,0,4);
    native_decode_asset=S2_NATIVE_DEZ_WINDOW;native_decode_bytes=256;
    CHECK(!native_art_ready(S2_NATIVE_DEZ_WINDOW) && native_decode_calls==calls);
    CHECK(!s_native_sprite_art[S2_NATIVE_DEZ_WINDOW].ready);
    memcpy(g_rom+0x100,"SEGA",4);CHECK(native_art_ready(S2_NATIVE_DEZ_WINDOW));
    CHECK(native_decode_calls==calls+1 && s_native_sprite_art[S2_NATIVE_DEZ_WINDOW].bytes==256);
    CHECK(native_art_ready(S2_NATIVE_DEZ_WINDOW) && native_decode_calls==calls+1);
    memset(s_native_sprite_art[S2_NATIVE_DEZ_WINDOW].pixels,0x22,256);
    word(g_rom,0x3A08C+8,16);memcpy(g_rom+0x3A08C+16,frame4,sizeof frame4);
    word(g_rom,0x3A08C+14,80);memcpy(g_rom+0x3A08C+80,frame7,sizeof frame7);
    clear_guest();CHECK(configure("32:9"));CHECK(width(1,1,320,224)==796);
    g_ram[0xF600]=12;g_ram[0xF711]=1;g_ram[0xB000]=1;word(g_ram,0xFE10,0xE00);
    g_ram[0xB500]=0xAF;g_ram[0xB524]=0x22;
    static GVDP v;memset(&v,0,sizeof v);v.reg[12]=1;
    memset(v.vram+0x378*32,0x11,256);memset(v.vram+0x508*32,0x33,16*32);
    static uint8_t donor_patterns[65536],donor_lines[2048];memset(donor_lines,0,sizeof donor_lines);
    for(unsigned i=0x378;i<0x380;++i)donor_lines[i]=GVDP_EXT_SPRITE_FORCE|2;
    gvdp_set_sprite_patterns_bank(0,donor_patterns,sizeof donor_patterns,donor_lines);
    CHECK(gvdp_sprite_palette_line(0,0x378,1)==0);
    CHECK(gvdp_sprite_palette_line(0x80,0x378,1)==2);
    static SceneFrame scene;scene.count=0;
    add_mapping(&scene,NULL,0xB5000000u,0x3A08C,4,0x378,4,-180,100,0,0);
    CHECK(scene.count==7);
    for(unsigned i=0;i<6;++i)CHECK(scene.sprites[i].art==1+S2_NATIVE_DEZ_WINDOW && !(scene.sprites[i].size&0xC0));
    CHECK(scene.sprites[6].art==0 && (scene.sprites[6].attr&2047)==0x508);
    s_display=&scene;s_frame_card=0;s_camera=s_camera_y=0;s_left=-238;
    uint32_t out[796]={0},palette[64]={0};palette[18]=0xFF123456;
    CHECK(reserve_sprite_row(796));memset(s_priority,0,796);draw_scene_sprites(&v,100,out,796,238,palette);
    CHECK(out[50]==palette[18] && gvdp_sprite_palette_line(0,0x378,1)==0 &&
          gvdp_sprite_palette_line(0x80,0x378,1)==2);
    CHECK(native_asset_pixel(&v,S2_NATIVE_DEZ_WINDOW,0x378,0x2378,0,0,320)==17);
    CHECK(native_asset_pixel(&v,S2_NATIVE_DEZ_WINDOW,0x378,0x2378,0,0,796)==18);
    CHECK(native_asset_pixel(&v,S2_NATIVE_DEZ_WINDOW,0x378,0x508,0,0,796)==3);
    scene.count=0;add_mapping(&scene,NULL,0xB5000000u,0x3A08C,7,0x378,4,-180,100,0,0);
    CHECK(scene.count==3 && scene.sprites[0].art==1+S2_NATIVE_DEZ_WINDOW &&
          scene.sprites[1].art==1+S2_NATIVE_DEZ_WINDOW && scene.sprites[2].art==0 &&
          (scene.sprites[2].attr&2047)==0x510);
    CHECK(dez_window_material(0xFFF60000u,0x3A08C,0x378));
    CHECK(!dez_window_material(0xB5000000u,0x3A08E,0x378));
    CHECK(!dez_window_material(0xB5000000u,0x3A08C,0x379));
    g_ram[0xB524]=6;CHECK(!dez_window_material(0xB5000000u,0x3A08C,0x378));
    g_ram[0xB524]=0x22;word(g_ram,0xFE10,0x700);
    CHECK(!dez_window_material(0xB5000000u,0x3A08C,0x378));
    gvdp_set_sprite_patterns_bank(0,NULL,0,NULL);s_display=NULL;
    s_native_sprite_art[S2_NATIVE_DEZ_WINDOW]=saved;native_decode_calls=calls;native_decode_bytes=bytes;native_decode_asset=asset;
    memcpy(g_rom+0x3A08C,map,sizeof map);memcpy(g_rom+0x100,header,4);
    CHECK(configure("off"));clear_guest();
}
static void cnz_closure_margin_test(void)
{
    /* Production captures only world data through A800. Zone/act belong to
     * frozen render metadata, not to guest-tail bytes beyond this buffer. */
    static uint8_t world[0xA800],before[0xA800];memset(world,0,sizeof world);
    unsigned zone=12,act=1;world[0x8C50]=0xF9;
    for(unsigned i=0;i<128;i+=2) {word(world,0xDD*128+i,1);word(world,0xF9*128+i,2);}
    for(unsigned i=0;i<8;i+=2) {word(world,0x9000+8+i,0x111);word(world,0x9000+16+i,0x222);}
    memcpy(before,world,sizeof before);clear_guest();g_ram[0xFE10]=7;
    for(int y=0x600;y<0x680;y+=8)for(int x=0x2800;x<0x2880;x+=8) {
        CHECK(world_attr(world,x,y,0)==0x222);
        CHECK(foreground_attr(world,x,y,0,zone,act)==0x222); /* Every native pixel keeps F9. */
        CHECK(foreground_attr(world,x,y,1,zone,act)==0x111); /* Frozen CNZ2 still owns the margin. */
    }
    CHECK(!memcmp(before,world,sizeof before));
    CHECK(foreground_attr(world,0x2800,0x600,1,zone,0)==0x222);
    CHECK(foreground_attr(world,0x2800,0x600,1,7,act)==0x222);
    world[0x8C50]=0xDD;CHECK(foreground_attr(world,0x2800,0x600,1,zone,act)==0x111);
    world[0x8C54]=0xF9;CHECK(foreground_attr(world,0x2A00,0x600,1,zone,act)==0x222); /* Right closure unchanged. */
    clear_guest();
}
static void wfz_early_background_margin_test(void)
{
    /* A real capture ends at A800: event state is separate frozen metadata.
     * First-room x160/y144 wraps raw -51 into11597 in native chunk93. */
    static uint8_t world[0xA800],before[0xA800];memset(world,0,sizeof world);
    static const unsigned composition[2][4]={{0x90,0x91,0x92,0x93},{0x97,0x96,0x95,0x94}};
    for(unsigned chunk=0x8F;chunk<=0x98;++chunk)
        for(unsigned i=0;i<128;i+=2)word(world,chunk*128+i,1);
    for(unsigned i=0;i<8;i+=2)word(world,0x9008+i,0x123);
    clear_guest();g_ram[0xEEDF]=4; /* Live state cannot override frozen early state. */
    for(unsigned row=0;row<2;++row)for(unsigned col=0;col<4;++col) {
        int x=0x2B80+(int)col*128,y=0x580+(int)row*128;
        unsigned layout=0x8080+((y>>7)&15)*256+(x>>7);
        world[layout]=(uint8_t)composition[row][col];
        for(int dy=0;dy<128;dy+=8)for(int dx=0;dx<128;dx+=8) {
            CHECK(background_attr(world,x+dx,y+dy,-51,1,6,2)==0);
            CHECK(background_attr(world,x+dx,y+dy,-51,0,6,2)==0x123); /* Native view/width. */
            CHECK(background_attr(world,x+dx,y+dy,0,1,6,2)==0x123); /* Positive interior. */
            CHECK(background_attr(world,x+dx,y+dy,-51,1,7,2)==0x123);
            CHECK(background_attr(world,x+dx,y+dy,-51,1,6,4)==0x123); /* Genuine getaway. */
        }
    }
    int x=11597,y=0x54C+144;
    unsigned layout=0x8080+((y>>7)&15)*256+(x>>7);
    for(unsigned chunk=0x8F;chunk<=0x98;chunk+=9) {
        world[layout]=(uint8_t)chunk;
        CHECK(background_attr(world,x,y,-51,1,6,2)==0x123); /* Other cloud/scenery. */
    }
    static const int outside[4][2]={{0x2B7F,0x5DC},{0x2D80,0x5DC},{0x2D4D,0x57F},{0x2D4D,0x680}};
    for(unsigned i=0;i<4;++i) {
        int ox=outside[i][0],oy=outside[i][1];
        world[0x8080+((oy>>7)&15)*256+(ox>>7)]=0x90;
        CHECK(background_attr(world,ox,oy,-51,1,6,2)==0x123);
    }
    memcpy(before,world,sizeof before);
    CHECK(background_attr(world,x,y,-51,1,6,2)==0x123);
    CHECK(!memcmp(before,world,sizeof before));clear_guest();
}
static void wfz_robotnik_preview_test(void)
{
    static SceneFrame scene;static uint8_t before[65536];uint8_t egg_map[20],platform_map[12];
    static NativeSpriteArt old_upper,old_lower;
    old_upper=s_native_sprite_art[S2_NATIVE_ROBOTNIK_UPPER];old_lower=s_native_sprite_art[S2_NATIVE_ROBOTNIK_LOWER];
    memset(&s_native_sprite_art[S2_NATIVE_ROBOTNIK_UPPER],0,sizeof old_upper);
    memset(&s_native_sprite_art[S2_NATIVE_ROBOTNIK_LOWER],0,sizeof old_lower);
    memcpy(egg_map,g_rom+0x3D0EE,sizeof egg_map);memcpy(platform_map,g_rom+0x3CEBC,sizeof platform_map);
    clear_guest();g_ram[0xF600]=12;g_ram[0xF711]=1;g_ram[0xB000]=1;word(g_ram,0xFE10,0x600);
    word(g_ram,0xF660,2);word(g_ram,0xF680,8);word(g_ram,0xF682,0xE000); /* Native PLC still busy. */
    g_ram[0xB400]=0xC5;g_ram[0xB424]=2;g_ram[0xB425]=2;g_ram[0xB421]=8;
    word(g_ram,0xB408,0x2B80);glue_poke32(0xFFB404,0x3CCD8);
    word(g_rom,0x3D0EE,2);word(g_rom,0x3D0F0,2);
    for(unsigned n=0;n<2;++n) {
        unsigned p=0x3D0F2+n*8;g_rom[p]=(uint8_t)(n?0:-26);g_rom[p+1]=13;
        word(g_rom,p+2,n?0x564:0x500);word(g_rom,p+4,0);word(g_rom,p+6,-8);
    }
    word(g_rom,0x3CEBC,2);word(g_rom,0x3CEBE,1);g_rom[0x3CEC0]=(uint8_t)-8;g_rom[0x3CEC1]=5;
    word(g_rom,0x3CEC2,0);word(g_rom,0x3CEC4,0);word(g_rom,0x3CEC6,-8);
    CHECK(configure("32:9"));int cam=0x2C60-310,cy=0x442;
    scene.count=0;wfz_robotnik_preview(&scene,NULL,796,cam,cy,0);
    CHECK(!scene.count); /* Unsupported art must never preview unrelated PLC tiles. */
    NativeSpriteArt *upper=&s_native_sprite_art[S2_NATIVE_ROBOTNIK_UPPER],*lower=&s_native_sprite_art[S2_NATIVE_ROBOTNIK_LOWER];
    upper->ready=lower->ready=1;upper->bytes=24*32;lower->bytes=28*32;
    memset(upper->pixels,0x11,upper->bytes);memset(lower->pixels,0x33,lower->bytes);
    memcpy(before,g_ram,sizeof before);wfz_robotnik_preview(&scene,NULL,796,cam,cy,0);
    CHECK(!memcmp(before,g_ram,sizeof before) && scene.count==3);
    CHECK(scene.sprites[0].art==(SCENE_ART_MARGIN_VIEW|(1+S2_NATIVE_ROBOTNIK_UPPER)));
    CHECK(scene.sprites[1].art==(SCENE_ART_MARGIN_VIEW|(1+S2_NATIVE_ROBOTNIK_LOWER)));
    scene.scene=1;scene.camera_x=cam;scene.camera_y=cy;s_display=&scene;
    s_camera=cam;s_camera_y=cy;s_left=cam-238;
    static GVDP v;memset(&v,0,sizeof v);v.reg[12]=1;memset(v.vram+0x500*32,0x22,24*32);
    uint32_t out[796]={0},palette[64]={0};palette[1]=0xFFFFFFFF;palette[2]=0xFF0000FF;
    CHECK(reserve_sprite_row(796));memset(s_priority,0,796);draw_scene_sprites(&v,148,out,796,238,palette);
    CHECK(!out[557] && out[558]==palette[1] && out[571]==palette[1] && !out[572]);
    g_ram[0xB425]=4;word(g_ram,0xB438,0xB440);
    g_ram[0xB440]=0xC5;g_ram[0xB464]=16;word(g_ram,0xB46C,0xB400);word(g_ram,0xB47E,0xB480);
    glue_poke32(0xFFB444,0x3D0EE);g_ram[0xB480]=0xC5;g_ram[0xB4A4]=18;
    word(g_ram,0xB4AC,0xB440);glue_poke32(0xFFB484,0x3CEBC);
    word(g_ram,0xAE80,2);word(g_ram,0xAE82,0xB440);
    scene.count=0;wfz_robotnik_preview(&scene,NULL,796,cam,cy,0);CHECK(scene.count==1); /* Platform allocation precedes its publication. */
    word(g_ram,0xAE80,4);word(g_ram,0xAE84,0xB480);
    scene.count=0;wfz_robotnik_preview(&scene,NULL,796,cam,cy,0);CHECK(!scene.count && (s_boss_preview_complete&4));
    word(g_ram,0xAE80,0);g_ram[0xB440]=g_ram[0xB480]=0;
    wfz_robotnik_preview(&scene,NULL,796,cam,cy,0);CHECK(!scene.count); /* No resurrection after live actors retire. */
    s_boss_preview_complete=0;g_ram[0xB425]=8;wfz_robotnik_preview(&scene,NULL,796,cam,cy,0);CHECK(!scene.count);
    g_ram[0xB425]=2;wfz_robotnik_preview(&scene,NULL,320,cam,cy,0);CHECK(!scene.count);
    memcpy(g_rom+0x3D0EE,egg_map,sizeof egg_map);memcpy(g_rom+0x3CEBC,platform_map,sizeof platform_map);
    s_native_sprite_art[S2_NATIVE_ROBOTNIK_UPPER]=old_upper;s_native_sprite_art[S2_NATIVE_ROBOTNIK_LOWER]=old_lower;
    s_display=NULL;CHECK(configure("off"));clear_guest();
}
static void half_pipe_width_test(void)
{
    clear_guest();CHECK(sonic2_video.configure("32:9"));
    g_ram[0xF600]=16;g_ram[0xB000]=9;
    CHECK(sonic2_video.width(1920,1080,256,224)==256);
    CHECK(sonic2_video.width(5120,1440,256,224)==256);
    /* A starpost departure still displays the outgoing wide level. */
    g_ram[0xB000]=1;
    CHECK(sonic2_video.width(1920,1080,320,224)==796);
    g_ram[0xF600]=12;
    CHECK(sonic2_video.width(1920,1080,320,224)==796);
    clear_guest();
}
static void outgoing_zone_test(void)
{
    unsigned old_zone=s_level_zone;int old_valid=s_level_zone_valid;
    clear_guest();word(g_ram,0xFE10,1);
    CHECK(!s2_video_hook(0x4450));CHECK(s2_video_level_zone()==1);
    word(g_ram,0xFE10,0xD00);g_ram[0xF600]=0x8C;
    CHECK(s2_video_level_zone()==1); /* still fading the EHZ display */
    S2StateIO size={0};s2_video_rb_state(&size);uint8_t *saved=malloc(size.pos);CHECK(saved);
    S2StateIO io={saved,size.pos,0,0,1};s2_video_rb_state(&io);CHECK(io.ok);
    CHECK(!s2_video_hook(0x4450));CHECK(s2_video_level_zone()==0xD00);
    io.pos=0;io.mode=2;s2_video_rb_state(&io);CHECK(io.ok && s2_video_level_zone()==1);
    CHECK(ram16(0xFE10)==0xD00); /* presentation identity never rewrites the guest */
    free(saved);s_level_zone=old_zone;s_level_zone_valid=old_valid;
    clear_guest();
}
static void slot_presentation_test(void)
{
    S2PartyConfig old_party=s2_party;int old_available=hud_available;
    static S2KnucklesArt old_art;old_art=hud_art;
    clear_guest();g_ram[0xF600]=12;g_ram[0xFE10]=12;g_ram[0xF711]=1;g_ram[0xB000]=1;
    s2_party.roster.slots=1;s2_party.s3k_enabled=1;strcpy(s2_party.roster.character[0],"knuckles");
    hud_available=slot_mock=1;hud_art.palette[1]=0xEEE;hud_art.palette[2]=0xE0;
    GVDP v={0};v.reg[1]=64;v.reg[2]=0x30;v.reg[4]=7;v.reg[5]=0x7C;v.reg[12]=1;v.reg[13]=0x3F;v.reg[16]=1;
    v.cram[1]=0xE;v.cram[3]=0xE00;memset(v.vram+0xAA00,0x11,512);
    for(unsigned at=0xC000;at<0xD000;at+=2)word(v.vram,at,0x550);
    /* A high-priority native sprite retains its blue pixels above the slot. */
    memset(v.vram+32,0x33,32);word(v.vram,0xF800,128);word(v.vram,0xF804,0x8001);word(v.vram,0xF806,128);
    CHECK(sonic2_video.configure("off"));gvdp_set_ws_extra(0);gvdp_set_ws_canvas(0);
    s2_video_prepare_knuckles_items();GVDP original=v;uint32_t out[320];
    for(unsigned x=0;x<320;++x)out[x]=0xFF123456;
    const GVDP *private=s2_video_slots_overlay(&v,0,out,320);
    CHECK(private!=&v && private->vram[0xAA00]==0x22 && !memcmp(&v,&original,sizeof v));
    for(unsigned x=0;x<320;++x)CHECK(out[x]==(x<8?0xFF0000FF:0xFF00FF00));
    memset(v.vram+0xF800,0,640);memset(v.vram+0xC000,0,4096);
    word(v.vram,0xFC00,0xFFFF);word(v.vram,0xC050,0x550);
    CHECK(s2_video_slots_overlay(&v,0,out,320)!=&v);
    CHECK(out[319]==0xFF00FF00 && out[318]==0xFF000000);
    word(v.vram,0xFC00,0);memset(v.vram+0xC000,0,4096);
    v.reg[11]=4;v.vsram[0]=8;word(v.vram,0xC080,0x550);
    CHECK(s2_video_slots_overlay(&v,0,out,320)!=&v);
    CHECK(out[4]==0xFF00FF00 && out[20]==0xFF000000);
    v.reg[11]=0;v.vsram[0]=0;
    /* Each competition view owns its skin. The other character's slot
     * keeps the actual native palette, including a live CRAM change. */
    memset(v.vram+0xF800,0,640);v.reg[12]=7;memset(v.vram+0xEA00,0x11,512);
    for(unsigned at=0xC000;at<0xD000;at+=2)word(v.vram,at,0x3A8);
    word(g_ram,0xFFD8,1);s2_party.roster.slots=2;
    strcpy(s2_party.roster.character[1],"sonic");s2_video_prepare_knuckles_items();
    memset(v.vram+0xC000,0,4096);word(v.vram,0xC080,0x3A8);
    v.reg[11]=3;word(v.vram,0xFC40,384);
    s2_video_slots_overlay(&v,0,out,320);
    for(unsigned x=0;x<320;++x)out[x]=0xFF123456;
    s2_video_slots_overlay(&v,16,out,320);CHECK(out[4]==0xFF00FF00);
    v.reg[11]=0;word(v.vram,0xFC40,0);
    for(unsigned at=0xC000;at<0xD000;at+=2)word(v.vram,at,0x3A8);
    static const char *other[]={"sonic","shadow"};
    for(unsigned which=0;which<2;++which)for(unsigned p=0;p<2;++p) {
        strcpy(s2_party.roster.character[p],"knuckles");strcpy(s2_party.roster.character[p^1],other[which]);
        s2_video_prepare_knuckles_items();
        for(unsigned row=0;row<2;++row) {
            int line=(int)row*224;
            CHECK(slot_theme(slot_role(line))==(row==p));
            CHECK(slot_material(&v,0x3A8)==1);
            if(row==p) {
                CHECK(slot_colors()[2]==0xFF00FF00);
                CHECK(s2_video_slots_overlay(&v,line,out,320)!=&v);
                CHECK(out[100]==0xFF00FF00);
            } else {
                uint8_t indices[GVDP_MAX_WIDTH];v.cram[1]=0xEE;
                CHECK(gvdp_render_scanline(&v,line,indices)==320);
                const uint32_t *host=gvdp_host_palette();unsigned index=indices[100];
                uint32_t native=index>=GVDP_HOST_PALETTE_BASE?(host?host[index-GVDP_HOST_PALETTE_BASE]:
                    genesis_dac_cram_to_argb(gvdp_ext_color(line,1+(int)(index-GVDP_HOST_PALETTE_BASE)/16,
                        (int)(index-GVDP_HOST_PALETTE_BASE)%16),GENESIS_DAC_NORMAL)):
                    genesis_dac_cram_to_argb(v.cram[index%64],GENESIS_DAC_NORMAL);
                CHECK(native==genesis_dac_cram_to_argb(0xEE,GENESIS_DAC_NORMAL));
                CHECK(s2_video_slots_overlay(&v,line,out,320)==&v);
            }
        }
    }
    /* Shared-screen co-op retains the leader's stage skin. Native demos
     * also retain Sonic/Tails, even with a configured custom roster. */
    word(g_ram,0xFFD8,0);strcpy(s2_party.roster.character[0],"sonic");strcpy(s2_party.roster.character[1],"knuckles");
    CHECK(!slot_theme(0));strcpy(s2_party.roster.character[0],"knuckles");g_ram[0xF600]=8;CHECK(!slot_theme(0));
    slot_mock=0;s2_party=old_party;hud_available=old_available;hud_art=old_art;
    clear_guest();s2_video_prepare_knuckles_items();
}
static void native_scene_art_test(void)
{
    CHECK(sonic2_video.configure("32:9"));CHECK(reserve_sprite_row(796));
    GVDP v={0};v.reg[12]=1;
    static SceneFrame scene;memset(&scene,0,sizeof scene);scene.scene=1;scene.count=1;s_display=&scene;
    uint32_t out[796],palette[64]={0};palette[1]=0xFFFF0000;palette[2]=0xFF00FF00;palette[17]=0xFFFFFFFF;
    static const unsigned bases[]={0x434,0x680,0x400,0x420,0x500,0x418};
    static const unsigned materials[]={SCENE_ART_SIGNPOST,SCENE_ART_CAPSULE,SCENE_ART_EHZ_CAR,SCENE_ART_EHZ_CAR,SCENE_ART_EHZ_CAR,SCENE_ART_EHZ_CAR};
    s_camera=s_camera_y=0;s_interpolating=0;
    for(unsigned asset=0;asset<sizeof bases/sizeof *bases;++asset) {
        s_frame_zone=asset>=S2_NATIVE_CPZ_POD?13:0;
        NativeSpriteArt *a=&s_native_sprite_art[asset];a->ready=1;a->bytes=32;memset(a->pixels,0x22,32);
        memset(v.vram+bases[asset]*32,0x11,32);
        scene.sprites[0]=(SceneSprite){700,0,(uint16_t)(0x8000|bases[asset]),0,(uint8_t)materials[asset]};
        memset(s_priority,0,796);for(unsigned x=0;x<796;++x)out[x]=0xFF123456;
        draw_scene_sprites(&v,0,out,796,0,palette);
        for(unsigned x=0;x<796;++x)CHECK(out[x]==(x>=700 && x<708?0xFF00FF00:0xFF123456));
        CHECK(native_scene_pixel(&v,materials[asset],(uint16_t)bases[asset],0,0,320)==1);
        a->pixels[0]=0x12;a->pixels[3]=0x34;a->pixels[28]=0x56;
        CHECK(native_scene_pixel(&v,materials[asset],(uint16_t)bases[asset],0,0,796)==1);
        CHECK(native_scene_pixel(&v,materials[asset],(uint16_t)(bases[asset]|0x800),0,0,796)==4);
        CHECK(native_scene_pixel(&v,materials[asset],(uint16_t)(bases[asset]|0x1000),0,0,796)==5);
    }
    static const unsigned extra_bases[]={0x332,0x3FF,0x422,0x402,0x39D};
    for(unsigned n=0;n<sizeof extra_bases/sizeof *extra_bases;++n) {
        unsigned asset=S2_NATIVE_OOZ_BLOCK_VERT+n,base=extra_bases[n];
        NativeSpriteArt *a=&s_native_sprite_art[asset];a->ready=1;a->bytes=32;
        memset(a->pixels,0x22,32);memset(v.vram+base*32,0x11,32);
        scene.sprites[0]=(SceneSprite){700,0,(uint16_t)(0x8000|base),0,0,(uint8_t)(asset+1)};
        memset(s_priority,0,796);memset(out,0,sizeof out);
        draw_scene_sprites(&v,0,out,796,0,palette);CHECK(out[700]==palette[2]);
        CHECK(native_asset_pixel(&v,asset,base,(uint16_t)base,0,0,320)==1);
        CHECK(native_asset_pixel(&v,asset,base,(uint16_t)base,0,0,796)==2);
        /* Out-of-range mappings and opt-out retain the live hardware art. */
        memset(v.vram+(base+1)*32,0x33,32);
        CHECK(native_asset_pixel(&v,asset,base,(uint16_t)(base+1),0,0,796)==3);
        a->pixels[3]=0x34;
        CHECK(native_asset_pixel(&v,asset,base,(uint16_t)(base|0x800),0,0,796)==4);
    }
    s_frame_zone=0;
    /* Capsule ink stays black during the boss flash, while an unmarked
     * object keeps the live white and an actual white fade remains white. */
    memset(s_native_sprite_art[S2_NATIVE_CAPSULE].pixels,0x11,32);v.cram[17]=0xEEE;
    scene.sprites[0]=(SceneSprite){700,0,0xA680,0,SCENE_ART_CAPSULE};
    memset(s_priority,0,796);draw_scene_sprites(&v,0,out,796,0,palette);CHECK(out[700]==0xFF000000);
    s_presented_fade=(DonorFade){FADE_TO_WHITE,22};
    memset(s_priority,0,796);draw_scene_sprites(&v,0,out,796,0,palette);CHECK(out[700]==0xFFFFFFFF);
    s_presented_fade=(DonorFade){0};scene.sprites[0].hud=0;
    memset(v.vram+0x680*32,0x11,32);memset(s_priority,0,796);
    draw_scene_sprites(&v,0,out,796,0,palette);CHECK(out[700]==0xFFFFFFFF);
    /* The mapping identity owns the material, never a coincident tile range. */
    static const unsigned maps[]={0x195BE,0x3F436,0x2FA58};
    clear_guest();
    for(unsigned n=0;n<3;++n) {
        unsigned map=maps[n];word(g_rom,map,2);word(g_rom,map+2,1);
        memset(g_rom+map+4,0,8);s_build.count=0;
        add_mapping(&s_build,NULL,0,map,0,bases[n],0,0,0,0,0);
        CHECK(s_build.count==1 && (s_build.sprites[0].hud&SCENE_ART_MASK)==materials[n]);
        word(g_ram,0xFFD8,1);g_ram[0xF600]=12;s_build.count=0;
        add_mapping(&s_build,NULL,0,map,0,bases[n],0,0,0,0,0);
        CHECK(s_build.count==1 && !(s_build.sprites[0].hud&SCENE_ART_MASK));
        word(g_ram,0xFFD8,0);g_ram[0xF600]=0;
    }
    memset(s_native_sprite_art,0,sizeof s_native_sprite_art);s_display=NULL;s_build.count=0;
}
static void native_art_rom_readiness_test(void)
{
    uint8_t header[4];memcpy(header,g_rom+0x100,4);
    unsigned calls=native_decode_calls,bytes=native_decode_bytes,asset=native_decode_asset;
    for(unsigned n=0;n<sizeof s_native_object_art/sizeof *s_native_object_art;++n) {
    const NativeObjectArt *b=&s_native_object_art[n];native_decode_asset=b->asset;
    NativeSpriteArt *art=&s_native_sprite_art[b->asset],saved=*art;
    memset(art,0,sizeof *art);memset(g_rom+0x100,0,4);
    native_decode_calls=0;native_decode_bytes=b->tiles*32;
    /* The actual settings-before-ROM sequence must leave a retryable cache,
     * then publish decoded pixels once the original image is present. */
    CHECK(!native_art_ready(b->asset));
    CHECK(!art->ready && !art->bytes && !native_decode_calls);
    memcpy(g_rom+0x100,"SEGA",4);
    CHECK(native_art_ready(b->asset));
    CHECK(art->ready==1 && art->bytes==b->tiles*32 && native_decode_calls==1);
    CHECK(art->pixels[0]==0x66 && art->pixels[b->tiles*32-1]==0x66);
    CHECK(native_art_ready(b->asset) && native_decode_calls==1);
    /* An already decoded cache remains independent of later ROM reads. */
    memset(g_rom+0x100,0,4);
    CHECK(native_art_ready(b->asset) && native_decode_calls==1);
    *art=saved;
    }
    memcpy(g_rom+0x100,header,4);
    native_decode_calls=calls;native_decode_bytes=bytes;native_decode_asset=asset;
}
static void wfz_catapult_native_art_test(void)
{
    clear_guest();memset(&g_cpu,0,sizeof g_cpu);
    g_ram[0xF600]=0x8C;g_ram[0xB000]=1;word(g_ram,0xFE10,0x600);
    g_ram[0xB400]=0xC0;glue_poke32(0xFFB404,0x3C098);word(g_ram,0xB402,0x245C);
    const NativeObjectArt *binding=native_object_binding(0xB400,0x3C098,0x245C);
    CHECK(binding && binding->asset==S2_NATIVE_WFZ_CATAPULT && binding->tiles==5);
    /* Wide host art does not mark or change the original native SAT. */
    g_cpu.A[0]=0xFFB400;g_cpu.A[1]=0x3C0A2;g_cpu.A[2]=0xFFF808;
    word(g_ram,0xF804,0x245C);static uint8_t before[65536];memcpy(before,g_ram,sizeof before);
    CHECK(!s2_video_hook(0x1684E));CHECK(!memcmp(before,g_ram,sizeof before));
    word(g_ram,0xFE10,0x700);CHECK(!native_object_binding(0xB400,0x3C098,0x245C));
    clear_guest();
}
static void explosion_native_art_test(void)
{
    static GVDP v;static uint8_t before[65536];uint8_t indices[GVDP_MAX_WIDTH];
    S2PartyConfig party=s2_party;int hud=hud_available,amy=amy_items_available,special=amy_special_available,imported=imported_hud_mock;
    NativeSpriteArt saved=s_native_sprite_art[S2_NATIVE_EXPLOSION];
    memset(&s2_party,0,sizeof s2_party);hud_available=amy_items_available=amy_special_available=imported_hud_mock=0;
    clear_guest();memset(&g_cpu,0,sizeof g_cpu);memset(&v,0,sizeof v);online=0;
    NativeSpriteArt *art=&s_native_sprite_art[S2_NATIVE_EXPLOSION];
    art->ready=1;art->bytes=68*32;memset(art->pixels,0x22,art->bytes);
    CHECK(configure("off"));s2_video_prepare_knuckles_items();
    unsigned bytes=0;const uint8_t *patterns=gvdp_sprite_patterns(0xC0,&bytes);
    CHECK(patterns && bytes==65536 && !memcmp(patterns+0x5A4*32,art->pixels,art->bytes));
    /* Only a native Obj27/map21120 publication selects this private atlas.
     * The unused SAT marker changes no object, PLC or combat state. */
    g_ram[0xB400]=0x27;glue_poke32(0xFFB404,0x21120);word(g_ram,0xB402,0x5A4);
    g_cpu.A[0]=0xFFB400;g_cpu.A[1]=0x2112A;g_cpu.A[2]=0xFFF808;
    word(g_ram,0xF804,0x5A4);memcpy(before,g_ram,sizeof before);
    CHECK(!s2_video_hook(0x1684E));CHECK(g_ram[0xF802]==0xC0);
    before[0xF802]|=0xC0;CHECK(!memcmp(before,g_ram,sizeof before));
    g_ram[0xF802]=0;g_ram[0xB400]=0x34;CHECK(!s2_video_hook(0x1684E) && !g_ram[0xF802]);
    g_ram[0xB400]=0x27;glue_poke32(0xFFB404,0x21122);
    CHECK(!s2_video_hook(0x1684E) && !g_ram[0xF802]);
    glue_poke32(0xFFB404,0x21120);word(g_ram,0xB402,0x5A5);
    CHECK(!s2_video_hook(0x1684E) && !g_ram[0xF802]);
    word(g_ram,0xB402,0x5A4);
    /* Native320: marked explosion and unmarked title glyph deliberately
     * share the same overwritten VRAM tile. Only the explosion is repaired. */
    v.reg[1]=64;v.reg[2]=0x30;v.reg[4]=7;v.reg[5]=0x7C;v.reg[12]=1;v.reg[16]=1;
    memset(v.vram+0x5A4*32,0x11,69*32);
    word(v.vram,0xF800,144);v.vram[0xF802]=0xC0;v.vram[0xF803]=1;
    word(v.vram,0xF804,0x5A4);word(v.vram,0xF806,144);
    word(v.vram,0xF808,144);word(v.vram,0xF80C,0x5A4);word(v.vram,0xF80E,168);
    CHECK(gvdp_render_scanline(&v,16,indices)==320 && indices[16]==2 && indices[40]==1);
    CHECK(native_asset_pixel(&v,S2_NATIVE_EXPLOSION,0x5A4,0x5A4,0,0,320)==2);
    CHECK(native_asset_pixel(&v,S2_NATIVE_EXPLOSION,0x5A4,0x5E7,0,0,320)==2);
    CHECK(native_asset_pixel(&v,S2_NATIVE_EXPLOSION,0x5A4,0x5E8,0,0,320)==1);
    CHECK(configure("32:9"));
    CHECK(native_asset_pixel(&v,S2_NATIVE_EXPLOSION,0x5A4,0x5A4,0,0,796)==2);
    /* Exact REV01 Obj27 pieces: small initial burst, then four 4x4
     * palette-1 frames spanning the complete 68-tile atlas. */
    static const uint8_t pieces[5][8]={
        {0xF8,5,0,0,0,0,0xFF,0xF8},
        {0xF0,15,0x20,4,0x20,2,0xFF,0xF0},
        {0xF0,15,0x20,0x14,0x20,0xA,0xFF,0xF0},
        {0xF0,15,0x20,0x24,0x20,0x12,0xFF,0xF0},
        {0xF0,15,0x20,0x34,0x20,0x1A,0xFF,0xF0}};
    for(unsigned f=0;f<5;++f) {
        unsigned mp=0x2112A+f*10;word(g_rom,0x21120+f*2,10+f*10);
        word(g_rom,mp,1);memcpy(g_rom+mp+2,pieces[f],8);
        s_build.count=0;add_mapping(&s_build,NULL,0xB4000000u,0x21120,f,0x5A4,0,0,0,0,0);
        CHECK(s_build.count==1 && s_build.sprites[0].art==1+S2_NATIVE_EXPLOSION);
        SceneSprite *piece=&s_build.sprites[0];
        CHECK(piece->size==(0xC0|pieces[f][1]));
        v.vram[0xF802]=(uint8_t)(0xC0|piece->size);word(v.vram,0xF804,piece->attr);
        word(v.vram,0xF80C,piece->attr);word(v.vram,0xF80E,192);
        CHECK(gvdp_render_scanline(&v,16,indices)==320 && indices[16]==(f?18:2) && indices[64]==(f?17:1));
        CHECK(native_asset_pixel(&v,S2_NATIVE_EXPLOSION,0x5A4,piece->attr,0,0,320)==(f?18:2));
        CHECK(native_asset_pixel(&v,S2_NATIVE_EXPLOSION,0x5A4,piece->attr,0,0,796)==(f?18:2));
    }
    s_build.count=0;add_mapping(&s_build,NULL,0,0x21120,0,0x5A5,0,0,0,0,0);
    CHECK(s_build.count==1 && !s_build.sprites[0].art);
    /* Donor animal palettes overlap the original explosion tile numbers.
     * The independent bank must preserve both materials simultaneously. */
    CHECK(S2_SPECIAL_FRAMES*S2_SPECIAL_TILES==0x240);
    hud_available=1;s2_party.s3k_enabled=1;strcpy(s2_party.roster.character[0],"knuckles");g_ram[0xF600]=12;
    s2_video_prepare_knuckles_items();
    CHECK(gvdp_sprite_palette_line(0x80,0x5A4,0)==2);
    CHECK(gvdp_sprite_palette_line(0xC0,0x5A4,0)==0);
    patterns=gvdp_sprite_patterns(0x80,&bytes);
    CHECK(patterns && patterns[0x4BE*32]==hud_art.shield_stars[0]);
    CHECK(patterns[0]==hud_art.special[0].tiles[0]);
    patterns=gvdp_sprite_patterns(0xC0,&bytes);
    CHECK(patterns && !memcmp(patterns+0x5A4*32,art->pixels,art->bytes));
    CHECK(gvdp_render_scanline(&v,16,indices)==320 && indices[16]==18 && indices[64]==17);
    amy_items_available=amy_special_available=1;strcpy(s2_party.roster.character[0],"amy");
    s2_video_prepare_knuckles_items();patterns=gvdp_sprite_patterns(0x80,&bytes);
    CHECK(patterns && patterns[0]==amy_special.frames[0].tiles[0]);
    CHECK(patterns[0x7D4*32]==amy_items.life[0]);
    CHECK(gvdp_sprite_palette_line(0xC0,0x5A4,0)==0);
    g_ram[0xF600]=16;s2_video_prepare_knuckles_items();
    CHECK(gvdp_sprite_palette_line(0x80,0,0)==3 && gvdp_sprite_palette_line(0xC0,0x5A4,0)==0);
    patterns=gvdp_sprite_patterns(0xC0,&bytes);
    CHECK(patterns && !memcmp(patterns+0x5A4*32,art->pixels,art->bytes));
#if OWN_BACKEND
    CHECK(!shadow_sprite_tagged(v.vram,0xF800,0));
    word(v.vram,0xF804,0x4DE);CHECK(shadow_sprite_tagged(v.vram,0xF800,0));
    word(v.vram,0xF804,0x26F);CHECK(shadow_sprite_tagged(v.vram,0xF800,1));
    word(v.vram,0xF804,0x25D8);
#endif
    memset(&s2_party,0,sizeof s2_party);hud_available=amy_items_available=amy_special_available=0;g_ram[0xF600]=0;
    /* A failed decode retains the native tile for both render paths. */
    art->ready=2;art->bytes=0;s2_video_prepare_knuckles_items();
    CHECK(!gvdp_sprite_patterns(0xC0,&bytes));
    CHECK(gvdp_render_scanline(&v,16,indices)==320 && indices[16]==17 && indices[64]==17);
    CHECK(native_asset_pixel(&v,S2_NATIVE_EXPLOSION,0x5A4,0x5A4,0,0,796)==1);
    s_native_sprite_art[S2_NATIVE_EXPLOSION]=saved;s2_party=party;
    hud_available=hud;amy_items_available=amy;amy_special_available=special;
    imported_hud_mock=imported;
    s_build.count=0;CHECK(configure("off"));s2_video_prepare_knuckles_items();clear_guest();
}
static void ehz_animal_native_art_test(void)
{
    static GVDP v;static uint8_t before[65536];uint8_t indices[GVDP_MAX_WIDTH];
    NativeSpriteArt saved[2];S2PartyConfig party=s2_party;
    int hud=hud_available,amy=amy_items_available,special=amy_special_available,imported=imported_hud_mock;
    memset(&s2_party,0,sizeof s2_party);hud_available=amy_items_available=amy_special_available=imported_hud_mock=0;
    clear_guest();memset(&v,0,sizeof v);memset(&g_cpu,0,sizeof g_cpu);online=0;
    g_ram[0xF600]=12;g_ram[0xF711]=1;g_ram[0xB000]=1;g_ram[0xB400]=0x28;
    const unsigned assets[]={S2_NATIVE_FLICKY,S2_NATIVE_SQUIRREL};
    for(unsigned n=0;n<2;++n) {
        const NativeObjectArt *b=native_asset_binding(assets[n]);
        NativeSpriteArt *art=&s_native_sprite_art[b->asset];saved[n]=*art;
        art->ready=1;art->bytes=b->tiles*32;memset(art->pixels,n?0x44:0x33,art->bytes);
    }
    s2_video_prepare_knuckles_items();
    v.reg[1]=64;v.reg[2]=0x30;v.reg[4]=7;v.reg[5]=0x7C;v.reg[12]=1;v.reg[16]=1;
    for(unsigned n=0;n<2;++n) {
        const NativeObjectArt *b=native_asset_binding(assets[n]);
        word(g_ram,0xB402,b->base);glue_poke32(0xFFB404,b->map);
        /* Exact native animal mapping frames, including the16x32 release pose. */
        const unsigned offsets[]={16,26,6},tiles[]={8,n?14:12,0};
        for(unsigned f=0;f<3;++f) {
            word(g_rom,b->map+f*2,offsets[f]);unsigned p=b->map+offsets[f];
            word(g_rom,p,1);g_rom[p+2]=(uint8_t)(f==2?-20:-8);
            g_rom[p+3]=(uint8_t)(f==2?7:n?9:5);word(g_rom,p+4,tiles[f]);
            word(g_rom,p+6,tiles[f]/2);word(g_rom,p+8,(n && f!=2)?-12:-8);
            s_build.count=0;add_mapping(&s_build,NULL,0xB4000000u,b->map,f,b->base,0x100,0,20,0,0);
            CHECK(s_build.count==1 && s_build.sprites[0].art==1+b->asset);
            CHECK((s_build.sprites[0].size&0xC0)==0xC0);
            CHECK(s_build.sprites[0].x==((n && f!=2)?-12:-8));
            CHECK(s_build.sprites[0].y==(f==2?0:12));
            CHECK(native_asset_pixel(&v,b->asset,b->base,b->base+tiles[f],0,0,320)==3+n);
            CHECK(native_asset_pixel(&v,b->asset,b->base,b->base+tiles[f],0,0,796)==3+n);
        }
        g_cpu.A[0]=0xFFB400;g_cpu.A[2]=0xFFF808;word(g_ram,0xF804,b->base);g_ram[0xF802]=0;
        memcpy(before,g_ram,sizeof before);s2_video_hook(0x1684E);before[0xF802]=0xC0;
        CHECK(!memcmp(before,g_ram,sizeof before));
        word(v.vram,0xF800,128);v.vram[0xF802]=0xC0;v.vram[0xF803]=1;
        word(v.vram,0xF804,b->base);word(v.vram,0xF806,128);
        word(v.vram,0xF808,128);word(v.vram,0xF80C,b->base);word(v.vram,0xF80E,168);
        memset(v.vram+b->base*32,0x11,b->tiles*32);
        CHECK(gvdp_render_scanline(&v,0,indices)==320 && indices[0]==3+n && indices[40]==1);
#if OWN_BACKEND
        CHECK(!shadow_sprite_tagged(v.vram,0xF800,0));
#endif
        word(g_ram,0xFE10,0x100);CHECK(!native_object_binding(0xB400,b->map,b->base));
        g_ram[0xF802]=0;s2_video_hook(0x1684E);CHECK(!g_ram[0xF802]);
        word(g_ram,0xFE10,0);g_ram[0xF600]=0x20;CHECK(!native_object_binding(0xB400,b->map,b->base));
        g_ram[0xF600]=12;g_ram[0xB400]=0x29;CHECK(!native_object_binding(0xB400,b->map,b->base));
        g_ram[0xB400]=0x28;CHECK(!native_object_binding(0xB400,b->map+2,b->base));
        CHECK(!native_object_binding(0xB400,b->map,b->base+1));
    }
    g_ram[0xB400]=0x29;glue_poke32(0xFFB404,0x11ED0);word(g_ram,0xB402,0x84AC);
    word(g_ram,0xF804,0x84AE);g_ram[0xF802]=0;
    s2_video_hook(0x1684E);CHECK(!g_ram[0xF802]); /* Original points keep live VRAM. */
    /* Native animal ink remains in live CRAM alongside cached donor/special art. */
    for(unsigned theme=0;theme<2;++theme) {
        hud_available=1;amy_items_available=amy_special_available=(int)theme;
        strcpy(s2_party.roster.character[0],theme?"amy":"knuckles");s2_party.s3k_enabled=1;
        s2_video_prepare_knuckles_items();unsigned bytes=0;
        const uint8_t *patterns=gvdp_sprite_patterns(0xC0,&bytes);CHECK(patterns && bytes==65536);
        CHECK(patterns[0x580*32]==0x44 && patterns[0x594*32]==0x33);
        CHECK(!gvdp_sprite_palette_line(0xC0,0x580,0) && !gvdp_sprite_palette_line(0xC0,0x594,0));
        CHECK(gvdp_sprite_palette_line(0x80,0x580,0)==2);
    }
    for(unsigned n=0;n<2;++n)s_native_sprite_art[assets[n]]=saved[n];
    s2_party=party;hud_available=hud;amy_items_available=amy;amy_special_available=special;imported_hud_mock=imported;
    s_build.count=0;CHECK(configure("off"));s2_video_prepare_knuckles_items();clear_guest();
}
static void ooz_static_fragment_art_test(void)
{
    static GVDP v;uint32_t out[796],palette[64]={0};static uint8_t before[65536];
    NativeSpriteArt saved=s_native_sprite_art[S2_NATIVE_OOZ_COLLAPSE];
    NativeSpriteArt *art=&s_native_sprite_art[S2_NATIVE_OOZ_COLLAPSE];
    clear_guest();memset(&v,0,sizeof v);g_ram[0xF600]=12;g_ram[0xB000]=1;
    word(g_ram,0xFE10,0xA01);g_ram[0xB400]=0x1F;
    art->ready=1;art->bytes=40*32;memset(art->pixels,0x22,art->bytes);
    memset(art->pixels+16*32,0x33,16*32);memset(art->pixels+32*32,0x44,8*32);
    memset(v.vram+0x39D*32,0x11,40*32);v.reg[12]=1;
    static const uint8_t pieces[7][8]={
        {0xF0,0x0F,0x08,0x10,0x08,0x08,0xFF,0xC0},
        {0xF0,0x0F,0x08,0x10,0x08,0x08,0xFF,0xE0},
        {0xF0,0x0F,0x08,0x10,0x08,0x08,0x00,0x00},
        {0xF0,0x0F,0x08,0x00,0x08,0x00,0x00,0x20},
        {0x10,0x0D,0x08,0x20,0x08,0x10,0xFF,0xC0},
        {0x10,0x0D,0x08,0x20,0x08,0x10,0xFF,0xE0},
        {0x10,0x0D,0x08,0x20,0x08,0x10,0x00,0x00}};
    memcpy(g_rom+0x110CE,pieces,sizeof pieces);word(g_rom,0x110C6,6);word(g_rom,0x110CC,7);
    CHECK(configure("32:9"));CHECK(reserve_sprite_row(796));s_interpolating=0;
    s_frame_level=1;s_frame_vs=0;s_camera=s_camera_y=0;
    palette[49]=0xFFFF0000;palette[50]=0xFF223344;palette[51]=0xFF556677;palette[52]=0xFF8899AA;
    memcpy(before,g_ram,sizeof before);
    for(unsigned i=0;i<7;++i) {
        unsigned map=0x110CE + i*8;s_build.count=0;s_build.scene=1;s_build.camera_x=s_build.camera_y=0;
        add_mapping(&s_build,NULL,0xB4000000u,map,0,0x639D,0x24,100,80,0,1);
        CHECK(s_build.count==1 && s_build.sprites[0].art==1+S2_NATIVE_OOZ_COLLAPSE);
        SceneSprite *q=&s_build.sprites[0];CHECK(q->size==pieces[i][1]);
        CHECK(q->x==100+(int16_t)((pieces[i][6]<<8)|pieces[i][7]));
        CHECK(q->y==80+(int8_t)pieces[i][0]);
        unsigned ink=i<3?51:i==3?50:52;
        CHECK(native_asset_pixel(&v,S2_NATIVE_OOZ_COLLAPSE,0x39D,q->attr,0,0,796)==ink);
        CHECK(native_asset_pixel(&v,S2_NATIVE_OOZ_COLLAPSE,0x39D,q->attr,0,0,320)==49);
        s_display=&s_build;memset(s_priority,0,796);memset(out,0,sizeof out);
        draw_scene_sprites(&v,q->y,out,796,0,palette);CHECK(out[q->x]==palette[ink]);
    }
    CHECK(!memcmp(before,g_ram,sizeof before));
    s_build.count=0;add_mapping(&s_build,NULL,0xB4000000u,0x110C6,0,0x639D,4,100,80,0,0);
    CHECK(s_build.count==7);for(unsigned i=0;i<7;++i)CHECK(s_build.sprites[i].art==1+S2_NATIVE_OOZ_COLLAPSE);
    CHECK(!ooz_collapse_material(0x110CE,0x639D,0));
    CHECK(!ooz_collapse_material(0x110CF,0x639D,1));
    CHECK(!ooz_collapse_material(0x11106,0x639D,1));
    CHECK(!ooz_collapse_material(0x110CE,0x639E,1));
    word(g_ram,0xFE10,0xB00);CHECK(!ooz_collapse_material(0x110CE,0x639D,1));
    *art=saved;s_display=NULL;s_build.count=0;CHECK(configure("off"));clear_guest();
}
static void wfz_finite_background_ownership_test(void)
{
    static GVDP v;static uint8_t before[65536];clear_guest();memset(&v,0,sizeof v);
    /* Authored pre-Earth tails25/50 are clouds over transparent sky.
     * Terminal55/51 repeat in the last two128px columns of the finite scene. */
    for(unsigned row=0;row<2;++row) {
        g_ram[0x8080+row*256+(0x2A80>>7)]=(uint8_t)(row?0x50:0x25);
        g_ram[0x8080+row*256+(0x2B00>>7)]=(uint8_t)(row?0xAD:0x9D);
        g_ram[0x8080+row*256+(0x3380>>7)]=(uint8_t)(row?0x51:0x55);
        g_ram[0x8080+row*256+(0x3400>>7)]=(uint8_t)(row?0x51:0x55);
    }
    const unsigned chunks[]={0x25,0x50,0x9D,0xAD,0x55,0x51};
    for(unsigned i=0;i<6;++i)for(unsigned b=0;b<64;++b)word(g_ram,chunks[i]*128+b*2,i/2+1);
    for(unsigned b=1;b<=3;++b) {
        for(unsigned cell=0;cell<4;++cell)word(g_ram,0x9000+b*8+cell*2,b);
        memset(v.vram+b*32,(int)(b*17),32);
    }
    memcpy(before,g_ram,sizeof before);
    for(unsigned row=0;row<2;++row) {
        int early_y=-4067+(int)row*128,late_y=-6136+(int)row*128;
        int sky=wfz_background_source(0x2B00,0x2B00,early_y,0x2950,320,1,6);
        CHECK(sky==0x2A80 && pattern_pixel(&v,world_attr(g_ram,sky,early_y,1),sky,early_y)==1);
        int space=wfz_background_source(0x3480,0,late_y,0x3330,320,1,6);
        CHECK(space==0x3400 && pattern_pixel(&v,world_attr(g_ram,space,late_y,1),space,late_y)==3);
        /* Actual clamped796px view: native origin276, early slab begins
         * at added x708; the old terminal wrap cuts to blue at added x612. */
        for(int x=0;x<796;++x) {
            int nx=x-276,margin=nx<0 || nx>=320;
            int raw=0x2950+nx,source=raw%0x3480;
            int selected=wfz_background_source(raw,source,early_y,0x2950,320,margin,6);
            if(!margin)CHECK(selected==source);
            if(raw>=0x2B00)CHECK(selected==0x2A80+(raw&127));
            raw=0x3330+nx;source=raw%0x3480;
            selected=wfz_background_source(raw,source,late_y,0x3330,320,margin,6);
            if(!margin)CHECK(selected==source);
            if(raw>=0x3480)CHECK(selected==0x3400+(raw&127));
        }
    }
    CHECK(wfz_background_source(0x2B10,0x2B10,29,0x29C0,320,1,6)==0x2A90); /* Right edge is exclusive. */
    CHECK(wfz_background_source(0x2B10,0x2B10,29,0x29C1,320,1,6)==0x2B10); /* Native scene now owns Earth. */
    CHECK(wfz_background_source(0x3488,8,8,0x3330,320,0,6)==8); /* Native320 untouched. */
    CHECK(wfz_background_source(0x3488,8,8,0x3330,320,1,7)==8);
    CHECK(wfz_background_source(0x3488,8,256,0x3330,320,1,6)==8);
    CHECK(wfz_background_source(0x3488,8,0x580,0x3330,320,1,6)==8); /* Getaway-ship rows. */
    CHECK(!memcmp(before,g_ram,sizeof before));clear_guest();
}
static void result_entry_travel_test(void)
{
    static SceneFrame scene;static uint8_t before[65536];S2PartyConfig party=s2_party;
    clear_guest();memset(&s2_party,0,sizeof s2_party);strcpy(s2_party.roster.character[0],"sonic");
    g_ram[0xF600]=12;g_ram[0xB400]=0x3A;glue_poke32(0xFFB404,0x14CBC);
    word(g_ram,0xAC00,2);word(g_ram,0xAC02,0xB400);
    word(g_ram,0xB40A,128);word(g_rom,0x14CBC,2);word(g_rom,0x14CBE,1);
    memset(g_rom+0x14CC0,0,8);
    const unsigned routines[]={2,4,6,8,0x16,0x12,0x14,0xA,0xC,0xE,0x10};
    CHECK(configure("32:9"));
    for(unsigned r=0;r<sizeof routines/sizeof *routines;++r)for(unsigned side=0;side<2;++side)for(unsigned settled=0;settled<2;++settled) {
        unsigned target=side?0x100:0x120;int native=settled?(int)target-128:side?336:-48;
        int wide=settled?native:side?645:-357;
        word(g_ram,0xB430,target);g_ram[0xB424]=(uint8_t)routines[r];
        word(g_ram,0xB408,settled?target:side?0x1D0:0x50);
        memcpy(before,g_ram,sizeof before);capture_objects_into(&scene,320,1,NULL);
        CHECK(scene.count==1 && scene.sprites[0].x==native);
        capture_objects_into(&scene,796,1,NULL);
        CHECK(scene.count==1 && scene.sprites[0].x==(r<5?wide:native));
        CHECK(!memcmp(before,g_ram,sizeof before));
    }
    s2_party=party;CHECK(configure("off"));clear_guest();
}
static void demo_native_simulation_test(void)
{
    static uint8_t original[65536];
    static const unsigned hooks[]={0x17B84,0x17C50,0x17CCC,0x170D0,0x16F16,0x16F3E,0xF7D4};
    for(unsigned vs=0;vs<2;++vs)for(unsigned phase=0;phase<3;++phase) {
        clear_guest();memset(&g_cpu,0,sizeof g_cpu);
        g_ram[0xF600]=(uint8_t)(phase==0?8:phase==1?0:4);
        g_ram[0xF711]=1;g_ram[0xB000]=1;word(g_ram,0xFFD8,vs);word(g_ram,0xFFF0,1);
        CHECK(sonic2_video.configure("32:9"));
        CHECK(sonic2_video.width(1,1,320,224)==796);
        CHECK(s2_video_main_cpu_divisor()==1);
        s_loader_active=1;
        g_cpu.D[0]=0x12345678;g_cpu.A[1]=0xFFEE0001;g_cpu.A[2]=0xFFEE0002;
        M68KState original_cpu=g_cpu;memcpy(original,g_ram,sizeof original);
        for(unsigned n=0;n<sizeof hooks/sizeof *hooks;++n) {
            CHECK(!s2_video_hook(hooks[n]));
            CHECK(!memcmp(original,g_ram,sizeof original));
            CHECK(!memcmp(&original_cpu,&g_cpu,sizeof g_cpu));
        }
    }
    s_loader_active=0;
}
static void outgoing_competition_mapping_test(void)
{
    clear_guest();memset(s_history,0,sizeof s_history);s_serial=0;
    g_ram[0xF600]=0;g_ram[0xF711]=1;g_ram[0xB000]=1;word(g_ram,0xFFD8,1);
    word(g_ram,0xE808,0xFFFF);
    CHECK(!competition() && presented_competition() && !presented_gameplay());
    CHECK(s2_video_main_cpu_divisor()==1);
    /* One original IM2 mapping with different normal/interlace tile words.
     * The outgoing native object queue must retain its interlace geometry. */
    word(g_rom,0x100,2);word(g_rom,0x102,1);
    g_rom[0x104]=0;g_rom[0x105]=5;word(g_rom,0x106,1);word(g_rom,0x108,2);word(g_rom,0x10A,0);
    g_ram[0xB400]=0x26;g_ram[0xB401]=0x84;word(g_ram,0xB404,0);word(g_ram,0xB406,0x100);
    word(g_ram,0xB408,100);word(g_ram,0xB40C,50);g_ram[0xB419]=16;
    word(g_ram,0xAC00,2);word(g_ram,0xAC02,0xB400);
    CHECK(!s2_video_hook(0x16604));
    CHECK(s_build.scene==3 && s_build.count==1 && (s_build.sprites[0].attr&2047)==2);
    CHECK(s_build.sprites[0].size==4 && s_build.sprites[0].x==100 && s_build.sprites[0].y==50);
    CHECK(!s2_video_hook(0x16B90));CHECK(s_history[s_serial%3].scene==3);
    g_ram[0xF711]=0;CHECK(!presented_competition());
    clear_guest();memset(s_history,0,sizeof s_history);s_serial=0;
}
static void widened_card_front_ui_test(void)
{
    CHECK(sonic2_video.configure("32:9"));CHECK(reserve_sprite_row(796));
    GVDP v={0};uint32_t out[796];
    memset(s_priority,0,796);s_frame_level=s_frame_vs=0;s_frame_card=1;
    for(unsigned x=0;x<796;++x)out[x]=0xFF00AA00;
    /* The completed widened card puts its glyph at a different location
     * from the original centered SAT. Restore that exact published ink. */
    s_priority[700]=4;s_front_rgb[700]=0xFFFFFFFF;
    s2_video_front_ui(&v,40,out,796);
    for(unsigned x=0;x<796;++x)CHECK(out[x]==(x==700?0xFFFFFFFF:0xFF00AA00));
    s_frame_card=0;memset(s_priority,0,796);
}
static void sega_outgoing_composition_test(void)
{
    static GVDP v;
    uint32_t native[320],logo[398],outgoing[398];
    clear_guest();memset(&v,0,sizeof v);s_display=NULL;
    v.reg[1]=64;v.reg[2]=0x30;v.reg[4]=5;v.reg[12]=1;
    v.reg[13]=0x3C;v.reg[16]=1;
    v.cram[1]=0xEEE;v.cram[2]=0x00E;
    memset(v.vram+32,0x11,32);memset(v.vram+64,0x22,32);
    for(unsigned y=0;y<32;++y)for(unsigned x=0;x<64;++x)
        word(v.vram,0xA000+(y*64+x)*2,x<40?1:2);
    for(unsigned x=0;x<320;++x)native[x]=genesis_dac_cram_to_argb(0xEEE,GENESIS_DAC_NORMAL);
    CHECK(sonic2_video.configure("16:9"));CHECK(reserve_sprite_row(398));
    s_render->frame_menu_background=0;g_ram[0xB040]=0xB0;
    g_ram[0xF600]=0;sonic2_video.scanline(&v,0,native,320,logo,398);
    /* The next mode is already title while the completed raster is SEGA.
     * Unpopulated plane columns deliberately contain old red stage tiles. */
    g_ram[0xF600]=4;sonic2_video.scanline(&v,0,native,320,outgoing,398);
    CHECK(!memcmp(logo,outgoing,sizeof logo));
    CHECK(outgoing[0]==native[0] && outgoing[397]==native[319]);
    CHECK(sonic2_video.configure("off"));
}
static void death_egg_escape_fade_test(void)
{
    s2_video_hook(0x3D9BE);
    CHECK(s_donor_fade.mode==FADE_TO_WHITE && s_donor_fade.steps==0);
    for(unsigned tick=1;tick<=22;++tick) {
        s2_video_hook(0x3D9D6);
        CHECK(s_donor_fade.mode==FADE_TO_WHITE && s_donor_fade.steps==tick);
    }
    CHECK(donor_fade_color(0x002,s_donor_fade)==0xEEE);
    s2_video_hook(0x3D9D6);
    CHECK(s_donor_fade.steps==22);
    s_donor_fade=(DonorFade){0};
}

static void composed_menu_fade_test(void)
{
    uint32_t pixels[512];
    for(unsigned mode=FADE_FROM_BLACK;mode<=FADE_TO_WHITE;++mode)
        for(unsigned steps=0;steps<=22;++steps) {
            s_presented_fade=(DonorFade){mode,steps};
            for(unsigned i=0;i<512;++i) {
                unsigned shade=((i&7)<<1)|((i&0x38)<<2)|((i&0x1C0)<<3);
                pixels[i]=genesis_dac_cram_to_argb((uint16_t)shade,GENESIS_DAC_NORMAL);
            }
            s2_video_donor_rgb_row(pixels,512);
            for(unsigned i=0;i<512;++i) {
                unsigned shade=((i&7)<<1)|((i&0x38)<<2)|((i&0x1C0)<<3);
                int entry=mode==FADE_FROM_BLACK || mode==FADE_FROM_WHITE;
                int white=mode==FADE_FROM_WHITE || mode==FADE_TO_WHITE;
                unsigned result=entry?(white?0xEEE:0):shade;
                /* Iterate the original routine: each update changes only
                 * the first channel that has not reached its target. */
                for(unsigned step=0;step<steps;++step)for(unsigned n=0;n<3;++n) {
                    unsigned shift=(entry?2-n:n)*4,mask=14u<<shift;
                    unsigned current=(result&mask)>>shift;
                    unsigned target=entry?((shade&mask)>>shift):(white?14:0);
                    if(current==target)continue;
                    result=(result&~mask)|((current<target?current+2:current-2)<<shift);
                    break;
                }
                CHECK(pixels[i]==genesis_dac_cram_to_argb((uint16_t)result,GENESIS_DAC_NORMAL));
            }
        }
    s_donor_fade=s_presented_fade=(DonorFade){0};
}
static void demo_readonly_publication_test(void)
{
    static uint8_t original[65536];
    CHECK(sonic2_video.configure("32:9"));CHECK(sonic2_video.width(1,1,320,224)==796);
    for(unsigned phase=0;phase<3;++phase) {
        clear_guest();memset(s_history,0,sizeof s_history);s_serial=0;
        g_ram[0xF600]=(uint8_t)(phase==0?8:phase==1?0:4);
        g_ram[0xB000]=1;g_ram[0xF711]=1;word(g_ram,0xFFD8,1);word(g_ram,0xFFF0,1);
        g_ram[0x800A]=1;word(g_ram,0xE808,0xFFFF);
        word(g_rom,0x100,2);word(g_rom,0x102,1);
        g_rom[0x104]=0;g_rom[0x105]=5;word(g_rom,0x106,1);word(g_rom,0x108,2);word(g_rom,0x10A,0);
        g_ram[0xB400]=0x26;g_ram[0xB401]=4;word(g_ram,0xB404,0);word(g_ram,0xB406,0x100);
        word(g_ram,0xB408,600);word(g_ram,0xB40C,50);g_ram[0xB419]=16;
        word(g_ram,0xAC00,2);word(g_ram,0xAC02,0xB400);
        g_ram[0xC100]=0x26;g_ram[0xC101]=4;s_visible_count=1;s_visible_objects[0]=0xC100;
        memcpy(original,g_ram,sizeof original);
        CHECK(!s2_video_hook(0x16604));CHECK(s_build.scene==3 && s_build.count==1);
        CHECK(s_visible_count==0);CHECK(!s2_video_hook(0x16B90));
        CHECK(!memcmp(original,g_ram,sizeof original));
    }
    /* Actual play retains the expanded activation contract. */
    g_ram[0xF600]=12;word(g_ram,0xFFF0,0);
    CHECK(!s2_video_hook(0x16604));CHECK(!s2_video_hook(0x16B90));
    CHECK(g_ram[0xB401]==0x84 && g_ram[0xC101]==4);
    clear_guest();memset(s_history,0,sizeof s_history);s_serial=s_visible_count=0;
}
static void native_demo_hud_test(void)
{
    S2PartyConfig saved=s2_party;int available=hud_available;
    clear_guest();g_ram[0xB000]=1;g_ram[0xF711]=1;
    word(g_ram,0xFFD8,1);word(g_ram,0xFFF0,1);
    strcpy(s2_party.roster.character[0],"knuckles");strcpy(s2_party.roster.character[1],"shadow");
    s2_party.s3k_enabled=1;hud_available=1;
    GVDP v={0};v.reg[1]=64;v.reg[5]=0x7C;v.reg[12]=7;
    memset(v.vram+64,0x11,64);uint32_t palette[64]={0},out[320];palette[1]=0xFFFFFFFF;
    for(unsigned mode=0;mode<=8;mode+=4)for(unsigned p=0;p<2;++p) {
        g_ram[0xF600]=(uint8_t)mode;
        unsigned y=p?424:200;
        word(v.vram,0xF800,y+256);word(v.vram,0xF802,0);
        word(v.vram,0xF804,0x8001);word(v.vram,0xF806,144);
        for(unsigned x=0;x<320;++x)out[x]=0xFFFF0000;
        competition_hud_sprites(&v,(int)y,out,320,palette);
        CHECK(out[16]==0xFFFFFFFF);
    }
    uint32_t color=0;
    word(g_ram,0xFFD8,0);g_ram[0xF600]=8;
    CHECK(!s2_video_knuckles_hud_pixel(&v,0x7D4,0,0,&color));
    g_ram[0xF600]=12;CHECK(s2_video_knuckles_hud_pixel(&v,0x7D4,0,0,&color));
    s2_party=saved;hud_available=available;clear_guest();
}
static void competition_intro_alignment_test(void)
{
    /* Each split's original CNZ scroll row uses a different band offset. */
    CHECK(competition_ehz_background_scroll(320,20)==0);
    CHECK(competition_ehz_background_scroll(320,22)==5);
    CHECK(competition_ehz_background_scroll(321,224+100)==6);
    CHECK(competition_ehz_background_scroll(321,112)==21);
    CHECK(competition_ehz_background_scroll(321,128)==32);
    CHECK(competition_ehz_background_scroll(320,144)==40);
    CHECK(competition_ehz_background_scroll(320,192)==100);
    CHECK(competition_mcz_background_scroll(320,37)==288);
    CHECK(competition_mcz_background_scroll(320,38)==256);
    CHECK(competition_mcz_background_scroll(321,511)==288);
    CHECK(competition_mcz_background_scroll(320,512)==288);
    CHECK(competition_cnz_background_scroll(280,9,0)==280);
    CHECK(competition_cnz_background_scroll(280,89,0)==126);
    CHECK(competition_cnz_background_scroll(243,89,1)==83);
    CHECK(competition_cnz_background_scroll(280,128,0)==17);
    CHECK(competition_cnz_background_scroll(280,144,0)==35);
    CHECK(sonic2_video.configure("32:9"));CHECK(reserve_sprite_row(796));
    GVDP v={0};v.reg[1]=64;v.reg[2]=0x30;v.reg[4]=7;v.reg[12]=7;v.reg[16]=1;
    memset(v.vram+64,0x11,64);s_video_frame=v;memset(s_world_frame,0,sizeof s_world_frame);
    uint32_t native[320],out[796],palette[64]={0};palette[0]=0xFF000000;palette[1]=0xFFFFFFFF;
    for(unsigned x=0;x<320;++x)native[x]=0xFFFF0000;
    s_frame_vs=1;s_vs_ready=0;s_vs_world=1;s_frame_zone=0;
    memset(s_vs_camera,0,sizeof s_vs_camera);
    static SceneFrame scene;memset(&scene,0,sizeof scene);scene.scene=3;scene.count=1;
    scene.sprites[0]=(SceneSprite){20,50,0x8001,0,0};s_display=&scene;
    for(unsigned p=0;p<2;++p) {
        CHECK(competition_scanline(&v,50+(int)p*224,native,320,out,796,NULL,NULL,palette));
        for(unsigned x=0;x<796;++x)CHECK(out[x]==(x>=20 && x<28?0xFFFFFFFF:0xFF000000));
    }
    /* A non-geometric foreground glyph must not wrap into a margin. */
    s_vs_world=0;s_display=NULL;word(v.vram,0xC000+3*64*2,0x8001);
    CHECK(competition_scanline(&v,50,native,320,out,796,NULL,NULL,palette));
    CHECK(out[750]==0xFF000000);
    /* The last CNZ2P band follows the wide view's own camera despite an
     * invalid live row from the native lower-half segment table. */
    s_vs_ready=s_vs_world=1;s_frame_zone=12;
    v.reg[11]=3;v.reg[13]=63;v.vsram[1]=(-224)&1023;
    s_world_frame[0x8180]=1;
    for(unsigned x=0;x<8;++x)word(s_world_frame,128+32+x*2,1);
    word(s_world_frame,0x9008,1);word(s_world_frame,0x900A,1);
    for(unsigned row=0;row<16;++row) {
        v.vram[64+row*4]=0x12;v.vram[65+row*4]=0x34;
        v.vram[66+row*4]=0x56;v.vram[67+row*4]=0x78;
    }
    s_video_frame=v;
    palette[2]=0xFF0000FF;
    int line=224+160;
    unsigned hscroll=0xFC00+(unsigned)(line/2)*4;
    word(v.vram,hscroll,0xFF00);word(v.vram,hscroll+2,0xBE6D);
    s_vs_camera[1][0]=256;s_vs_camera[1][3]=0;
    CHECK(competition_scanline(&v,line,native,320,out,796,NULL,NULL,palette));
    CHECK(out[0]==palette[1] && out[1]==palette[2]);
    s_vs_camera[1][3]=8;
    CHECK(competition_scanline(&v,line,native,320,out,796,NULL,NULL,palette));
    CHECK(out[0]==palette[2]);
    s_frame_vs=s_vs_ready=s_vs_world=0;s_display=NULL;
}
static void menu_background_lifetime_test(void)
{
    clear_guest();memset(s_history,0,sizeof s_history);
    s_display=NULL;s_serial=0;s_menu_background_ready=0;
    GVDP v={0};v.reg[1]=64;v.reg[2]=0x30;v.reg[4]=7;
    v.reg[12]=1;v.reg[13]=0x28;v.reg[16]=1;
    static const uint16_t shades[]={0,0x00E,0x0E0,0xE00,0x0EE,0xE0E,0xEE0,0xEEE};
    uint32_t colors[8];
    for(unsigned i=1;i<8;++i) {
        v.cram[48+i]=shades[i];memset(v.vram+i*32,(int)(i*17),32);
        colors[i]=genesis_dac_cram_to_argb(shades[i],GENESIS_DAC_NORMAL);
    }
    uint32_t native[320],out[796];
    for(unsigned col=0;col<40;++col) {
        unsigned attr=0x6000|(col?col%6+1:7);
        word(g_ram,col*2,attr);word(v.vram,0xE000+col*2,attr);
    }
    CHECK(!s2_video_hook(0x8C92));
    CHECK(s_menu_background_ready);
    for(unsigned x=0;x<320;++x)native[x]=colors[x<8?7:(x/8)%6+1];
    CHECK(sonic2_video.configure("32:9"));
    for(unsigned phase=0;phase<3;++phase) {
        g_ram[0xF600]=(uint8_t)(phase==0?0x24:phase==1?0x1C:4);
        for(int width=398;width<=796;width+=398) {
            render_source_scanline(&v,0,native,320,out,width,NULL,NULL,-1);
            CHECK(s_render->frame_menu_background);
            for(int x=0;x<width;++x) {
                int bx=x-(width-320)/2,phase=bx%48;if(phase<0)phase+=48;
                unsigned color=bx>=0 && bx<8?7:(unsigned)phase/8+1;
                CHECK(out[x]==colors[color]);
            }
        }
    }
    word(v.vram,0xE000,0x4001); /* The incoming title owns a different map. */
    render_source_scanline(&v,0,native,320,out,796,NULL,NULL,-1);
    CHECK(!s_render->frame_menu_background);
    s_menu_background_ready=0;
}
static void imported_character_hud_test(void)
{
    S2PartyConfig saved=s2_party;
    GVDP v={0};uint32_t color;
    clear_guest();g_ram[0xF600]=12;
    memset(amy_items.life,0x11,sizeof amy_items.life);
    memset(v.vram+0x7DD*32,0x22,32);
    memset(v.vram+0x7DF*32,0x22,32);
    amy_item_bank.palette[1]=0xEEE;v.cram[2]=0x00E;
    imported_hud_mock=1;
    const char *names[]={"amy","mighty","shadow"};
    for(unsigned i=0;i<3;++i) {
        strcpy(s2_party.roster.character[0],names[i]);
        CHECK(s2_video_knuckles_hud_pixel(&v,0x7D4,0,0,&color));
        CHECK(color==genesis_dac_cram_to_argb(0xEEE,GENESIS_DAC_NORMAL));
        CHECK(s2_video_knuckles_hud_pixel(&v,0x7DE,0,0,&color));
        CHECK(color==genesis_dac_cram_to_argb(0xEEE,GENESIS_DAC_NORMAL));
        /* Both decimal digit cells remain live native VRAM. */
        CHECK(s2_video_knuckles_hud_pixel(&v,0x7DD,0,0,&color));
        CHECK(color==genesis_dac_cram_to_argb(amy_item_bank.palette[2],GENESIS_DAC_NORMAL));
        CHECK(s2_video_knuckles_hud_pixel(&v,0x7DF,0,0,&color));
        CHECK(!s2_video_knuckles_hud_pixel(&v,0x7E0,0,0,&color));
        g_ram[0xF600]=8;CHECK(!s2_video_knuckles_hud_pixel(&v,0x7D4,0,0,&color));
        g_ram[0xF600]=12;
    }
    imported_hud_mock=0;s2_party=saved;clear_guest();
}
static void pylon_viewport_test(void)
{
    static SceneFrame frame;
    static uint32_t keys[SCENE_SPRITES];
    clear_guest();memset(&frame,0,sizeof frame);
    memset(g_rom+0x40A9A,0,32);
    g_ram[0xF600]=12;g_ram[0xF711]=1;g_ram[0xFE10]=13;g_ram[0xB000]=1;
    g_ram[0xB340]=0x7C;g_ram[0xB364]=2;
    word(g_ram,0xB342,0xC373);glue_poke32(0xFFB344,0x2103C);
    word(g_ram,0xEE00,800);word(g_ram,0xEEF0,800);word(g_ram,0xE808,0xFFFF);
    word(g_rom,0x2103C,2);word(g_rom,0x2103E,1);
    g_rom[0x21040]=0;g_rom[0x21041]=0;
    word(g_rom,0x21042,0xA000);word(g_rom,0x21044,0);word(g_rom,0x21046,0xFFF0);
    /* Phase800 is past Obj7C's native736 submission gate; the expanded
     * viewport can still contain a column. No guest object is created. */
    static uint8_t before[65536];memcpy(before,g_ram,sizeof before);
    capture_objects_into(&frame,796,1,keys);
    CHECK(frame.count==1 && !memcmp(before,g_ram,sizeof before));
    CHECK(frame.sprites[0].hud&SCENE_FOREGROUND_SCREEN);
    CHECK(frame.sprites[0].attr==0x6373);
    GVDP v={0};memset(v.vram+0x373*32,0x11,32);
    uint32_t palette[64]={0},a[796],b[796];palette[49]=0xFF123456;
    s_display=&frame;s_camera_y=0;s_left=0;s_camera=0;
    CHECK(reserve_sprite_row(796));
    for(unsigned n=0;n<2;++n) {
        s_camera=n?128:0;
        memset(s_priority,0,796);memset(a,0,sizeof a);
        draw_scene_sprites(&v,128,a,796,238,palette);
        for(unsigned x=0;x<796;++x)CHECK(a[x]==(x>=428 && x<436?palette[49]:0));
    }
    memcpy(b,a,sizeof b);
    s_left=1024;memset(s_priority,0,796);memset(a,0,sizeof a);
    draw_scene_sprites(&v,128,a,796,238,palette);
    CHECK(!memcmp(a,b,sizeof a)); /* Exact native1024 camera cycle. */
    s_left=262;s_camera=500;memset(s_priority,0,796);memset(a,0,sizeof a);
    draw_scene_sprites(&v,128,a,796,238,palette);
    CHECK(a[231]==palette[49] && !a[230] && !a[239]);
    memset(s_priority,1,796);memset(a,0,sizeof a);
    draw_scene_sprites(&v,128,a,796,238,palette);
    for(unsigned x=0;x<796;++x)CHECK(!a[x]); /* Card planes retain priority. */
    s_left=s_camera=500;memset(s_priority,0,796);memset(a,0,sizeof a);
    draw_scene_sprites(&v,128,a,320,0,palette);
    CHECK(a[0]==palette[49]);
    for(unsigned x=1;x<320;++x)CHECK(!a[x]); /* Native SAT places this piece at-7. */
    s_left=s_camera=1000;memset(s_priority,0,796);memset(a,0,sizeof a);
    draw_scene_sprites(&v,128,a,320,0,palette);
    for(unsigned x=0;x<320;++x)CHECK(!a[x]);
    s_display=NULL;
}
static void amy_item_art_test(void)
{
    clear_guest();memset(&s2_party,0,sizeof s2_party);
    s_display=NULL;s_donor_fade=s_presented_fade=(DonorFade){0};
    hud_available=1;amy_special_available=0;amy_items_available=1;
    s2_party.amy_enabled=1;strcpy(s2_party.roster.character[0],"amy");g_ram[0xF600]=12;
    memset(amy_items.life,0x33,sizeof amy_items.life);
    memset(amy_items.continue_icons,0x44,sizeof amy_items.continue_icons);
    memset(amy_items.signpost,0x55,sizeof amy_items.signpost);amy_items.signpost_tiles=78;
    amy_item_bank.palette[3]=0x00E;amy_item_bank.water_palette[3]=0xE00;
    amy_item_bank.water_palette_alt[3]=0x0E0;amy_item_bank.water_palette_count=2;
    s2_video_prepare_knuckles_items();unsigned bytes=0;
    CHECK(!gvdp_ext_active()); /* Amy's private colours belong to sprites. */
    CHECK(gvdp_host_palette() && gvdp_host_palette()[16+3]==
        genesis_dac_cram_to_argb(0x00E,GENESIS_DAC_NORMAL));
    const uint8_t *patterns=gvdp_sprite_patterns(0x80,&bytes);
    CHECK(patterns && bytes==65536 && patterns[0x7D4*32]==0x33 && patterns[0x5F4*32]==0x44);
    CHECK(patterns[0x434*32]==0x55 && patterns[0x481*32]==0x55);
    CHECK(gvdp_sprite_palette_line(0x80,0x434,0)==2);
    CHECK(gvdp_sprite_palette_line(0x80,0x481,1)==2);
    CHECK(gvdp_sprite_palette_line(0x80,0x7D4,0)==2);
    CHECK(gvdp_sprite_palette_line(0x80,0x7D4,1)==2);
    CHECK(!amy_item_tile(0x7D3) && !amy_item_tile(0x7D8) && !amy_item_tile(0x600));
    CHECK(s_item_top[16+3]==0x00E && s_item_bottom[16+3]==0x00E);
    g_ram[0xF64C]=1;g_ram[0xFE10]=13;s2_video_prepare_knuckles_items();
    CHECK(!gvdp_ext_active());
    CHECK(s_item_top[16+3]==0x00E && s_item_bottom[16+3]==0xE00);
    GVDP water={0};word(g_ram,0xF080,0x002);water.cram[0]=0x002;
    item_palette_row(NULL,&water,NULL);
    CHECK(gvdp_host_palette() && gvdp_host_palette()[16+3]==
        genesis_dac_cram_to_argb(0xE00,GENESIS_DAC_NORMAL));
    g_ram[0xFE10]=15;s2_video_prepare_knuckles_items();CHECK(s_item_bottom[16+3]==0x0E0);
    unsigned sites[]={0x1684E,0x1688E,0x168E6,0x16938};
    g_cpu.A[0]=0xFFFFD180;g_cpu.A[2]=0xFFFFF810;amy_item_owner=1;item_owner=0;
    for(unsigned n=0;n<4;++n) {
        word(g_ram,0xF80C,0x87D4);g_ram[0xF80A]=5;s2_video_hook(sites[n]);CHECK(g_ram[0xF80A]==0x85);
        word(g_ram,0xF80C,0x84BE);g_ram[0xF80A]=5;s2_video_hook(sites[n]);CHECK(g_ram[0xF80A]==5);
    }
    word(g_ram,0xFFD8,1);s2_video_prepare_knuckles_items();CHECK(s_item_bottom[16+3]==0x00E);
    CHECK(!gvdp_ext_active());
    word(g_ram,0xF80C,0x83EA);g_ram[0xF80A]=5;s2_video_hook(0x16DEE);CHECK(g_ram[0xF80A]==0x85);
    amy_item_owner=0;g_ram[0xF80A]=5;s2_video_hook(0x16DEE);CHECK(g_ram[0xF80A]==5);
    word(g_ram,0xFFD8,0);strcpy(s2_party.roster.character[0],"knuckles");s2_video_prepare_knuckles_items();
    CHECK(gvdp_ext_active()); /* Knuckles' world palette still needs it. */
    CHECK(!gvdp_host_palette()); /* Retire Amy's palette on a wet-world switch. */
    patterns=gvdp_sprite_patterns(0x80,&bytes);CHECK(patterns[0x7D4*32]==hud_art.life[0]);
    amy_items_available=0;hud_available=0;s2_video_prepare_knuckles_items();
    CHECK(!gvdp_sprite_patterns(0x80,&bytes));
    clear_guest();memset(&s2_party,0,sizeof s2_party);
}
static unsigned native_fetches;
static void donor_palette_fade_test(void)
{
    /* Explicit DAC values from REV01's four Pal_Fade routines. */
    CHECK(donor_fade_color(0xEEE,(DonorFade){FADE_FROM_BLACK,7})==0xE00);
    CHECK(donor_fade_color(0xEEE,(DonorFade){FADE_FROM_BLACK,14})==0xEE0);
    CHECK(donor_fade_color(0x08E,(DonorFade){FADE_FROM_BLACK,4})==0x080);
    CHECK(donor_fade_color(0x08E,(DonorFade){FADE_FROM_BLACK,11})==0x08E);
    CHECK(donor_fade_color(0xEEE,(DonorFade){FADE_TO_BLACK,7})==0xEE0);
    CHECK(donor_fade_color(0xEEE,(DonorFade){FADE_TO_BLACK,14})==0xE00);
    CHECK(donor_fade_color(0xE0E,(DonorFade){FADE_TO_BLACK,7})==0xE00);
    CHECK(donor_fade_color(0xE0E,(DonorFade){FADE_TO_BLACK,14})==0);
    CHECK(donor_fade_color(0x08E,(DonorFade){FADE_FROM_WHITE,7})==0x0EE);
    CHECK(donor_fade_color(0x08E,(DonorFade){FADE_FROM_WHITE,10})==0x08E);
    CHECK(donor_fade_color(0x08E,(DonorFade){FADE_TO_WHITE,3})==0x0EE);
    CHECK(donor_fade_color(0x08E,(DonorFade){FADE_TO_WHITE,10})==0xEEE);
    s2_video_hook(0x2592);s2_video_vblank();
    CHECK(s2_video_donor_color(0x08E)==0x08E);
    for(unsigned n=0;n<3;++n)s2_video_hook(0x25B2);
    CHECK(s2_video_donor_color(0x08E)==0x08E); /* wait for palette DMA */
    s2_video_vblank();CHECK(s2_video_donor_color(0x08E)==0x0EE);
    S2StateIO len={0};s2_video_rb_state(&len);
    uint8_t *saved=malloc(len.pos);CHECK(saved);
    S2StateIO state={0};state.data=saved;state.size=len.pos;state.ok=1;
    s2_video_rb_state(&state);CHECK(state.ok);
    s2_video_hook(0x246A);s2_video_vblank();
    state.pos=0;state.mode=2;s2_video_rb_state(&state);CHECK(state.ok);
    CHECK(s2_video_donor_color(0x08E)==0x0EE);
    free(saved);
    s2_video_hook(0x246A);for(unsigned n=0;n<22;++n)s2_video_hook(0x248A);
    s2_video_vblank();CHECK(s2_video_donor_color(0x08E)==0);
    /* Level title cards install fresh palettes without Pal_FadeFromBlack. */
    s2_video_hook(0x4450);s2_video_vblank();CHECK(s2_video_donor_color(0x08E)==0x08E);
    s_donor_fade=s_presented_fade=(DonorFade){0};
}
static const uint32_t *fetch_native(void *ctx)
{++native_fetches;return (const uint32_t *)ctx;}
static void special_actor_test(void)
{
    static GVDP v,before;static uint8_t ram_before[65536];uint32_t out[398];
    memset(&v,0,sizeof v);clear_guest();memset(&hud_art,0,sizeof hud_art);
    memset(s_special_history,0,sizeof s_special_history);memset(&s_special_display,0,sizeof s_special_display);
    hud_available=0;s2_video_prepare_knuckles_items();hud_available=1;
    s_donor_fade=s_presented_fade=(DonorFade){0};
    strcpy(s2_party.roster.character[0],"knuckles");s2_party.roster.slots=1;
    g_ram[0xF600]=16;g_ram[0xB000]=9;
    v.reg[1]=64;v.reg[2]=0x30;v.reg[4]=7;v.reg[5]=0x7C;v.reg[12]=8;v.reg[16]=1;
    v.cram[17]=0xE00;v.cram[3]=0x0E0;
    unsigned map=0x34212+36;
    word(g_rom,0x34212,36);word(g_rom,0x34214,36);word(g_rom,map,1);
    g_rom[map+2]=0xF8;g_rom[map+3]=5;word(g_rom,map+4,0x8000);word(g_rom,map+8,0xFFF8);
    word(g_rom,0x345FA,36);word(g_rom,0x345FA+36,1);word(g_rom,0x345FA+38,0x3000);
    g_rom[0x33AA2]=0;g_rom[0x33AA3]=0xFF;g_rom[0x33AA4]=g_rom[0x33AA5]=0;
    memset(g_ram,0x11,128);memset(v.vram+0x300*32,0x11,128);
    S2SpecialArtFrame *f=&hud_art.special[0];f->count=1;
    f->pieces[0]=(S2SpecialPiece){-8,-8,9,0x8000}; /* wider than native shape */
    memset(f->tiles,0x22,6*32);f->tiles[4*32]=0x20; /* real transparent hole */
    hud_art.special[1]=*f;memset(hud_art.special[1].tiles,0x33,6*32);
    hud_art.special_palette[2]=0x00E;hud_art.special_palette[3]=0x0E0;
    word(g_ram,0xF800,160);g_ram[0xF802]=5;
    word(g_ram,0xF804,0xA300);word(g_ram,0xF806,160);
    front_ui_begin();g_cpu.A[0]=0xFFFFB000;g_cpu.A[1]=map+10;g_cpu.A[2]=0xFFFFF808;
    s2_video_hook(0x1684E);front_ui_publish();
    CHECK(s_special_build.ready && s_special_build.piece[0]==1 && s_special_build.tiles==4);
    memcpy(v.vram+0xF800,g_ram+0xF800,640);s2_video_prepare_knuckles_items();
    memcpy(ram_before,g_ram,sizeof g_ram);before=v;
    for(unsigned wide=0;wide<2;++wide) {
        CHECK(configure(wide?"16:9":"off"));int width=wide?398:256;
        memset(out,0,sizeof out);s2_video_special_actor(&v,0,out,width);
        s2_video_special_actor(&v,32,out,width);
        for(int ox=0;ox<width;++ox) {
            int source=wide?318:256,nx=ox*source/width-(source-256)/2;
            if(nx==48)CHECK(out[ox]==genesis_dac_cram_to_argb(0xE,GENESIS_DAC_NORMAL));
            if(nx==49 || nx==56)CHECK(out[ox]==0xFF000000);
        }
    }
    CHECK(!memcmp(g_ram,ram_before,sizeof g_ram) && !memcmp(&v,&before,sizeof v));
    /* Amy needs no KiS2 resource and her original frame can have six pieces.
     * The last piece must survive beyond the native Sonic silhouette. */
    memset(&amy_special,0,sizeof amy_special);
    amy_special.frames[0]=hud_art.special[0];amy_special.frames[0].count=6;
    for(unsigned i=1;i<6;++i)
        amy_special.frames[0].pieces[i]=(S2SpecialPiece){(int16_t)(16+i*8),-8,0,(uint16_t)(0x8000|5+i)};
    memset(amy_special.frames[0].tiles+6*32,0x44,5*32);
    amy_special.palette[2]=0x0E0;amy_special.palette[4]=0xE0E;
    hud_available=0;amy_special_available=1;strcpy(s2_party.roster.character[0],"amy");
    front_ui_begin();s2_video_hook(0x1684E);front_ui_publish();
    s2_video_prepare_knuckles_items();CHECK(configure("off"));
    s2_video_special_actor(&v,0,out,256);s2_video_special_actor(&v,32,out,256);
    CHECK(out[48]==genesis_dac_cram_to_argb(0x0E0,GENESIS_DAC_NORMAL));
    CHECK(out[96]==genesis_dac_cram_to_argb(0xE0E,GENESIS_DAC_NORMAL));
    CHECK(!memcmp(g_ram,ram_before,sizeof g_ram) && !memcmp(&v,&before,sizeof v));
    amy_special_available=0;s2_video_prepare_knuckles_items();out[96]=0x12345678;
    s2_video_special_actor(&v,0,out,256);s2_video_special_actor(&v,32,out,256);
    CHECK(out[96]==0x12345678);
    hud_available=1;strcpy(s2_party.roster.character[0],"knuckles");s2_video_prepare_knuckles_items();
    S2StateIO length={0};s2_video_rb_state(&length);uint8_t *saved=malloc(length.pos);CHECK(saved);
    S2StateIO state={0};state.data=saved;state.size=length.pos;state.ok=1;
    s2_video_rb_state(&state);CHECK(state.ok);
    memset(&s_special_build,0,sizeof s_special_build);memset(s_special_history,0,sizeof s_special_history);
    s_special_serial=0;state.pos=0;state.mode=2;s2_video_rb_state(&state);CHECK(state.ok);
    CHECK(special_actor_matches(&v,&s_special_build));free(saved);
    /* A newer publication with the same SAT must wait for its own DMA. */
    SpecialActorFrame newer=s_special_build;newer.serial=++s_special_serial;newer.frame=1;
    memset(newer.patterns,0x44,128);s_special_history[newer.serial%3]=newer;
    CHECK(configure("off"));s2_video_special_actor(&v,0,out,256);s2_video_special_actor(&v,32,out,256);
    CHECK(out[48]==genesis_dac_cram_to_argb(0xE,GENESIS_DAC_NORMAL));
    memset(v.vram+0x300*32,0x44,128);
    s2_video_special_actor(&v,0,out,256);s2_video_special_actor(&v,32,out,256);
    CHECK(out[48]==genesis_dac_cram_to_argb(0x0E0,GENESIS_DAC_NORMAL));
    memset(v.vram+0x300*32,0x11,128);
    /* Earlier native sprites retain precedence; native operators still shade
     * the replacement instead of treating it as an unshaded ARGB overlay. */
    memcpy(g_ram+0xF808,g_ram+0xF800,8);g_ram[0xF803]=1;
    g_ram[0xF802]=0;word(g_ram,0xF804,0x8304);g_ram[0xF80B]=0;
    memset(v.vram+0x304*32,0x33,32);
    front_ui_begin();g_cpu.A[2]=0xFFFFF810;s2_video_hook(0x1684E);front_ui_publish();
    memcpy(v.vram+0xF800,g_ram+0xF800,640);
    s2_video_special_actor(&v,0,out,256);s2_video_special_actor(&v,32,out,256);
    CHECK(out[32]==genesis_dac_cram_to_argb(0x0E0,GENESIS_DAC_NORMAL));
    CHECK(out[40]==genesis_dac_cram_to_argb(0xE,GENESIS_DAC_NORMAL));
    word(g_ram,0xF804,0x6304);memset(v.vram+0x304*32,0xFF,32);
    front_ui_begin();s2_video_hook(0x1684E);front_ui_publish();
    memcpy(v.vram+0xF800,g_ram+0xF800,640);
    s2_video_special_actor(&v,0,out,256);s2_video_special_actor(&v,32,out,256);
    CHECK(out[32]==genesis_dac_cram_to_argb(0xE,GENESIS_DAC_SHADOW));
    /* Without a verified presentation resource, keep the native image. */
    hud_available=0;s2_video_prepare_knuckles_items();out[48]=0x12345678;
    s2_video_special_actor(&v,0,out,256);s2_video_special_actor(&v,32,out,256);
    CHECK(out[48]==0x12345678);
    clear_guest();memset(s_special_history,0,sizeof s_special_history);
    memset(&s_special_display,0,sizeof s_special_display);
}
static void results_identity_test(void)
{
    static GVDP v;uint32_t out[398];uint8_t original[65536];
    memset(&v,0,sizeof v);clear_guest();memset(&hud_art,0,sizeof hud_art);
    g_ram[0xF600]=12;hud_available=1;strcpy(s2_party.roster.character[0],"knuckles");
    v.reg[1]=64;v.reg[2]=0x30;v.reg[4]=7;v.reg[5]=0x7C;v.reg[12]=1;v.reg[16]=1;
    v.cram[1]=v.cram[6]=0xEEE;v.cram[3]=0xE;
    memset(hud_art.font_k,0x33,sizeof hud_art.font_k);memset(ui_font,0x22,sizeof ui_font);
    memset(title_font,0x11,sizeof title_font);
    static const unsigned tiles[]={0x5D0,0x588,0x584,0x5C0,0x5B4,0x5B8,0x588,0x5D4};
    static const int offsets[]={-64,-48,-32,-16,-8,16,32,47};
    for(unsigned n=0;n<8;++n) {
        unsigned at=0xF800+n*8;word(v.vram,at,184);
        v.vram[at+2]=n==3?1:5;v.vram[at+3]=n==7?0:(uint8_t)(n+1);
        word(v.vram,at+4,0x8000|tiles[n]);word(v.vram,at+6,288+offsets[n]);
        memset(v.vram+tiles[n]*32,0x11,(n==3?2:4)*32);
    }
    memcpy(original,g_ram,sizeof original);
    for(unsigned wide=0;wide<2;++wide) {
        CHECK(sonic2_video.configure(wide?"16:9":"off"));int width=wide?398:320,center=(width-320)/2;
        for(int line=0;line<72;++line) {
            for(int x=0;x<width;++x)out[x]=0xFF00FF00;
            s2_video_results_name(&v,line,out,width,0);
            s2_video_results_name(&v,line,out,width,1);
            if(line==56) {
                /* Original KiS2 shifts the complete eight-letter name/GOT
                 * row 24 pixels right, retaining its centered footprint. */
                CHECK(out[64+center]==0xFFFF0000 && out[80+center]==0xFFFFFFFF);
                CHECK(out[128+center]==0xFFFF0000 && out[200+center]==0xFFFFFFFF);
                CHECK(out[40+center]==0xFF00FF00);
            }
        }
    }
    strcpy(s2_party.roster.character[0],"shadow");
    for(int line=0;line<=56;++line) {
        memset(out,0,sizeof out);s2_video_results_name(&v,line,out,398,0);
        s2_video_results_name(&v,line,out,398,1);
    }
    /* SHADOW uses the native24-pixel W and keeps the8-pixel gap beforeGOT.
     * The full row remains centered; the scaled menu-font advance was16. */
    CHECK(out[80+39]==0xFFFFFFFF && out[183+39]==0xFFFFFFFF);
    CHECK(out[184+39]==0 && out[192+39]==0xFFFFFFFF);
    strcpy(s2_party.roster.character[0],"sonic");
    for(int x=0;x<320;++x)out[x]=0xFF00FF00;
    s2_video_results_name(&v,0,out,320,0);s2_video_results_name(&v,56,out,320,1);
    CHECK(out[96]==0xFF00FF00 && !memcmp(original,g_ram,sizeof original));
    hud_available=0;CHECK(sonic2_video.configure("off"));
}
static void special_results_scroll_test(void)
{
    uint8_t before[65536];
    /* These are ordinary queued level rows, not results-screen sprites.
     * Vint_Level copies the source flags before invoking tile streaming. */
    for (unsigned wide=0;wide<2;++wide) {
        s_mode=wide?VIDEO_RATIO:VIDEO_OFF;
        memset(g_ram,0xA5,sizeof g_ram);g_ram[0xF600]=16;
        memcpy(before,g_ram,sizeof before);
        s2_video_hook(0x5300);
        for (unsigned i=0;i<sizeof g_ram;++i) {
            int flags=(i>=0xEE50 && i<0xEE60) || (i>=0xEEA0 && i<0xEEB0);
            CHECK(g_ram[i]==(flags?0:before[i]));
        }
        /* The subsequent native V-int must copy only retired requests. */
        memcpy(g_ram+0xEEA0,g_ram+0xEE50,16);
        for(unsigned i=0;i<16;++i)CHECK(!g_ram[0xEEA0+i]);
        for(unsigned mode=0;mode<32;++mode)if(mode!=16) {
            memset(g_ram,0xA5,sizeof g_ram);g_ram[0xF600]=(uint8_t)mode;
            memcpy(before,g_ram,sizeof before);s2_video_hook(0x5300);
            CHECK(!memcmp(g_ram,before,sizeof g_ram));
        }
    }
    s_mode=VIDEO_OFF;
}

static void special_results_identity_test(void)
{
    static GVDP v,before;static uint8_t original_ram[65536];uint32_t out[398];
    memset(&v,0,sizeof v);clear_guest();memset(&hud_art,0,sizeof hud_art);
    s2_video_hook(0x4F64);hud_available=0;s2_video_prepare_knuckles_items();hud_available=1;
    s2_party.roster.slots=1;strcpy(s2_party.roster.character[0],"knuckles");
    g_ram[0xF600]=16;g_ram[0xFFB0]=1;g_ram[0xB800]=g_ram[0xB840]=0x6F;
    word(g_ram,0xB804,1);word(g_ram,0xB806,0x4ED0);
    word(g_ram,0xB844,1);word(g_ram,0xB846,0x4ED0);g_ram[0xB85A]=1;
    word(g_ram,0xB848,288);word(g_ram,0xB84A,152);
    v.reg[1]=64;v.reg[2]=0x30;v.reg[4]=7;v.reg[5]=0x7C;v.reg[12]=1;v.reg[16]=1;
    v.cram[2]=0xE00;v.cram[3]=0xE;v.cram[4]=0x0E0;v.cram[6]=0xEEE;
    memset(v.vram+10*32,0x22,128);memset(v.vram+2*32,0x44,128);memset(hud_art.font_k,0x33,128);
    hud_art.special_results[1].count=1;
    hud_art.special_results[1].pieces[0]=(S2SpecialPiece){-48,0,5,0x8002};
    hud_art.special_results[29].count=1;
    hud_art.special_results[29].pieces[0]=(S2SpecialPiece){-112,0,5,0x803E};
    word(g_ram,0xF800,152);g_ram[0xF802]=5;word(g_ram,0xF804,0x800A);word(g_ram,0xF806,216);
    front_ui_begin();g_cpu.A[0]=0xFFFFB840;g_cpu.A[1]=0x14ED8;g_cpu.A[2]=0xFFFFF808;
    s2_video_hook(0x1684E);front_ui_publish();
    CHECK(s_ss_result_build.count==2 && s_ss_result_build.owner[0]==2);
    memcpy(v.vram+0xF800,g_ram+0xF800,640);s2_video_prepare_knuckles_items();
    memcpy(original_ram,g_ram,sizeof g_ram);before=v;
    for(unsigned wide=0;wide<2;++wide) {
        int width=wide?398:320,origin=(width-320)/2;
        for(int x=0;x<width;++x)out[x]=0xFF123456;
        s2_video_special_results(&v,0,out,width);s2_video_special_results(&v,24,out,width);
        CHECK(out[56+origin]==genesis_dac_cram_to_argb(0xE,GENESIS_DAC_NORMAL));
        CHECK(out[192+origin]==genesis_dac_cram_to_argb(0x0E0,GENESIS_DAC_NORMAL));
        CHECK(out[88+origin]==0xFF000000); /* removed native name silhouette */
        if(wide)CHECK(out[0]==0xFF123456);
    }
    CHECK(!memcmp(g_ram,original_ram,sizeof g_ram) && !memcmp(&v,&before,sizeof v));
    strcpy(s2_party.roster.character[0],"sonic");out[56]=0x12345678;
    s2_video_special_results(&v,0,out,320);s2_video_special_results(&v,24,out,320);CHECK(out[56]==0x12345678);
    strcpy(s2_party.roster.character[0],"knuckles");hud_available=0;s2_video_prepare_knuckles_items();
    s2_video_special_results(&v,0,out,320);s2_video_special_results(&v,24,out,320);CHECK(out[56]==0x12345678);
    s2_video_hook(0x4F64);CHECK(!s_special_serial && !s_ss_result_serial);
}

static void special_results_party_name_test(void)
{
    static GVDP v,before;
    uint32_t out[320];
    clear_guest();memset(&v,0,sizeof v);s2_video_hook(0x4F64);
    hud_available=amy_special_available=0;
    strcpy(s2_party.roster.character[0],"sonic");s2_video_prepare_knuckles_items();
    memset(title_font+86*32,0x66,4*32);
    memset(ui_font+(14+'M'-'A')*32,0x55,32);
    s2_party.roster.slots=2;
    g_ram[0xF600]=16;g_ram[0xB800]=g_ram[0xB840]=0x6F;
    strcpy(s2_party.roster.character[0],"shadow");s2_video_prepare_knuckles_items();
    v.reg[1]=64;v.reg[5]=0x7C;v.reg[12]=1;
    v.cram[2]=0xE00;v.cram[3]=0x0E0;v.cram[4]=0xE;v.cram[6]=0xEEE;v.cram[0x1C]=0xEC0;
    memset(v.vram+0x2A*32,0x22,4*32); /* S */
    memset(v.vram+0x12*32,0x33,4*32); /* H */
    memset(v.vram+0x1C*32,0x33,6*32); /* M */
    memset(v.vram+0x0E*32,0x44,4*32); /* G */
    word(g_ram,0xB844,1);word(g_ram,0xB846,0x4ED0);
    word(g_ram,0xB848,288);word(g_ram,0xB84A,152);
    const unsigned frames[]={1,22};
    for(unsigned i=0;i<2;++i) {
        unsigned frame=frames[i],mapping=i?0x14F2A:0x14F0A;
        word(g_rom,0x14ED0+frame*2,mapping-0x14ED0);
        word(g_rom,mapping,6);word(g_rom,mapping+8,i?-120:-72);
        g_rom[mapping+43]=5;word(g_rom,mapping+44,i?0x8012:0x800E);
        word(g_rom,mapping+48,i?-40:8);
        g_ram[0xB85A]=(uint8_t)frame;
        word(g_ram,0xF800,152);g_ram[0xF802]=5;
        word(g_ram,0xF804,0x802A);word(g_ram,0xF806,i?168:216);
        front_ui_begin();special_results_piece(0,0xB840);front_ui_publish();
        memcpy(v.vram+0xF800,g_ram+0xF800,640);before=v;
        s2_video_special_results(&v,0,out,320);s2_video_special_results(&v,24,out,320);
        CHECK(out[i?24:72]==genesis_dac_cram_to_argb(0xE00,GENESIS_DAC_NORMAL));
        CHECK(out[i?40:88]==genesis_dac_cram_to_argb(0x0E0,GENESIS_DAC_NORMAL));
        CHECK(out[i?136:184]==genesis_dac_cram_to_argb(i?0x0E0:0xE,GENESIS_DAC_NORMAL));
        CHECK(!memcmp(&v,&before,sizeof v) && ram16(0xF804)==0x802A);
    }
    strcpy(s2_party.roster.character[0],"mighty");s2_video_prepare_knuckles_items();
    memcpy(v.vram+0xF800,g_ram+0xF800,640);
    s2_video_special_results(&v,0,out,320);s2_video_special_results(&v,24,out,320);
    CHECK(out[28]==genesis_dac_cram_to_argb(0x0E0,GENESIS_DAC_NORMAL));
    CHECK(out[108]==genesis_dac_cram_to_argb(0xEEE,GENESIS_DAC_NORMAL));
    /* Zero-point P1 ring row: emit the compact name, shift RINGS by one
     * character for Mighty, and leave the zero at its original x. */
    unsigned tally=0x15100;
    word(g_rom,0x14ED0+18*2,tally-0x14ED0);
    word(g_rom,tally,6);word(g_rom,tally+8,-96);
    g_rom[tally+19]=5;word(g_rom,tally+20,0x8012);word(g_rom,tally+24,-48);
    g_rom[tally+43]=1;word(g_rom,tally+44,0x8001);word(g_rom,tally+48,88);
    memset(v.vram+32,0x66,2*32);
    g_ram[0xB85A]=18;
    word(g_ram,0xF806,192);
    front_ui_begin();special_results_piece(0,0xB840);front_ui_publish();
    memcpy(v.vram+0xF800,g_ram+0xF800,640);
    s2_video_special_results(&v,0,out,320);s2_video_special_results(&v,28,out,320);
    CHECK(out[64]==genesis_dac_cram_to_argb(0xEC0,GENESIS_DAC_NORMAL));
    CHECK(out[120]==genesis_dac_cram_to_argb(0x0E0,GENESIS_DAC_NORMAL));
    CHECK(out[248]==genesis_dac_cram_to_argb(0xEEE,GENESIS_DAC_NORMAL));
    s2_video_hook(0x4F64);memset(title_font+86*32,0,4*32);
    memset(ui_font+(14+'M'-'A')*32,0,32);
    strcpy(s2_party.roster.character[0],"sonic");s2_video_prepare_knuckles_items();
}

static void amy_special_ui_test(void)
{
    static GVDP v,before;static uint8_t original_ram[65536];uint32_t out[398];
    memset(&v,0,sizeof v);clear_guest();memset(&amy_special,0,sizeof amy_special);
    s2_video_hook(0x4F64);hud_available=amy_special_available=0;
    strcpy(s2_party.roster.character[0],"amy");s2_party.roster.slots=1;
    s2_video_prepare_knuckles_items();amy_special_available=1;
    g_ram[0xF600]=16;
    v.reg[1]=64;v.reg[2]=0x30;v.reg[4]=7;v.reg[5]=0x7C;v.reg[12]=8;v.reg[16]=1;
    amy_special.palette[4]=0xE0E;memset(amy_special.hud_name,0x44,sizeof amy_special.hud_name);
    amy_special.hud_name[0]=0x40;memset(v.vram+0x1FA*32,0x11,5*32);v.cram[17]=0xE00;
    word(v.vram,0xF800,144);v.vram[0xF802]=12;
    word(v.vram,0xF804,0xA1FA);word(v.vram,0xF806,160);
    s2_video_prepare_knuckles_items();memcpy(original_ram,g_ram,sizeof g_ram);before=v;
    CHECK(!s_special_serial); /* Native blink/no-body publication still has a HUD. */
    for(unsigned wide=0;wide<2;++wide) {
        CHECK(configure(wide?"16:9":"off"));int width=wide?398:256,source=wide?318:256;
        for(int x=0;x<width;++x)out[x]=0xFF123456;
        s2_video_special_actor(&v,0,out,width);s2_video_special_actor(&v,16,out,width);
        for(int x=0;x<width;++x) {
            int nx=x*source/width-(source-256)/2;
            if(nx==32)CHECK(out[x]==genesis_dac_cram_to_argb(0xE0E,GENESIS_DAC_NORMAL));
            if(nx==33)CHECK(out[x]==0xFF000000);
        }
    }
    CHECK(!memcmp(g_ram,original_ram,sizeof g_ram) && !memcmp(&v,&before,sizeof v));
    /* Original Amy result mappings use their native anchors, a new Y glyph,
     * and eight changed HUD-name tiles, without the optional KiS2 resource. */
    g_ram[0xFFB0]=255;g_ram[0xB800]=g_ram[0xB840]=0x6F;
    word(g_ram,0xB804,1);word(g_ram,0xB806,0x4ED0);
    word(g_ram,0xB844,1);word(g_ram,0xB846,0x4ED0);g_ram[0xB85A]=1;
    word(g_ram,0xB848,288);word(g_ram,0xB84A,152);
    word(g_ram,0xF800,152);g_ram[0xF802]=5;word(g_ram,0xF804,0x800A);word(g_ram,0xF806,216);
    v.reg[12]=1;memcpy(v.vram+0xF800,g_ram+0xF800,640);
    memset(v.vram+10*32,0x22,128);memset(v.vram+2*32,0x44,128);
    memset(v.vram+0x5B0*32,0x11,8*32);memset(v.vram+0x5D8*32,0x22,128);
    memset(amy_special.font_y,0x33,sizeof amy_special.font_y);
    memset(amy_special.result_name,0x55,sizeof amy_special.result_name);
    amy_special.results[1].count=3;
    amy_special.results[1].pieces[0]=(S2SpecialPiece){0,0,5,0x8002};
    amy_special.results[1].pieces[1]=(S2SpecialPiece){-64,0,5,0x05D8};
    amy_special.results[1].pieces[2]=(S2SpecialPiece){-32,0,5,0xA5B0};
    v.cram[3]=0xE;v.cram[4]=0x0E0;v.cram[21]=0xEEE;
    amy_special_available=0;s2_video_prepare_knuckles_items();amy_special_available=1;
    front_ui_begin();g_cpu.A[0]=0xFFFFB840;g_cpu.A[1]=0x14ED8;g_cpu.A[2]=0xFFFFF808;
    s2_video_hook(0x1684E);front_ui_publish();s2_video_prepare_knuckles_items();
    memcpy(original_ram,g_ram,sizeof g_ram);before=v;
    for(unsigned wide=0;wide<2;++wide) {
        int width=wide?398:320,origin=(width-320)/2;
        s2_video_special_results(&v,0,out,width);s2_video_special_results(&v,24,out,width);
        CHECK(out[88+origin]==0xFF000000);
        CHECK(out[96+origin]==genesis_dac_cram_to_argb(0xE,GENESIS_DAC_NORMAL));
        CHECK(out[128+origin]==genesis_dac_cram_to_argb(0xEEE,GENESIS_DAC_NORMAL));
        CHECK(out[160+origin]==genesis_dac_cram_to_argb(0x0E0,GENESIS_DAC_NORMAL));
    }
    CHECK(!memcmp(g_ram,original_ram,sizeof g_ram) && !memcmp(&v,&before,sizeof v));
    amy_special_available=0;s2_video_prepare_knuckles_items();out[96]=0x12345678;
    s2_video_special_results(&v,0,out,320);s2_video_special_results(&v,24,out,320);
    CHECK(out[96]==0x12345678);s2_video_hook(0x4F64);
}

static void knuckles_special_ui_test(void)
{
    static GVDP v,before;static uint8_t original_ram[65536];uint32_t out[398];
    memset(&v,0,sizeof v);clear_guest();memset(&hud_art,0,sizeof hud_art);
    s2_video_hook(0x4F64);hud_available=1;amy_special_available=0;
    strcpy(s2_party.roster.character[0],"knuckles");
    strcpy(s2_party.roster.character[1],"tails");s2_party.roster.slots=2;
    g_ram[0xF600]=16;
    v.reg[1]=64;v.reg[2]=0x30;v.reg[4]=7;v.reg[5]=0x7C;v.reg[12]=8;v.reg[16]=1;
    v.cram[17]=0xE00;v.cram[18]=0xEEE;v.cram[33]=v.cram[34]=0x0E0;
    hud_art.special_palette[1]=0x00E;hud_art.special_palette[2]=0x0E0;
    /* Native blue high-priority terrain remains visible under omitted names.
     * The checkpoint's native ink is deliberately absent from donor patterns. */
    memset(v.vram+0x1BC*32,0x11,32);memset(v.vram+0x1BA*32,0x22,32);
    memset(v.vram+0x1FA*32,0x22,11*32);
    for(unsigned at=0xC000;at<0xD000;at+=2)word(v.vram,at,0xA1BC);
    static const struct {int x,y;unsigned size,attr;} pieces[]={
        {32,16,12,0xA1FA},{64,16,0,0xA1FE}, /* primary SONIC */
        {160,16,0,0xC1FF},{80,16,0,0xA204}, /* Tails and other HUD ink */
        {64,64,0,0xA1BC},{104,64,0,0xC1BC},{144,64,0,0xA1BA}
    };
    for(unsigned i=0;i<sizeof pieces/sizeof pieces[0];++i) {
        unsigned at=0xF800+i*8;word(v.vram,at,pieces[i].y+128);
        v.vram[at+2]=(uint8_t)pieces[i].size;
        v.vram[at+3]=(uint8_t)(i+1<sizeof pieces/sizeof pieces[0]?i+1:0);
        word(v.vram,at+4,pieces[i].attr);word(v.vram,at+6,pieces[i].x+128);
    }
    s2_video_prepare_knuckles_items();memcpy(original_ram,g_ram,sizeof g_ram);before=v;
    CHECK(!s_special_serial); /* The UI must work during native actor blink. */
    for(unsigned wide=0;wide<2;++wide) {
        CHECK(configure(wide?"16:9":"off"));int width=wide?398:256,source=wide?318:256;
        s2_video_special_actor(&v,0,out,width);
        for(unsigned line=16;line<=64;line+=48) {
            for(int x=0;x<width;++x)out[x]=0xFF123456;
            s2_video_special_actor(&v,(int)line,out,width);
            for(int x=0;x<width;++x) {
                int nx=x*source/width-(source-256)/2;
                if(line==16) {
                    if(nx>=32 && nx<72)CHECK(out[x]==genesis_dac_cram_to_argb(0xE00,GENESIS_DAC_NORMAL));
                    if(nx>=160 && nx<168)CHECK(out[x]==genesis_dac_cram_to_argb(0x0E0,GENESIS_DAC_NORMAL));
                    if(nx>=80 && nx<88)CHECK(out[x]==genesis_dac_cram_to_argb(0xEEE,GENESIS_DAC_NORMAL));
                } else {
                    if(nx>=64 && nx<72)CHECK(out[x]==genesis_dac_cram_to_argb(0x00E,GENESIS_DAC_NORMAL));
                    if(nx>=104 && nx<112)CHECK(out[x]==genesis_dac_cram_to_argb(0x0E0,GENESIS_DAC_NORMAL));
                    if(nx>=144 && nx<152)CHECK(out[x]==genesis_dac_cram_to_argb(0xEEE,GENESIS_DAC_NORMAL));
                    if(nx==32)CHECK(out[x]==genesis_dac_cram_to_argb(0xE00,GENESIS_DAC_NORMAL));
                }
            }
        }
    }
    CHECK(!memcmp(g_ram,original_ram,sizeof g_ram) && !memcmp(&v,&before,sizeof v));
    /* Checkpoint sprites may cross the top edge. Fully clipped pieces must
     * leave the presented row alone; partially visible native ink stays red. */
    CHECK(configure("off"));word(v.vram,0xF820,112);
    for(unsigned x=0;x<256;++x)out[x]=0x12345678;
    s2_video_special_actor(&v,0,out,256);
    for(unsigned x=0;x<256;++x)CHECK(out[x]==0x12345678);
    word(v.vram,0xF820,124);s2_video_special_actor(&v,0,out,256);
    CHECK(out[64]==genesis_dac_cram_to_argb(0x00E,GENESIS_DAC_NORMAL));
    for(unsigned x=0;x<256;++x)out[x]=0x12345678;
    s2_video_special_actor(&v,4,out,256);
    for(unsigned x=0;x<256;++x)CHECK(out[x]==0x12345678);
    v=before;
    hud_available=0;s2_video_prepare_knuckles_items();out[32]=0x12345678;
    s2_video_special_actor(&v,0,out,256);s2_video_special_actor(&v,16,out,256);
    CHECK(out[32]==0x12345678);s2_video_hook(0x4F64);
}

static void scene_scroll_phase_test(void)
{
    static GVDP v;memset(&v,0,sizeof v);clear_guest();online=0;
    memset(s_history,0,sizeof s_history);memset(&s_display_frame,0,sizeof s_display_frame);
    memset(s_native_history,0,sizeof s_native_history);memset(&s_native_display,0,sizeof s_native_display);
    CHECK(configure("32:9"));v.reg[13]=0x3F;v.vsram[0]=25;word(v.vram,0xFC00,-1500);
    for(unsigned i=0;i<3;++i) {
        SceneFrame *f=&s_history[i];f->serial=i+1;f->scene=1;
        f->native_hscroll=i==1?-1508:-1500;f->native_vscroll=i==1?1057:1049;
        f->camera_x=i==1?1500:9999; /* World cameras are deliberately misleading. */
        f->actors.sprites[0].x=i==1?1608:1600;
    }
    select_scene(&v);CHECK(s_display && s_display->serial==3); /* Newest exact tie. */
    s_history[2].serial=0;s_display_frame.serial=0;
    select_scene(&v);CHECK(s_display && s_display->serial==1); /* Same SAT, displayed scroll. */
    memcpy(s_native_history,s_history,sizeof s_history);CHECK(configure("off"));
    const SceneFrame *f=native_select_scene(&v);CHECK(f && f->serial==1);
    S2RuntimeSprite actors[S2_MAX_PLAYERS+1],dust[S2_PRESENTATION_DUST];
    CHECK(s2_video_presented_actors(&v,actors,dust) && actors[0].x==1600);
    interpolation_reset();
}
static void arz_background_stream_limit_test(void)
{
    S2RenderContext r={0};r.frame_zone=15;r.frame_fg_x=174;
    r.frame_bg_x[0]=182*65536;r.frame_arz_bg_x=174*281*256;
    /* Native spindash camera catch-up: streamed BG is nine pixels behind
     * its logical target. A stationary wide origin has no such stream limit. */
    CHECK(background_view_base(&r,174,0,190,602,-182)==0);
    CHECK(background_view_base(&r,174,32,190,602,-182)==35);
    CHECK(background_view_base(&r,174,0,190,602,-185)==3); /* Independent native offset. */
    CHECK(background_view_base(&r,174,0,130,542,-156)==0); /* Absolute row unaffected. */
}
static void clamped_background_interpolation_test(void)
{
    static uint32_t pixels[796*224],completed[2][796*224];
    clear_guest();memset(&s2_party,0,sizeof s2_party);online=0;
    CHECK(configure("32:9"));CHECK(reserve_sprite_row(796));presentation_configure(1);
    /* Two adjacent ARZ camera samples inside the stationary wide viewport.
     * At source Y412 the native top row scrolls from -1 to -3. Quantizing
     * its fractional camera again must not move the stationary background. */
    for(unsigned limited=0;limited<2;++limited) {
    for(unsigned f=0;f<2;++f) {
        PresentationFrame *p=&s_present[f];memset(&p->render,0,sizeof p->render);
        memset(&p->scene,0,sizeof p->scene);memset(&p->party,0,sizeof p->party);
        p->scene.scene=1;
        p->valid=1;p->lines=224;p->width=796;p->zone=0xF00;p->tick=f+1;
        p->image=completed[f];p->capacity=796*224;
        S2RenderContext *r=&p->render;GVDP *v=&r->video_frame;
        r->frame_level=1;r->frame_zone=15;r->width=796;r->stage_width=2048;
        r->frame_fg_x=r->camera=limited?(f?182:174):(f?31:19);
        r->frame_bg_y=limited?600:412;r->left=0;
        r->frame_arz_bg_x=r->frame_fg_x*281*256;
        r->frame_bg_x[0]=limited?(f?198:182)*65536:r->frame_arz_bg_x;
        v->reg[1]=64;v->reg[2]=0x30;v->reg[4]=7;v->reg[5]=0x7C;
        v->reg[12]=1;v->reg[13]=0x3F;v->reg[16]=1;
        v->cram[1]=0xE;v->cram[2]=0xE00;
        memset(v->vram+32,0x11,32);memset(v->vram+64,0x22,32);
        for(unsigned y=0;y<16;++y)for(unsigned x=0;x<4;++x)r->world_frame[0x8080+y*256+x]=(uint8_t)(1+x%2);
        for(unsigned n=0;n<64;++n){word(r->world_frame,128+n*2,1);word(r->world_frame,256+n*2,2);}
        for(unsigned n=0;n<4;++n){word(r->world_frame,0x9008+n*2,1);word(r->world_frame,0x9010+n*2,2);}
        for(unsigned y=0;y<224;++y){
            memcpy(p->cram[y],v->cram,sizeof v->cram);
            p->hscroll[y][0]=(uint16_t)-r->camera;
            p->hscroll[y][1]=(uint16_t)(limited?(f?-198:-182):(f?-3:-1));
            p->vscroll[y][0]=0;p->vscroll[y][1]=(uint16_t)r->frame_bg_y;
        }
    }
    s_present_front=1;
    for(unsigned n=1;n<8;++n) {
        CHECK(interpolate(n/8.0,pixels,796,224));
        CHECK(pixels[127]==0xFFFF0000 && pixels[128]==0xFF0000FF);
    }
    }
    /* A vertical camera tick moves an authored parallax boundary through
     * screen row15. Interpolating that screen row's two different bands
     * invents an intermediate horizontal offset (a one-row seam). Follow
     * the same background world row at both endpoints instead. */
    for(unsigned f=0;f<2;++f) {
        PresentationFrame *p=&s_present[f];S2RenderContext *r=&p->render;
        r->frame_fg_x=r->camera=500;r->left=262;r->frame_bg_y=416+(int)f;
        r->frame_arz_bg_x=500*281*256;r->frame_bg_x[0]=r->frame_arz_bg_x;
        for(unsigned y=0;y<224;++y) {
            p->hscroll[y][0]=(uint16_t)-500;
            p->hscroll[y][1]=(uint16_t)source_background_scroll(r,500,(int)y,r->frame_bg_y+(int)y);
            p->vscroll[y][1]=(uint16_t)r->frame_bg_y;
        }
    }
    CHECK(interpolate(.25,pixels,796,224));
    CHECK(pixels[15*796+100]==0xFFFF0000);
    CHECK(interpolate(.75,pixels,796,224));
    CHECK(pixels[15*796+100]==0xFF0000FF);
    /* The fixture buffers are static; ordinary capture owns heap buffers. */
    for(unsigned f=0;f<2;++f){s_present[f].image=NULL;s_present[f].capacity=0;}
    presentation_configure(0);CHECK(configure("off"));
}
static void cpz_interpolation_band_test(void)
{
    static PresentationFrame f;
    f.render.frame_zone=13;
    f.render.frame_fg_x=3360;
    f.render.frame_bg_y=148;
    f.render.frame_bg_x[1]=1664*65536;
    f.vscroll[154][1]=f.vscroll[156][1]=148;
    f.hscroll[154][0]=f.hscroll[156][0]=(uint16_t)-3360;
    f.hscroll[154][1]=(uint16_t)-413;
    f.hscroll[156][1]=(uint16_t)-1664;
    /* World row 304 is two scanlines below the old ripple sample. */
    f.width=320;
    CHECK(captured_background_base(&f,154,302)==413);
    CHECK(captured_background_base(&f,154,304)==1664);
    f.width=398;f.render.left=3321;
    CHECK(captured_background_base(&f,154,304)==1644);
}
static void ooz_interpolation_band_test(void)
{
    static PresentationFrame early,late;
    PresentationFrame *ends[]={&early,&late};
    for(unsigned i=0;i<2;++i) {
        PresentationFrame *f=ends[i];
        f->width=320;
        f->render.frame_zone=10;
        f->render.frame_bg_y=90+(int)i;
        f->render.frame_fg_x=800;
        f->render.frame_bg_x[0]=100*65536;
        for(int line=164;line<=166;++line) {
            f->vscroll[line][1]=(uint16_t)f->render.frame_bg_y;
            f->hscroll[line][0]=(uint16_t)-800;
            f->hscroll[line][1]=(uint16_t)(line+f->render.frame_bg_y<256?-13:-100);
        }
        /* The native sky and factory have different horizontal scroll.
         * A one-row camera shift must not blend these adjacent bands. */
        CHECK(captured_background_base(f,165,255)==13);
        CHECK(captured_background_base(f,165,256)==100);
        f->width=398;f->render.left=761;
        CHECK(captured_background_base(f,165,255)==12);
        CHECK(captured_background_base(f,165,256)==95);
    }
}
static void native_interpolation_test(void)
{
    static GVDP v;
    static uint32_t ordinary[2][320*224],fraction[320*224];
    static uint8_t original_ram[65536];
    CHECK(sonic2_video.configure("off"));online=0;s_loader_active=0;
    clear_guest();memset(g_rom,0,sizeof g_rom);
    memset(&s2_party,0,sizeof s2_party);s2_party.roster.slots=2;
    strcpy(s2_party.roster.character[0],"sonic");strcpy(s2_party.roster.character[1],"tails");
    g_ram[0xF600]=12;g_ram[0xF711]=1;g_ram[0x8003]=1;
    g_ram[0xB000]=1;g_ram[0xB001]=4;g_ram[0xB024]=2;
    word(g_ram,0xB002,0x8004);write32(0xB004,0x100);word(g_ram,0xB00C,40);
    word(g_ram,0xAC00,2);word(g_ram,0xAC02,0xB000);word(g_ram,0xE808,65535);
    word(g_rom,0x100,2);word(g_rom,0x102,1);
    v.reg[1]=64;v.reg[2]=0x30;v.reg[4]=7;v.reg[5]=0x7C;
    v.reg[12]=1;v.reg[13]=0x3C;v.reg[16]=1;v.cram[1]=0xE;
    memset(v.vram+4*32,0x11,32);
    presentation_configure(1);
    S2StateIO size={0};s2_video_rb_state(&size);
    uint8_t *before=(uint8_t*)malloc(size.pos),*after=(uint8_t*)malloc(size.pos);
    CHECK(before && after);
    S2StateIO state={0};state.data=before;state.size=size.pos;state.ok=1;
    s2_video_rb_state(&state);CHECK(state.ok);
    for(unsigned frame=0;frame<2;++frame) {
        word(g_ram,0xFE04,frame+1);word(g_ram,0xB008,20+frame*4);
        word(g_ram,0xF800,168);word(g_ram,0xF804,0x8004);word(g_ram,0xF806,148+frame*4);
        memcpy(v.vram+0xF800,g_ram+0xF800,640);
        memcpy(original_ram,g_ram,sizeof g_ram);M68KState cpu=g_cpu;
        CHECK(!s2_video_hook(0x16604));CHECK(!s2_video_hook(0x16712));
        CHECK(!memcmp(original_ram,g_ram,sizeof g_ram) && !memcmp(&cpu,&g_cpu,sizeof cpu));
        CHECK(!sonic2_video.enabled() && s2_video_main_cpu_divisor()==1);
        for(int y=0;y<224;++y) {
            uint8_t indices[GVDP_MAX_WIDTH];CHECK(gvdp_render_scanline(&v,y,indices)==320);
            for(int x=0;x<320;++x)ordinary[frame][y*320+x]=
                genesis_dac_cram_to_argb(v.cram[indices[x]&63],GENESIS_DAC_NORMAL);
            presentation_capture(&v,y,ordinary[frame]+y*320,320,320,NULL);
        }
    }
    CHECK(ordinary[0][40*320+20]==0xFFFF0000 && ordinary[1][40*320+24]==0xFFFF0000);
    CHECK(interpolate(0,fraction,320,224) && !memcmp(fraction,ordinary[0],sizeof fraction));
    CHECK(interpolate(1,fraction,320,224) && !memcmp(fraction,ordinary[1],sizeof fraction));
    CHECK(interpolate(.5,fraction,320,224));
    CHECK(fraction[40*320+22]==0xFFFF0000 && fraction[40*320+21]==0xFF000000);
    /* The donor body uses the same immutable SAT publication as its shield,
     * even after the next producer has moved the live player ahead. */
    S2RuntimeSprite actors[S2_MAX_PLAYERS+1],dust[S2_PRESENTATION_DUST];
    word(g_ram,0xB008,100);
    CHECK(s2_video_presented_actors(&v,actors,dust) && actors[0].x==24);
    word(g_ram,0xB008,24);
    /* A changed native pose can insert a new body/shield mapping piece.
     * Its new geometry follows the shared root without requiring an old
     * matching piece or dropping the entire fractional picture. */
    PresentationFrame *new_pose=&s_present[s_present_front];
    unsigned pose_piece=new_pose->scene.count++;
    new_pose->scene.sprites[pose_piece]=(SceneSprite){40,40,0x8004,0,0,0,1};
    new_pose->keys[pose_piece]=0xD180001Fu;
    CHECK(interpolate(.5,fraction,320,224));
    CHECK(fraction[40*320+38]==0xFFFF0000 && fraction[40*320+37]==0xFF000000);
    --new_pose->scene.count;
    CHECK(!memcmp(original_ram,g_ram,sizeof g_ram));
    state.pos=0;state.data=after;s2_video_rb_state(&state);
    CHECK(state.ok && !memcmp(before,after,size.pos));

    /* Asteron's frame 1 inserts an eye before its two unchanged body pieces.
     * Reusing mapping ordinals pairs opposite halves during fractional
     * presentation, opening an 8px gap at the midpoint in either direction. */
    static const uint8_t asteron_mapping[]={
        0x00,0x0A,0x00,0x1C,0x00,0x36,0x00,0x40,0x00,0x4A,
        0x00,0x02,0xF0,0x07,0x00,0x00,0x00,0x00,0xFF,0xF0,
        0xF0,0x07,0x08,0x00,0x08,0x00,0x00,0x00,
        0x00,0x03,0xFD,0x00,0x20,0x0E,0x20,0x07,0xFF,0xFC,
        0xF0,0x07,0x00,0x00,0x00,0x00,0xFF,0xF0,
        0xF0,0x07,0x08,0x00,0x08,0x00,0x00,0x00
    };
    memcpy(g_rom+0x38A96,asteron_mapping,sizeof asteron_mapping);
    for(unsigned direction=0;direction<2;++direction) {
        unsigned counts[2];
        for(unsigned frame=0;frame<2;++frame) {
            PresentationFrame *f=&s_present[s_present_front^(frame?0:1)];
            counts[frame]=f->scene.count;
            add_mapping(&f->scene,f->keys,0xB4000000u,0x38A96,frame^direction,
                        0x8368,4,100,60,0,0);
            memset(f->render.video_frame.vram+0x368*32,0x11,16*32);
            for(unsigned y=0;y<224;++y)f->cram[y][17]=0xE00;
        }
        for(unsigned phase=1;phase<4;++phase) {
            CHECK(interpolate(phase*.25,fraction,320,224));
            for(unsigned x=84;x<116;++x)CHECK(fraction[44*320+x]==0xFFFF0000);
            CHECK(fraction[44*320+83]==0xFF000000 && fraction[44*320+116]==0xFF000000);
            const PresentationFrame *current=&s_present[s_present_front];
            for(unsigned n=counts[1];n<current->scene.count;++n) {
                int x=current->scene.sprites[n].x,y=current->scene.sprites[n].y;
                interpolation_sprite(n,&x,&y);
                CHECK(x==current->scene.sprites[n].x && y==current->scene.sprites[n].y);
            }
        }
        for(unsigned frame=0;frame<2;++frame)
            s_present[s_present_front^(frame?0:1)].scene.count=counts[frame];
    }
    CHECK(!memcmp(original_ram,g_ram,sizeof g_ram));

    /* Cannon pose transitions remove nozzle pieces before the ball body.
     * Old piece ordinals must never slide one body row into another. */
    static const uint8_t cannon_mapping[]={
        0,4,0,30,
        0,3, 0xE8,0,0,0,0,0,0xFF,0xF8,
             0xF8,4,0,2,0,0,0xFF,0xF8,
             0,4,0,4,0,0,0xFF,0xF8,
        0,2, 0xF8,4,0,2,0,0,0xFF,0xF8,
             0,4,0,4,0,0,0xFF,0xF8
    };
    memcpy(g_rom+0x254FE,cannon_mapping,sizeof cannon_mapping);
    for(unsigned direction=0;direction<2;++direction) {
        unsigned counts[2];
        for(unsigned frame=0;frame<2;++frame) {
            PresentationFrame *f=&s_present[s_present_front^(frame?0:1)];
            counts[frame]=f->scene.count;
            add_mapping(&f->scene,f->keys,0xB4000000u,0x254FE,frame^direction,
                        0x8000,4,100,60,0,0);
        }
        CHECK(interpolate(.5,fraction,320,224));
        const PresentationFrame *pose=&s_present[s_present_front];
        for(unsigned n=counts[1];n<pose->scene.count;++n) {
            int x=pose->scene.sprites[n].x,y=pose->scene.sprites[n].y;
            interpolation_sprite(n,&x,&y);
            CHECK(x==pose->scene.sprites[n].x && y==pose->scene.sprites[n].y);
        }
        for(unsigned frame=0;frame<2;++frame)
            s_present[s_present_front^(frame?0:1)].scene.count=counts[frame];
    }

    /* Keep the donor life icon on fractional pictures. The stock VRAM tile
     * intentionally has different ink, as it does in an imported campaign. */
    s2_party.s3k_enabled=1;
    strcpy(s2_party.roster.character[0],"knuckles");
    hud_available=1;
    memset(&hud_art,0,sizeof hud_art);
    hud_art.palette[1]=0xE00;
    memset(hud_art.life,0x11,32);
    for(unsigned frame=0;frame<2;++frame) {
        PresentationFrame *f=&s_present[frame];
        unsigned icon=f->scene.count++;
        f->scene.sprites[icon]=(SceneSprite){16,200,0x87D4,0,1};
        f->keys[icon]=0xD4000000u;
        memset(f->render.video_frame.vram+0x7D4*32,0x22,32);
        for(unsigned y=200;y<208;++y)f->cram[y][2]=0x0E0;
    }
    CHECK(interpolate(.5,fraction,320,224));
    CHECK(fraction[200*320+16]==0xFF0000FF);
    hud_available=0;
    s2_party.s3k_enabled=0;
    strcpy(s2_party.roster.character[0],"sonic");

    /* A new object can precede the player in publication order. Its appearance
     * must preserve the player's midpoint and leave the new piece in place. */
    PresentationFrame *current=&s_present[s_present_front];
    unsigned added=current->scene.count++;
    current->scene.sprites[added]=current->scene.sprites[0];
    current->keys[added]=current->keys[0];
    current->scene.sprites[0]=(SceneSprite){100,40,0x8004,0,0};
    current->keys[0]=0xC0000000u;
    CHECK(interpolate(.5,fraction,320,224));
    CHECK(fraction[40*320+22]==0xFFFF0000);
    CHECK(fraction[40*320+100]==0xFFFF0000);
    current->scene.sprites[0]=current->scene.sprites[added];
    current->keys[0]=current->keys[added];
    --current->scene.count;

    /* The sidekick's newly published root can be discontinuous without
     * snapping the leader or disabling the entire fractional picture. */
    added=current->scene.count++;
    current->scene.sprites[added]=(SceneSprite){.x=104,.y=40,.attr=0x8004,.actor=2};
    current->keys[added]=0xB0400100u;
    current->scene.actors.object[1]=0xB040;
    current->scene.actors.x[1]=104;
    current->scene.actors.y[1]=40;
    CHECK(interpolate(.5,fraction,320,224));
    CHECK(fraction[40*320+22]==0xFFFF0000 && fraction[40*320+104]==0xFFFF0000);
    --current->scene.count;

    /* Imported character art priority and blink phases can change while
     * their root is continuous. These are pose changes, not scene cuts. */
    S2RuntimePresentation *older=&s_present[s_present_front^1].party;
    S2RuntimePresentation *newer=&current->party;
    older->sprites[0]=(S2RuntimeSprite){.x=20,.y=40,.art=0x78,.visible=1};
    newer->sprites[0]=(S2RuntimeSprite){.x=24,.y=40,.art=0x8078,.visible=1};
    older->sprites[1]=(S2RuntimeSprite){.x=40,.y=40,.visible=1};
    newer->sprites[1]=(S2RuntimeSprite){.x=180,.y=40,.visible=1};
    unsigned matched_actors=0;
    CHECK(match_presentation_actors(older,newer,&matched_actors) && matched_actors==1);
    CHECK(interpolate(.5,fraction,320,224));
    CHECK(fraction[40*320+22]==0xFFFF0000);
    older->sprites[0].visible=0;
    CHECK(match_presentation_actors(older,newer,&matched_actors) && !matched_actors);
    CHECK(interpolate(.5,fraction,320,224));
    older->sprites[0].visible=1;
    newer->sprites[0].x+=100;
    CHECK(!interpolate(.5,fraction,320,224));
    memset(older->sprites,0,sizeof older->sprites);
    memset(newer->sprites,0,sizeof newer->sprites);

    /* Scanline buckets preserve original publication order. A low-priority
     * pose still owns its sprite pixel before a later high-priority pose. */
    const unsigned first_piece=current->scene.count++;
    current->scene.sprites[first_piece]=(SceneSprite){120,55,4,0,0};
    current->keys[first_piece]=0xC4000100u;
    const unsigned second_piece=current->scene.count++;
    current->scene.sprites[second_piece]=(SceneSprite){120,55,0x8005,0,0};
    current->keys[second_piece]=0xC4400100u;
    memset(current->render.video_frame.vram+5*32,0x22,32);
    for(unsigned row=55;row<63;++row)current->cram[row][2]=0xE00;
    CHECK(interpolate(.5,fraction,320,224));
    CHECK(fraction[55*320+120]==0xFFFF0000 && fraction[62*320+127]==0xFFFF0000);
    CHECK(fraction[54*320+120]==0xFF000000 && fraction[63*320+120]==0xFF000000);
    CHECK(s_sprite_row_offset[56]-s_sprite_row_offset[55]>=2);
    /* The ordinary row-list path must match the former per-row projection,
     * including sprite order, coverage and pixel priority. */
    const SceneFrame *saved_scene=s_display;
    s_display=&current->scene;s_sprite_rows_ready=0;
    uint32_t colors[64];
    for(unsigned i=0;i<64;++i)colors[i]=genesis_dac_cram_to_argb(current->cram[55][i],GENESIS_DAC_NORMAL);
    CHECK(s_priority_capacity>=320);
    for(int row=0;row<224;++row) {
        uint32_t cached[320]={0},direct[320]={0};
        uint8_t cached_priority[320];
        memset(s_priority,0,320);
        draw_scene_sprites(&current->render.video_frame,row,cached,320,0,colors);
        memcpy(cached_priority,s_priority,sizeof cached_priority);
        memset(s_priority,0,320);
        for(unsigned n=0;n<s_display->count;++n) {
            const SceneSprite *sprite=&s_display->sprites[n];
            int base,y;
            project_sprite(sprite,n,320,0,&base,&y);
            if(row>=y && row<y+((sprite->size&3)+1)*8)
                draw_scene_sprite_row(&current->render.video_frame,sprite,row,direct,320,0,colors,base,y,8);
        }
        CHECK(!memcmp(cached,direct,sizeof cached));
        CHECK(!memcmp(cached_priority,s_priority,sizeof cached_priority));
    }
    CHECK(s_sprite_rows_ready && s_sprite_row_view.scene==s_display);
    s_display=saved_scene;s_sprite_rows_ready=0;
    current->scene.count=first_piece;

    /* Actor animation identities may change; replacing the actor or
     * teleporting its camera still repeats an ordinary picture. */
    s_present[s_present_front].keys[0]^=1;CHECK(interpolate(.5,fraction,320,224));
    s_present[s_present_front].keys[0]^=1;
    s_present[s_present_front].scene.actors.object[0]^=64;CHECK(!interpolate(.5,fraction,320,224));
    s_present[s_present_front].scene.actors.object[0]^=64;
    s_present[s_present_front].render.frame_fg_x+=512;CHECK(!interpolate(.5,fraction,320,224));
    presentation_configure(0);CHECK(!interpolate(.5,fraction,320,224));
    free(before);free(after);
}

static void signpost_palette_test(void)
{
    static GVDP v,before;
    GVDPRasterRow row;uint8_t pixels[GVDP_MAX_WIDTH],ram_before[65536];
    const unsigned tiles[]={0x434,0x440,0x450,0x454,0x456,0x46E,0x472,0x481,0x482};
    memset(&v,0,sizeof v);clear_guest();memset(&hud_art,0,sizeof hud_art);
    memset(&s2_party,0,sizeof s2_party);hud_available=0;s2_video_prepare_knuckles_items();
    hud_available=1;s2_party.roster.slots=1;strcpy(s2_party.roster.character[0],"knuckles");
    g_ram[0xF600]=12;s_donor_fade=s_presented_fade=(DonorFade){0};
    hud_art.palette[1]=0x00E;hud_art.palette[2]=0x0E0;
    memset(hud_art.signpost,0x22,sizeof hud_art.signpost);
    v.reg[1]=64;v.reg[2]=0x30;v.reg[4]=7;v.reg[5]=0x7C;v.reg[12]=1;v.reg[16]=1;
    v.cram[1]=0xE00;s2_video_prepare_knuckles_items();
    for(unsigned owner=0;owner<2;++owner)for(unsigned i=0;i<sizeof tiles/sizeof tiles[0];++i) {
        unsigned tile=tiles[i];memset(v.vram+tile*32,0x11,32);
        memset(g_ram+0xF800,0,640);word(g_ram,0xF800,128);word(g_ram,0xF804,tile);word(g_ram,0xF806,128);
        g_cpu.A[0]=0xFFFFD180;g_cpu.A[2]=0xFFFFF808;item_owner=(int)owner;
        s2_video_hook(0x1684E);memcpy(v.vram+0xF800,g_ram+0xF800,640);
        before=v;memcpy(ram_before,g_ram,sizeof g_ram);
        CHECK(gvdp_capture_presented_scanline(&v,0,&row)==320 && gvdp_compose_captured(&row,pixels));
        uint32_t actual=pixels[0]>=GVDP_HOST_PALETTE_BASE?
            row.host_palette[pixels[0]-GVDP_HOST_PALETTE_BASE]:
            genesis_dac_cram_to_argb(v.cram[pixels[0]&63],GENESIS_DAC_NORMAL);
        uint32_t expected=!owner || tile==0x482?0xFF0000FFu:tile==0x456?0xFF00FF00u:0xFFFF0000u;
        CHECK(actual==expected);
        CHECK(!memcmp(&v,&before,sizeof v) && !memcmp(g_ram,ram_before,sizeof g_ram));
    }
    item_owner=0;hud_available=0;s2_video_prepare_knuckles_items();
}
static void private_water_palette_test(void)
{
    static GVDP v;
    uint8_t original[65536];GVDPRasterRow captured;
    clear_guest();memset(&s2_party,0,sizeof s2_party);
    s2_party.roster.slots=1;strcpy(s2_party.roster.character[0],"knuckles");
    g_ram[0xF600]=12;g_ram[0xFE10]=13;g_ram[0xF64C]=1;hud_available=1;
    for(unsigned c=0;c<16;++c) {
        word(g_ram,0xFB00+c*2,0xEEE);word(g_ram,0xF080+c*2,0xAEE);
        word(g_ram,0xF000+c*2,0xAEE);v.cram[c]=0xEEE;
        hud_art.palette[c]=0xE;hud_art.water_palette[0][c]=0xE00;
    }
    v.reg[1]=64;v.reg[2]=0x30;v.reg[4]=7;v.reg[5]=0x7C;v.reg[12]=1;v.reg[13]=0x3F;
    word(v.vram,0xF800,228);word(v.vram,0xF804,0x400);word(v.vram,0xF806,128);
    v.vram[0xF802]=GVDP_SPRITE_PALETTE_ONLY;memset(v.vram+0x400*32,0x11,32);
    /* A next-frame RAM split must not recolour the current native row. */
    g_ram[0xF625]=0;s2_video_prepare_knuckles_items();memcpy(original,g_ram,sizeof original);
    CHECK(!s2_video_water_palette(&v));
    CHECK(gvdp_capture_presented_scanline(&v,100,&captured)==320);
    CHECK(captured.host_palette[17]==0xFFFF0000u && !memcmp(original,g_ram,sizeof original));
    g_ram[0xF625]=223;for(unsigned c=0;c<16;++c)v.cram[c]=0xAEE;
    CHECK(s2_video_water_palette(&v));
    CHECK(gvdp_capture_presented_scanline(&v,100,&captured)==320);
    CHECK(captured.host_palette[17]==0xFF0000FFu);
    s_item_water_enabled=0;gvdp_set_host_sprites(NULL,NULL);
    gvdp_set_sprite_patterns(NULL,0,NULL);gvdp_set_ext_tiles(NULL);
}
static void special_stage_pixel_shape_test(void)
{
    static GVDP v;
    uint32_t native[256],out[398],lazy[398];uint8_t ram[65536];
    memset(&v,0,sizeof v);clear_guest();
    memset(&s_build,0,sizeof s_build);memset(s_history,0,sizeof s_history);
    s_display=NULL;g_ram[0xF600]=16;
    v.reg[1]=64;v.reg[12]=8;v.cram[1]=0xE;
    for(unsigned x=0;x<256;++x)native[x]=0xFF010000u+x;
    CHECK(sonic2_video.configure("16:9"));memcpy(ram,g_ram,sizeof ram);
    sonic2_video.scanline(&v,0,native,256,out,398);
    CHECK(s_width==398 && s_native_x==39);
    int populated=0;unsigned seen[256]={0};
    for(int x=0;x<398;++x) {
        int sample=x*318/398,local=sample-31;
        CHECK(out[x]==(local>=0 && local<256?native[local]:0xFF000000u));
        if(local>=0 && local<256){++populated;++seen[local];}
    }
    CHECK(populated>=319 && populated<=321);
    for(unsigned x=0;x<256;++x)CHECK(seen[x]);
    native_fetches=0;sonic2_video.scanline_lazy(&v,0,256,lazy,398,fetch_native,native);
    CHECK(native_fetches==1 && !memcmp(out,lazy,sizeof out));
    sonic2_video.scanline(&v,0,native,256,out,256);
    CHECK(!memcmp(out,native,sizeof native));
    CHECK(!memcmp(ram,g_ram,sizeof ram));CHECK(sonic2_video.configure("off"));
}

static void front_ui_and_background_priority_test(void)
{
    static GVDP v;uint32_t out[398];uint8_t ram[65536];
    memset(&v,0,sizeof v);clear_guest();memset(&s2_party,0,sizeof s2_party);
    presentation_configure(0);CHECK(sonic2_video.configure("off"));
    g_ram[0xF600]=12;g_ram[0xF711]=1;g_ram[0xB800]=0x3A;
    v.reg[1]=64;v.reg[4]=7;v.reg[5]=0x7C;v.reg[12]=1;v.reg[16]=1;v.cram[1]=0xEEE;
    memset(v.vram+32,0x10,32); /* real ink followed by transparent donor-visible holes */
    word(g_ram,0xF800,128);word(g_ram,0xF804,0x8001);word(g_ram,0xF806,128);
    s2_video_hook(0x16604);g_cpu.A[0]=0xFFFFB800;g_cpu.A[2]=0xFFFFF808;
    s2_video_hook(0x1684E);s2_video_hook(0x16712);
    memcpy(v.vram+0xF800,g_ram+0xF800,640);memcpy(ram,g_ram,sizeof ram);
    for(unsigned x=0;x<320;++x)out[x]=0xFFFF0000;
    s2_video_front_ui(&v,0,out,320);
    CHECK(out[0]==0xFFFFFFFF && out[1]==0xFFFF0000);
    CHECK(!memcmp(ram,g_ram,sizeof ram));
    /* Native zone-title sprites are Obj34, separate from Obj0A bubbles. */
    g_ram[0xB800]=0x34;g_cpu.A[1]=0;
    s2_video_hook(0x16604);s2_video_hook(0x1684E);s2_video_hook(0x16712);
    CHECK(s_front_build.kind[0][0]==3);
    out[0]=out[1]=0xFFFF0000;s2_video_front_ui(&v,0,out,320);
    CHECK(out[0]==0xFFFFFFFF && out[1]==0xFFFF0000);
    g_ram[0xB800]=0x3A;
    /* BuildHUD has no object in A0: its direct native mapping must still
     * cover a donor body, while transparent glyph holes expose that body. */
    g_cpu.A[0]=0;g_cpu.A[1]=0x40AAA;
    s2_video_hook(0x16604);s2_video_hook(0x1684E);s2_video_hook(0x16712);
    CHECK(s_front_build.kind[0][0]==1);
    out[0]=out[1]=0xFFFF0000;s2_video_front_ui(&v,0,out,320);
    CHECK(out[0]==0xFFFFFFFF && out[1]==0xFFFF0000);
    /* The native second viewport uses the same direct mapping path. */
    word(g_ram,0xFFD8,1);g_cpu.A[2]=0xFFFFDD08;g_cpu.A[1]=0x40C88;
    s2_video_hook(0x16604);s2_video_hook(0x16DEE);
    CHECK(s_front_build.kind[1][0]==1);
    word(g_ram,0xFFD8,0);g_cpu.A[0]=0xFFFFB800;g_cpu.A[1]=0;
    g_cpu.A[2]=0xFFFFF808;
    /* The separate KiS2 HUD pass owns the entire old Sonic name footprint,
     * including places where the replacement glyph is transparent. */
    strcpy(s2_party.roster.character[0],"knuckles");hud_available=1;
    int saved_s3k=s2_party.s3k_enabled;s2_party.s3k_enabled=1;
    word(g_ram,0xF804,0xA7D8);memset(v.vram+0x7D8*32,0x11,32);
    g_cpu.A[0]=0;g_cpu.A[1]=0x40ABA;
    s2_video_hook(0x16604);s2_video_hook(0x1684E);s2_video_hook(0x16712);
    memcpy(v.vram+0xF800,g_ram+0xF800,640);
    out[0]=out[1]=0xFFFF0000;s2_video_front_ui(&v,0,out,320);
    CHECK(out[0]==0xFFFF0000 && out[1]==0xFFFF0000);
    hud_available=0;s2_party.s3k_enabled=saved_s3k;
    s2_party.roster.character[0][0]=0;word(g_ram,0xF804,0x8001);
    g_cpu.A[0]=0xFFFFB800;g_cpu.A[1]=0;
    /* Front UI retains a tagged piece's private palette as well as its art. */
    static uint8_t patterns[65536],lines[2048];uint16_t colors[GVDP_EXT_LINES*16]={0};
    memset(patterns+32,0x10,32);lines[1]=2;colors[17]=0xE;
    g_ram[0xF802]=0x80;s2_video_hook(0x16604);g_cpu.A[0]=0xFFFFB800;g_cpu.A[2]=0xFFFFF808;
    s2_video_hook(0x1684E);s2_video_hook(0x16712);memcpy(v.vram+0xF800,g_ram+0xF800,640);
    CHECK(gvdp_set_sprite_patterns_bank(0,patterns,sizeof patterns,lines));
    gvdp_set_ext_colors(colors,colors,1<<20);
    s2_video_front_ui(&v,0,out,320);CHECK(out[0]==0xFFFF0000);
    gvdp_set_sprite_patterns(NULL,0,NULL);memset(colors,0,sizeof colors);
    gvdp_set_ext_colors(colors,colors,1<<20);
    memcpy(ram,g_ram,sizeof ram);
    /* A disk state restores the matching publication/classification, while
     * rollback invalidates this presentation-only cache for the next build. */
    S2StateIO len={0};s2_video_state(&len);
    uint8_t *saved=malloc(len.pos);CHECK(saved);
    S2StateIO state={0};state.data=saved;state.size=len.pos;state.ok=1;
    s2_video_state(&state);CHECK(state.ok);
    memset(s_front_history,0,sizeof s_front_history);
    memset(s_front_display,0,sizeof s_front_display);s_front_serial=0;
    state.pos=0;state.mode=2;s2_video_state(&state);CHECK(state.ok);
    CHECK(s_front_serial && s_front_history[s_front_serial%3].kind[0][0]==2);
    out[0]=0xFFFF0000;s2_video_front_ui(&v,0,out,320);CHECK(out[0]==0xFFFFFFFF);
    free(saved);
    S2StateIO rb_len={0};s2_video_rb_state(&rb_len);
    saved=malloc(rb_len.pos);CHECK(saved);
    state=(S2StateIO){0};state.data=saved;state.size=rb_len.pos;state.ok=1;
    s2_video_rb_state(&state);CHECK(state.ok);
    state.pos=0;state.mode=2;s2_video_rb_state(&state);CHECK(state.ok);
    CHECK(!s_front_serial && !s_front_display[0].serial);free(saved);
    /* A background plane with priority also hides low-priority actors. */
    word(v.vram,0xE000,0x8001);
    CHECK(!s2_video_actor_pixel_visible(&v,0,0,0,g_ram));
    CHECK(s2_video_actor_pixel_visible(&v,0,0,1,g_ram));
    /* Capture a real queued Obj34 mapping through the widescreen path, then
     * restore its opaque UI pixel after a donor while preserving its hole. */
    CHECK(sonic2_video.configure("16:9"));CHECK(reserve_sprite_row(398));
    memset(s_priority,0,398);memset(&s_display_frame,0,sizeof s_display_frame);
    memset(g_rom,0,sizeof g_rom);word(g_ram,0xE808,0xFFFF);
    g_ram[0xB000]=1;g_ram[0xB800]=0x34;g_ram[0xB801]=0;
    word(g_ram,0xB802,0x8001);word(g_ram,0xB804,0);word(g_ram,0xB806,0x100);
    word(g_ram,0xB808,128);word(g_ram,0xB80A,128);word(g_ram,0xB830,128);
    word(g_ram,0xAC00,2);word(g_ram,0xAC02,0xB800);
    word(g_rom,0x100,2);word(g_rom,0x102,1);
    memset(g_rom+0x104,0,8);memcpy(ram,g_ram,sizeof ram);
    capture_objects_into(&s_display_frame,398,1,NULL);
    CHECK(s_display_frame.count==1);s_display=&s_display_frame;
    s_frame_level=1;s_frame_vs=0;s_camera=s_camera_y=0;
    uint32_t palette[64]={0};palette[1]=0xFFFFFFFF;
    for(unsigned x=0;x<398;++x)out[x]=0xFFFF0000;
    draw_scene_sprites(&v,0,out,398,0,palette);
    CHECK(out[39]==0xFFFFFFFF && out[40]==0xFFFF0000);
    out[39]=0xFFFF0000;s2_video_front_ui(&v,0,out,398);
    CHECK(out[39]==0xFFFFFFFF && out[40]==0xFFFF0000);
    CHECK(!memcmp(ram,g_ram,sizeof ram));CHECK(sonic2_video.configure("off"));
}

static void foreground_world_sprite_test(void)
{
    static GVDP v;uint32_t out[398];uint8_t ram[65536];
    memset(&v,0,sizeof v);clear_guest();memset(&s2_party,0,sizeof s2_party);
    presentation_configure(0);CHECK(sonic2_video.configure("off"));
    memset(&s_front_build,0,sizeof s_front_build);memset(s_front_history,0,sizeof s_front_history);
    memset(s_front_display,0,sizeof s_front_display);s_front_serial=0;
    g_ram[0xF600]=12;g_ram[0xF711]=1;g_ram[0xB840]=0xA8;
    g_ram[0xB841]=4;g_ram[0xB858]=2;
    v.reg[1]=64;v.reg[4]=7;v.reg[5]=0x7C;v.reg[12]=1;v.reg[16]=1;v.cram[1]=0xEEE;
    memset(v.vram+32,0x10,32);
    word(g_ram,0xF800,128);word(g_ram,0xF804,1);word(g_ram,0xF806,128);
    front_ui_begin();g_cpu.A[0]=0xFFFFB840;g_cpu.A[1]=0;g_cpu.A[2]=0xFFFFF808;
    /* DisplaySprite3 can enqueue stars ahead of actors without changing
     * the object's nominal priority byte. Use the queue being consumed. */
    g_cpu.D[7]=6;g_cpu.A[4]=0xFFFFAC80;
    s2_video_hook(0x1684E);front_ui_publish();
    memcpy(v.vram+0xF800,g_ram+0xF800,640);memcpy(ram,g_ram,sizeof ram);
    CHECK(s_front_build.kind[0][0]==4);
    for(unsigned x=0;x<320;++x)out[x]=0xFF223344;
    s2_video_preserve_foreground_row(out,320);
    out[0]=out[1]=0xFFFF0000;s2_video_front_ui(&v,0,out,320);
    /* Keep the actual composed colour, not a raw sprite palette lookup.
     * Its opaque pixel hides a donor; its transparent hole exposes it. */
    CHECK(out[0]==0xFF223344 && out[1]==0xFFFF0000);
    CHECK(!memcmp(ram,g_ram,sizeof ram));
    g_ram[0xB858]=1;g_cpu.D[7]=5;g_cpu.A[4]=0xFFFFAD00;
    front_ui_begin();s2_video_hook(0x1684E);front_ui_publish();
    CHECK(!s_front_build.kind[0][0]);
    out[0]=0xFFFF0000;s2_video_front_ui(&v,0,out,320);CHECK(out[0]==0xFFFF0000);
    /* The second competition viewport has its own completed SAT. */
    word(g_ram,0xFFD8,1);g_ram[0xB858]=2;g_cpu.D[7]=7;v.reg[12]=7;
    g_cpu.A[4]=0xFFFFEE20; /* Multipart P2 mappings borrow the queue register. */
    memset(v.vram+64,0x10,64); /* Mode 2 tile 1 is an 8x16 cell. */
    word(g_ram,0xDD00,480);word(g_ram,0xDD04,1);word(g_ram,0xDD06,128);
    front_ui_begin();g_cpu.A[2]=0xFFFFDD08;s2_video_hook(0x16DEE);front_ui_publish();
    memcpy(v.vram+0xF800,g_ram+0xDD00,640);
    CHECK(s_front_build.kind[1][0]==4);
    out[0]=0xFF445566;s2_video_preserve_foreground_row(out,320);
    out[0]=0xFFFF0000;s2_video_front_ui(&v,224,out,320);CHECK(out[0]==0xFF445566);
    /* The expanded renderer keeps the same rule even where a high plane
     * hides the foreground sprite: the actor behind it cannot shine through. */
    word(g_ram,0xFFD8,0);v.reg[12]=1;g_ram[0xB000]=1;
    CHECK(sonic2_video.configure("16:9"));CHECK(reserve_sprite_row(398));
    memset(g_rom,0,sizeof g_rom);word(g_ram,0xE808,0xFFFF);
    word(g_ram,0xB842,1);word(g_ram,0xB844,0);word(g_ram,0xB846,0x100);
    word(g_ram,0xB848,0);word(g_ram,0xB84C,0);
    word(g_ram,0xAC80,2);word(g_ram,0xAC82,0xB840);
    word(g_rom,0x100,2);word(g_rom,0x102,1);memset(g_rom+0x104,0,8);
    capture_objects_into(&s_display_frame,398,1,NULL);s_display=&s_display_frame;
    CHECK(s_display_frame.count==1 && (s_display_frame.sprites[0].hud&64));
    s_frame_level=1;s_frame_vs=0;s_camera=s_camera_y=0;
    memset(s_priority,0,398);s_priority[0]=1;
    for(unsigned x=0;x<398;++x)out[x]=0xFF112233;
    uint32_t palette[64]={0};palette[1]=0xFFFFFFFF;
    draw_scene_sprites(&v,0,out,398,0,palette);
    CHECK(out[0]==0xFF112233);
    out[0]=out[1]=0xFFFF0000;s2_video_front_ui(&v,0,out,398);
    CHECK(out[0]==0xFF112233 && out[1]==0xFFFF0000);
    CHECK(sonic2_video.configure("off"));
}

static void water_surface_span_test(unsigned map)
{
    static GVDP v;uint32_t out[796],palette[64]={0};uint8_t ram[65536];
    memset(&v,0,sizeof v);clear_guest();memset(g_rom,0,sizeof g_rom);
    memset(&s2_party,0,sizeof s2_party);online=0;s_interpolating=0;
    g_ram[0xF600]=12;g_ram[0xF711]=1;g_ram[0xB000]=1;g_ram[0x801F]=1;word(g_ram,0xE808,0xFFFF);
    word(g_ram,0xEEF0,512);word(g_ram,0xEE00,512);
    /* Both REV01 Obj04 maps use three 32x16 pieces in their first frame. */
    word(g_rom,map,12);word(g_rom,map+12,3);
    for(unsigned n=0;n<3;++n) {
        unsigned at=map+14+n*8;g_rom[at]=0xF8;g_rom[at+1]=0x0D;
        word(g_rom,at+6,(unsigned)(-96+(int)n*64));
    }
    for(unsigned n=0;n<2;++n) {
        unsigned o=0xB380+n*64;g_ram[o]=4;g_ram[o+1]=4;g_ram[o+0x19]=128;
        word(g_ram,o+2,0x8400);word(g_ram,o+4,map>>16);word(g_ram,o+6,map);
        word(g_ram,o+8,640+n*192);word(g_ram,o+12,8);
        word(g_ram,0xAC02+n*2,o);
    }
    word(g_ram,0xAC00,4);memcpy(ram,g_ram,sizeof ram);
    v.reg[12]=1;memset(v.vram+0x400*32,0x11,8*32);palette[1]=0xFFFFFFFF;
    CHECK(reserve_sprite_row(796));s_frame_campaign_view=0;s_camera=512;s_camera_y=0;
    capture_objects_into(&s_display_frame,320,1,NULL);s_display=&s_display_frame;
    CHECK(s_display_frame.count==6);
    memset(s_priority,0,796);memset(out,0,sizeof out);
    draw_scene_sprites(&v,0,out,320,0,palette);
    for(unsigned x=0;x<320;++x)CHECK(out[x]==((x%64)>=32?0xFFFFFFFFu:0));
    capture_objects_into(&s_display_frame,796,1,NULL);
    memset(s_priority,0,796);memset(out,0,sizeof out);
    draw_scene_sprites(&v,0,out,796,238,palette);
    for(int x=0;x<796;++x)CHECK(out[x]==(((x-14)&63)<32?0xFFFFFFFFu:0));
    /* A separate camera gets the same native phase, including a wrapped
     * negative origin. An earlier sprite still owns its opaque pixel. */
    s_frame_campaign_view=1;s_camera=512+777;
    memset(s_priority,0,796);memset(out,0,sizeof out);s_priority[10]=2;
    draw_scene_sprites(&v,0,out,320,238,palette);
    for(int x=0;x<320;++x)CHECK(out[x]==(x!=10 && ((x-5)&63)<32?0xFFFFFFFFu:0));
    CHECK(!memcmp(ram,g_ram,sizeof ram));s_frame_campaign_view=0;s_camera=0;
}

static void competition_hud_replacement_test(void)
{
    static GVDP v;uint32_t out[320];
    static const char *names[]={"sonic","tails","amy","knuckles","shadow","mighty"};
    memset(&v,0,sizeof v);clear_guest();memset(&s2_party,0,sizeof s2_party);
    CHECK(sonic2_video.configure("off"));g_ram[0xF600]=12;g_ram[0xF711]=1;
    g_ram[0xB000]=1;g_ram[0xB040]=2;
    word(g_ram,0xFFD8,1);v.reg[1]=64;v.reg[5]=0x7C;v.reg[12]=7;v.cram[1]=0xEEE;
    memset(v.vram+64,0x10,64);
    for(unsigned p=0;p<2;++p) {
        unsigned table=p?0xDD00:0xF800,life_y=p?424:200;
        word(g_ram,table,life_y+256);word(g_ram,table+2,1);
        word(g_ram,table+4,0x8001);word(g_ram,table+6,144);
        word(g_ram,table+8,life_y+256);word(g_ram,table+10,0);
        word(g_ram,table+12,0x8001);word(g_ram,table+14,160);
        g_cpu.A[0]=0;g_cpu.A[1]=0x40C88;g_cpu.A[2]=0xFFFF0000u|(table+8);
        s2_video_hook(0x16604);s2_video_hook(0x16DEE);
        g_cpu.A[1]+=8;g_cpu.A[2]+=8;s2_video_hook(0x16DEE);s2_video_hook(0x16712);
        memcpy(v.vram+0xF800,g_ram+table,640);
        for(unsigned c=0;c<6;++c) {
            strcpy(s2_party.roster.character[p],names[c]);
            for(unsigned x=0;x<320;++x)out[x]=0xFFFF0000;
            s2_video_front_ui(&v,p?224:0,out,320);
            s2_video_front_ui(&v,(int)life_y,out,320);
            /* The native role keeps its own glyph. Other roles delegate the
             * entire old life-name footprint to competition_hud's drawer. */
            uint32_t expected=c==(p?1u:0u)?0xFFFFFFFF:0xFFFF0000;
            if(out[16]!=expected || out[32]!=expected)
                fprintf(stderr,"VS life HUD p%u %s: %08x/%08x expected %08x\n",p+1,names[c],out[16],out[32],expected);
            CHECK(out[16]==expected && out[32]==expected);
            CHECK(out[17]==0xFFFF0000 && out[33]==0xFFFF0000);
        }
    }
}

uint16_t s2_runtime_title_card_ink(void){return 0;}
int main(void)
{
    /* Byte-matched SwScrl_EHZ_2P band boundaries and fixed-point gradient;
     * CPZ's row18 ripple stays in BG1 before the half-speed BG2 band. */
    /* Attract end/skip selects Sega/title before the old world fades. Its
     * display stays wide until incoming initialization retires the level;
     * original loading/fade CPU timing remains unchanged. */
    CHECK(sonic2_video.configure("32:9"));
    for(unsigned destination=0;destination<=4;destination+=4) {
        clear_guest();g_ram[0xF600]=(uint8_t)destination;
        g_ram[0xB000]=1;g_ram[0xF711]=1;
        CHECK(s2_video_outgoing_level() && !gameplay() && !competition() && s2_video_main_cpu_divisor()==1);
        word(g_ram,0xFFD8,1);CHECK(s2_video_outgoing_level() && !gameplay() && !competition());
        g_ram[0xF711]=0;CHECK(!s2_video_outgoing_level());
        g_ram[0xF711]=1;g_ram[0xB000]=0;CHECK(!s2_video_outgoing_level());
    }
    clear_guest();
    CHECK(sonic2_video.configure("off"));
#if OWN_BACKEND
    /* Avoid palette work in ordinary play, but retain an expiring star's
     * palette until both queued and currently displayed SATs retire it. */
    CHECK(!shadow_palette_needed());g_ram[0xF600]=12;
    shadow_item_owner=1;g_ram[0xD200]=0x35;
    CHECK(shadow_palette_needed());
    shadow_item_owner=0;g_ram[0xD200]=0;
    g_ram[0xF802]=0xC0;word(g_ram,0xF804,0x5A4);
    CHECK(!shadow_palette_needed()); /* Explosion shares the pattern bank only. */
    word(g_ram,0xF804,0x4DE);CHECK(shadow_palette_needed());g_ram[0xF802]=0;
    word(g_ram,0xFFD8,1);g_ram[0xDD02]=0xC0;
    word(g_ram,0xDD04,0x2D2);CHECK(!shadow_palette_needed());
    word(g_ram,0xDD04,0x26F); /* Competition cells are16px tall. */
    CHECK(shadow_palette_needed());g_ram[0xDD02]=0;word(g_ram,0xFFD8,0);
    g_machine.vdp.reg[5]=0x7C;g_machine.vdp.reg[12]=1;
    g_machine.vdp.vram[0xF802]=0xC0;word(g_machine.vdp.vram,0xF804,0x5A4);
    CHECK(!shadow_palette_needed());word(g_machine.vdp.vram,0xF804,0x4DE);
    CHECK(shadow_palette_needed());g_machine.vdp.vram[0xF802]=0;
    s_display_frame.count=1;s_display_frame.sprites[0].size=0xC0;s_display=&s_display_frame;
    s_display_frame.sprites[0].attr=0x5A4;CHECK(!shadow_palette_needed());
    s_display_frame.sprites[0].attr=0x4DE;
    CHECK(shadow_palette_needed());s_display=NULL;s_display_frame.count=0;
    CHECK(!shadow_palette_needed());g_ram[0xF600]=0;
#endif
    /* Original item art is sprite-only; missing resources retain native VRAM.
     * Piece tags are independent of owner-resource availability. */
    unsigned pattern_bytes=0; s2_video_prepare_knuckles_items();
    CHECK(!gvdp_sprite_patterns(0x80,&pattern_bytes) && !pattern_bytes);
    CHECK(knuckles_item_tile(0x4BE) && knuckles_item_tile(0x4FF));
    CHECK(knuckles_item_tile(0x6AC) && knuckles_item_tile(0x6B3));
    CHECK(!knuckles_item_tile(0x4BD) && !knuckles_item_tile(0x500));
    CHECK(!knuckles_item_tile(0x6AB) && !knuckles_item_tile(0x6B4));
    hud_available=1; hud_art.shield_stars[0]=0x67; hud_art.monitor[0]=0x98;hud_art.signpost[0]=0xAB;
    s2_video_prepare_knuckles_items();
    const uint8_t *patterns=gvdp_sprite_patterns(0x80,&pattern_bytes);
    CHECK(patterns && pattern_bytes==65536);
    CHECK(patterns[0x4BE*32]==0x67 && patterns[0x6AC*32]==0x98);
    CHECK(patterns[0x456*32]==0xAB);
    CHECK(knuckles_item_tile(0x456) && knuckles_item_tile(0x46D) && !knuckles_item_tile(0x46E));
    CHECK(gvdp_sprite_palette_line(0x80,0x456,0)==2);
    CHECK(gvdp_sprite_palette_line(GVDP_SPRITE_PALETTE_ONLY,0x580,0)==2);
    CHECK(!gvdp_sprite_palette_line(0,0x580,0) && !gvdp_sprite_palette_line(GVDP_SPRITE_PALETTE_ONLY,0x580,1));
    CHECK(!gvdp_sprite_patterns(0,&pattern_bytes));
    unsigned sites[]={0x1684E,0x1688E,0x168E6,0x16938};
    g_cpu.A[0]=0xFFFFD180; g_cpu.A[2]=0xFFFFF810; item_owner=1;
    word(g_ram,0xF80C,0x84BE);
    for(unsigned n=0;n<4;++n) {
        g_ram[0xF80A]=15; CHECK(!s2_video_hook(sites[n]));
        CHECK(g_ram[0xF80A]==0x8F && ram16(0xF80C)==0x84BE);
    }
    item_owner=0; g_ram[0xF80A]=15;s2_video_hook(0x1684E);CHECK(g_ram[0xF80A]==15);
    item_owner=1;word(g_ram,0xF80C,0x8680);s2_video_hook(0x1684E);CHECK(g_ram[0xF80A]==15);
    unsigned competition_sites[]={0x16DEE,0x16E40,0x16E9C,0x16F00};
    word(g_ram,0xFFD8,1);word(g_ram,0xF80C,0x825F);
    for(unsigned n=0;n<4;++n) {
        g_ram[0xF80A]=13;CHECK(!s2_video_hook(competition_sites[n]));CHECK(g_ram[0xF80A]==0x8D);
    }
    word(g_ram,0xF80C,0x8356);g_ram[0xF80A]=4;s2_video_hook(0x16DEE);CHECK(g_ram[0xF80A]==0x84);
    word(g_ram,0xF80C,0x8340);g_ram[0xF80A]=4;s2_video_hook(0x16DEE);CHECK(g_ram[0xF80A]==4);
    g_cpu.A[2]=0xFFFFDD10;word(g_ram,0xDD0C,0x825F);g_ram[0xDD0A]=13;
    s2_video_hook(0x16DEE);CHECK(g_ram[0xDD0A]==0x8D);
    word(g_ram,0xFFD8,0);g_ram[0xDD0A]=13;s2_video_hook(0x1684E);CHECK(g_ram[0xDD0A]==13);
    item_owner=0;hud_available=0;s2_video_prepare_knuckles_items();clear_guest();
    GVDP hud={0}; uint32_t colour=0x12345678;
    s2_party.s3k_enabled=1; strcpy(s2_party.roster.character[0],"knuckles");
    CHECK(!s2_video_knuckles_hud_pixel(&hud,0xA7D8,0,0,&colour) && colour==0x12345678);
    hud_available=1; hud.cram[6]=0xEEE; hud_art.palette[3]=0x00E;
    memset(hud_art.life+4*32,0x33,32);
    CHECK(s2_video_knuckles_hud_pixel(&hud,0xA7D8,0,0,&colour) && colour==0xFFFF0000);
    CHECK(s2_video_knuckles_hud_pixel(&hud,0x87D4,0,0,&colour) && !colour);
    memset(hud.vram+0x7DD*32,0x33,32); /* the native counter, not static donor digits */
    CHECK(s2_video_knuckles_hud_pixel(&hud,0xA7DD,0,0,&colour) && colour==0xFFFF0000);
    /* The E column sits between the two live digit tiles in column-major
     * VRAM. A contiguous static-prefix assumption corrupts just this glyph. */
    memset(hud_art.life+10*32,0x33,32);
    CHECK(s2_video_knuckles_hud_pixel(&hud,0xA7DE,0,0,&colour) && colour==0xFFFF0000);
    memset(hud.vram+0x7DF*32,0x33,32);
    CHECK(s2_video_knuckles_hud_pixel(&hud,0xA7DF,0,0,&colour) && colour==0xFFFF0000);
    g_ram[0xF64C]=1;g_ram[0xFE10]=13;hud_art.water_palette[0][3]=0xE00;
    for(unsigned i=0;i<16;++i) {
        word(g_ram,0xFB00+i*2,0xEEE);word(g_ram,0xF080+i*2,0xAEE);hud.cram[i]=0xAEE;
    }
    CHECK(s2_video_knuckles_hud_pixel(&hud,0xA7DD,0,0,&colour) && colour==0xFF0000FF);
    s2_video_hook(0x246A);for(unsigned i=0;i<7;++i)s2_video_hook(0x248A);
    s2_video_vblank();
    CHECK(s2_video_knuckles_hud_pixel(&hud,0xA7DD,0,0,&colour) && colour==0xFF000000);
    s_donor_fade=s_presented_fade=(DonorFade){0};g_ram[0xF64C]=0;g_ram[0xFE10]=0;
    strcpy(s2_party.roster.character[0],"sonic");
    CHECK(!s2_video_knuckles_hud_pixel(&hud,0xA7D8,0,0,&colour));
    hud_available=0;
    /* The title card hides donor actors using its actual displayed planes,
     * then reveals them before the later Level_started flag. */
    GVDP intro={0}; intro.reg[2]=0x30; intro.reg[4]=7; intro.reg[13]=0x3C;
    memset(intro.vram+5*32,0x11,32); word(intro.vram,0xC000,0x8005);
    CHECK(!s2_video_intro_pixel_visible(&intro,0,0,0));
    CHECK(s2_video_intro_pixel_visible(&intro,0,0,1));
    CHECK(s2_video_intro_pixel_visible(&intro,8,0,0));
    word(intro.vram,0xF000,8); CHECK(s2_video_intro_pixel_visible(&intro,0,0,0));
    word(intro.vram,0xF000,0); intro.vsram[0]=8;
    CHECK(s2_video_intro_pixel_visible(&intro,0,0,0));
    word(intro.vram,0xE000,0x8005); CHECK(!s2_video_intro_pixel_visible(&intro,0,0,0));
    CHECK(!sonic2_video.enabled());CHECK(!sonic2_video.width(2560,720,320,224));
    g_ram[0xF600]=12;g_ram[0xB000]=1;
    for(int n=0;n<16;++n)g_ram[0x8000+n]=1;
    CHECK(stage_width()==2048);
    CHECK(sonic2_video.configure("fit"));
    CHECK(s2_video_main_cpu_divisor()==1);
    g_ram[0xF711]=1;CHECK(s2_video_main_cpu_divisor()==4);
    g_ram[0xFFD9]=1;CHECK(s2_video_main_cpu_divisor()==4);
    CHECK(sonic2_video.configure("off"));CHECK(s2_video_main_cpu_divisor()==1);
    CHECK(sonic2_video.configure("fit"));g_ram[0xFFD9]=0;
    g_ram[0xF600]=0x8C;g_ram[0xF711]=0;g_ram[0xB080]=0x34;
    CHECK(s2_video_main_cpu_divisor()==1);
    word(g_ram,0xB0BE,0xFFFF);CHECK(s2_video_main_cpu_divisor()==4);
    g_ram[0xF600]=0x88;CHECK(s2_video_main_cpu_divisor()==1);
    g_ram[0xF600]=0x8C;CHECK(sonic2_video.configure("off"));CHECK(s2_video_main_cpu_divisor()==1);
    CHECK(sonic2_video.configure("fit"));
    g_ram[0xB080]=0;word(g_ram,0xB0BE,0);g_ram[0xF600]=12;g_ram[0xF711]=1;
    g_ram[0xF600]=16;CHECK(s2_video_main_cpu_divisor()==1);g_ram[0xF600]=12;
    CHECK(sonic2_video.configure("off"));CHECK(s2_video_main_cpu_divisor()==1);
    CHECK(sonic2_video.configure("fit"));g_ram[0xF711]=0;
    CHECK(sonic2_video.width(4000,500,320,224)==1792);
    CHECK(sonic2_video.width(320,224,320,224)==320);
    CHECK(sonic2_video.configure("16:9"));CHECK(sonic2_video.width(1,1,320,224)==398);
    CHECK(sonic2_video.configure("21:9"));CHECK(sonic2_video.width(1,1,320,224)==523);
    CHECK(sonic2_video.configure("32:9"));CHECK(sonic2_video.width(1,1,320,224)==796);
    CHECK(sonic2_video.configure("64:9"));CHECK(sonic2_video.width(1,1,320,224)==1593);
    CHECK(sonic2_video.configure("stage"));CHECK(sonic2_video.width(1,1,320,224)==2048);
    CHECK(!sonic2_video.configure("32:0"));CHECK(!sonic2_video.configure("nan:9"));
    CHECK(!sonic2_video.configure("inf:1"));CHECK(!sonic2_video.configure("32:9junk"));
    CHECK(native_terrain_streamed(943,718));CHECK(!native_terrain_streamed(944,718));
    CHECK(native_terrain_streamed(991,763));CHECK(!native_terrain_streamed(992,763));
    CHECK(native_terrain_streamed(688,718));CHECK(!native_terrain_streamed(687,718));
    /* Interleaved 128px chunks, 16px blocks, independent block/tile flips. */
    word(g_ram,128,3);for(int n=0;n<4;++n)word(g_ram,0x9000+24+n*2,100+n);
    CHECK(world_attr(g_ram,0,0,0)==100);CHECK(world_attr(g_ram,8,8,0)==103);
    word(g_ram,128,3|0x400);CHECK(world_attr(g_ram,0,0,0)==(101^0x800));
    word(g_ram,128,3|0x800);CHECK(world_attr(g_ram,0,0,0)==(102^0x1000));
    word(g_ram,128,3|0xC00);CHECK(world_attr(g_ram,0,0,0)==(103^0x1800));
    g_ram[0x8080]=1;CHECK(world_attr(g_ram,0,0,1)==world_attr(g_ram,0,0,0));
    /* x=640 must not spawn a cell before the culler will retain it. */
    word(g_rom,0xE6800,0x100);word(g_rom,0xE6900,640);word(g_rom,0xE6902,0x8060);
    g_rom[0xE6904]=0x18;word(g_rom,0xE6906,1000);word(g_rom,0xE6908,0xE060);
    g_rom[0xE690A]=0x19;word(g_rom,0xE690C,65535);
    CHECK(sonic2_video.configure("10:7"));sonic2_video.width(1,1,320,224);
    s2_video_hook(0x17AA4);g_ram[0xF76C]=2;write16(0xEE00,1);
    CHECK(s2_video_hook(0x17B84)==1);CHECK(g_ram[0xB400]==0);
    write16(0xEE00,128);CHECK(s2_video_hook(0x17B84)==1);
    CHECK(g_ram[0xB400]==0x18 && ram16(0xB408)==640);
    g_cpu.D[0]=640-ram16(0xF7DA);s2_video_hook(0x105C6);CHECK((uint16_t)g_cpu.D[0]<=640);
    CHECK(sonic2_video.configure("32:9"));sonic2_video.width(1,1,320,224);
    s2_video_hook(0x17B84);CHECK(g_ram[0xB440]==0x19);
    CHECK(g_ram[0xB441]==3 && g_ram[0xB462]==3); /* placement flip bits 13/14 */
    g_cpu.D[0]=0xA5000200;g_cpu.D[1]=0x12340000u|(uint16_t)(1000-ram16(0xF7DA));
    s2_video_hook(0x2ABAC);
    CHECK(g_cpu.D[0]==0xA5000200 && (g_cpu.D[1]>>16)==0x1234 && (uint16_t)g_cpu.D[1]<=640);
    g_cpu.D[1]=0x12340000u|(uint16_t)(5000-ram16(0xF7DA));
    s2_video_hook(0x2ABAC);CHECK((uint16_t)g_cpu.D[1]>640);
    /* Both Sonic and Tails retain the native ring collision code. */
    word(g_ram,0xE808,100);word(g_ram,0xE80E,65535);g_cpu.A[0]=0xFFFFB040;
    s2_video_hook(0x170D0);CHECK(g_cpu.A[1]==0xFFFFE806 && g_cpu.A[2]==0xFFFFE80C);
    CHECK(sonic2_video.configure("off"));
    g_cpu.D[0]=12345;s2_video_hook(0x105C6);CHECK(g_cpu.D[0]==12345);
    g_cpu.D[1]=0x12345678;s2_video_hook(0x2ABAC);CHECK(g_cpu.D[1]==0x12345678);
    /* H32 special-stage scenery fills the wide canvas while retaining the
     * original track/sprite projection. Snapshot art stays immutable, CRAM
     * remains live for genuine HBlank palette effects. */
    static GVDP v;uint32_t native[256],out[796];
    for(unsigned n=0;n<256;++n)native[n]=0xFF123456;
    v.reg[1]=64;v.reg[2]=0x20;v.reg[4]=6;v.reg[13]=0x2C;v.reg[16]=3;
    memset(v.vram+32,0x11,32);v.cram[1]=0xE;
    for(unsigned n=0;n<4096;++n)word(v.vram,0xC000+n*2,1);
    g_ram[0xF600]=16;g_ram[0xB000]=9;
    CHECK(sonic2_video.configure("32:9"));
    sonic2_video.scanline(&v,0,native,256,out,796);
    CHECK(out[0]==0xFFFF0000 && out[795]==0xFFFF0000);
    CHECK(!memcmp(out+270,native,sizeof native));
    {
        uint32_t expected[796];memcpy(expected,out,sizeof expected);
        native_fetches=0;
        sonic2_video.scanline_lazy(&v,0,256,out,796,fetch_native,native);
        CHECK(native_fetches==1);CHECK(!memcmp(expected,out,sizeof expected));
    }
    memset(v.vram+32,0,32);v.cram[1]=0xE0;
    sonic2_video.scanline(&v,1,native,256,out,796);
    CHECK(out[0]==0xFF00FF00 && out[795]==0xFF00FF00);
    /* Actual SpecialStage setup is register 12=$08, not plain H32. */
    v.reg[12]=8;memset(v.vram+32,0x11,32);
    sonic2_video.scanline(&v,0,native,256,out,796);
    CHECK(out[0]==genesis_dac_cram_to_argb(0xE0,GENESIS_DAC_SHADOW));
    CHECK(!memcmp(out+270,native,sizeof native));
    /* Completed host HUD/ring publication survives a partial native DMA and
     * producer-ring rollover, without a one-frame centered native fallback. */
    CHECK(sonic2_video.configure("off"));CHECK(sonic2_video.configure("32:9"));
    g_ram[0xF600]=12;g_ram[0xF711]=1;g_ram[0xB000]=1;
    memset(&v,0,sizeof v);v.reg[1]=64;v.reg[2]=0x30;v.reg[4]=7;
    v.reg[5]=0x7C;v.reg[12]=1;v.reg[13]=0x3F;v.reg[16]=1;
    v.cram[1]=0xE;memset(v.vram+32,0x11,32);
    uint32_t native320[320];for(unsigned n=0;n<320;++n)native320[n]=0xFF123456;
    s_build.count=2;s_build.scene=1;
    s_build.sprites[0]=(SceneSprite){16,0,0x8001,0,1};
    s_build.sprites[1]=(SceneSprite){700,0,0x8001,0,0};
    publish_sprites();memcpy(v.vram+0xF800,g_ram+0xF800,640);
    sonic2_video.scanline(&v,0,native320,320,out,796);
    CHECK(out[16]==0xFFFF0000 && out[700]==0xFFFF0000);
    {
        uint32_t expected[796];
        sonic2_video.scanline(&v,1,native320,320,expected,796);
        native_fetches=0;
        sonic2_video.scanline_lazy(&v,1,320,out,796,fetch_native,native320);
        CHECK(native_fetches==0);CHECK(!memcmp(expected,out,sizeof expected));
    }
    v.vram[0xF806]^=1;
    sonic2_video.scanline(&v,0,native320,320,out,796);
    CHECK(out[16]==0xFFFF0000 && out[700]==0xFFFF0000);
    CHECK(s_scene_holds==1);
    /* Retained world sprites use their captured camera, not the new camera:
     * ring world X=700 is now at X=438; HUD stays at X=16. */
    word(g_ram,0xEE60,500);word(v.vram,0xFC00,(uint16_t)-500);
    v.vsram[0]=4;
    sonic2_video.scanline(&v,0,native320,320,out,796);
    CHECK(out[16]==0xFFFF0000 && out[438]==0xFFFF0000);
    sonic2_video.scanline(&v,4,native320,320,out,796);
    CHECK(out[16]==0xFFFF0000 && out[438]!=0xFFFF0000);
    word(g_ram,0xEE60,0);word(v.vram,0xFC00,0);
    v.vsram[0]=0;
    v.vram[0xF806]^=1;
    sonic2_video.scanline(&v,0,native320,320,out,796);
    for(unsigned n=0;n<3;++n) {
        word(g_ram,0xF806,n+1);s_build.sprites[0].x=24;publish_sprites();
    }
    sonic2_video.scanline(&v,1,native320,320,out,796);
    CHECK(out[16]==0xFFFF0000 && out[700]==0xFFFF0000);
    sonic2_video.scanline(&v,0,native320,320,out,796);
    CHECK(out[16]==0xFFFF0000 && out[700]==0xFFFF0000);
    memcpy(v.vram+0xF800,g_ram+0xF800,640);
    sonic2_video.scanline(&v,0,native320,320,out,796);
    CHECK(out[24]==0xFFFF0000); /* new publication accepted */
    g_ram[0xF600]=4;g_ram[0xB000]=0;v.vram[0xF806]^=1;
    sonic2_video.scanline(&v,0,native320,320,out,796);
    CHECK(!s_display); /* never hold an old level over a different scene */
    CHECK(sonic2_video.configure("off"));CHECK(sonic2_video.configure("fit"));
    CHECK(!s_display_frame.serial);
    /* IRQ-entry cadence distinguishes real missed ticks from the variable
     * end-of-wall-frame phase of native V-int DMA work. */
    CHECK(sonic2_video.configure("off"));s2_video_vblank();
    CHECK(sonic2_video.configure("fit"));
    g_ram[0xF600]=12;g_ram[0xF711]=1;g_ram[0xB000]=1;g_ram[0xB024]=2;
    word(g_ram,0xFE04,10);s2_video_vblank();
    word(g_ram,0xFE04,11);++s_serial;s2_video_vblank();
    CHECK(s_tick_samples==1 && s_tick_updates==1 && !s_tick_lag && !s_tick_multi && !s_publication_lag);
    s2_video_vblank();CHECK(s_tick_lag==1 && s_publication_lag==1);
    word(g_ram,0xFE04,13);++s_serial;s2_video_vblank();CHECK(s_tick_multi==1);
    CHECK(sonic2_video.configure("off"));s2_video_vblank();CHECK(s_tick_samples==3);
    /* Four distant actors share activation even with native video settings.
     * Visited cells survive departure, with a safe all-view union if the
     * original object routines run out of slots. No local-seat input here. */
    online=1;CHECK(sonic2_video.enabled());
    CHECK(sonic2_video.width(1920,1080,320,224)==320);
    memset(g_ram+0x8000,0,0x1000);
    for(int n=0;n<80;++n)g_ram[0x8000+n]=1;
    word(g_ram,0xEE00,0);word(g_ram,0xEECA,9900);word(g_ram,0xEECE,1024);
    for(int p=0;p<4;++p){positions[p][0]=144+p*2048;positions[p][1]=96;}
    memset(s_active_cells,0,sizeof s_active_cells);campaign_cells();
    CHECK(active_x(0) && active_x(2048) && active_x(4096) && active_x(6144));
    CHECK(!active_x(1024));
    positions[3][0]=8336;campaign_cells();CHECK(active_x(6144) && active_x(8192));
    s_streaming_pressure=1;campaign_cells();CHECK(!active_x(6144) && active_x(8192));
    /* Each seat reprojects the SAME immutable scene, including world pieces
     * outside P1's vertical screen. Rendering must not change rollback data. */
    memset(&s_build,0,sizeof s_build);s_build.scene=1;
    s_build.count=2;
    s_build.sprites[0]=(SceneSprite){16,0,0x8001,0,1};
    s_build.sprites[1]=(SceneSprite){2048,400,0x8001,0,0};
    s_build.views[1][0]=2048;s_build.views[1][1]=400;
    s_build.views[2][0]=2046;s_build.views[2][1]=400;
    s_build.views[3][0]=2044;s_build.views[3][1]=400;
    publish_sprites();memcpy(v.vram+0xF800,g_ram+0xF800,640);
    S2StateIO measure={0};s2_video_rb_state(&measure);
    unsigned char *before=malloc(measure.pos),*after=malloc(measure.pos);
    CHECK(before && after);
    S2StateIO save={0};save.data=before;save.size=measure.pos;save.ok=1;s2_video_rb_state(&save);
    local_player=0;sonic2_video.scanline(&v,0,native320,320,out,320);
    CHECK(out[0]!=0xFFFF0000 && out[16]==0xFFFF0000);
    local_player=1;sonic2_video.scanline(&v,0,native320,320,out,320);
    CHECK(out[0]==0xFFFF0000 && out[16]==0xFFFF0000);
    int left,top;s2_video_actor_origin(&v,0,320,&left,&top);CHECK(left==2048 && top==400);
    local_player=2;sonic2_video.scanline(&v,0,native320,320,out,320);CHECK(out[2]==0xFFFF0000);
    local_player=3;sonic2_video.scanline(&v,0,native320,320,out,320);
    CHECK(out[4]==0xFFFF0000);s2_video_actor_origin(&v,0,320,&left,&top);CHECK(left==2044 && top==400);
    save.pos=0;save.data=after;s2_video_rb_state(&save);CHECK(!memcmp(before,after,measure.pos));
    online=0;CHECK(!sonic2_video.enabled());
    /* Use the same published scene in a grid and address the second row at
     * full height. A CPU-only gap must not leave a stale tile behind. */
    static uint32_t grid[960*448];
    local_view_players=1;
    CHECK(!game_video_prepare_frame(&sonic2_video,1920,1080,320,224).height);
    for(unsigned count=2;count<=6;++count) {
        local_view_players=(1u<<count)-1u;
        GameVideoFrame frame=game_video_prepare_frame(&sonic2_video,1920,1080,320,224);
        CHECK(frame.requested_width==320);
        CHECK(frame.width==(count>4?960:640) && frame.height==(count>2?448:224));
    }
    local_view_players=(1u<<0)|(1u<<2)|(1u<<3);
    GameVideoFrame grid_frame=game_video_prepare_frame(&sonic2_video,1920,1080,320,224);
    CHECK(grid_frame.width==640 && grid_frame.height==448);
    sonic2_video.canvas_scanline(&v,0,320,grid,960,640,448,test_native_row,native320);
    CHECK(grid[16]==0xFFFF0000 && grid[320+2]==0xFFFF0000);
    CHECK(grid[224*960+4]==0xFFFF0000 && grid[224*960+320]==0xFF000000);
    sonic2_video.canvas_scanline(&v,1,320,grid,960,640,448,test_native_row,native320);
    CHECK(grid[960+320+2]==0xFFFF0000 && grid[225*960+4]==0xFFFF0000);
    CHECK(grid[640]==0 && grid[224*960+640]==0); /* respected stride and width */
    save.pos=0;s2_video_rb_state(&save);CHECK(!memcmp(before,after,measure.pos));
    online=1;CHECK(!game_video_prepare_frame(&sonic2_video,1920,1080,320,224).height);
    online=0;
    word(g_ram,0xFFD8,1);CHECK(!game_video_prepare_frame(&sonic2_video,1920,1080,320,224).height);
    word(g_ram,0xFFD8,0);
    g_ram[0xF600]=16;CHECK(!game_video_prepare_frame(&sonic2_video,1920,1080,320,224).height);
    g_ram[0xF600]=12;local_view_players=0;
    /* A return to local play may select a different aspect before the cold
     * reset. Restore world bookkeeping while retaining that local choice. */
    CHECK(sonic2_video.configure("16:9"));memset(s_active_cells,0,sizeof s_active_cells);
    save.pos=0;save.mode=1;save.data=before;s2_video_rb_state(&save);CHECK(save.ok);
    save.pos=0;save.mode=2;s2_video_rb_state(&save);CHECK(save.ok && active_x(8192));
    CHECK(s_mode==VIDEO_RATIO && sonic2_video.width(1,1,320,224)==398);
    free(before);free(after);
    /* Original Sonic uses ink3; the Knuckles card patch uses ink4. Both
     * full-width fills must follow live VRAM, including palette-1 yellow. */
    clear_guest();memset(&v,0,sizeof v);
    memset(&s_build,0,sizeof s_build);memset(s_history,0,sizeof s_history);
    memset(&s_display_frame,0,sizeof s_display_frame);
    g_ram[0xF600]=0x8C;g_ram[0xB080]=0x34;g_ram[0xB000]=1;
    v.reg[1]=64;v.reg[2]=0x30;v.reg[4]=7;v.reg[5]=0x7C;v.reg[13]=0x3C;v.reg[16]=1;
    memset(v.vram+0x5DA*32,0x33,64);v.cram[3]=0xE00;
    for(unsigned at=0xC000;at<0xD000;at+=2)word(v.vram,at,0x85DA);
    publish_sprites();sonic2_video.scanline(&v,0,native320,320,out,796);
    CHECK(s_frame_card && out[0]==0xFF0000FF && out[795]==0xFF0000FF);
    g_ram[0xF600]=0x88; /* Attract loading uses the same native Obj34 card. */
    sonic2_video.scanline(&v,0,native320,320,out,796);
    CHECK(s_frame_card && out[0]==0xFF0000FF && out[795]==0xFF0000FF);
    CHECK(card_fill(0xA5DC) && card_fill(0x85D8) && !card_fill(0xA5DA));
    sonic2_video.scanline(&v,0,native320,320,out,320);
    CHECK(!s_frame_card && !memcmp(out,native320,sizeof native320));
    /* The divider hides both hardware planes and all actor types; retain
     * the HUD's final row215 and the lower viewport's first row224. */
    word(g_ram,0xFFD8,1);v.reg[12]=6;g_ram[0xF600]=0x8C;
    CHECK(!s2_video_competition_divider(&v,215,out,796));
    CHECK(s2_video_competition_divider(&v,216,out,796));
    for(unsigned x=0;x<796;++x) CHECK(out[x]==0xFF000000);
    CHECK(s2_video_competition_divider(&v,223,out,320));
    CHECK(!s2_video_competition_divider(&v,224,out,320));
    /* Automatic demo end selects Sega; Start selects title. The old
     * interlaced display still owns its gap throughout either fade. */
    for(unsigned mode=0;mode<=4;mode+=4) {
        g_ram[0xF600]=(uint8_t)mode;
        for(unsigned width=320;width<=796;width+=476) {
            for(unsigned x=0;x<796;++x)out[x]=0xFF123456;
            CHECK(s2_video_competition_divider(&v,216,out,(int)width));
            for(unsigned x=0;x<796;++x)
                CHECK(out[x]==(x<width?0xFF000000:0xFF123456));
        }
        v.reg[12]=1;out[0]=0xFF123456;
        CHECK(!s2_video_competition_divider(&v,216,out,320));
        CHECK(out[0]==0xFF123456); /* incoming non-interlaced title/logo */
        v.reg[12]=6;
    }
    g_ram[0xF600]=0x24;
    CHECK(!s2_video_competition_divider(&v,216,out,320));
    g_ram[0xF600]=8;
    word(g_ram,0xFFD8,0);CHECK(!s2_video_competition_divider(&v,216,out,320));
    /* Cross-check the actual native VDP's IM2 cell shape. A block is a
     * horizontal pair of8x16 cells; its second map pair is not another row. */
    clear_guest();memset(&v,0,sizeof v);
    v.reg[1]=64;v.reg[2]=0x30;v.reg[4]=7;v.reg[5]=0x7C;v.reg[12]=7;v.reg[16]=1;
    memset(v.vram+64,0x11,32);memset(v.vram+96,0x22,32);
    for(unsigned at=0xC000;at<0xD000;at+=2)word(v.vram,at,1);
    uint8_t indices[GVDP_MAX_WIDTH];
    CHECK(gvdp_render_scanline(&v,0,indices)==320 && indices[100]==1);
    CHECK(gvdp_render_scanline(&v,8,indices)==320 && indices[100]==2);
    CHECK(pattern_pixel(&v,1,4,0)==1 && pattern_pixel(&v,1,4,8)==2);
    g_ram[0x8000]=1;word(g_ram,0x80,1);
    word(g_ram,0x9008,1);word(g_ram,0x900A,2);word(g_ram,0x900C,3);word(g_ram,0x900E,4);
    CHECK(world_attr_2p(g_ram,0,0,0)==1 && world_attr_2p(g_ram,8,0,0)==2);
    CHECK(world_attr_2p(g_ram,0,8,0)==1 && world_attr(g_ram,0,8,0)==3);
    word(g_ram,0x80,0x401);CHECK(world_attr_2p(g_ram,0,0,0)==0x802);
    word(g_ram,0x80,0x801);CHECK(world_attr_2p(g_ram,0,0,0)==0x1001);
    CHECK(pattern_pixel(&v,0x1001,4,0)==2);
    /* Adopt an existing native2P spring without duplicating it. Turning
     * widescreen off retains host ownership, releases an out-of-range live
     * object, and permits a clean respawn when its margin is visible again. */
    clear_guest();g_ram[0x8000+31]=1;
    g_ram[0xF600]=12;g_ram[0xF711]=1;word(g_ram,0xFFD8,1);
    word(g_ram,0xEE00,512);word(g_ram,0xEE20,512);
    word(g_rom,0xE6800,0x100);word(g_rom,0xE6900,300);word(g_rom,0xE6902,0x8000);
    g_rom[0xE6904]=0x41;g_rom[0xE6905]=6;word(g_rom,0xE6906,0xFFFF);
    s_placement_base=s_placement_count=0;
    g_ram[0xB400]=0x41;g_ram[0xB401]=4;g_ram[0xB423]=1;g_ram[0xFC03]=128;
    g_ram[0xB424]=2;g_ram[0xB438]=99;word(g_ram,0xB408,300);
    CHECK(sonic2_video.configure("32:9"));CHECK(sonic2_video.width(1,1,320,224)==796);
    spawn_scene();CHECK(s_placements[0].slot==0xB400 && g_ram[0xB440]==0);
    CHECK(sonic2_video.configure("off"));
    word(g_ram,0xB408,600); /* A moving root can remain inside a view after its birth cell leaves. */
    CHECK(s2_video_hook(0x17CCC));CHECK(g_ram[0xB400]==0x41 && (g_ram[0xFC03]&128));
    word(g_ram,0xB408,300);CHECK(s2_video_hook(0x17CCC));
    CHECK(s_loader_active && !g_ram[0xB400] && !(g_ram[0xFC03]&128));
    CHECK(sonic2_video.configure("32:9"));CHECK(sonic2_video.width(1,1,320,224)==796);
    spawn_scene();CHECK(g_ram[0xB400]==0x41 && !g_ram[0xB424] && !g_ram[0xB438] && g_ram[0xB428]==6);
    /* A Knuckles campaign consumes the same bounded host list in either
     * viewport. The stock ROM has no fixture for these object records. */
    static const uint8_t campaign_records[]={
        0x00,0x20,0x80,0x20,0x41,0x06,
        0x01,0x70,0x00,0x30,0x19,0x02,
        0xFF,0xFF,0x00,0x00,0x00,0x00
    };
    campaign_test_layout=(S2StagePlacementView){campaign_records,sizeof campaign_records};
    clear_guest();g_ram[0xF600]=12;g_ram[0xFE10]=12;g_ram[0xB000]=1;
    CHECK(sonic2_video.configure("off"));
    CHECK(s2_video_hook(0x17AA4)==1 && g_ram[0xF76C]==2 && !s_loader_active);
    CHECK(g_ram[0xB400]==0x41 && g_ram[0xB440]==0x19 && g_ram[0xFC03]==128);
    CHECK(scene_read32(0xFFF770)==12);

    clear_guest();g_ram[0xF600]=12;g_ram[0xFE10]=12;g_ram[0xB000]=1;
    CHECK(sonic2_video.configure("32:9"));
    CHECK(sonic2_video.width(1,1,320,224)==796);
    CHECK(s2_video_hook(0x17AA4)==1 && g_ram[0xF76C]==2 && s_loader_active);
    CHECK(g_ram[0xB400]==0x41 && g_ram[0xB440]==0x19 && g_ram[0xFC03]==128);
    CHECK(scene_read32(0xFFF770)==12);
    campaign_test_layout=(S2StagePlacementView){0};
    clear_guest();
    clamped_background_interpolation_test();
    cpz_interpolation_band_test();
    ooz_interpolation_band_test();
    arz_background_stream_limit_test();
    native_interpolation_test();
    scene_scroll_phase_test();
    half_pipe_width_test();
    outgoing_zone_test();
    donor_palette_fade_test();
    results_identity_test();
    special_results_scroll_test();
    signpost_palette_test();
    private_water_palette_test();
    special_stage_pixel_shape_test();
    special_actor_test();
    special_results_identity_test();
    special_results_party_name_test();
    amy_special_ui_test();
    knuckles_special_ui_test();
    front_ui_and_background_priority_test();
    foreground_world_sprite_test();
    water_surface_span_test(0x20A0E);
    water_surface_span_test(0x20AFE);
    competition_hud_replacement_test();
    amy_item_art_test();
    clamped_background_alignment_test();
    ending_background_extension_test();
    act_runoff_test();
    parked_car_test();
    cloud_edge_test();
    ending_background_world_test();
    behind_screen_lifetime_test();
    mapping_vertical_bounds_test();
    mapping_horizontal_bounds_test();
    dormant_dez_margin_test();
    dez_eggman_preview_test();
    dez_window_preview_test();
    dez_window_native_material_test();
    dormant_wfz_laser_test();
    dez_corpse_presentation_test();
    ending_plane_preview_test();
    ending_scenery_birth_test();
    ending_scenery_retirement_test();
    title_card_world_margin_test();
    title_card_publication_handoff_test();
    htz_background_domain_test();
    cnz_closure_margin_test();
    wfz_early_background_margin_test();
    wfz_robotnik_preview_test();
    slot_presentation_test();
    native_scene_art_test();
    native_art_rom_readiness_test();
    wfz_catapult_native_art_test();
    explosion_native_art_test();
    ehz_animal_native_art_test();
    ooz_static_fragment_art_test();
    wfz_finite_background_ownership_test();
    result_entry_travel_test();
    demo_native_simulation_test();
    demo_readonly_publication_test();
    native_demo_hud_test();
    competition_intro_alignment_test();
    outgoing_competition_mapping_test();
    widened_card_front_ui_test();
    death_egg_escape_fade_test();
    composed_menu_fade_test();
    menu_background_lifetime_test();
    imported_character_hud_test();
    pylon_viewport_test();
    sega_outgoing_composition_test();
    puts("Sonic 2 video defaults, terrain, spawn/cull cells and native presentation isolation PASS");
    return 0;
}
