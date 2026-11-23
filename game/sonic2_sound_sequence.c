#include "sonic2_sound_sequence.h"
#include <string.h>

enum {
    REST=0x80, TIE=0xFE, COMMAND=0xFF,
    ONE_UP=0x98, SAVE_WARP=0xC3,
    STOP_EFFECTS=0xF8, FADE_MUSIC=0xF9, SEGA_SAMPLE=0xFA,
    SPEED_UP=0xFB, SLOW_DOWN=0xFC, STOP_ALL=0xFD, PAUSE=0xFE, RESUME=0xFF,
    DIRTY_VOICE=1, DIRTY_VOLUME=2, DIRTY_PITCH=4, DIRTY_PAN=8,
    KEY_OFF=16, KEY_ON=32, DIRTY_NOISE=64, DIRTY_ALL=127
};

typedef struct Cue {
    const uint8_t *data;
    unsigned end, bytes;
} Cue;

typedef struct Output {
    S2SoundWrite write;
    void *context;
} Output;

static unsigned word(const uint8_t *p)
{
    return (unsigned)p[0]<<8|p[1];
}

static int music_id(unsigned cue) { return cue>=0x81 && cue<=0x9F; }
static int effect_id(unsigned cue) { return cue>=0xA0 && cue<=0xF0; }

static Cue get_cue(const S2StockSound *bank,unsigned id)
{
    Cue cue={0};
    unsigned asset=id;
    if(id==SAVE_WARP) {
        cue.data=bank->save_warp;
        cue.bytes=bank->save_warp_bytes;
    } else if(music_id(asset)) {
        cue.data=bank->music[asset-0x81];
        cue.bytes=bank->music_bytes[asset-0x81];
    } else if(effect_id(asset)) {
        cue.data=bank->sfx[asset-0xA0];
        cue.bytes=bank->sfx_bytes[asset-0xA0];
    }
    if(cue.bytes>=4)cue.end=word(cue.data);
    return cue;
}

static int fault(S2SoundSequence *s,const S2SoundTrack *track)
{
    s->fault_cue=track->cue;
    s->fault_channel=track->channel;
    s->fault_offset=track->pc;
    return 0;
}

void s2_sequence_reset(S2SoundSequence *s)
{
    memset(s,0,sizeof *s);
    memset(s->effect_owner,0xFF,sizeof s->effect_owner);
}

int s2_sequence_playing(const S2SoundSequence *s)
{
    for(unsigned ch=0;ch<S2_SEQUENCE_TRACKS;++ch)
        if(s->music[ch].active)return 1;
    return 0;
}

static void init_track(S2SoundTrack *t,unsigned cue,unsigned channel,
                       unsigned pc,uint8_t divider,uint8_t key,uint8_t volume)
{
    memset(t,0,sizeof *t);
    t->cue=(uint8_t)cue;
    t->channel=(uint8_t)channel;
    t->pc=(uint16_t)pc;
    t->active=1;
    t->remaining=t->duration=1;
    t->divider=divider;
    t->transpose=key;
    t->volume=volume;
    t->note=REST;
    t->pan=0xC0;
    t->dirty=DIRTY_ALL;
    t->dirty&=(uint8_t)~KEY_ON;
}

static void stop_track(S2SoundTrack *t)
{
    t->active=0;
    t->gate=0;
    t->dirty=(t->dirty&~KEY_ON)|KEY_OFF;
}

static void stop_music(S2SoundSequence *s)
{
    for(unsigned ch=0;ch<S2_SEQUENCE_TRACKS;++ch)stop_track(&s->music[ch]);
    s->music_cue=0;
}

static void stop_effects(S2SoundSequence *s)
{
    for(unsigned ch=0;ch<S2_TONE_CHANNELS;++ch)stop_track(&s->effect[ch]);
    s->effect_priority=s->push_playing=0;
}

static int start_music(S2SoundSequence *s,const S2StockSound *bank,unsigned id)
{
    Cue cue=get_cue(bank,id);
    unsigned fm=cue.data[2],psg=cue.data[3],header=8+fm*4+psg*6;
    if(!fm || fm>7 || psg>3 || cue.end<header || cue.end>cue.bytes)return 0;
    if(id==ONE_UP && s->music_cue!=ONE_UP) {
        memcpy(s->saved,s->music,sizeof s->music);
        s->saved_cue=s->music_cue;
        s->saved_tempo=s->tempo;
        s->saved_accumulator=s->tempo_accumulator;
        stop_effects(s);
    } else if(id!=ONE_UP) {
        s->saved_cue=0;
        s->fade_in=0;
    }
    stop_music(s);
    s->music_cue=(uint8_t)id;
    s->tempo=cue.data[7];
    s->tempo_accumulator=0;
    s->pal_count=0;
    s->fade=0;
    for(unsigned i=0;i<fm+psg;++i) {
        unsigned ch=i==0?S2_DAC_CHANNEL:i<fm?i-1:S2_FM_CHANNELS+i-fm;
        const uint8_t *part=cue.data+8+(i<fm?i*4:fm*4+(i-fm)*6);
        init_track(&s->music[ch],id,ch,word(part),cue.data[6],part[2],part[3]);
        if(i>=fm) {
            if(part[5]>S2_PSG_ENVELOPES)return 0;
            s->music[ch].voice=part[5];
        }
    }
    if(id==ONE_UP)s->pcm_id=0;
    return 1;
}

static int sfx_channel(unsigned control)
{
    if(control>=0x80 && control<=0xE0 && !(control&31))
        return control==0xE0?8:S2_FM_CHANNELS+(int)((control-0x80)/32);
    if(control<=2)return (int)control;
    if(control>=4 && control<=6)return (int)control-1;
    return -1;
}

static int start_effect(S2SoundSequence *s,const S2StockSound *bank,unsigned id)
{
    if(s->music_cue==ONE_UP)return 1;
    unsigned priority=bank->sfx_priority[id-0xA0];
    if(priority<s->effect_priority)return 1;
    if(!(priority&0x80))s->effect_priority=(uint8_t)priority;
    if(id==0xB5 && (s->ring_side^=1u))id=0xCE;
    if(id==0xA7) {
        if(s->push_playing)return 1;
        s->push_playing=1;
    }
    Cue cue=get_cue(bank,id);
    unsigned tracks=cue.data[3];
    if(!tracks || tracks>6 || cue.end<4+tracks*6 || cue.end>cue.bytes)return 0;
    for(unsigned i=0;i<tracks;++i) {
        const uint8_t *part=cue.data+4+i*6;
        int ch=sfx_channel(part[1]);
        if(ch<0)return 0;
        S2SoundTrack *t=&s->effect[ch];
        init_track(t,id,(unsigned)ch,word(part+2),cue.data[2],part[4],part[5]);
        if(part[1]==0xE0)t->noise=0xE7;
    }
    return 1;
}

static void restore_music(S2SoundSequence *s)
{
    stop_music(s);
    if(!s->saved_cue)return;
    memcpy(s->music,s->saved,sizeof s->music);
    for(unsigned ch=0;ch<S2_SEQUENCE_TRACKS;++ch)
        s->music[ch].dirty=DIRTY_ALL;
    s->music_cue=s->saved_cue;
    s->tempo=s->saved_tempo;
    s->tempo_accumulator=s->saved_accumulator;
    s->saved_cue=0;
    s->fade_in=24;
}

static void pcm_start(S2SoundSequence *s,unsigned id)
{
    s->pcm_id=(uint8_t)id;
    s->pcm_position=0;
    s->pcm_phase=0;
}

static int request(S2SoundSequence *s,const S2StockSound *bank,unsigned id)
{
    if(!id || id<0x81)return 1;
    if(music_id(id))return start_music(s,bank,id);
    if(effect_id(id))return start_effect(s,bank,id);
    switch(id) {
    case STOP_EFFECTS:stop_effects(s);break;
    case FADE_MUSIC:s->fade=1;s->fade_clock=0;stop_effects(s);break;
    case SEGA_SAMPLE:stop_music(s);stop_effects(s);pcm_start(s,SEGA_SAMPLE);break;
    case SPEED_UP:s->speed_up=1;break;
    case SLOW_DOWN:s->speed_up=0;break;
    case STOP_ALL:stop_music(s);stop_effects(s);s->saved_cue=0;s->pcm_id=0;s->fade=s->fade_in=0;break;
    case PAUSE:s->paused=1;break;
    case RESUME:
        if(s->paused) {
            memset(s->effect_owner,0xFF,sizeof s->effect_owner);
            for(unsigned ch=0;ch<S2_SEQUENCE_TRACKS;++ch)s->music[ch].dirty=DIRTY_ALL;
            for(unsigned ch=0;ch<S2_TONE_CHANNELS;++ch)s->effect[ch].dirty=DIRTY_ALL;
            s->dac_enabled=0;
        }
        s->paused=0;
        break;
    default:break; /* reserved sound IDs have no command */
    }
    return 1;
}

static int command(S2SoundSequence *s,S2SoundTrack *t,Cue cue,const S2StockSound *bank)
{
    /* Operand widths of the normalized stock cue format. */
    static const uint8_t widths[]={
        1,1,1,0,0,1,1,0,1,1,1,1,1,1,4,0,0,1,0,1,2,4,2,2,0,1,1,1,1,1,2,0,2,4,1
    };
    if(t->pc>=cue.end)return 0;
    unsigned code=cue.data[t->pc++];
    if(code>=sizeof widths || widths[code]>cue.end-t->pc)return 0;
    const uint8_t *p=cue.data+t->pc;
    t->pc+=widths[code];
    switch(code) {
    case 0:t->pan=(t->pan&0x37)|p[0];t->dirty|=DIRTY_PAN;break;
    case 1:t->detune=p[0];t->dirty|=DIRTY_PITCH;break;
    case 2:s->communication=p[0];break;
    case 3:
        if(!t->depth)return 0;
        t->pc=t->returns[--t->depth];
        break;
    case 4:s->restore_pending=1;stop_track(t);break;
    case 5:t->divider=p[0];break;
    case 6:case 12:t->volume+=p[0];t->dirty|=DIRTY_VOLUME;break;
    case 7:case 16:case 24:
        if(effect_id(t->cue))s->effect_priority=0;
        stop_track(t);
        break;
    case 8:case 29:t->timeout_master=t->timeout=p[0];break;
    case 9:t->transpose+=p[0];break;
    case 10:s->tempo=p[0];break;
    case 11:
        for(unsigned ch=0;ch<S2_SEQUENCE_TRACKS;++ch)s->music[ch].divider=p[0];
        break;
    case 13:
        if(t->channel<S2_FM_CHANNELS && cue.end+25u*(p[0]+1)>cue.bytes)return 0;
        t->voice=p[0];t->dirty|=DIRTY_VOICE;
        break;
    case 14:case 33:
        t->mod_mode=code==14?1:2;
        t->mod_wait=p[0];t->mod_speed=p[1];t->mod_delta=p[2];t->mod_steps=p[3];
        t->mod_delay=p[0];t->mod_counter=p[1];t->mod_direction=p[2];
        t->mod_remaining=code==14?p[3]/2:p[3];
        t->modulation=0;
        break;
    case 15:t->mod_mode=1;break;
    case 17:t->noise=p[0];t->dirty|=DIRTY_NOISE|DIRTY_VOLUME;break;
    case 18:t->mod_mode=0;t->modulation=0;t->dirty|=DIRTY_PITCH;break;
    case 19:
        if(p[0]>S2_PSG_ENVELOPES)return 0;
        t->voice=p[0];
        break;
    case 20:case 21:case 22: {
        if(code==21) {
            unsigned id=p[0];
            if(id>=S2_LOOP_COUNTERS)return 0;
            if(!t->loops[id])t->loops[id]=p[1];
            if(!--t->loops[id])break;
        }
        int destination=(int)t->pc-(code==22?0:1)+(int16_t)word(cue.data+t->pc-2);
        if(destination<0 || (unsigned)destination>=cue.end)return 0;
        if(code==22) {
            if(t->depth==S2_PHRASE_DEPTH)return 0;
            t->returns[t->depth++]=t->pc;
        }
        t->pc=(uint16_t)destination;
        break;
    }
    case 23:case 32:
        if(t->channel<S2_FM_CHANNELS) {
            if(t->raw_count==sizeof t->raw_register)return 0;
            t->raw_register[t->raw_count]=p[0];
            if(code==32)t->raw_global|=(uint8_t)(1u<<t->raw_count);
            t->raw_value[t->raw_count++]=p[1];
        }
        break;
    case 25:
        if(p[0]>REST && p[0]<=REST+S2_PCM_SLOTS)pcm_start(s,p[0]);
        break;
    case 26:
        if(!request(s,bank,p[0]))return 0;
        break;
    case 27:t->transpose=p[0];break;
    case 28:t->volume=p[0];t->dirty|=DIRTY_VOLUME;break;
    case 31:s->push_playing=0;break;
    default:return 0; /* an unimplemented opcode is an explicit fault */
    }
    return 1;
}

static void do_modulation(S2SoundTrack *t)
{
    if(!t->mod_mode || !t->gate)return;
    if(t->mod_delay) {
        --t->mod_delay;
        return;
    }
    int step=!--t->mod_counter;
    if(step)t->mod_counter=t->mod_speed;
    if(t->mod_mode==2) {
        if(step)t->modulation+=(int8_t)t->mod_direction;
        if(!--t->mod_remaining) {
            t->mod_remaining=t->mod_steps;
            t->mod_direction=(uint8_t)-t->mod_direction;
        }
    } else if(step) {
        if(t->mod_remaining) {
            --t->mod_remaining;
            t->modulation+=(int8_t)t->mod_direction;
        } else {
            t->mod_remaining=t->mod_steps;
            t->mod_direction=(uint8_t)-t->mod_direction;
            step=0;
        }
    }
    if(step)t->dirty|=DIRTY_PITCH;
}

static void reset_modulation(S2SoundTrack *t)
{
    t->mod_delay=t->mod_wait;
    t->mod_counter=t->mod_speed;
    t->mod_direction=t->mod_delta;
    t->mod_remaining=t->mod_mode==1?t->mod_steps/2:t->mod_steps;
    t->modulation=0;
}

static int update_track(S2SoundSequence *s,const S2StockSound *bank,S2SoundTrack *t)
{
    if(!t->active)return 1;
    Cue cue=get_cue(bank,t->cue);
    if(!--t->remaining) {
        t->hold=0;
        unsigned limit=0;
        while(t->active) {
            if(t->pc>=cue.end || ++limit>cue.end)return fault(s,t);
            unsigned byte=cue.data[t->pc++];
            if(byte==COMMAND) {
                if(!command(s,t,cue,bank))return fault(s,t);
                continue;
            }
            if(byte==TIE) {
                t->hold=1;
                continue;
            }
            if(byte>=0xE0)return fault(s,t);
            if(byte>=REST) {
                t->note=(uint8_t)byte;
                if(t->pc<cue.end && cue.data[t->pc]<REST)
                    t->duration=(uint8_t)(cue.data[t->pc++]*t->divider);
            } else t->duration=(uint8_t)(byte*t->divider);
            t->remaining=t->duration;
            if(t->channel==S2_DAC_CHANNEL) {
                if(t->note>REST+S2_PCM_SLOTS)return fault(s,t);
                if(t->note>REST)pcm_start(s,t->note);
                return 1;
            }
            if(t->note==REST) {
                t->gate=0;
                t->dirty=(t->dirty&~KEY_ON)|KEY_OFF;
                return 1;
            }
            if(byte>=REST) {
                if(t->channel<S2_FM_CHANNELS) {
                    unsigned key=(t->note-REST+t->transpose)&127u;
                    t->period=bank->fm_pitch[key];
                } else {
                    unsigned key=(t->note-REST-1+t->transpose)&127u;
                    t->period=bank->psg_pitch[key];
                }
            }
            t->dirty|=DIRTY_PITCH;
            if(!t->hold || !t->gate) {
                t->gate=1;
                t->timeout=t->timeout_master;
                t->envelope_position=t->envelope_level=0;
                reset_modulation(t);
                t->dirty|=KEY_OFF|KEY_ON|DIRTY_VOLUME;
            }
            break;
        }
    } else if(t->gate) {
        if(t->timeout && !--t->timeout) {
            t->gate=0;
            t->dirty=(t->dirty&~KEY_ON)|KEY_OFF;
        }
        do_modulation(t);
    }
    if(t->channel>=S2_FM_CHANNELS && t->channel!=S2_DAC_CHANNEL && t->gate && t->voice) {
        unsigned value=bank->envelope[t->voice-1][t->envelope_position];
        if(value<0x80) {
            if(t->envelope_level!=value)t->dirty|=DIRTY_VOLUME;
            t->envelope_level=(uint8_t)value;
            ++t->envelope_position;
        }
    }
    return 1;
}

static void fm(Output out,unsigned channel,unsigned reg,unsigned value)
{
    out.write(out.context,0,channel<3?S2_CHIP_FM1:S2_CHIP_FM2,
              (uint8_t)(reg+channel%3),(uint8_t)value);
}

static void key(Output out,unsigned channel,int on)
{
    out.write(out.context,0,S2_CHIP_FM1,0x28,(uint8_t)(channel+(channel>=3)+(on?0xF0:0)));
}

static void psg(Output out,unsigned value)
{
    out.write(out.context,0,S2_CHIP_PSG,0,(uint8_t)value);
}

static unsigned attenuation(const S2SoundSequence *s)
{
    return (s->fade?s->fade:0)+s->fade_in;
}

static void render_track(S2SoundSequence *s,const S2StockSound *bank,S2SoundTrack *t,
                         unsigned channel,unsigned dirty,Output out)
{
    if(!dirty || channel==S2_DAC_CHANNEL)return;
    if(channel<S2_FM_CHANNELS) {
        if(dirty&KEY_OFF)key(out,channel,0);
        Cue cue=get_cue(bank,t->cue);
        unsigned voice=cue.end+25u*t->voice;
        if(t->active && voice+25<=cue.bytes) {
            const uint8_t *bytes=cue.data+voice;
            if(dirty&DIRTY_VOICE) {
                fm(out,channel,0xB0,bytes[0]);
                for(unsigned i=0;i<4;++i)fm(out,channel,0x30+i*4,bytes[1+i]);
                for(unsigned i=0;i<16;++i)fm(out,channel,0x50+i*4,bytes[9+i]);
            }
            if(dirty&(DIRTY_VOICE|DIRTY_VOLUME)) {
                for(unsigned i=0;i<4;++i) {
                    unsigned level=bytes[5+i]&127;
                    if(bytes[5+i]&0x80)level+=t->volume+attenuation(s)*2;
                    fm(out,channel,0x40+i*4,level>127?127:level);
                }
            }
        }
        if(t->active && (dirty&(DIRTY_PAN|DIRTY_VOICE)))fm(out,channel,0xB4,t->pan);
        for(unsigned i=0;i<t->raw_count;++i) {
            if(t->raw_global&(1u<<i))
                out.write(out.context,0,S2_CHIP_FM1,t->raw_register[i],t->raw_value[i]);
            else fm(out,channel,t->raw_register[i],t->raw_value[i]);
        }
        if(t->gate && (dirty&DIRTY_PITCH)) {
            unsigned pitch=(t->period+(int8_t)t->detune+t->modulation)&0x3FFF;
            fm(out,channel,0xA4,pitch>>8);
            fm(out,channel,0xA0,pitch);
        }
        if(t->gate && (dirty&KEY_ON) && !(channel==5 && s->pcm_id))
            key(out,channel,1);
    } else {
        unsigned tone=0x80+(channel-S2_FM_CHANNELS)*32;
        if(channel==8 && (dirty&DIRTY_NOISE)) {
            psg(out,t->noise?t->noise:0xFF);
            if(t->noise)psg(out,0xDF);
        }
        if(t->gate && (dirty&DIRTY_PITCH)) {
            unsigned pitch=(t->period+(int8_t)t->detune+t->modulation)&0x3FF;
            psg(out,tone|(pitch&15));
            psg(out,pitch>>4);
        }
        unsigned volume=t->gate?(t->volume>>3)+t->envelope_level+attenuation(s)/2:15;
        if(dirty&(KEY_OFF|KEY_ON|DIRTY_VOLUME|DIRTY_NOISE))
            psg(out,(channel==8 && t->noise?0xF0:tone|0x10)|(volume>15?15:volume));
    }
}

static void output_tracks(S2SoundSequence *s,const S2StockSound *bank,Output out)
{
    for(unsigned ch=0;ch<S2_TONE_CHANNELS;++ch) {
        S2SoundTrack *t=s->effect[ch].active?&s->effect[ch]:&s->music[ch];
        unsigned owner=s->effect[ch].active?1:0;
        if(ch==5 && s->pcm_id)continue;
        unsigned dirty=t->dirty;
        if(owner!=s->effect_owner[ch])dirty|=DIRTY_ALL;
        if(t->raw_count && !dirty)dirty=DIRTY_PAN;
        render_track(s,bank,t,ch,dirty,out);
        s->effect_owner[ch]=(uint8_t)owner;
        s->effect[ch].dirty=s->effect[ch].raw_count=0;
        s->music[ch].dirty=s->music[ch].raw_count=0;
        s->effect[ch].raw_global=s->music[ch].raw_global=0;
    }
}

static void render_pcm(S2SoundSequence *s,const S2StockSound *bank,uint32_t hz,uint32_t span,Output out)
{
    const uint8_t *sample=NULL;
    unsigned length=0,rate=0;
    if(s->pcm_id==SEGA_SAMPLE) {
        sample=bank->sega;
        length=sizeof bank->sega;
        rate=bank->sega_rate;
    } else if(s->pcm_id>REST && s->pcm_id<=REST+S2_PCM_SLOTS) {
        const S2SoundSample *desc=&bank->samples[s->pcm_id-REST-1];
        sample=bank->pcm[desc->bank];
        length=desc->bytes;
        rate=desc->rate;
    }
    if(!sample || !rate) {
        if(s->dac_enabled)out.write(out.context,0,S2_CHIP_FM1,0x2B,0);
        s->dac_enabled=0;
        return;
    }
    if(!s->dac_enabled) {
        key(out,5,0);
        fm(out,5,0xB4,0xC0);
        out.write(out.context,0,S2_CHIP_FM1,0x2B,0x80);
        s->dac_enabled=1;
    }
    uint64_t phase=s->pcm_phase;
    uint64_t end=(uint64_t)span*rate;
    while(phase<end && s->pcm_position<length) {
        out.write(out.context,(uint32_t)(phase/rate),S2_CHIP_FM1,0x2A,sample[s->pcm_position++]);
        phase+=hz;
    }
    if(s->pcm_position==length && phase<end) {
        uint32_t at=(uint32_t)(phase/rate);
        out.write(out.context,at,S2_CHIP_FM1,0x2A,128);
        out.write(out.context,at,S2_CHIP_FM1,0x2B,0);
        s->dac_enabled=s->pcm_id=0;
        s->music[5].dirty=DIRTY_ALL;
        s->effect[5].dirty=DIRTY_ALL;
    } else s->pcm_phase=(uint32_t)(phase-end);
}

static void mute(Output out)
{
    for(unsigned ch=0;ch<S2_FM_CHANNELS;++ch)key(out,ch,0);
    for(unsigned ch=0;ch<4;++ch)psg(out,0x9F+ch*32);
    out.write(out.context,0,S2_CHIP_FM1,0x2B,0);
}

int s2_sequence_frame(S2SoundSequence *s,const S2StockSound *bank,
                      uint8_t queue[5],int pal,uint32_t hz,uint32_t span,
                      S2SoundWrite write,void *context)
{
    if(!s || !bank || !queue || !write || !hz || !span || s->fault_cue)return 0;
    Output out={write,context};
    int was_paused=s->paused;
    unsigned music_slot=queue[0]?0:4;
    if(!request(s,bank,queue[music_slot]))return 0;
    queue[music_slot]=0;
    for(unsigned i=1;i<=3;++i) {
        if(!request(s,bank,queue[i]))return 0;
        queue[i]=0;
    }
    if(s->paused) {
        if(!was_paused)mute(out);
        return 1;
    }
    unsigned ticks=1;
    if(pal && s->music_cue && !bank->music_pal[s->music_cue-0x81] && ++s->pal_count==5) {
        s->pal_count=0;
        ++ticks;
    }
    while(ticks--) {
        unsigned tempo=s->speed_up && s->music_cue && s->music_cue!=ONE_UP?
            bank->music_tempo[s->music_cue-0x81]:s->tempo;
        unsigned wait=s->tempo_accumulator+tempo;
        s->tempo_accumulator=(uint8_t)wait;
        if(wait>255)
            for(unsigned ch=0;ch<S2_SEQUENCE_TRACKS;++ch)++s->music[ch].remaining;
        for(unsigned ch=0;ch<S2_SEQUENCE_TRACKS;++ch)
            if(!update_track(s,bank,&s->music[ch]))return 0;
        if(!ticks)for(unsigned ch=0;ch<S2_TONE_CHANNELS;++ch)
            if(!update_track(s,bank,&s->effect[ch]))return 0;
        if(s->restore_pending || (s->music_cue==ONE_UP && !s2_sequence_playing(s))) {
            s->restore_pending=0;
            restore_music(s);
        }
        if(s->fade && ++s->fade_clock==3) {
            s->fade_clock=0;
            if(++s->fade==64) {stop_music(s);s->fade=0;s->pcm_id=0;}
            for(unsigned ch=0;ch<S2_SEQUENCE_TRACKS;++ch)s->music[ch].dirty|=DIRTY_VOLUME;
        }
        if(s->fade_in) {
            --s->fade_in;
            for(unsigned ch=0;ch<S2_SEQUENCE_TRACKS;++ch)s->music[ch].dirty|=DIRTY_VOLUME;
        }
        output_tracks(s,bank,out);
    }
    render_pcm(s,bank,hz,span,out);
    return 1;
}
