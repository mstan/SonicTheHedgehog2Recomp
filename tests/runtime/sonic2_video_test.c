/* Exercise the actual per-game helpers, including shared spawn/cull cells. */
#include "sonic2_video.c"

uint8_t g_ram[65536],g_rom[0x400000];
M68KState g_cpu;
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
const S2SpecialStageArt *s2_resource_amy_special(void) {return amy_special_available?&amy_special:NULL;}
const S2ItemArt *s2_resource_amy_items(void) {return amy_items_available?&amy_items:NULL;}
const S2DonorBank *s2_resource_bank(unsigned id) {return amy_items_available && id==S2_RESOURCE_AMY?&amy_item_bank:NULL;}
const S2DonorBank *s2_resource_shadow_bank(void) { return NULL; }
int s2_native_stars_decode(const uint8_t *r,size_t size,uint8_t out[34*32])
{ (void)r;(void)size;(void)out;return 0; }
unsigned s2_native_sprite_art_decode(const uint8_t *r,size_t size,unsigned asset,uint8_t *out,size_t cap)
{(void)r;(void)size;(void)asset;(void)out;(void)cap;return 0;}
static int slot_mock;
int s2_knuckles_slot_patterns(const uint8_t *r,size_t size,const S2KnucklesArt *art,unsigned slot,const uint8_t source[512],uint8_t out[512])
{(void)r;(void)size;(void)art;(void)slot;if(!slot_mock || source[0]!=0x11)return 0;memset(out,0x22,512);return 1;}
uint16_t s2_shadow_color(const S2DonorBank *b,unsigned i,int wet,unsigned phase,unsigned frame)
{ (void)b;(void)i;(void)wet;(void)phase;(void)frame;return 0; }
static int hud_available;
static uint8_t ui_font[88*32];
const uint8_t *s2_runtime_ui_font(void) {return ui_font;}
const S2KnucklesArt *s2_resource_knuckles_art(void) { return hud_available?&hud_art:NULL; }
static int online,local_player,item_owner;
static int shadow_item_owner;
int s2_runtime_knuckles_item(unsigned o) { return item_owner && o==0xD180; }
int s2_runtime_amy_item(unsigned o) { return amy_item_owner && o==0xD180; }
int s2_runtime_shadow_star(unsigned o) { return shadow_item_owner && o==0xD200; }
static int positions[4][2];
int genesis_netplay_active(void){return online;}
int genesis_netplay_local_player(void){return local_player;}
int s2_runtime_campaign_online(void){return online && !ram16(0xFFD8) && (g_ram[0xF600]&127)==12;}
int s2_runtime_player_position(unsigned p,int *x,int *y)
{if(p>=4)return 0;*x=positions[p][0];*y=positions[p][1];return 1;}
void s2_options_overlay(const GVDP *v,int line,uint32_t *out,int width)
{ (void)v; (void)line; (void)out; (void)width; }
void s2_runtime_presentation_capture(S2RuntimePresentation *out)
{ memset(out,0,sizeof *out); }
int s2_runtime_presentation_matches(const S2RuntimePresentation *a,const S2RuntimePresentation *b)
{ (void)a;(void)b;return 1; }
void s2_runtime_presentation_row(const S2RuntimePresentation *a,const S2RuntimePresentation *b,
                                double phase,const GVDP *v,int line,uint32_t *out,int width)
{ (void)a;(void)b;(void)phase;(void)v;(void)line;(void)out;(void)width; }
void glue_poke8(uint32_t a,uint8_t v){g_ram[a&65535]=v;}
void glue_poke16(uint32_t a,uint16_t v){glue_poke8(a,v>>8);glue_poke8(a+1,v);}
void glue_poke32(uint32_t a,uint32_t v){glue_poke16(a,v>>16);glue_poke16(a+2,v);}
void cmd_send_response(const char *json){(void)json;}
#define CHECK(c) do{if(!(c)){fprintf(stderr,"line %d: %s\n",__LINE__,#c);exit(1);}}while(0)
static void word(uint8_t *p,unsigned a,unsigned v){p[a]=(uint8_t)(v>>8);p[a+1]=(uint8_t)v;}
/* Synthetic stage fixtures replace RAM without running Level_SetPlayerMode. */
static void clear_guest(void)
{ memset(g_ram,0,sizeof g_ram);s_level_zone_valid=0;s_boss_preview_complete=0; }
static void row_parallax_test(void)
{
    /* Original deformation table boundaries, including CNZ's ripple row.
     * MCZ quantizes camera/10 before multiplying each band's speed. */
    CHECK(arz_parallax(160,336)==16 && arz_parallax(160,432)==48);
    CHECK(arz_parallax(160,576)==175);
    CHECK(mcz_parallax(160,0)==144 && mcz_parallax(160,37)==128);
    CHECK(mcz_parallax(160,95)==32 && mcz_parallax(160,96)==16);
    CHECK(mcz_parallax(161,97)==16 && mcz_parallax(161,145)==80);
    CHECK(cnz_parallax(160,0)==160 && cnz_parallax(160,96)==55);
    CHECK(cnz_parallax(160,112)==10 && cnz_parallax(160,128)==10);
    CHECK(cnz_parallax(160,144)==20);
    /* Captured native deformation rows, without custom viewport drawing. */
    CHECK(htz_scroll(0,484,128)==-64 && htz_scroll(0,484,208)==-214);
    CHECK(htz_scroll(54,868,136)==-148 && htz_scroll(54,868,176)==-292);
    CHECK(htz_scroll(360,1396,131)==-254 && htz_scroll(360,1396,208)==-775);
    CHECK(ooz_parallax(160,80)==20 && ooz_parallax(160,79)==3);
    CHECK(ooz_parallax(160,64)==2 && ooz_parallax(160,60)==5);
    CHECK(ooz_parallax(160,32)==0 && ooz_parallax(160,8)==3);
    CHECK(dez_parallax(160,127)==160 && dez_parallax(160,128)==0);
    CHECK(dez_parallax(160,383)==0 && dez_parallax(160,384)==160);
    GVDP v={0};v.reg[13]=0x3F;
    for(unsigned row=0;row<224;++row)word(v.vram,0xFC02+row*4,(unsigned)htz_scroll(54,868,(int)row));
    unsigned phase=0;CHECK(htz_display_phase(&v,54,872,&phase) && phase==868);
    word(v.vram,0xFC02+208*4,0x1234);CHECK(!htz_display_phase(&v,54,872,&phase));
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
    for(unsigned asset=0;asset<S2_NATIVE_SPRITE_ART_COUNT;++asset) {
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
    s_frame_vs=s_vs_ready=s_vs_world=0;s_display=NULL;
}
static void menu_background_lifetime_test(void)
{
    clear_guest();memset(s_history,0,sizeof s_history);
    s_display=NULL;s_serial=0;s_menu_background_ready=0;
    GVDP v={0};v.reg[1]=64;v.reg[2]=0x30;v.reg[4]=7;
    v.reg[12]=1;v.reg[13]=0x28;v.reg[16]=1;
    v.cram[49]=0x00E;v.cram[50]=0x0E0;
    memset(v.vram+32,0x11,32);memset(v.vram+64,0x22,32);
    uint32_t native[320],out[796];
    for(unsigned col=0;col<40;++col) {
        unsigned attr=0x6000|(col<20?1:2);
        word(g_ram,col*2,attr);word(v.vram,0xE000+col*2,attr);
    }
    CHECK(!s2_video_hook(0x8C92));
    CHECK(s_menu_background_ready);
    for(unsigned x=0;x<320;++x)native[x]=x<160?0xFFFF0000:0xFF00FF00;
    CHECK(sonic2_video.configure("32:9"));
    for(unsigned phase=0;phase<3;++phase) {
        g_ram[0xF600]=(uint8_t)(phase==0?0x24:phase==1?0x1C:4);
        for(int width=398;width<=796;width+=398) {
            render_source_scanline(&v,0,native,320,out,width,NULL,NULL);
            CHECK(s_render->frame_menu_background);
            for(int x=0;x<width;++x) {
                int bx=(x-(width-320)/2)%320;if(bx<0)bx+=320;
                CHECK(out[x]==(bx<160?0xFFFF0000:0xFF00FF00));
            }
        }
    }
    word(v.vram,0xE000,0x4001); /* The incoming title owns a different map. */
    render_source_scanline(&v,0,native,320,out,796,NULL,NULL);
    CHECK(!s_render->frame_menu_background);
    s_menu_background_ready=0;
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
    amy_item_bank.palette[3]=0x00E;amy_item_bank.water_palette[3]=0xE00;
    amy_item_bank.water_palette_alt[3]=0x0E0;amy_item_bank.water_palette_count=2;
    s2_video_prepare_knuckles_items();unsigned bytes=0;
    CHECK(!gvdp_ext_active()); /* Amy's private colours belong to sprites. */
    CHECK(gvdp_host_palette() && gvdp_host_palette()[16+3]==
        genesis_dac_cram_to_argb(0x00E,GENESIS_DAC_NORMAL));
    const uint8_t *patterns=gvdp_sprite_patterns(0x80,&bytes);
    CHECK(patterns && bytes==65536 && patterns[0x7D4*32]==0x33 && patterns[0x5F4*32]==0x44);
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
    CHECK(out[80+39]==0xFFFFFFFF && out[184+39]==0xFFFFFFFF);
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
    CHECK(!memcmp(original_ram,g_ram,sizeof g_ram));
    state.pos=0;state.data=after;s2_video_rb_state(&state);
    CHECK(state.ok && !memcmp(before,after,size.pos));
    /* New source identities and teleports repeat an ordinary picture. */
    s_present[s_present_front].keys[0]^=1;CHECK(!interpolate(.5,fraction,320,224));
    s_present[s_present_front].keys[0]^=1;
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
    word(g_ram,0xF804,0xA7D8);memset(v.vram+0x7D8*32,0x11,32);
    g_cpu.A[0]=0;g_cpu.A[1]=0x40ABA;
    s2_video_hook(0x16604);s2_video_hook(0x1684E);s2_video_hook(0x16712);
    memcpy(v.vram+0xF800,g_ram+0xF800,640);
    out[0]=out[1]=0xFFFF0000;s2_video_front_ui(&v,0,out,320);
    CHECK(out[0]==0xFFFF0000 && out[1]==0xFFFF0000);
    hud_available=0;s2_party.roster.character[0][0]=0;word(g_ram,0xF804,0x8001);
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

static void water_surface_span_test(void)
{
    static GVDP v;uint32_t out[796],palette[64]={0};uint8_t ram[65536];
    memset(&v,0,sizeof v);clear_guest();memset(g_rom,0,sizeof g_rom);
    memset(&s2_party,0,sizeof s2_party);online=0;s_interpolating=0;
    g_ram[0xF600]=12;g_ram[0xF711]=1;g_ram[0xB000]=1;g_ram[0x801F]=1;word(g_ram,0xE808,0xFFFF);
    word(g_ram,0xEEF0,512);word(g_ram,0xEE00,512);
    /* REV01 Obj04's three 32x16 pieces, repeated by its two native objects. */
    word(g_rom,0x20A0E,12);word(g_rom,0x20A1A,3);
    for(unsigned n=0;n<3;++n) {
        unsigned at=0x20A1C+n*8;g_rom[at]=0xF8;g_rom[at+1]=0x0D;
        word(g_rom,at+6,(unsigned)(-96+(int)n*64));
    }
    for(unsigned n=0;n<2;++n) {
        unsigned o=0xB380+n*64;g_ram[o]=4;g_ram[o+1]=4;g_ram[o+0x19]=128;
        word(g_ram,o+2,0x8400);word(g_ram,o+4,2);word(g_ram,o+6,0xA0E);
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

int main(void)
{
    /* Byte-matched SwScrl_EHZ_2P band boundaries and fixed-point gradient;
     * CPZ's row18 ripple stays in BG1 before the half-speed BG2 band. */
    CHECK(ehz_2p_parallax(64,21)==0 && ehz_2p_parallax(64,22)==1);
    CHECK(ehz_2p_parallax(64,101)==1 && ehz_2p_parallax(64,102)==0);
    CHECK(ehz_2p_parallax(64,112)==4 && ehz_2p_parallax(64,128)==6);
    CHECK(ehz_2p_parallax(64,144)==8 && ehz_2p_parallax(64,146)==9);
    CHECK(ehz_2p_parallax(64,222)==28);
    CHECK(cpz_parallax(80,288)==10 && cpz_parallax(80,303)==10);
    CHECK(cpz_parallax(80,304)==40);
    /* Attract end/skip selects Sega/title before the old world fades. Its
     * display stays wide until incoming initialization retires the level;
     * original loading/fade CPU timing remains unchanged. */
    CHECK(sonic2_video.configure("32:9"));
    for(unsigned destination=0;destination<=4;destination+=4) {
        clear_guest();g_ram[0xF600]=(uint8_t)destination;
        g_ram[0xB000]=1;g_ram[0xF711]=1;
        CHECK(outgoing_level() && !gameplay() && !competition() && s2_video_main_cpu_divisor()==1);
        word(g_ram,0xFFD8,1);CHECK(outgoing_level() && !gameplay() && !competition());
        g_ram[0xF711]=0;CHECK(!outgoing_level());
        g_ram[0xF711]=1;g_ram[0xB000]=0;CHECK(!outgoing_level());
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
    g_ram[0xF802]=0xC0;CHECK(shadow_palette_needed());g_ram[0xF802]=0;
    word(g_ram,0xFFD8,1);g_ram[0xDD02]=0xC0;
    CHECK(shadow_palette_needed());g_ram[0xDD02]=0;word(g_ram,0xFFD8,0);
    g_machine.vdp.reg[5]=0x7C;g_machine.vdp.vram[0xF802]=0xC0;
    CHECK(shadow_palette_needed());g_machine.vdp.vram[0xF802]=0;
    s_display_frame.count=1;s_display_frame.sprites[0].size=0xC0;s_display=&s_display_frame;
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
    g_ram[0xF600]=0x8C;CHECK(s2_video_main_cpu_divisor()==1);g_ram[0xF600]=12;
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
    /* Both Sonic and Tails retain the native ring collision code. */
    word(g_ram,0xE808,100);word(g_ram,0xE80E,65535);g_cpu.A[0]=0xFFFFB040;
    s2_video_hook(0x170D0);CHECK(g_cpu.A[1]==0xFFFFE806 && g_cpu.A[2]==0xFFFFE80C);
    CHECK(sonic2_video.configure("off"));
    g_cpu.D[0]=12345;s2_video_hook(0x105C6);CHECK(g_cpu.D[0]==12345);
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
    native_interpolation_test();
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
    amy_special_ui_test();
    knuckles_special_ui_test();
    front_ui_and_background_priority_test();
    foreground_world_sprite_test();
    water_surface_span_test();
    competition_hud_replacement_test();
    amy_item_art_test();
    row_parallax_test();
    act_runoff_test();
    parked_car_test();
    cloud_edge_test();
    slot_presentation_test();
    native_scene_art_test();
    demo_native_simulation_test();
    demo_readonly_publication_test();
    native_demo_hud_test();
    competition_intro_alignment_test();
    outgoing_competition_mapping_test();
    widened_card_front_ui_test();
    composed_menu_fade_test();
    menu_background_lifetime_test();
    pylon_viewport_test();
    puts("Sonic 2 video defaults, terrain, spawn/cull cells and native presentation isolation PASS");
    return 0;
}
