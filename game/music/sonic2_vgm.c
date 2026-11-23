#include "sonic2_vgm.h"
#include "sonic2_state_io.h"
#include "app_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void s2_vgm_fm_init(void);
void s2_vgm_fm_set_output_mask(unsigned mask);
void s2_vgm_fm_advance(uint32_t cycles);
void s2_vgm_fm_write(uint8_t port, uint8_t value);
size_t s2_vgm_fm_render_pair(int16_t *filtered, int16_t *raw, size_t samples);
size_t s2_vgm_fm_rb_save(void *out, size_t size);
int s2_vgm_fm_rb_load(const void *data, size_t size);
void s2_vgm_psg_init(void);
void s2_vgm_psg_set_output_mask(unsigned mask);
void s2_vgm_psg_advance(uint32_t cycles);
void s2_vgm_psg_write(uint8_t value);
size_t s2_vgm_psg_render_pair(int16_t *filtered, int16_t *raw, size_t samples);
size_t s2_vgm_psg_rb_save(void *out, size_t size);
int s2_vgm_psg_rb_load(const void *data, size_t size);

enum {
    MASTER_RATE = 53693175, VGM_RATE = 44100, TICK = 895780,
    FM_PERIOD = 1008, PSG_PERIOD = 240,
    FM_SAMPLES = (TICK + FM_PERIOD - 1) / FM_PERIOD,
    PSG_SAMPLES = (TICK + PSG_PERIOD - 1) / PSG_PERIOD,
    CHIP_CAPACITY = 8192
};

struct S2Vgm {
    uint8_t *commands, *boundaries, *bank;
    uint32_t start, end, repeat, bank_size;
};

typedef struct Transport {
    uint64_t master, next_sample;
    uint32_t track, cursor, seek, active;
    /* At a tick boundary the native buffers have been drained. These are
     * the previous samples until the next chip sample becomes available. */
    int16_t fm[2], raw_fm[2], psg, raw_psg;
} Transport;

typedef struct Checkpoint {
    Transport transport;
    uint8_t fm[CHIP_CAPACITY], psg[CHIP_CAPACITY];
} Checkpoint;

static Transport current;
static Checkpoint powered_on, suspended;
static size_t fm_size, psg_size;
static uint32_t fm_payload_size;
static int has_tracks;
static struct {
    unsigned valid, duration, fm_phase, psg_phase;
    unsigned fm_count, psg_count;
    int16_t fm[(FM_SAMPLES+1)*2], raw_fm[(FM_SAMPLES+1)*2];
    int16_t psg[PSG_SAMPLES+1], raw_psg[PSG_SAMPLES+1];
} frame;

static uint32_t le32(const uint8_t *p)
{
    return p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24;
}

static unsigned operand_count(unsigned op)
{
    switch(op) {
    case 0x4f: case 0x50: return 1;
    case 0x52: case 0x53: case 0x61: return 2;
    case 0xe0: return 4;
    default: return 0;
    }
}

/* Packs are validated by the importer before transport construction. Mark
 * command starts here so restoring a snapshot never has to scan the file. */
S2Vgm *s2_vgm_open(const uint8_t *data,size_t bytes,int looping)
{
    if(bytes<=0x40 || bytes>UINT32_MAX)return NULL;
    S2Vgm *track=calloc(1,sizeof *track);
    if(!track)return NULL;
    track->end=le32(data+4)+4;
    track->start=le32(data+8)>=0x150 && le32(data+0x34)?le32(data+0x34)+0x34:0x40;
    track->repeat=looping && le32(data+0x1c)?le32(data+0x1c)+0x1c:0;
    if(track->end>bytes || track->start>=track->end ||
       (track->repeat && (track->repeat<track->start || track->repeat>=track->end)))goto fail;
    track->commands=malloc(track->end);
    track->boundaries=calloc(((size_t)track->end+7)/8,1);
    if(!track->commands || !track->boundaries)goto fail;
    memcpy(track->commands,data,track->end);
    for(uint32_t pc=track->start;pc<track->end;) {
        track->boundaries[pc/8]|=(uint8_t)(1u<<(pc&7));
        unsigned op=data[pc++];
        if(op==0x66)break;
        if(op==0x67) {
            if(track->end-pc<6 || data[pc]!=0x66 || data[pc+1])goto fail;
            uint32_t length=le32(data+pc+2);pc+=6;
            if(length>track->end-pc)goto fail;
            if(length) {
                uint8_t *bank=realloc(track->bank,(size_t)track->bank_size+length);
                if(!bank)goto fail;
                memcpy(bank+track->bank_size,data+pc,length);
                track->bank=bank;track->bank_size+=length;
            }
            pc+=length;
        } else {
            unsigned length=operand_count(op);
            if(length>track->end-pc)goto fail;
            pc+=length;
        }
    }
    if(!fm_size) {
        s2_vgm_fm_init();s2_vgm_psg_init();
        fm_size=s2_vgm_fm_rb_save(NULL,0);
        psg_size=s2_vgm_psg_rb_save(NULL,0);
        if(!fm_size || !psg_size || fm_size>CHIP_CAPACITY || psg_size>CHIP_CAPACITY) {
            fm_size=psg_size=0;
            goto fail;
        }
        if(s2_vgm_fm_rb_save(powered_on.fm,fm_size)!=fm_size ||
           s2_vgm_psg_rb_save(powered_on.psg,psg_size)!=psg_size) {
            fm_size=psg_size=0;
            goto fail;
        }
        memcpy(&fm_payload_size,powered_on.fm,sizeof fm_payload_size);
    }
    has_tracks=1;
    return track;
fail:
    s2_vgm_close(track);
    return NULL;
}

void s2_vgm_close(S2Vgm *track)
{
    if(!track)return;
    free(track->commands);
    free(track->boundaries);
    free(track->bank);
    free(track);
}

void s2_vgm_stop(void)
{
    current.active=0;
    frame.valid=0;
}

void s2_vgm_discard_suspended(void)
{
    memset(&suspended,0,sizeof suspended);
}

static int save_chips(Checkpoint *checkpoint)
{
    checkpoint->transport=current;
    return s2_vgm_fm_rb_save(checkpoint->fm,fm_size)==fm_size
        && s2_vgm_psg_rb_save(checkpoint->psg,psg_size)==psg_size;
}

static int load_chips(const Checkpoint *checkpoint)
{
    uint32_t size;
    memcpy(&size,checkpoint->fm,sizeof size);
    return size==fm_payload_size
        && s2_vgm_fm_rb_load(checkpoint->fm,fm_size)
        && s2_vgm_psg_rb_load(checkpoint->psg,psg_size);
}

void s2_vgm_reset(void)
{
    memset(&current,0,sizeof current);
    s2_vgm_discard_suspended();
    if(fm_size)load_chips(&powered_on);
    has_tracks=0;
    frame.valid=0;
}

void s2_vgm_start(unsigned track)
{
    /* The chip's warm reset leaves the DAC latch and internal counters
     * intact. A track starting at sample zero needs the power-on image. */
    memset(&current,0,sizeof current);
    current.track=track;
    current.active=(uint32_t)load_chips(&powered_on);
    frame.valid=0;
}

void s2_vgm_suspend(void)
{
    if(!save_chips(&suspended))suspended.transport.active=0;
}

void s2_vgm_resume(void)
{
    if(suspended.transport.active && load_chips(&suspended))current=suspended.transport;
    s2_vgm_discard_suspended();
    frame.valid=0;
}

static void advance(uint32_t cycles)
{
    s2_vgm_fm_advance(cycles);
    s2_vgm_psg_advance(cycles);
    current.master+=cycles;
}

static void write_fm(unsigned port,unsigned address,unsigned value)
{
    s2_vgm_fm_write((uint8_t)port,(uint8_t)address);
    s2_vgm_fm_write((uint8_t)(port+1),(uint8_t)value);
}

static void dispatch(const S2Vgm *track)
{
    const uint8_t *p=track->commands+current.cursor;
    unsigned op=*p++;
    current.cursor+=1+operand_count(op);
    switch(op) {
    case 0x50: s2_vgm_psg_write(*p);break;
    case 0x52: case 0x53: write_fm((op-0x52)*2,p[0],p[1]);break;
    case 0x61: current.next_sample+=p[0]|(unsigned)p[1]<<8;break;
    case 0x62: current.next_sample+=735;break;
    case 0x63: current.next_sample+=882;break;
    case 0x66:
        if(track->repeat)current.cursor=track->repeat;
        else current.active=0;
        break;
    case 0x67: current.cursor+=6+le32(p+2);break;
    case 0xe0: current.seek=le32(p);break;
    default:
        if(op>=0x70 && op<=0x7f)current.next_sample+=(op&15)+1;
        if(op>=0x80 && op<=0x8f) {
            if(current.seek>=track->bank_size) {
                fprintf(stderr,"[music] VGM DAC bank exhausted at the loop boundary\n");
                current.active=0;
                break;
            }
            write_fm(0,0x2a,track->bank[current.seek++]);
            current.next_sample+=op&15;
        }
        break;
    }
}

void s2_vgm_frame(const S2Vgm *track,unsigned mask)
{
    uint64_t start=current.master,target=start+TICK;
    frame.valid=1;
    frame.duration=TICK;
    frame.fm_phase=(unsigned)(start%FM_PERIOD);
    frame.psg_phase=(unsigned)(start%PSG_PERIOD);
    memcpy(frame.fm,current.fm,sizeof current.fm);
    memcpy(frame.raw_fm,current.raw_fm,sizeof current.raw_fm);
    frame.psg[0]=current.psg;
    frame.raw_psg[0]=current.raw_psg;
    s2_vgm_fm_set_output_mask(mask&127);
    s2_vgm_psg_set_output_mask(mask>>7);
    if(!current.cursor)current.cursor=track->start;
    while(current.active) {
        uint64_t step=current.next_sample*MASTER_RATE/VGM_RATE;
        if(step>=target)break;
        advance((uint32_t)(step-current.master));
        dispatch(track);
    }
    if(!current.active)frame.duration=(unsigned)(current.master-start);
    advance((uint32_t)(target-current.master));
    frame.fm_count=(unsigned)s2_vgm_fm_render_pair(frame.fm+2,frame.raw_fm+2,FM_SAMPLES);
    frame.psg_count=(unsigned)s2_vgm_psg_render_pair(frame.psg+1,frame.raw_psg+1,PSG_SAMPLES);
    memcpy(current.fm,frame.fm+frame.fm_count*2,sizeof current.fm);
    memcpy(current.raw_fm,frame.raw_fm+frame.fm_count*2,sizeof current.raw_fm);
    current.psg=frame.psg[frame.psg_count];
    current.raw_psg=frame.raw_psg[frame.psg_count];
}

static int16_t clip(int value)
{
    if(value>32767)return 32767;
    if(value<-32768)return -32768;
    return (int16_t)value;
}

void s2_vgm_mix(int16_t *output,size_t frames,double rate,unsigned volume)
{
    if(!frame.valid || rate<1 || rate>384000)return;
    const int16_t *fm=g_app_config.audio_filter_bypass?frame.raw_fm:frame.fm;
    const int16_t *psg=g_app_config.audio_filter_bypass?frame.raw_psg:frame.psg;
    uint64_t at=0,step=(uint64_t)(MASTER_RATE*65536.0/rate+0.5);
    for(size_t i=0;i<frames;i++,at+=step) {
        unsigned cycles=(unsigned)(at>>16);
        if(cycles>=frame.duration)break;
        unsigned f=(frame.fm_phase+cycles)/FM_PERIOD,p=(frame.psg_phase+cycles)/PSG_PERIOD;
        if(f>frame.fm_count)f=frame.fm_count;
        if(p>frame.psg_count)p=frame.psg_count;
        int tone=psg[p]/8;
        for(unsigned ch=0;ch<2;ch++) {
            int value=(int)((int64_t)(fm[f*2+ch]+tone)*volume/65536);
            output[i*2+ch]=clip(output[i*2+ch]+value);
        }
    }
}

static int valid(const Transport *clock,const S2Vgm *const *tracks,unsigned expected)
{
    if(!clock->active)return 1;
    if(clock->active!=1 || expected==~0u || clock->track!=expected)return 0;
    const S2Vgm *track=tracks[clock->track];
    if(clock->cursor && (clock->cursor>=track->end ||
        !(track->boundaries[clock->cursor/8]&(1u<<(clock->cursor&7)))))return 0;
    if(clock->seek>track->bank_size || clock->next_sample>UINT64_MAX/MASTER_RATE)return 0;
    return clock->master<=clock->next_sample*MASTER_RATE/VGM_RATE;
}

static void transfer(S2StateIO *io,Checkpoint *state)
{
    S2_STATE(io,state->transport);
    s2_state_bytes(io,state->fm,fm_size);
    s2_state_bytes(io,state->psg,psg_size);
}

void s2_vgm_state(S2StateIO *io,const S2Vgm *const *tracks,
                  unsigned active_track,unsigned suspended_track)
{
    if(!has_tracks)return;
    Checkpoint live={0};
    if(io->mode==0 && !io->data) {
        transfer(io,&live);transfer(io,&live);
        return;
    }
    if(!save_chips(&live)) {io->ok=0;return;}
    if(io->mode) {
        Checkpoint incoming={0},backup={0};
        S2StateIO read=*io;read.mode=2;
        transfer(&read,&incoming);
        transfer(&read,&backup);
        if(!read.ok || !valid(&incoming.transport,tracks,active_track) ||
           !valid(&backup.transport,tracks,suspended_track)) {io->ok=0;return;}
        int loaded=(!backup.transport.active || load_chips(&backup)) && load_chips(&incoming);
        if(!loaded || io->mode==1)load_chips(&live);
        if(!loaded){io->ok=0;return;}
        io->pos=read.pos;
        if(io->mode==2) {
            current=incoming.transport;
            suspended=backup;
            frame.valid=0;
        }
        return;
    }
    transfer(io,&live);
    transfer(io,&suspended);
}
