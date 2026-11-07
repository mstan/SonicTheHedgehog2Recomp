/* Exercise the actual per-game helpers, including shared spawn/cull cells. */
#include "sonic2_video.c"

uint8_t g_ram[65536],g_rom[0x400000];
M68KState g_cpu;
#if OWN_BACKEND
GenesisMachine g_machine;
#endif
S2PartyConfig s2_party;
static S2KnucklesArt hud_art;
const S2DonorBank *s2_resource_bank(unsigned id) { (void)id;return NULL; }
const S2DonorBank *s2_resource_shadow_bank(void) { return NULL; }
int s2_native_stars_decode(const uint8_t *r,size_t size,uint8_t out[34*32])
{ (void)r;(void)size;(void)out;return 0; }
uint16_t s2_shadow_color(const S2DonorBank *b,unsigned i,int wet,unsigned phase,unsigned frame)
{ (void)b;(void)i;(void)wet;(void)phase;(void)frame;return 0; }
static int hud_available;
static uint8_t ui_font[88*32];
const uint8_t *s2_runtime_ui_font(void) {return ui_font;}
const S2KnucklesArt *s2_resource_knuckles_art(void) { return hud_available?&hud_art:NULL; }
static int online,local_player,item_owner;
static int shadow_item_owner;
int s2_runtime_knuckles_item(unsigned o) { return item_owner && o==0xD180; }
int s2_runtime_shadow_star(unsigned o) { return shadow_item_owner && o==0xD200; }
static int positions[4][2];
int genesis_netplay_active(void){return online;}
int genesis_netplay_local_player(void){return local_player;}
int s2_runtime_campaign_online(void){return online && !ram16(0xFFD8) && (g_ram[0xF600]&127)==12;}
int s2_runtime_player_position(unsigned p,int *x,int *y)
{if(p>=4)return 0;*x=positions[p][0];*y=positions[p][1];return 1;}
void s2_options_overlay(const GVDP *v,int line,uint32_t *out,int width)
{ (void)v; (void)line; (void)out; (void)width; }
void glue_poke8(uint32_t a,uint8_t v){g_ram[a&65535]=v;}
void glue_poke16(uint32_t a,uint16_t v){glue_poke8(a,v>>8);glue_poke8(a+1,v);}
void glue_poke32(uint32_t a,uint32_t v){glue_poke16(a,v>>16);glue_poke16(a+2,v);}
void cmd_send_response(const char *json){(void)json;}
#define CHECK(c) do{if(!(c)){fprintf(stderr,"line %d: %s\n",__LINE__,#c);exit(1);}}while(0)
static void word(uint8_t *p,unsigned a,unsigned v){p[a]=(uint8_t)(v>>8);p[a+1]=(uint8_t)v;}
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
    memset(&v,0,sizeof v);memset(g_ram,0,sizeof g_ram);memset(&hud_art,0,sizeof hud_art);
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
    memset(g_ram,0,sizeof g_ram);memset(s_special_history,0,sizeof s_special_history);
    memset(&s_special_display,0,sizeof s_special_display);
}
static void results_identity_test(void)
{
    static GVDP v;uint32_t out[398];uint8_t original[65536];
    memset(&v,0,sizeof v);memset(g_ram,0,sizeof g_ram);memset(&hud_art,0,sizeof hud_art);
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
                CHECK(out[40+center]==0xFFFF0000 && out[56+center]==0xFFFFFFFF);
                CHECK(out[104+center]==0xFFFF0000 && out[176+center]==0xFFFFFFFF);
                CHECK(out[24+center]==0xFF00FF00);
            }
        }
    }
    strcpy(s2_party.roster.character[0],"shadow");
    for(int line=0;line<=56;++line) {
        memset(out,0,sizeof out);s2_video_results_name(&v,line,out,398,0);
        s2_video_results_name(&v,line,out,398,1);
    }
    CHECK(out[72+39]==0xFFFFFFFF && out[176+39]==0xFFFFFFFF);
    strcpy(s2_party.roster.character[0],"sonic");
    for(int x=0;x<320;++x)out[x]=0xFF00FF00;
    s2_video_results_name(&v,0,out,320,0);s2_video_results_name(&v,56,out,320,1);
    CHECK(out[96]==0xFF00FF00 && !memcmp(original,g_ram,sizeof original));
    hud_available=0;CHECK(sonic2_video.configure("off"));
}
static void special_results_identity_test(void)
{
    static GVDP v,before;static uint8_t original_ram[65536];uint32_t out[398];
    memset(&v,0,sizeof v);memset(g_ram,0,sizeof g_ram);memset(&hud_art,0,sizeof hud_art);
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

static void native_interpolation_test(void)
{
    static GVDP v;
    static uint32_t ordinary[2][320*224],fraction[320*224];
    static uint8_t original_ram[65536];
    CHECK(sonic2_video.configure("off"));online=0;s_loader_active=0;
    memset(g_ram,0,sizeof g_ram);memset(g_rom,0,sizeof g_rom);
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

static void private_water_palette_test(void)
{
    static GVDP v;
    uint8_t original[65536];GVDPRasterRow captured;
    memset(g_ram,0,sizeof g_ram);memset(&s2_party,0,sizeof s2_party);
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
    memset(&v,0,sizeof v);memset(g_ram,0,sizeof g_ram);
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
    memset(&v,0,sizeof v);memset(g_ram,0,sizeof g_ram);memset(&s2_party,0,sizeof s2_party);
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

static void competition_hud_replacement_test(void)
{
    static GVDP v;uint32_t out[320];
    static const char *names[]={"sonic","tails","amy","knuckles","shadow","mighty"};
    memset(&v,0,sizeof v);memset(g_ram,0,sizeof g_ram);memset(&s2_party,0,sizeof s2_party);
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
        memset(g_ram,0,sizeof g_ram);g_ram[0xF600]=(uint8_t)destination;
        g_ram[0xB000]=1;g_ram[0xF711]=1;
        CHECK(outgoing_level() && !gameplay() && !competition() && s2_video_main_cpu_divisor()==1);
        word(g_ram,0xFFD8,1);CHECK(outgoing_level() && !gameplay() && !competition());
        g_ram[0xF711]=0;CHECK(!outgoing_level());
        g_ram[0xF711]=1;g_ram[0xB000]=0;CHECK(!outgoing_level());
    }
    memset(g_ram,0,sizeof g_ram);
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
    item_owner=0;hud_available=0;s2_video_prepare_knuckles_items();memset(g_ram,0,sizeof g_ram);
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
    memset(g_ram,0,sizeof g_ram);memset(&v,0,sizeof v);
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
    word(g_ram,0xFFD8,0);CHECK(!s2_video_competition_divider(&v,216,out,320));
    /* Cross-check the actual native VDP's IM2 cell shape. A block is a
     * horizontal pair of8x16 cells; its second map pair is not another row. */
    memset(g_ram,0,sizeof g_ram);memset(&v,0,sizeof v);
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
    memset(g_ram,0,sizeof g_ram);g_ram[0x8000+31]=1;
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
    donor_palette_fade_test();
    results_identity_test();
    private_water_palette_test();
    special_stage_pixel_shape_test();
    special_actor_test();
    special_results_identity_test();
    front_ui_and_background_priority_test();
    competition_hud_replacement_test();
    puts("Sonic 2 video defaults, terrain, spawn/cull cells and native presentation isolation PASS");
    return 0;
}
