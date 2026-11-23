/* Exercise the title's real native sound hooks with the verified original
 * image and the shared event queue interface. No game or audio device runs. */
#include "sonic2_sound_driver.h"
#include "sonic2_sound_sequence.h"
#include "sonic2_state_io.h"
#include "genesis_runtime.h"
#include "audio/event_queue.h"
#include "sha256.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if(!(x)){fprintf(stderr,"line %d: %s\n",__LINE__,#x);return 1;} } while(0)
M68KState g_cpu;
uint8_t g_ram[65536],g_rom[0x400000];
uint32_t g_audio_cycle_counter=13,g_68k_stamp_rebase=20;
static unsigned writes, reset_calls, guest_calls;
static uint32_t digest;
uint8_t glue_peek8(uint32_t address) { assert(address==0xA10001u);return 0x80; }
void glue_poke16(uint32_t address,uint16_t value)
{ assert(address==0xA11200u && !value);++reset_calls; }
void recomp_call_addr(uint32_t pc) { (void)pc;++guest_calls; }
void m68k_write8(uint32_t address,uint8_t value) { g_ram[address&65535]=value; }
void m68k_write16(uint32_t address,uint16_t value)
{ m68k_write8(address,(uint8_t)(value>>8));m68k_write8(address+1,(uint8_t)value); }
uint16_t m68k_read16(uint32_t address)
{ return (uint16_t)((g_ram[address&65535]<<8)|g_ram[(address+1)&65535]); }
void audio_event_push(uint32_t stamp,uint8_t port,uint8_t value)
{
    assert(stamp>=111 && stamp<111+895780 && port<=AUDIO_PORT_PSG);
    ++writes;
    digest=(digest^stamp)*16777619u;
    digest=(digest^((unsigned)port<<8|value))*16777619u;
}

int main(int argc,char **argv)
{
    CHECK(argc==3);
    FILE *file=fopen(argv[1],"rb");
    CHECK(file && fread(g_rom,1,0x100000,file)==0x100000 && fgetc(file)==EOF);
    fclose(file);
    uint8_t before[32],after[32];
    recompui_sha256_compute(g_rom,sizeof g_rom,before);
    s2_sound_load_settings(argv[2]);
    CHECK(!s2_sound_selected() && !s2_sound_active() && s2_sound_netplay_allowed());
    CHECK(!s2_sound_hook(0x1400) && !s2_sound_dispatch_miss(0x100000));
    s2_sound_post_reset();
    g_cpu.D[0]=0xCAFE;
    g_cpu.SR=0x2704;
    M68KState cpu=g_cpu;
    CHECK(s2_sound_hook(0xEC000) && reset_calls==1 && !guest_calls);
    CHECK(!s2_sound_music_playing() && !memcmp(&g_cpu,&cpu,sizeof cpu));
    g_ram[0xFFE0]=0x82;g_ram[0xFFE4]=0x81;
    CHECK(s2_sound_hook(0x1084) && !g_ram[0xFFE0] && g_ram[0xFFE4]==0x81);
    CHECK(s2_sound_music_playing() && writes && !guest_calls && !memcmp(&g_cpu,&cpu,sizeof cpu));
    g_ram[0xFFE4]=0;
    for(unsigned tick=0;tick<100;++tick)CHECK(s2_sound_hook(0x1084));

    uint8_t snapshot[sizeof(S2SoundSequence)];
    S2StateIO state={snapshot,sizeof snapshot,0,0,1};
    s2_sound_sequence_state(&state);
    CHECK(state.ok && state.pos==sizeof snapshot);
    S2SoundSequence invalid;
    memcpy(&invalid,snapshot,sizeof invalid);
    invalid.pcm_id=1;
    S2StateIO bad={(uint8_t *)&invalid,sizeof invalid,0,1,1};
    s2_sound_sequence_state(&bad);
    CHECK(!bad.ok);

    digest=writes=0;
    for(unsigned tick=0;tick<72;++tick)CHECK(s2_sound_hook(0x1084));
    uint32_t original=digest;
    unsigned count=writes;
    state.pos=0;state.mode=2;
    s2_sound_sequence_state(&state);
    CHECK(state.ok && state.pos==sizeof snapshot);
    digest=writes=0;
    for(unsigned tick=0;tick<72;++tick)CHECK(s2_sound_hook(0x1084));
    CHECK(writes==count && digest==original);

    recompui_sha256_compute(g_rom,sizeof g_rom,after);
    CHECK(!memcmp(before,after,sizeof before) && !guest_calls);
    puts("Native sound hooks, queue acknowledgment, event timing, snapshot replay and immutable image passed");
    return 0;
}
