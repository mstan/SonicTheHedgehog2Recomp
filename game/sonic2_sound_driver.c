#include "sonic2_sound_driver.h"
#include "genesis_runtime.h"
#ifdef SONIC2_MUSIC_DRIVER
#include "sonic2_music.h"
#include "sonic2_cd_themes.h"
#include "sonic2_music_clone_metadata.h"
#endif
#include "host_atomic_file.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef SONIC2_CLONE_DRIVER
#include "sonic2_clone_metadata.h"
#ifdef S2_CLONE_ASSETS_FROM_ROM
#include "sonic2_sound_assets.h"
#include "sonic2_clone_program.h"
#endif
#endif

static int selected, active, verified, bound;
#ifdef SONIC2_MUSIC_DRIVER
static int music_active;
static uint8_t music_identity[32];
#define DRIVER(name) (music_active?S2_MUSIC_CLONE_##name:S2_CLONE_##name)
#else
#define DRIVER(name) S2_CLONE_##name
#endif
static char settings_file[1200], error[256];

int s2_sound_available(void)
{
#if defined(S2_CLONE_ASSETS_FROM_ROM) && !defined(GENESIS_Z80_RECOMP)
    return 1;
#else
    return 0;
#endif
}
int s2_sound_selected(void) { return selected; }
int s2_sound_uses_rom(void) {
#ifdef S2_CLONE_ASSETS_FROM_ROM
    return 1;
#else
    return 0;
#endif
}
int s2_sound_active(void) { return active; }
int s2_sound_music_playing(void)
{
#ifdef SONIC2_CLONE_DRIVER
    if(active){
#ifdef SONIC2_MUSIC_DRIVER
        unsigned stride=music_active?46:44;
#else
        unsigned stride=44;
#endif
        unsigned base=DRIVER(RAM_ADDRESS)&65535u;
        for(unsigned i=0;i<10;i++)if(g_ram[base+i*stride]&128)return 1;
        return 0;
    }
#endif
    return -1;
}
int s2_sound_verified(void) { return verified; }
const char *s2_sound_error(void) { return error; }

int s2_sound_select(int enabled)
{
    if(enabled && !s2_sound_available()) {
        snprintf(error,sizeof error,"This build does not include Clone Driver with the standard Z80 backend.");return 0;
    }
#ifdef SONIC2_MUSIC_DRIVER
    if(!enabled&&s2_themes_enabled()){snprintf(error,sizeof error,"Disable Music Customization sections before disabling Clone Driver.");return 0;}
#endif
    selected=enabled!=0;verified=s2_sound_available();error[0]=0;return 1;
}
void s2_sound_load_settings(const char *settings)
{
    selected=verified=active=bound=0;error[0]=0;
#ifdef SONIC2_MUSIC_DRIVER
    music_active=0;memset(music_identity,0,sizeof music_identity);
#endif
    const char *slash1=strrchr(settings,'/'), *slash2=strrchr(settings,'\\');
    const char *slash=slash1;
    if (!slash || (slash2 && slash2>slash)) slash=slash2;
    size_t prefix=slash?(size_t)(slash-settings)+1:0;
    if (prefix+25>=sizeof settings_file) { fprintf(stderr,"[sound-driver] Settings path too long\n"); exit(1); }
    memcpy(settings_file,settings,prefix);
    snprintf(settings_file+prefix,sizeof settings_file-prefix,"sonic2-sound.ini");
    SDL_RWops *f=SDL_RWFromFile(settings_file,"rb");
    if (f) {
        Sint64 bytes=SDL_RWsize(f);
        if (bytes>=0 && bytes<4096) {
            char text[4096]; size_t n=SDL_RWread(f,text,1,(size_t)bytes); text[n]=0;
            char *line=text;
            while (line && *line) {
                char *next=strchr(line,'\n'); if(next)*next++=0;
                size_t len=strlen(line);if(len && line[len-1]=='\r')line[len-1]=0;
                if (!strcmp(line,"enabled=1")) selected=1;
                line=next;
            }
        }
        SDL_RWclose(f);
    }
    verified=s2_sound_available();
#ifdef SONIC2_MUSIC_DRIVER
    s2_music_load(settings);
#endif
    fprintf(stderr,"[sound-driver] Selected %s from %s\n",selected?"Clone Driver":"original",settings_file);
}
int s2_sound_commit(void)
{
    if (selected && !s2_sound_select(1)) return 0;
    char text[1200];
    int n=snprintf(text,sizeof text,"[sound_driver]\nenabled=%d\n",selected);
    if(n<0 || n>=(int)sizeof text || !genesis_atomic_file_store(settings_file,text,(size_t)n)) {
        snprintf(error,sizeof error,"Unable to save Sonic 2 sound settings.");return 0;
    }
    return 1;
}
void s2_sound_post_reset(void)
{
    if(bound)return;
    active=selected;bound=1;
    if (!active) return;
    if(!s2_sound_available()){fprintf(stderr,"[sound-driver] Unsupported Z80 backend\n");exit(1);}
#ifdef S2_CLONE_ASSETS_FROM_ROM
    const S2SoundAssetLayout layout={S2_CLONE_MUSICINDEX,S2_CLONE_SOUNDINDEX,S2_CLONE_SPECSOUNDINDEX,S2_CLONE_PSG_INDEX,S2_CLONE_SAMPLE_TABLE};
    memcpy(g_rom+S2_CLONE_LOAD_ADDRESS,s2_clone_program,sizeof s2_clone_program);
    if(!s2_sound_build_assets(g_rom,0x100000,g_rom,0x400000,&layout,error,sizeof error)) {
        fprintf(stderr,"[sound-driver] %s\n",error);exit(1);
    }
#ifdef SONIC2_MUSIC_DRIVER
    music_active=s2_themes_enabled();
    s2_music_config_identity(music_identity);
    if(music_active&&!s2_music_prepare(g_rom,0x400000,layout.music_index,layout.sfx_index,layout.psg_index,layout.sample_table,error,sizeof error)){
        fprintf(stderr,"[music] %s\n",error);exit(1);
    }
#endif
    verified=1;fprintf(stderr,"[sound-driver] Clone Driver + Mega PCM; sound assets prepared locally from ROM\n");
#endif
}

/* Consume the original WRAM queue at its original VBlank call site. This
 * also catches direct queue writes, pause, competition and local-only SFX. */
static int queue(unsigned id,unsigned slot)
{
#ifdef SONIC2_CLONE_DRIVER
    if(id>=0xFE) {
        m68k_write8(DRIVER(RAM_ADDRESS)+DRIVER(PAUSE_OFFSET),id==0xFE?1:0x80);
        return 1;
    }
    unsigned mapped=id>=0x81 && id<=0xF0?id-0x80:id>=0xF8 && id<=0xFD?id+2:0;
#ifdef SONIC2_MUSIC_DRIVER
    if(music_active&&id>=0xA0&&id<=0xF0)mapped=id-0x60;
#endif
    uint32_t address=DRIVER(RAM_ADDRESS)+DRIVER(QUEUE_OFFSET)+slot*2;
    if(m68k_read16(address))return 0;
#ifdef SONIC2_MUSIC_DRIVER
    if(music_active&&mapped&&(id<=0x9F||id>=0xF8))s2_cd_theme_request(mapped);
#endif
    if(mapped)m68k_write16(address,(uint16_t)mapped);
    return 1;
#else
    (void)id;(void)slot;return 0;
#endif
}
int s2_sound_hook(uint32_t pc)
{
#ifdef SONIC2_CLONE_DRIVER
    /* The ROM's actual sound initialisation is also a binding point, so
     * native reset paths do not depend on an optional lifecycle callback. */
    if(pc==0xEC000u && !bound)s2_sound_post_reset();
    if(!active)return 0;
#ifdef SONIC2_MUSIC_DRIVER
    if(pc==0x3BA8u && music_active && s2_music_title_frames()!=640) {
        /* The original timer write has completed. Continue its function;
         * returning handled here would return from title initialization. */
        glue_poke16(0xFFF614u,(uint16_t)s2_music_title_frames());
        return 0;
    }
#endif
    if(pc==0xEC000u) {
        M68KState saved=g_cpu;
        memset(g_ram+(S2_CLONE_RAM_ADDRESS&65535),0,0x400);
        recomp_call_addr(DRIVER(INITIALISE));
#ifdef SONIC2_MUSIC_DRIVER
        if(music_active)s2_cd_theme_request(0xFF);
#endif
        g_cpu=saved;return 1;
    }
    if(pc==0x1084u) {
        M68KState saved=g_cpu;
        unsigned music=g_ram[0xFFE0]?0xFFE0:0xFFE4;
        if(g_ram[music] && queue(g_ram[music],0))m68k_write8(0xFF0000u+music,0);
        /* The original queue only has three SFX slots. Do not consume
         * Music1 as a fourth SFX (a stock off-by-one defect). */
        for(unsigned i=0;i<3;i++)if(g_ram[0xFFE1+i]) {
            if(queue(g_ram[0xFFE1+i],i+1))m68k_write8(0xFFFFE1u+i,0);
        }
        recomp_call_addr(DRIVER(GUARDED_UPDATE));
#ifdef SONIC2_MUSIC_DRIVER
        if(music_active)s2_cd_theme_frame((m68k_read8(DRIVER(RAM_ADDRESS)+DRIVER(PAUSE_OFFSET))&1)!=0,
            (m68k_read8(DRIVER(RAM_ADDRESS)+S2_MUSIC_CLONE_TEMPO_OFFSET+1)&1)!=0);
#endif
        g_cpu=saved;return 1;
    }
#else
    (void)pc;
#endif
    return 0;
}
int s2_sound_dispatch_miss(uint32_t pc)
{
#ifdef SONIC2_CLONE_DRIVER
    /* The stock ROM identity remains 1 MiB. Its generic miss recorder
     * filters larger addresses, so enforce coverage for our appended bank. */
    uint32_t end=S2_CLONE_LOAD_ADDRESS+S2_CLONE_PAYLOAD_SIZE;
#ifdef S2_CLONE_ASSETS_FROM_ROM
    end=S2_SOUND_ASSETS_END;
#endif
#ifdef SONIC2_MUSIC_DRIVER
    if(music_active&&pc>=S2_MUSIC_CLONE_LOAD_ADDRESS&&pc<S2_MUSIC_CLONE_LOAD_ADDRESS+S2_MUSIC_CLONE_PAYLOAD_SIZE){
        fprintf(stderr,"[music] Missing native sequencer target $%06X\n",pc);exit(1);
    }
#endif
    if(active && pc>=S2_CLONE_LOAD_ADDRESS && pc<end) {
        fprintf(stderr,"[sound-driver] Missing native sound target $%06X; refusing to skip driver code\n",pc);
        exit(1);
    }
#else
    (void)pc;
#endif
    return 0;
}
void s2_sound_state_save(void *out)
{
    uint8_t bytes[S2_SOUND_STATE_BYTES]={'S','2','S','D',2,0,0,0};
    /* The frame-0 snapshot can precede entry into the game fiber. Its driver
     * identity must already match the first running tick. Selection becomes
     * immutable for this process at entry; later UI edits are for a restart. */
    bytes[5]=(uint8_t)(bound?active:selected);
#ifdef SONIC2_CLONE_DRIVER
    if(bytes[5])for(unsigned i=0;i<32;i++) {
        unsigned a=S2_CLONE_SHA256[2*i],b=S2_CLONE_SHA256[2*i+1];
        bytes[8+i]=(uint8_t)(((a<='9'?a-'0':a-'a'+10)<<4)|(b<='9'?b-'0':b-'a'+10));
    }
#endif
#ifdef SONIC2_MUSIC_DRIVER
    bytes[6]=(uint8_t)(bound?music_active:(selected&&s2_themes_enabled()));
    if(bytes[6]){
        const char *sha=S2_MUSIC_CLONE_SHA256;
        for(unsigned i=0;i<32;i++){unsigned a=sha[2*i],b=sha[2*i+1];bytes[8+i]=(uint8_t)(((a<='9'?a-'0':a-'a'+10)<<4)|(b<='9'?b-'0':b-'a'+10));}
        if(bound)memcpy(bytes+40,music_identity,32);else s2_music_config_identity(bytes+40);
    }
#endif
    memcpy(out,bytes,sizeof bytes);
}
int s2_sound_state_valid(const void *data)
{
    uint8_t expected[S2_SOUND_STATE_BYTES];s2_sound_state_save(expected);
    return data && !memcmp(data,expected,sizeof expected);
}
int s2_sound_netplay_allowed(void) { return !(selected || active); }
void s2_sound_config_image(char *out,size_t size)
{
    uint8_t state[S2_SOUND_STATE_BYTES];s2_sound_state_save(state);
    char hash[65];for(unsigned i=0;i<32;i++)snprintf(hash+2*i,3,"%02x",state[8+i]);
    snprintf(out,size,"sonic2.sound_driver=%s\nsonic2.sound_pack=%s\n",state[5]?"clone":"original",hash);
}
