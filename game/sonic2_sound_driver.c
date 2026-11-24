#include "sonic2_sound_driver.h"
#include "genesis_runtime.h"
#include "genesis_clocks.h"
#include "audio/event_queue.h"
#include "sonic2_sound_sequence.h"
#include "sonic2_state_io.h"
#ifdef SONIC2_MUSIC_DRIVER
#include "sonic2_music.h"
#include "sonic2_cd_themes.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static S2StockSound *sound_bank;
static S2SoundSequence sequence;
enum { S2_SOUND_FRAME_CYCLES=895780 };
extern uint32_t g_68k_stamp_rebase;
#ifdef SONIC2_MUSIC_DRIVER
static S2MusicLibrary library;
static int music_active;
static uint8_t music_identity[32];
#endif

void s2_sound_load_settings(const char *settings)
{
#ifdef SONIC2_MUSIC_DRIVER
    s2_music_release(sound_bank,&library);
    music_active=0;
    memset(music_identity,0,sizeof music_identity);
    s2_music_load(settings);
#else
    (void)settings;
#endif
    free(sound_bank);
    sound_bank=NULL;
    s2_sequence_reset(&sequence);
}

void s2_sound_post_reset(void)
{
    if(sound_bank)return;
    char error[256]={0};
    sound_bank=s2_sound_decode_stock(g_rom,0x100000,error,sizeof error);
    if(!sound_bank) {
        fprintf(stderr,"[sound-driver] %s\n",error);
        exit(1);
    }
    s2_sequence_reset(&sequence);
#ifdef SONIC2_MUSIC_DRIVER
    music_active=s2_themes_enabled();
    s2_music_config_identity(music_identity);
    if(music_active && !s2_music_prepare(sound_bank,&library,error,sizeof error)) {
        fprintf(stderr,"[music] %s\n",error);
        exit(1);
    }
#endif
}

int s2_sound_music_playing(void)
{
    return sound_bank?s2_sequence_playing(&sequence):-1;
}

unsigned s2_sound_music_channels(unsigned native)
{
    S2MusicCue cue=s2_sound_music_cue(sound_bank,native+0x80);
    if(cue.bytes<8)return 0x7FF;
    const uint8_t *p=cue.data;
    unsigned fm=p[2],psg=p[3],end=(unsigned)p[0]<<8|p[1],mask=1;
    if(8+fm*4+psg*6>end || end>cue.bytes)return mask;
    for(unsigned i=0;i<fm+psg;++i) {
        unsigned header=8+(i<fm?i*4:fm*4+(i-fm)*6);
        unsigned at=(unsigned)p[header]<<8|p[header+1];
        while(at+2<end && p[at]==0xFF && p[at+1]==2)at+=3;
        if(at>=end || (at+1<end && p[at]==0xFF && (p[at+1]==16 || p[at+1]==4)))continue;
        unsigned voice=i==0?7:i<fm?i:8+i-fm;
        mask|=1u<<voice;
    }
    return mask;
}

const char *s2_sound_music_channel_name(unsigned channel,int playing)
{
    static const char *const names[]={
        "ALL CHANNELS","FM 1","FM 2","FM 3","FM 4","FM 5","FM 6","DAC","PSG 1","PSG 2","PSG 3 / NOISE"
    };
    if(channel==10 && playing && sequence.music[8].active)
        return sequence.music[8].noise?"PSG NOISE":"PSG 3";
    return channel<sizeof names/sizeof names[0]?names[channel]:"";
}

static void chip_write(void *context,uint32_t cycle,uint8_t chip,uint8_t reg,uint8_t value)
{
    uint32_t stamp=*(const uint32_t *)context+cycle;
    if(chip==S2_CHIP_PSG)audio_event_push(stamp,AUDIO_PORT_PSG,value);
    else audio_event_push_latched(stamp,chip==S2_CHIP_FM1?AUDIO_PORT_FM1_DATA:AUDIO_PORT_FM2_DATA,reg,value);
}

#ifdef SONIC2_MUSIC_DRIVER
static void request_stream(unsigned id)
{
    if(id>=0x81 && id<=0x9F)s2_cd_theme_request(id-0x80);
    else if(id>=0xF8 && id<=0xFD)s2_cd_theme_request(id+2);
}
#endif

int s2_sound_hook(uint32_t pc)
{
#ifdef SONIC2_MUSIC_DRIVER
    if(pc==0x3BA8u && music_active && s2_music_title_frames()!=640) {
        glue_poke16(0xFFF614u,(uint16_t)s2_music_title_frames());
        return 0;
    }
#endif
    if(pc!=0xEC000u && pc!=0x1084u)return 0;
    if(!sound_bank)s2_sound_post_reset();
    if(pc==0xEC000u) {
        /* Audio queues are sequenced on the host. Hold the unused CPU reset. */
        glue_poke16(0xA11200u,0);
        s2_sequence_reset(&sequence);
#ifdef SONIC2_MUSIC_DRIVER
        if(music_active)s2_cd_theme_request(0xFF);
#endif
        return 1;
    }

    uint8_t *queue=g_ram+0xFFE0;
#ifdef SONIC2_MUSIC_DRIVER
    if(music_active) {
        request_stream(queue[queue[0]?0:4]);
        for(unsigned i=1;i<=3;++i)request_stream(queue[i]);
    }
#endif
    uint32_t start=g_audio_cycle_counter*7u+g_68k_stamp_rebase;
    int pal=(glue_peek8(0xA10001u)&0x40)!=0;
    if(!s2_sequence_frame(&sequence,sound_bank,queue,pal,GENESIS_MASTER_CLOCK_NTSC,
                          S2_SOUND_FRAME_CYCLES,chip_write,&start)) {
        fprintf(stderr,"[sound-driver] Native cue $%02X channel %u offset $%04X is unsupported\n",
                sequence.fault_cue,sequence.fault_channel,sequence.fault_offset);
        exit(1);
    }
#ifdef SONIC2_MUSIC_DRIVER
    if(music_active) {
        s2_cd_theme_frame(sequence.paused,sequence.music_cue==0x98);
        if(g_ram[0xF600]==4 && s2_music_title_frames()>640) {
            unsigned remaining=(unsigned)g_ram[0xF614]<<8|g_ram[0xF615];
            if(remaining>90 && remaining<1620 && !s2_sequence_playing(&sequence))
                glue_poke16(0xFFF614u,90);
        }
    }
#endif
    return 1;
}

void s2_sound_state_save(void *out)
{
    uint8_t bytes[S2_SOUND_STATE_BYTES]={'S','2','S','D',3,0,0,0};
#ifdef SONIC2_MUSIC_DRIVER
    bytes[5]=(uint8_t)(sound_bank?music_active:s2_themes_enabled());
    if(bytes[5]) {
        if(sound_bank)memcpy(bytes+8,music_identity,sizeof music_identity);
        else s2_music_config_identity(bytes+8);
    }
#endif
    memcpy(out,bytes,sizeof bytes);
}

static int track_valid(const S2SoundTrack *t,unsigned channel,int effect)
{
    if(!t->active)return 1;
    if(!sound_bank || t->channel!=channel || t->depth>S2_PHRASE_DEPTH ||
       t->envelope_position>256 || t->mod_position>256 ||
       t->raw_count>sizeof t->raw_register || t->mod_mode>2 || t->note<0x80 || t->note>=0xE0)
        return 0;
    S2MusicCue cue;
    if(effect) {
        if(t->cue<0xA0 || t->cue>0xF0)return 0;
        unsigned index=t->cue-0xA0;
        const uint8_t *data=t->cue==0xC3?sound_bank->save_warp:sound_bank->sfx[index];
        uint16_t bytes=t->cue==0xC3?sound_bank->save_warp_bytes:sound_bank->sfx_bytes[index];
        cue=(S2MusicCue){data,NULL,bytes,0,0};
    } else cue=s2_sound_music_cue(sound_bank,t->cue);

    if(cue.bytes<4 || (t->mod_envelope && (!cue.donor || t->mod_envelope>S2_DONOR_MOD_ENVELOPES)))return 0;
    unsigned end=(unsigned)cue.data[0]<<8|cue.data[1];
    if(end>cue.bytes || t->pc>end)return 0;
    for(unsigned i=0;i<t->depth;++i)if(t->returns[i]>end)return 0;
    return channel<S2_FM_CHANNELS || channel==S2_DAC_CHANNEL ||
           t->voice<=(cue.donor?S2_DONOR_PSG_ENVELOPES:S2_PSG_ENVELOPES);
}

static int sequence_valid(const S2SoundSequence *state)
{
    unsigned cue=state->music_cue,saved=state->saved_cue;
    if((cue && (cue<0x81 || cue>0x9F)) || (saved && (saved<0x81 || saved>0x9F)) ||
       state->pcm_phase>GENESIS_MASTER_CLOCK_NTSC || state->pal_count>=5 || state->saved_pal_count>=5 ||
       state->speed_count>8 || state->saved_speed_count>8 ||
       state->paused>1 || state->dac_enabled>1 || state->speed_up>1 ||
       state->fade>=64 || state->fault_cue)return 0;
    if(state->pcm_id) {
        uint32_t bytes,rate;
        if(!s2_sound_pcm_view(sound_bank,state->pcm_cue,state->pcm_id,&bytes,&rate) ||
           state->pcm_position>bytes || !rate)return 0;
    }
    for(unsigned ch=0;ch<S2_SEQUENCE_TRACKS;++ch) {
        if(!track_valid(&state->music[ch],ch,0))return 0;
        if(saved && !track_valid(&state->saved[ch],ch,0))return 0;
    }
    for(unsigned ch=0;ch<S2_TONE_CHANNELS;++ch)
        if(!track_valid(&state->effect[ch],ch,1))return 0;
    return 1;
}

void s2_sound_sequence_state(S2StateIO *io)
{
    if(io->data && io->mode) {
        S2SoundSequence incoming;
        if(io->pos>io->size || sizeof incoming>io->size-io->pos) {io->ok=0;return;}
        memcpy(&incoming,io->data+io->pos,sizeof incoming);
        if(!sequence_valid(&incoming)) {io->ok=0;return;}
    }
    S2_STATE(io,sequence);
}

int s2_sound_state_valid(const void *data)
{
    uint8_t expected[S2_SOUND_STATE_BYTES];
    s2_sound_state_save(expected);
    return data && !memcmp(data,expected,sizeof expected);
}

int s2_sound_netplay_allowed(void)
{
#ifdef SONIC2_MUSIC_DRIVER
    return !music_active && !s2_themes_enabled();
#else
    return 1;
#endif
}
