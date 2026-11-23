#include "sonic2_sound_assets.h"
#include "sha256.h"
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
    free(assets);
    puts("Sonic 2 sound bank: immutable ROM, 31 music cues, 81 SFX, 13 envelopes and 17 drum pitches passed");
    return 0;
}
