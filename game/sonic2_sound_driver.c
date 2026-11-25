#include "sonic2_sound_driver.h"
#include "genesis_runtime.h"
#ifdef SONIC2_MUSIC_DRIVER
#include "sonic2_music.h"
#include "sonic2_cd_themes.h"
#include "sonic2_music_clone_metadata.h"
#include "sonic3_sound_legacy.h"
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
static unsigned stock_channel_masks[31];
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
unsigned s2_sound_music_channels(unsigned native)
{
#ifdef SONIC2_MUSIC_DRIVER
    if(!active && native>=1 && native<=31){
        unsigned *cached=&stock_channel_masks[native-1];
        if(*cached)return *cached;
        /* Reuse the bounded cartridge header decoder; original-Z80 audition
         * needs only its channel roots, not a second prepared sound bank. */
        S3LegacySource source;
        if(s3_legacy_music_source(g_rom,0x100000,S2_DONOR_S2,native-1,&source)){
            const uint8_t *data=source.compressed?source.song:g_rom+source.bank;
            unsigned bytes=source.compressed?source.high-source.low:0x8000;
            unsigned at=source.compressed?0:source.source-source.bank;
            if(at<=bytes && bytes-at>=6){
                unsigned fm=data[at+2],psg=data[at+3],mask=1;
                if(fm<=7 && psg<=3 && bytes-at>=6+fm*4+psg*6){
                    for(unsigned i=0;i<fm+psg;i++){
                        unsigned h=at+6+(i<fm?i*4:fm*4+(i-fm)*6);
                        unsigned stream=data[h]|((unsigned)data[h+1]<<8);
                        stream=source.compressed?stream-source.low:stream&0x7FFFu;
                        for(unsigned n=0;n<8 && stream<bytes && bytes-stream>=2 && data[stream]==0xE2;n++)stream+=2;
                        unsigned voice=i==0?7:i<fm?i:8+i-fm;
                        if(stream<bytes && data[stream]!=0xF2 && data[stream]!=0xE4 && data[stream]!=0xEE)mask|=1u<<voice;
                    }
                    return *cached=mask;
                }
            }
        }
    }
#endif
#ifdef SONIC2_CLONE_DRIVER
    if(active && native>=1 && native<=31){
        unsigned table=S2_CLONE_MUSICINDEX;
#ifdef SONIC2_MUSIC_DRIVER
        if(music_active)table=S2_MUSIC_CLONE_MUSIC_INDEX;
#endif
        const uint8_t *entry=g_rom+table+(native-1)*4;
        unsigned at=(((unsigned)entry[1]<<16)|((unsigned)entry[2]<<8)|entry[3])&0xFFFFFEu;
        if(at<0x3FFFFCu){
            unsigned fm=g_rom[at+2],psg=g_rom[at+3],mask=1;
            if(fm<=7 && psg<=3 && at<=0x400000u-(8+fm*4+psg*6)){
                /* Headers include dummy DAC/PSG streams that stop at once.
                 * Do not offer those silent placeholders as audition voices. */
                for(unsigned i=0;i<fm+psg;i++){
                    unsigned h=at+8+(i<fm?i*4:fm*4+(i-fm)*6);
                    unsigned stream=at+((unsigned)g_rom[h]<<8)+g_rom[h+1];
                    unsigned voice=i==0?7:i<fm?i:8+i-fm;
                    for(unsigned n=0;n<8 && stream<0x3FFFFDu && g_rom[stream]==0xFF && g_rom[stream+1]==2;n++)stream+=3;
                    if(stream<0x3FFFFFu && !(g_rom[stream]==0xFF &&
                       (g_rom[stream+1]==0x10 || g_rom[stream+1]==4)))mask|=1u<<voice;
                }
                return mask;
            }
        }
    }
#endif
    return 0x7FF;
}
const char *s2_sound_music_channel_name(unsigned channel,int playing)
{
    static const char *const names[]={"ALL CHANNELS","FM 1","FM 2","FM 3","FM 4","FM 5","FM 6","DAC","PSG 1","PSG 2","PSG 3"};
#ifdef SONIC2_CLONE_DRIVER
    if(channel==10 && active && playing){
        unsigned stride=44;
#ifdef SONIC2_MUSIC_DRIVER
        if(music_active)stride=46;
#endif
        unsigned at=(DRIVER(RAM_ADDRESS)&65535u)+9*stride;
        if(g_ram[at+1]==0xE0)return "PSG NOISE";
    }
#endif
    if(channel==10 && (!playing || !active))return "PSG 3 / NOISE";
    return channel<11?names[channel]:"";
}
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
    music_active=0;memset(music_identity,0,sizeof music_identity);memset(stock_channel_masks,0,sizeof stock_channel_masks);
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
/* Reserve the cartridge's unused Giant Ring slot for the Sonic 3 save-entry
 * variant. Its voice, note and duration match S2 Special Stage Entry; only
 * the modulation delta differs. Prepare one immutable execution image before
 * boot and cold-state validation, independent of the chosen menu preference. */
static void prepare_save_warp(void)
{
    enum { SOURCE=0xFF782, DEST=0xFF5E5, BYTES=45 };
    memcpy(g_rom+DEST,g_rom+SOURCE,BYTES);
    unsigned voice=DEST+20-0xF0000,track=DEST+10-0xF0000;
    g_rom[DEST]=(uint8_t)voice;g_rom[DEST+1]=(uint8_t)(voice>>8);
    g_rom[DEST+6]=(uint8_t)track;g_rom[DEST+7]=(uint8_t)(track>>8);
    g_rom[DEST+15]=0x50;
#if defined(SONIC2_CLONE_DRIVER) && defined(S2_CLONE_ASSETS_FROM_ROM)
    if(active){
        unsigned from=S2_SOUND_SFX_BASE+(0xCA-0xA0)*S2_SOUND_SFX_STRIDE;
        unsigned to=S2_SOUND_SFX_BASE+(0xC3-0xA0)*S2_SOUND_SFX_STRIDE;
        memcpy(g_rom+to,g_rom+from,S2_SOUND_SFX_STRIDE);
        unsigned stream=((unsigned)g_rom[to+6]<<8)|g_rom[to+7];
        /* Native S3 modulation uses Z80 timing, including its turn frames. */
        g_rom[to+stream+4]=0x21;
        g_rom[to+stream+7]=0x50;
    }
#endif
    /* Keep the game's genuine checksum gate for the derived execution image.
     * Original source-media identity is still verified before this step. */
    unsigned checksum=0;
    for(unsigned at=0x200;at<0x100000;at+=2)checksum+=((unsigned)g_rom[at]<<8)|g_rom[at+1];
    g_rom[0x18E]=(uint8_t)(checksum>>8);g_rom[0x18F]=(uint8_t)checksum;
}
void s2_sound_post_reset(void)
{
    if(bound)return;
    active=selected;bound=1;
    if (!active) {prepare_save_warp();return;}
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
    prepare_save_warp();
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
        /* HandlePause changes the request byte from 1 to the held state 2.
         * Both positive states pause recordings; 0x80 is an unpause request. */
        if(music_active)s2_cd_theme_frame((m68k_read8(DRIVER(RAM_ADDRESS)+DRIVER(PAUSE_OFFSET))&0x7F)!=0,
            (m68k_read8(DRIVER(RAM_ADDRESS)+S2_MUSIC_CLONE_TEMPO_OFFSET+1)&1)!=0);
        if(music_active && g_ram[0xF600]==4 && s2_music_title_frames()>640){
            /* The longer allowance protects complete imported title cues.
             * Once the cue finishes, retain a short logo hold instead of
             * waiting out the full allowance. Startup cannot count as EOF. */
            unsigned remaining=((unsigned)g_ram[0xF614]<<8)|g_ram[0xF615];
            if(remaining>90 && remaining<1620 && !s2_sound_music_playing())
                glue_poke16(0xFFF614u,90);
        }
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
