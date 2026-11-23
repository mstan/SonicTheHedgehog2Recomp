#include "sonic2_sound_assets.h"
#include "sha256.h"
#include "sonic2_sound_sequence.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned word(const uint8_t *p){return ((unsigned)p[0]<<8)|p[1];}
/* Walk executable track data, including called phrases and loop targets.
 * EHZ's PSG swells must leave its original one-tick global divisor intact.
 * Scanning raw bytes would mistake instrument data/relative pointers for
 * commands, so only reachable instruction starts are inspected. */
static void ehz_tempo_contract(const uint8_t *song,unsigned bytes)
{
    static const uint8_t command_size[]={3,3,3,2,2,3,3,2,3,3,3,3,3,3,6,2,2,3,2,3,4,6,4};
    unsigned pending[32768],count=0;uint8_t seen[S2_SOUND_MUSIC_STRIDE]={0};
    unsigned fm=song[2],psg=song[3],volume_changes=0;
    assert(song[6]==1);
    for(unsigned i=0;i<fm+psg;i++)pending[count++]=word(song+8+(i<fm?i*4:fm*4+(i-fm)*6));
    while(count) {
        unsigned pc=pending[--count];assert(pc<bytes);
        if(seen[pc])continue;seen[pc]=1;
        unsigned op=song[pc],length=1,command=0xFF;
        if(op==0xFF) {
            assert(pc+1<bytes);command=song[pc+1];
            assert(command<sizeof command_size || command==0x1F || command==0x20);
            length=command<sizeof command_size?command_size[command]:command==0x20?4:2;
            assert(length<=bytes-pc);
            if(command==0x0B)assert(song[pc+2]==1);
            if(command==0x0C)++volume_changes;
            if(command>=0x14 && command<=0x16) {
                int target=(int)pc+(int)length-(command==0x16?0:1)+(int16_t)word(song+pc+length-2);
                assert(target>=0 && target<(int)bytes);pending[count++]=(unsigned)target;
            }
        } else assert(op<0xE0 || op==0xFE);
        if(command!=3 && command!=4 && command!=7 && command!=0x10 && command!=0x14)pending[count++]=pc+length;
        assert(count<sizeof pending/sizeof *pending);
    }
    assert(volume_changes>0);
}
typedef struct ChipTrace {
    uint32_t hash, last_pcm_cycle;
    unsigned fm[2], psg, dac, key_on, dac_enabled;
} ChipTrace;

static void chip_write(void *context,uint32_t cycle,uint8_t chip,uint8_t reg,uint8_t value)
{
    ChipTrace *trace=context;
    assert(chip<=S2_CHIP_PSG && cycle<127841);
    trace->hash=(trace->hash^cycle)*16777619u;
    trace->hash=(trace->hash^((unsigned)chip<<16|reg<<8|value))*16777619u;
    if(chip==S2_CHIP_PSG)++trace->psg;
    else ++trace->fm[chip];
    if(chip!=S2_CHIP_FM1)return;
    if(reg==0x28 && (value&0xF0))++trace->key_on;
    if(reg==0x2B)trace->dac_enabled=value!=0;
    if(reg==0x2A) {
        assert(trace->dac_enabled && cycle>=trace->last_pcm_cycle);
        ++trace->dac;
        trace->last_pcm_cycle=cycle;
    }
}

static void frame(S2SoundSequence *sequence,const S2StockSound *bank,
                  uint8_t queue[5],ChipTrace *trace,unsigned tick)
{
    trace->last_pcm_cycle=0;
    if(!s2_sequence_frame(sequence,bank,queue,0,7670454,127841,chip_write,trace)) {
        fprintf(stderr,"Native sequencer failed at frame %u with cue $%02X channel %u offset $%04X\n",
                tick,sequence->fault_cue,sequence->fault_channel,sequence->fault_offset);
        exit(1);
    }
}

static void sequence_contract(const S2StockSound *bank)
{
    S2SoundSequence state;
    uint8_t queue[5];
    ChipTrace trace;
    for(unsigned id=0x81;id<=0xF0;++id) {
        memset(queue,0,sizeof queue);
        memset(&trace,0,sizeof trace);
        s2_sequence_reset(&state);
        queue[id<0xA0?0:1]=(uint8_t)id;
        for(unsigned tick=0;tick<(id<0xA0?1050u:320u);++tick)
            frame(&state,bank,queue,&trace,tick);
        assert(!queue[0] && !queue[1]);
        if(id==0x82)assert(trace.fm[0] && trace.fm[1] && trace.key_on && trace.psg && trace.dac);
        if(id==0xE5)assert(trace.psg);
    }

    memset(queue,0,sizeof queue);
    memset(&trace,0,sizeof trace);
    s2_sequence_reset(&state);
    queue[0]=0x82;queue[4]=0x81;
    frame(&state,bank,queue,&trace,0);
    assert(!queue[0] && queue[4]==0x81 && state.music_cue==0x82);
    queue[4]=0;
    for(unsigned tick=0;tick<173;++tick)frame(&state,bank,queue,&trace,tick);

    S2SoundSequence saved=state;
    queue[0]=0xFE;
    frame(&state,bank,queue,&trace,0);
    assert(state.paused && !memcmp(state.music,saved.music,sizeof state.music));
    ChipTrace muted=trace;
    frame(&state,bank,queue,&trace,1);
    assert(trace.hash==muted.hash && !memcmp(state.music,saved.music,sizeof state.music));
    queue[0]=0xFF;
    frame(&state,bank,queue,&trace,2);
    assert(!state.paused);
    saved=state;
    ChipTrace first={.dac_enabled=state.dac_enabled},second=first;
    for(unsigned tick=0;tick<87;++tick)frame(&state,bank,queue,&first,tick);
    S2SoundSequence end=state;
    state=saved;
    for(unsigned tick=0;tick<87;++tick)frame(&state,bank,queue,&second,tick);
    assert(!memcmp(&first,&second,sizeof first) && !memcmp(&end,&state,sizeof state));
    trace.dac_enabled=state.dac_enabled;
    queue[0]=0x98;
    frame(&state,bank,queue,&trace,0);
    assert(state.saved_cue==0x82 && state.music_cue==0x98);
    for(unsigned tick=0;tick<300;++tick)frame(&state,bank,queue,&trace,tick);
    assert(!state.saved_cue && state.music_cue==0x82);

    s2_sequence_reset(&state);
    memset(&trace,0,sizeof trace);
    queue[0]=0xFA;
    for(unsigned tick=0;tick<120;++tick)frame(&state,bank,queue,&trace,tick);
    assert(!state.pcm_id && !trace.dac_enabled && trace.dac==S2_SEGA_PCM_BYTES+1);
    puts("Native playback processed stock music and effect cues, timed DAC, pause, 1-UP and restored state");
}

int main(int argc,char **argv)
{
    enum { ROM_BYTES=0x100000 };
    assert(argc==2);
    FILE *file=fopen(argv[1],"rb");
    assert(file);
    uint8_t *rom=malloc(ROM_BYTES);
    assert(rom && fread(rom,1,ROM_BYTES,file)==ROM_BYTES);
    assert(fgetc(file)==EOF);
    fclose(file);
    uint8_t before[32],after[32];
    recompui_sha256_compute(rom,ROM_BYTES,before);

    char error[256];
    S2StockSound *assets=s2_sound_decode_stock(rom,ROM_BYTES,error,sizeof error);
    if(!assets)fprintf(stderr,"%s\n",error);
    assert(assets);
    assert(s2_sound_decode_stock(rom,ROM_BYTES-1,error,sizeof error)==NULL && *error);
    size_t sega_offset=0xF1E8C;
    assert(!memcmp(assets->sega,rom+sega_offset,sizeof assets->sega));
    recompui_sha256_compute(rom,ROM_BYTES,after);
    assert(!memcmp(before,after,sizeof before));
    free(rom);

    ehz_tempo_contract(assets->music[1],assets->music_bytes[1]);
    for(unsigned i=0;i<S2_MUSIC_CUES;++i) {
        const uint8_t *song=assets->music[i];
        unsigned length=assets->music_bytes[i];
        assert(length && length<=sizeof assets->music[i]);
        unsigned fm=song[2],psg=song[3],header=8+fm*4+psg*6;
        assert(fm>=1 && fm<=7 && psg<=3);
        assert(word(song)>=header && word(song)<=length);
        for(unsigned t=0;t<fm+psg;++t) {
            unsigned offset=word(song+8+(t<fm?t*4:fm*4+(t-fm)*6));
            assert(offset>=header && offset<word(song));
        }
    }
    for(unsigned i=0;i<S2_SFX_CUES;++i) {
        const uint8_t *sfx=assets->sfx[i];
        unsigned length=assets->sfx_bytes[i];
        assert(length && length<=sizeof assets->sfx[i]);
        assert(sfx[3]>=1 && sfx[3]<=6);
        for(unsigned t=0;t<sfx[3];++t) {
            unsigned offset=word(sfx+6+t*6);
            assert(offset>=4+sfx[3]*6 && offset<word(sfx));
        }
    }
    /* E5 leaf collision clocks PSG noise using its zero-period sentinel. */
    const uint8_t *leaf=assets->sfx[0xE5-0xA0];
    assert(leaf[5]==0xC0 && leaf[8]==4);
    unsigned stream=word(leaf+6);
    assert(leaf[stream]==0xFF && leaf[stream+1]==0x13);
    assert(leaf[stream+3]==0xFF && leaf[stream+4]==0x11 && leaf[stream+5]==0xE7);
    assert(leaf[stream+6]==0xCF && ((leaf[stream+6]-0x81+leaf[8])&127)==0x52);

    for(unsigned i=0;i<S2_PSG_ENVELOPES;++i)
        assert(memchr(assets->envelope[i],0x81,sizeof assets->envelope[i]));
    unsigned banks=0;
    for(unsigned i=0;i<S2_PCM_SLOTS;++i) {
        const S2SoundSample *sample=&assets->samples[i];
        assert(sample->bank<S2_PCM_BANKS && sample->rate);
        assert(sample->bytes && sample->bytes<=sizeof assets->pcm[0]);
        banks|=1u<<sample->bank;
    }
    assert(banks==(1u<<S2_PCM_BANKS)-1);
    sequence_contract(assets);
    free(assets);
    puts("Sonic 2 sound bank: immutable ROM, 31 music cues, 81 SFX, 13 envelopes and 17 drum pitches passed");
    return 0;
}
