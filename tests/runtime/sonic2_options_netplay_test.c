/* Exercise the real menu, roster and rollback stream without ROM/network I/O. */
#include <assert.h>
#define s2_party_save test_party_save
#include "sonic2_options.c"
#undef s2_party_save

uint8_t g_ram[65536];
M68KState g_cpu;
void s2_sound_load_settings(const char *path) { (void)path; }
static int sound_netplay_allowed=1;
int s2_sound_netplay_allowed(void) { return sound_netplay_allowed; }
static int net_active = 1, saves, save_ok = 1, save_overlay_calls;
static uint32_t tail_pc;
int genesis_netplay_active(void) { return net_active; }
int test_party_save(void) { ++saves; return save_ok; }
void recomp_tail_call(uint32_t pc) { tail_pc = pc; }
void s2_resources_load(const char *path) { (void)path; }
int s2_resource_verified(unsigned i) { (void)i; return 1; }
const S2DonorBank *s2_resource_shadow_bank(void) { static S2DonorBank b; return &b; }
const S2SaveAssets *s2_resource_save_assets(void) { return NULL; }
static int stages_ready=1;
const S2StageLayouts *s2_resource_knuckles_stages(void) { static S2StageLayouts s;return stages_ready?&s:NULL; }
void s2_save_menu_load(const char *path) { (void)path; }
void s2_runtime_load(void) {}
void s2_runtime_overlay(const GVDP *v, int line, uint32_t *out, int width)
{ (void)v; (void)line; (void)out; (void)width; }
int s2_save_menu_overlay(int line, uint32_t *out, int width)
{ (void)line; (void)out; (void)width; ++save_overlay_calls; return 0; }
static int available(void) { return 1; }
static const S2Character knuckles = {"knuckles", "KNUCKLES", 1, available};
static const S2Character amy = {"amy", "AMY", 1, available};
static const S2Character shadow = {"shadow", "SHADOW EXP", 1, available};
static const S2Character mighty = {"mighty", "MIGHTY", 1, available};

static void press(unsigned port, unsigned value)
{
    g_ram[0xF605] = port == 0 ? (uint8_t)value : 0;
    g_ram[0xF607] = port == 1 ? (uint8_t)value : 0;
    assert(s2_options_hook(0x90E0) == 1);
}
static size_t snapshot(uint8_t *out, size_t cap)
{
    S2StateIO io = {out, cap, 0, 0, 1};
    s2_options_rb_state(&io);
    assert(io.ok);
    return io.pos;
}
static void restore(uint8_t *data, size_t size)
{
    S2StateIO io = {data, size, 0, 1, 1};
    s2_options_rb_state(&io);
    assert(io.ok && io.pos == size);
    io.pos = 0; io.mode = 2;
    s2_options_rb_state(&io);
    assert(io.ok);
}
int main(void)
{
    uint8_t before[256], first[256], replay[256];
    s2_party_defaults();
    assert(s2_character_register(&knuckles) && s2_character_register(&amy));
    g_ram[0xF600] = 0x24;
    assert(!s2_options_hook(0x8FCC));
    press(0, 8); press(1, 8); /* Both synchronized native ports can edit: 2 -> 4. */
    assert(s2_party.roster.slots == 4);
    press(0, 2); press(0, 2); press(0, 2); /* P3 */
    press(0, 8); press(0, 8); press(0, 8);
    assert(!strcmp(s2_party.roster.character[2], "knuckles"));
    size_t len = snapshot(before, sizeof before);
    press(1, 2); press(1, 4); /* P4: NONE -> AMY. */
    assert(!strcmp(s2_party.roster.character[3], "amy"));
    assert(snapshot(first, sizeof first) == len);
    restore(before, len);
    assert(!strcmp(s2_party.roster.character[3], "none"));
    press(1, 2); press(1, 4);
    assert(snapshot(replay, sizeof replay) == len && !memcmp(first, replay, len));
    memcpy(first,replay,len);memset(first,255,sizeof(unsigned));
    unsigned saved_cursor=s_cursor;
    S2StateIO invalid={first,len,0,1,1};s2_options_rb_state(&invalid);
    assert(!invalid.ok&&s_cursor==saved_cursor);
    invalid.pos=0;invalid.mode=2;invalid.ok=1;s2_options_rb_state(&invalid);
    assert(!invalid.ok&&s_cursor==saved_cursor);

    static GVDP vdp;
    uint32_t pixels[320] = {0};
    s2_options_overlay(&vdp, 16, pixels, 320);
    assert(pixels[16] == 0xFF708ADCu && !save_overlay_calls); /* Online menu visible. */
    save_ok = 0; /* Online APPLY must work even if local disk writes would fail. */
    assert(s2_options_hook(0x909A) == 1 && g_ram[0xF600] == 4 && saves == 0 && !tail_pc);

    net_active = 0;
    g_ram[0xF600] = 0x24;
    assert(s2_options_hook(0x909A) == 1 && saves == 1 && tail_pc == 0x9060);
    save_ok = 1;
    assert(s2_options_hook(0x909A) == 1 && saves == 2 && g_ram[0xF600] == 4);
    assert(s2_character_register(&shadow) && s2_character_register(&mighty));
    assert(s2_netplay_config_adopt("r=shadow,mighty,none,none m=0011")==0);
    assert(s2_party.mighty_enabled && !strcmp(s2_party.roster.character[0],"shadow"));
    assert(!strcmp(s2_party.roster.character[1],"mighty"));
    char config[64]; s2_netplay_config_image(config,sizeof config);
    assert(!strcmp(config,"r=4,5,6,6 m=0011 k=0 d=0 f=0 c=00"));
    assert(s2_netplay_config_adopt("r=absent,mighty m=000")==-1 && s2_party.mighty_enabled);
    assert(s2_netplay_config_adopt("r=shadow,tails,sonic,amy,knuckles,mighty m=1111")==0);
    assert(s2_party.roster.slots==6 && !strcmp(s2_party.roster.character[5],"mighty"));
    s2_netplay_config_image(config,sizeof config);
    assert(!strcmp(config,"r=4,1,0,2,3,5 m=1111 k=0 d=0 f=0 c=00"));
    assert(s2_netplay_config_adopt(config)==0 && s2_party.roster.slots==6);
    assert(s2_netplay_config_adopt("r=shadow,tails,sonic,amy,knuckles,mighty,sonic m=1111")==-1);
    assert(s2_netplay_config_adopt("r=knuckles,shadow,knuckles,knuckles,knuckles,knuckles m=1111")==0);
    s2_netplay_config_image(config,sizeof config);
    assert(!strcmp(config,"r=3,4,3,3,3,3 m=1111 k=0 d=0 f=0 c=00"));
    assert(strlen(config)<48 && s2_netplay_config_adopt(config)==0);
    s2_party.infinite_lives=1;s2_party.infinite_time=1;
    s2_netplay_config_image(config,sizeof config);assert(strstr(config," c=11"));
    s2_party.infinite_lives=s2_party.infinite_time=0;
    assert(s2_netplay_config_adopt(config)==0 && s2_party.infinite_lives && s2_party.infinite_time);
    assert(s2_netplay_config_adopt("r=0,1 m=0000 c=12")==-1);
    assert(s2_netplay_config_adopt("r=0,1 m=0000")==0 && !s2_party.infinite_lives && !s2_party.infinite_time);
    net_active=0;
    s2_roster_slots(&s2_party.roster,6);
    assert(s2_options_netplay_allowed());
    sound_netplay_allowed=0;
    S2PartyConfig keep=s2_party;
    assert(!s2_options_netplay_allowed());
    assert(s2_netplay_config_adopt("r=0,1 m=0000 c=00")==-1);
    assert(!memcmp(&keep,&s2_party,sizeof keep));
    sound_netplay_allowed=1;
    stages_ready=0;
    keep=s2_party;
    assert(s2_netplay_config_adopt("r=0,1 m=0000 k=1 c=00")==-1);
    assert(!memcmp(&keep,&s2_party,sizeof keep));
    stages_ready=1;
    assert(s2_netplay_config_adopt("r=0,1 m=0000 k=1 c=00")==0 && s2_party.knuckles_campaign_enabled);
    assert(s2_netplay_config_adopt("r=0,1 m=0000 k=0 c=00")==0 && !s2_party.knuckles_campaign_enabled);
    assert(s2_netplay_config_adopt("r=0,1 m=0000 k=2 c=00")==-1);
    assert(s2_netplay_config_adopt("r=0,1 m=0000 k=0 d=1 c=00")==0 && s2_party.easy_debug);
    s2_netplay_config_image(config,sizeof config);assert(strstr(config," d=1 "));
    assert(s2_netplay_config_adopt(config)==0 && s2_party.easy_debug);
    assert(s2_netplay_config_adopt("r=0,1 m=0000 k=0 d=2 c=00")==-1);
    assert(s2_netplay_config_adopt("r=0,1 m=0000 f=1 c=00")==0&&s2_party.knuckles_flow);
    s2_netplay_config_image(config,sizeof config);assert(strlen(config)<48&&strstr(config," f=1 "));
    assert(s2_netplay_config_adopt("r=0,1 m=0000 f=2 c=00")==-1&&s2_party.knuckles_flow);
    assert(s2_netplay_config_adopt("r=0,1 m=0000 c=00")==0&&!s2_party.knuckles_flow);
    s2_party=keep;
    g_ram[0xF600]=0x24;
    assert(!s2_options_hook(0x8FCC));
    for(unsigned row=0;row<SOUND_ROW;++row)press(0,2);
    g_ram[0xF605]=8;g_ram[0xF607]=0;
    assert(!s2_options_hook(0x90E0) && g_ram[0xFF8C]==2);
    g_ram[0xF605]=0x20;
    assert(!s2_options_hook(0x90E0));
    press(0,2); /* Level Select uses the original menu game mode. */
    press(0,0x20);
    assert(g_ram[0xF605]&128);
    assert(s2_options_hook(0x909A)==1 && g_ram[0xF600]==0x28);
    g_ram[0xF600]=0x24;
    assert(!s2_options_hook(0x8FCC));
    for(unsigned row=0;row<LEVEL_ROW;++row)press(0,2);
    press(0,16); /* B returns to the title even on Level Select. */
    assert(s2_options_hook(0x909A)==1 && g_ram[0xF600]==4);
    assert(s2_roster_set(&s2_party.roster,4,"amy") && s2_roster_set(&s2_party.roster,5,"mighty"));
    s2_roster_slots(&s2_party.roster,2);strcpy(s2_party.roster.character[0],"knuckles");strcpy(s2_party.roster.character[1],"tails");
    s2_party.s3k_enabled=s2_party.knuckles_campaign_enabled=1;stages_ready=0;
    g_cpu.D[0]=0;g_ram[0xF600]=4;
    assert(s2_options_hook(0x3CCE)==1 && g_ram[0xF600]==0x24 && s_notice==NOTICE_KIS2);
    assert(!strcmp(s2_party.roster.character[0],"knuckles"));
    g_cpu.D[0]=1;assert(!s2_options_hook(0x3CF6)); /* VS needs no campaign donor. */
    s_cursor=LEVEL_ROW;unsigned before_saves=saves;
    assert(s2_options_hook(0x909A)==1 && tail_pc==0x9060 && saves==before_saves);
    stages_ready=1;assert(s2_options_hook(0x909A)==1 && g_ram[0xF600]==0x28);
    net_active=0;g_ram[0xF600]=0x24;s2_options_hook(0x8FCC);
    s2_party.presentation_enabled=s2_party.options_panel_fade=s2_party.options_slow_backdrop=1;
    unsigned old_cursor=s_cursor;press(0,2);assert(s_cursor!=old_cursor&&!s_panel_age);
    for(unsigned i=0;i<6;++i)s2_options_hook(0x111C);
    assert(s_panel_age==6);
    s_bg_frame=1;g_ram[0xF7F1]=2;g_ram[0xF7F0]=5;press(0,0);
    assert(g_ram[0xF7F0]==11);g_ram[0xF7F0]=10;press(0,0);assert(g_ram[0xF7F0]==10);
    net_active=1;g_ram[0xF7F1]=3;g_ram[0xF7F0]=5;press(0,0);assert(g_ram[0xF7F0]==5);
    puts("PASS online four-character Options, visible overlay, rollback replay, no online disk writes, offline save");
    return 0;
}
