#include "sonic2_sound_assets.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned be(const uint8_t *p){return ((unsigned)p[0]<<24)|((unsigned)p[1]<<16)|((unsigned)p[2]<<8)|p[3];}
static unsigned word(const uint8_t *p){return ((unsigned)p[0]<<8)|p[1];}
/* Walk executable track data, including called phrases and loop targets.
 * EHZ's PSG swells must leave its original one-tick global divisor intact.
 * Scanning raw bytes would mistake instrument data/relative pointers for
 * commands, so only reachable instruction starts are inspected. */
static void ehz_tempo_contract(const uint8_t *song)
{
    static const uint8_t command_size[]={3,3,3,2,2,3,3,2,3,3,3,3,3,3,6,2,2,3,2,3,4,6,4};
    unsigned pending[32768],count=0;uint8_t seen[S2_SOUND_MUSIC_STRIDE]={0};
    unsigned fm=song[2],psg=song[3],volume_changes=0;
    assert(song[6]==1);
    for(unsigned i=0;i<fm+psg;i++)pending[count++]=word(song+8+(i<fm?i*4:fm*4+(i-fm)*6));
    while(count) {
        unsigned pc=pending[--count];assert(pc<S2_SOUND_MUSIC_STRIDE);
        if(seen[pc])continue;seen[pc]=1;
        unsigned op=song[pc],length=1,command=0xFF;
        if(op==0xFF) {
            assert(pc+1<S2_SOUND_MUSIC_STRIDE);command=song[pc+1];
            assert(command<sizeof command_size || command==0x1F || command==0x20);
            length=command<sizeof command_size?command_size[command]:command==0x20?4:2;
            if(command==0x0B)assert(song[pc+2]==1);
            if(command==0x0C)++volume_changes;
            if(command>=0x14 && command<=0x16) {
                int target=(int)pc+(int)length-(command==0x16?0:1)+(int16_t)word(song+pc+length-2);
                assert(target>=0 && target<S2_SOUND_MUSIC_STRIDE);pending[count++]=(unsigned)target;
            }
        } else assert(op<0xE0 || op==0xFE);
        if(command!=3 && command!=4 && command!=7 && command!=0x10 && command!=0x14)pending[count++]=pc+length;
        assert(count<sizeof pending/sizeof *pending);
    }
    assert(volume_changes>0);
}
int main(int argc,char **argv)
{
    assert(argc==2);FILE *f=fopen(argv[1],"rb");assert(f);
    uint8_t *rom=malloc(0x100000),*memory=malloc(0x400000),*copy=malloc(0x400000);assert(rom&&memory&&copy);
    assert(fread(rom,1,0x100000,f)==0x100000);assert(fgetc(f)==EOF);fclose(f);
    memset(memory,0xA5,0x400000);memcpy(memory,rom,0x100000);
    S2SoundAssetLayout layout={0x101000,0x101100,0x101300,0x101400,0x101500};char error[256];
    int ok=s2_sound_build_assets(rom,0x100000,memory,0x400000,&layout,error,sizeof error);
    if(!ok)fprintf(stderr,"%s\n",error);assert(ok);
    assert(!memcmp(rom,memory,0x100000));
    ehz_tempo_contract(memory+S2_SOUND_MUSIC_BASE+S2_SOUND_MUSIC_STRIDE);
    for(unsigned i=0;i<31;i++) {
        unsigned p=be(memory+layout.music_index+i*4)&0xFFFFFE;
        assert(p==S2_SOUND_MUSIC_BASE+i*S2_SOUND_MUSIC_STRIDE);
        assert(memory[p+2]>=1 && memory[p+2]<=7 && memory[p+3]<=3);
    }
    for(unsigned i=0;i<81;i++) {
        unsigned p=be(memory+layout.sfx_index+i*4)&0xFFFFFF;
        assert(p==S2_SOUND_SFX_BASE+i*S2_SOUND_SFX_STRIDE);
        assert(memory[p+3]>=1 && memory[p+3]<=6);
    }
    for(unsigned i=0;i<17;i++) {
        const uint8_t *p=memory+layout.sample_table+i*10;
        assert(p[0]==0x12 && p[1]);unsigned start=be(p+2),end=be(p+6);
        assert(start>=S2_SOUND_PCM_BASE && start<end && end<=S2_SOUND_SEGA_BASE);
    }
    for(unsigned i=S2_SOUND_ASSETS_END;i<0x400000;i++)assert(memory[i]==0xA5);
    memcpy(copy,memory,0x400000);
    assert(s2_sound_build_assets(memory,0x100000,memory,0x400000,&layout,error,sizeof error));
    assert(!memcmp(copy,memory,0x400000));
    rom[17]^=1;
    assert(!s2_sound_build_assets(rom,0x100000,memory,0x400000,&layout,error,sizeof error));
    assert(*error && !memcmp(copy,memory,0x400000));
    layout.music_index=0xFFFF0;
    assert(!s2_sound_build_assets(memory,0x100000,memory,0x400000,&layout,error,sizeof error));
    puts("Sonic 2 ROM sound conversion: 31 music, 81 SFX, 17 drum pitches; aliasing/bounds/identity passed");
    free(copy);free(memory);free(rom);return 0;
}
