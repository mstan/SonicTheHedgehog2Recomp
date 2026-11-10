/* Contract checks use native driver entry stubs; the built game qualifies
 * actual music execution and sample playback separately. */
#include "sonic2_sound_driver.h"
#ifdef SONIC2_CLONE_DRIVER
#include "sonic2_clone_metadata.h"
#endif
#include "genesis_runtime.h"
#include "recomp_launcher.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;} } while(0)
M68KState g_cpu;
uint8_t g_ram[65536],g_rom[0x400000];
static uint32_t called;
void recomp_call_addr(uint32_t pc) { called=pc;g_cpu.D[0]=0xBAD;g_cpu.SR=0; }
void m68k_write8(uint32_t p,uint8_t v) { g_ram[p&65535]=v; }
void m68k_write16(uint32_t p,uint16_t v) { m68k_write8(p,(uint8_t)(v>>8));m68k_write8(p+1,(uint8_t)v); }
uint16_t m68k_read16(uint32_t p) { return (uint16_t)((g_ram[p&65535]<<8)|g_ram[(p+1)&65535]); }
static int context;
static int count(void *ctx) { return ctx==&context; }
static int package(void *ctx,int n,RecompLauncherCModPackage *out)
{ if(ctx!=&context || n)return 0;memset(out,0,sizeof *out);strcpy(out->id,"common");return 1; }
int main(int argc,char **argv)
{
    CHECK(argc==3);s2_sound_load_settings(argv[2]);CHECK(!s2_sound_selected() && !s2_sound_active());
    uint8_t stock[S2_SOUND_STATE_BYTES],clone[S2_SOUND_STATE_BYTES];s2_sound_state_save(stock);
    CHECK(!s2_sound_hook(0x1084));CHECK(s2_sound_state_valid(stock) && !s2_sound_state_valid(NULL));
#if !defined(SONIC2_CLONE_DRIVER) || defined(GENESIS_Z80_RECOMP)
    CHECK(!s2_sound_available());
#ifndef SONIC2_CLONE_DRIVER
    CHECK(!s2_sound_uses_rom());
#endif
    CHECK(!s2_sound_select(1) && !s2_sound_selected());
    CHECK(s2_sound_select(0) && s2_sound_commit());
    s2_sound_post_reset();CHECK(!s2_sound_active() && s2_sound_netplay_allowed());
    CHECK(!s2_sound_hook(0xEC000) && !s2_sound_dispatch_miss(0x100000));
    CHECK(s2_sound_state_valid(stock));
    puts("PASS: stock-only build rejects alternate activation and retains original hooks, state and netplay");
    return 0;
#else
    CHECK(s2_sound_available());FILE *f=fopen(argv[1],"rb");CHECK(f);
    CHECK(fread(g_rom,1,0x100000,f)==0x100000);fclose(f);
    RecompLauncherCModProvider base={0};base.ctx=&context;base.package_count=base.feature_count=count;base.package_get=package;
    const RecompLauncherCModProvider *mods=s2_sound_mods(&base);RecompLauncherCModPackage p;
    CHECK(mods->package_count(mods->ctx)==2 && mods->package_get(mods->ctx,1,&p) && !strcmp(p.id,"common"));
    CHECK(mods->feature_enable(mods->ctx,"sonic2.audio.clone-driver","sound-driver",1));
    CHECK(mods->commit(mods->ctx,"unused"));s2_sound_load_settings(argv[2]);CHECK(s2_sound_selected());
    s2_sound_state_save(clone);CHECK(s2_sound_hook(0xEC000));CHECK(s2_sound_active() && s2_sound_state_valid(clone));
    CHECK(!s2_sound_netplay_allowed() && !mods->commit_netplay(mods->ctx,"unused"));
    CHECK(!memcmp(g_rom+0x100000,"S2CR",4));
    g_ram[0xF100]=0x5A;g_ram[0xF1FF]=0xA5;g_ram[0xF600]=12;g_cpu.D[0]=123;g_cpu.SR=0x271F;
    M68KState saved=g_cpu;
    CHECK(s2_sound_hook(0xEC000) && called==S2_CLONE_INITIALISE);
    CHECK(!memcmp(&g_cpu,&saved,sizeof saved) && g_ram[0xF100]==0x5A && g_ram[0xF1FF]==0xA5 && g_ram[0xF600]==12);
    unsigned q=(S2_CLONE_RAM_ADDRESS+S2_CLONE_QUEUE_OFFSET)&65535;
    for(unsigned slot=0;slot<4;slot++)for(unsigned id=1;id<256;id++) {
        memset(g_ram+q,0,8);memset(g_ram+0xFFE0,0,5);
        g_ram[0xFFE0+slot]=(uint8_t)id;g_cpu=saved;
        CHECK(s2_sound_hook(0x1084) && called==S2_CLONE_GUARDED_UPDATE);
        unsigned mapped=id>=0x81 && id<=0xF0?id-0x80:id>=0xF8 && id<=0xFD?id+2:0;
        CHECK(!g_ram[0xFFE0+slot] && !memcmp(&saved,&g_cpu,sizeof saved));
        CHECK(m68k_read16(q+slot*2)==mapped);
        if(id>=0xFE)CHECK(g_ram[(S2_CLONE_RAM_ADDRESS+S2_CLONE_PAUSE_OFFSET)&65535]==(id==0xFE?1:128));
    }
    memset(g_ram+q,0,8);g_ram[0xFFE0]=0x82;g_ram[0xFFE4]=0x83;
    CHECK(s2_sound_hook(0x1084));CHECK(m68k_read16(q)==2 && g_ram[0xFFE4]==0x83);
    /* An occupied internal queue retains the pending native request. */
    CHECK(s2_sound_hook(0x1084));CHECK(g_ram[0xFFE4]==0x83);
    m68k_write16(q,0);CHECK(s2_sound_hook(0x1084));CHECK(!g_ram[0xFFE4] && m68k_read16(q)==3);
    CHECK(s2_sound_select(0));CHECK(s2_sound_active() && s2_sound_state_valid(clone));
    s2_sound_post_reset();CHECK(s2_sound_active());CHECK(s2_sound_commit());
    s2_sound_load_settings(argv[2]);s2_sound_post_reset();CHECK(!s2_sound_active());
    CHECK(s2_sound_state_valid(stock) && !s2_sound_state_valid(clone) && s2_sound_netplay_allowed());
    puts("PASS: 256-ID ranges, four queues, pause, CPU/RAM isolation, snapshot identity, fixed boot selection, mod delegation and persistence");return 0;
#endif
}
