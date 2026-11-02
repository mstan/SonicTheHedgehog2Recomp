/* Exercise actual menu controls; isolate disk/rendering from navigation. */
#include <assert.h>
#include "sonic2_save_menu.c"

uint8_t g_ram[65536],g_rom[0x400000];
S2PartyConfig s2_party;
static S2CampaignStore fixture;
static S2SaveAssets assets;
static unsigned sound_writes,commits;
static int commit_fails;
void glue_poke8(uint32_t a,uint8_t v)   /* the host write (was m68k_write8) */
{ assert(a==0xFFFFE1); g_ram[a&65535]=v; ++sound_writes; }
void recomp_tail_call(uint32_t pc) { (void)pc; assert(0); }
void recomp_call_addr(uint32_t pc)
{
    assert(pc==0x246A && menu==3);
    assert(!s2_save_menu_hook(pc)); /* native fade entry must not recurse */
    uint32_t line[398];
    assert(s2_save_menu_overlay(0,line,398));
    putword(0xFB00,0);
    assert(s2_save_menu_overlay(0,line,398));
    for (unsigned x=0;x<398;++x) assert(line[x]==0xFF000000);
}
void s2_campaign_file_load(const char *path) { (void)path; }
const S2CampaignStore *s2_campaign_file_store(void) { return &fixture; }
int s2_campaign_file_commit(const S2CampaignData *data)
{ ++commits; if(commit_fails)return 0; fixture.data=*data; return 1; }
int s2_resources_prepare_stage_images(const unsigned char *r,unsigned n)
{ (void)r; (void)n; return 1; }
const S2SaveAssets *s2_resource_save_assets(void) { return &assets; }
void s2_save_draw_line(const S2SaveAssets *a,const S2SaveView *v,int l,uint32_t *o,int w)
{ (void)a; (void)v; (void)l; for(int x=0;x<w;++x)o[x]=0xFFEEAA66; }
void s2_save_draw_notice(const S2SaveAssets *a,const char *m,int l,uint32_t *o,int w)
{ (void)a; (void)m; (void)l; (void)o; (void)w; }

static void reset(unsigned selection,int state)
{
    memset(g_ram,0,sizeof g_ram); memset(&fixture,0,sizeof fixture);
    fixture.ready=1; store=&fixture;
    memset(&view,0,sizeof view); view.selection=selection;
    view.cursor=40+(int)selection*104;
    if (selection>=1 && selection<=8) view.data.slots[selection-1].state=(uint8_t)state;
    fixture.data=view.data;
    replay_stage=S2_CAMPAIGN_STAGES; sound_writes=0;
    session=-1; dirty=exit_action=0;
    commits=0; commit_fails=0;
}
static void input(unsigned press,unsigned expected)
{
    unsigned before=sound_writes;
    g_ram[0xF605]=(uint8_t)press;
    /* An unrelated pending sound/music must survive an ignored input. */
    g_ram[0xFFE1]=0xA5; g_ram[0xFFE0]=0x87;
    controls();
    assert(sound_writes==before+(expected!=0));
    assert(g_ram[0xFFE1]==(expected?expected:0xA5));
    assert(g_ram[0xFFE0]==0x87);
}
int main(void)
{
    reset(1,S2_SAVE_COMPLETE); input(4,0xC0); assert(view.selection==0);
    input(8,0); /* sliding: no second move/sound */
    reset(0,S2_SAVE_EMPTY); input(4,0); input(8,0xC0); assert(view.selection==1);
    reset(9,S2_SAVE_EMPTY); input(8,0); input(4,0xC0); assert(view.selection==8);
    reset(1,S2_SAVE_COMPLETE); input(12,0); assert(view.selection==1);
    input(0,0); input(3,0); assert(replay_stage==S2_CAMPAIGN_STAGES);
    input(1,0xCD); assert(replay_stage==0);
    input(0,0); /* held input has no new press edge */
    input(2,0xCD); assert(replay_stage==S2_CAMPAIGN_STAGES);
    input(2,0xCD); assert(replay_stage==S2_CAMPAIGN_STAGES-1);
    input(1,0xCD); assert(replay_stage==S2_CAMPAIGN_STAGES);
    for (unsigned i=0;i<2;++i) {
        reset(1,(int)i); input(1,0); input(2,0); /* new/in-progress: not replayable */
    }
    reset(0,0); input(1,0); input(2,0);
    reset(9,0); input(1,0); input(2,0);
    reset(1,S2_SAVE_COMPLETE); view.erase=1;
    input(4,0); input(1,0); input(2,0); input(8,0xD8); assert(view.selection==2);
    reset(1,S2_SAVE_COMPLETE); view.confirm=1; input(1,0); input(8,0); assert(!view.confirm);
    reset(9,0); input(0x80,0xA1); assert(view.erase && !view.confirm && !commits);
    input(0,0); input(0x80,0); assert(!view.erase && !exit_action && !commits);
    reset(1,S2_SAVE_ACTIVE); view.erase=1;
    input(0x80,0xA1); assert(view.confirm && view.erase && !commits);
    input(8,0); assert(!view.erase && !view.confirm && !commits);
    assert(view.data.slots[0].state==S2_SAVE_ACTIVE);
    reset(1,S2_SAVE_ACTIVE); view.erase=view.confirm=1;
    input(16,0); assert(!view.erase && !view.confirm && !commits && !exit_action);
    assert(view.data.slots[0].state==S2_SAVE_ACTIVE);
    reset(1,S2_SAVE_EMPTY); view.erase=1;
    input(0x80,0); assert(!view.erase && !view.confirm && !commits && !exit_action);
    reset(1,S2_SAVE_ACTIVE); view.erase=1; view.read_only=fixture.read_only=1;
    input(0x80,0); assert(view.erase && !view.confirm && !commits);
    reset(1,S2_SAVE_ACTIVE); view.erase=view.confirm=1;
    input(4,0xC5); assert(!view.erase && !view.confirm && commits==1);
    assert(!view.data.slots[0].state && !fixture.data.slots[0].state);
    reset(1,S2_SAVE_ACTIVE); view.erase=view.confirm=1; commit_fails=1;
    input(4,0); assert(view.erase && view.confirm && commits==1);
    assert(view.data.slots[0].state==S2_SAVE_ACTIVE && fixture.data.slots[0].state==S2_SAVE_ACTIVE && view.notice);
    reset(1,S2_SAVE_COMPLETE); s2_party.save_menu_enabled=0;
    menu=1; g_ram[0xF600]=0x24; g_ram[0xF605]=8;
    assert(!s2_save_menu_hook(0x90E0)); assert(!sound_writes);
    /* Entry owns the complete scene before the native fade begins. Partial
     * palettes fade every overlay pixel; outgoing game/title fades retain
     * the menu until black, even though Game_Mode has already changed. */
    s2_party.save_menu_enabled=1; menu=1; menu_ready=0;
    assert(!s2_save_menu_hook(0x8FCC)); assert(menu_ready);
    uint32_t line[398];
    putword(0xFB80,0xEEE); putword(0xFB00,0);
    assert(s2_save_menu_overlay(0,line,398));
    for (unsigned x=0;x<398;++x) assert(line[x]==0xFF000000);
    putword(0xFB00,0x666);
    assert(s2_save_menu_overlay(0,line,398));
    for (unsigned x=0;x<398;++x) assert(line[x]==0xFF66482B);
    putword(0xFB00,0xEEE);
    assert(s2_save_menu_overlay(0,line,398));
    for (unsigned x=0;x<398;++x) assert(line[x]==0xFFEEAA66);
    for (unsigned mode=4;mode<=12;mode+=8) {
        menu=2; menu_ready=1; g_ram[0xF600]=(uint8_t)mode;
        assert(s2_save_menu_overlay(0,line,398));
        assert(s2_save_menu_hook(0x246A));
        assert(!menu && !menu_ready);
        assert(!s2_save_menu_overlay(0,line,398));
    }
    puts("PASS save-menu movement sounds, wrap, bounds, opposite/idle/sliding inputs and disabled mod");
    return 0;
}
