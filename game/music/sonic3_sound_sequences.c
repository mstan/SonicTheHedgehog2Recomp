/* Reused ROM music decoder from Sonic3AndKnucklesRecomp 70d69e6. */
#include "sonic3_sound_assets.h"
#include "sonic3_sound_legacy.h"
#include "sonic3_sound_themes.h"
#include "sonic2_sound_assets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { MAX_NODES=16384, KINDS=4 };
typedef struct Node {
    uint16_t pc,target,output;
    uint8_t kind,length,encoded;
} Node;
typedef struct Convert {
    S3SoundSource source;
    const uint8_t *rom;
    uint8_t *bank;
    size_t capacity;
    uint32_t input_bank;
    int bad,count,voices;
    uint8_t *drums;
    uint32_t last_pc,last_op;
    int map[0x8000*KINDS];
    Node nodes[MAX_NODES];
} Convert;
static unsigned rb(Convert *c,uint32_t p)
{if(p>=c->source.rom_size){c->bad=1;return 0;}return c->rom[p];}
static unsigned rw(Convert *c,uint32_t p){return rb(c,p)|(rb(c,p+1)<<8);}
static unsigned zb(Convert *c,uint32_t p)
{if(p>=sizeof c->source.z80){c->bad=1;return 0;}return c->source.z80[p];}
static unsigned zw(Convert *c,uint32_t p){return zb(c,p)|(zb(c,p+1)<<8);}
static void ww(Convert *c,size_t p,unsigned v)
{if(p>c->capacity || c->capacity-p<2){c->bad=1;return;}c->bank[p]=(uint8_t)(v>>8);c->bank[p+1]=(uint8_t)v;}
static int node(Convert *c,unsigned pc,unsigned kind)
{
    if(pc<0x8000 || pc>0xFFFF || kind>=KINDS || c->count>=MAX_NODES){c->bad=1;return 0;}
    unsigned key=(pc-0x8000)*KINDS+kind;
    if(c->map[key]>=0)return c->map[key];
    int n=c->count++;c->map[key]=n;c->nodes[n]=(Node){(uint16_t)pc,0,0,(uint8_t)kind,0,0};return n;
}
static int find_node(Convert *c,unsigned pc,unsigned kind)
{
    if(pc<0x8000 || pc>0xFFFF || kind>=KINDS){c->bad=1;return 0;}
    int n=c->map[(pc-0x8000)*KINDS+kind];if(n<0){c->bad=1;return 0;}return n;
}
static int order(const void *a,const void *b)
{
    const Node *x=(const Node*)a,*y=(const Node*)b;
    if(x->kind!=y->kind)return x->kind<y->kind?-1:1;
    return x->pc<y->pc?-1:x->pc>y->pc;
}
static unsigned parameter_length(unsigned op)
{
    static const uint8_t lengths[32]={1,1,1,0,1,2,1,0,1,0,1,3,1,1,2,1,4,2,0,1,1,1,2,4,2,0,0,1,2,1,4,0};
    return lengths[op-0xE0];
}
static unsigned psg_id(Convert *c,unsigned id)
{
    if(id>c->source.volume_count){c->bad=1;return 0;}
    return id;
}
static unsigned mod_id(Convert *c,unsigned id)
{
    if(c->source.prototype && id==0xFF)return S2_DONOR_MOD_ENVELOPES;
    unsigned env=id&63u;
    if(!(id&0xC0) && env){
        if(env>c->source.modulation_count){c->bad=1;return 0;}
        return env;
    }
    return id;
}
static int convert_song(Convert *c,uint32_t input,size_t dest,int music)
{
    unsigned roots[10],kinds[10],headers[10],root_count=0,header;
    unsigned competition_loop=0;
    int prototype_oneup=music && c->source.prototype &&
        input==s3_sound_music_source(&c->source,c->rom,0x2A);
    c->input_bank=input&~0x7FFFu;c->count=0;c->voices=-1;memset(c->map,0xFF,sizeof c->map);
    unsigned fm=music?rb(c,input+2):0,psg=music?rb(c,input+3):0;
    if(music){
        if(fm<1 || fm>7 || psg>3){c->bad=1;return 0;}
        header=8+fm*4+psg*6;
        if(dest>c->capacity || header>c->capacity-dest){c->bad=1;return 0;}
        memcpy(c->bank+dest,c->rom+input,4);c->bank[dest+4]=0;c->bank[dest+5]=1; /* native S3 PSG ties */
        c->bank[dest+6]=(uint8_t)rb(c,input+4);c->bank[dest+7]=(uint8_t)rb(c,input+5);
        for(unsigned i=0;i<fm+psg;i++){
            unsigned old=input+6+(i<fm?i*4:fm*4+(i-fm)*6),out=old-input+2;
            /* The prototype stops every 1-UP channel without a restore
             * command. FM5 echoes FM1 four ticks later and finishes last.
             * Keep its graph separate so its final stop can restore the
             * preceding music without cutting off that echo. */
            unsigned kind=i==0?1:i>=fm?2:prototype_oneup && i==fm-1?3:0;
            roots[root_count]=rw(c,old);kinds[root_count]=kind;headers[root_count++]=out;
            unsigned n=i<fm?4:6;memcpy(c->bank+dest+out,c->rom+old,n);
            if(i>=fm){
                c->bank[dest+out+3]<<=3;
                c->bank[dest+out+4]=(uint8_t)mod_id(c,rb(c,old+4));
                c->bank[dest+out+5]=(uint8_t)psg_id(c,rb(c,old+5));
            }
        }
        if(fm>1)c->voices=0;
        if(c->source.prototype && input==s3_sound_music_source(&c->source,c->rom,0x2D)){
            /* Competition Menu's shared FM2/FM5 loop skips the initial
             * voice, detune and modulation setup. Restore those settings at
             * the backward jump, preserving the first pass and its one-time
             * 96-tick rest. */
            static const uint8_t setup[]={0xEF,0x0D,0xE1,0x03,0xF0,0x0F,0x01,0x06,0x06,0x80,0x60};
            if(fm<6)return 0;
            uint32_t start=c->input_bank+(roots[2]&0x7FFFu);
            if(start>c->source.rom_size-sizeof setup || memcmp(c->rom+start,setup,sizeof setup))return 0;
            competition_loop=roots[2]+sizeof setup;
        }
    }else{
        unsigned tracks=rb(c,input+3);
        if(tracks<1 || tracks>6){c->bad=1;return 0;}
        header=4+tracks*6;
        if(dest>c->capacity || header>c->capacity-dest){c->bad=1;return 0;}
        memcpy(c->bank+dest,c->rom+input,header);
        for(unsigned i=0;i<tracks;i++){
            unsigned old=input+4+i*6,out=old-input,ch=rb(c,old+1),kind=ch&128?2:0;
            if(ch!=2 && ch!=4 && ch!=5 && ch!=0x80 && ch!=0xA0 && ch!=0xC0){c->bad=1;return 0;}
            roots[root_count]=rw(c,old+2);kinds[root_count]=kind;headers[root_count++]=out+2;
            if(kind==2)c->bank[dest+out+5]<<=3;else c->voices=0;
        }
    }
    for(unsigned i=0;i<root_count;i++)node(c,roots[i],kinds[i]);
    for(int i=0;i<c->count && !c->bad;i++){
        Node *n=&c->nodes[i];uint32_t pc=c->input_bank+(n->pc&0x7FFFu);unsigned op=rb(c,pc),length=1,encoded=1;
        c->last_pc=pc;c->last_op=op;
        unsigned drum=n->kind==1 && op>=0x81 && op<0xE0?op:op==0xEA?rb(c,pc+1):0;
        if(drum>=0x81){
            if(c->drums)c->drums[drum&127]=1;
            unsigned banks=c->source.standalone_sonic3?0xDC:0xD6;
            if(c->source.prototype ? drum>0x9B : drum>0xC4 || ((uint32_t)c->source.z80[banks+(drum&0x7F)]<<15)>=c->source.rom_size){c->bad=1;break;}
        }
        if(op>=0xE0){
            length=1+parameter_length(op);encoded=length+1;
            if(op==0xE7)encoded=1;
            else if(op==0xE9)encoded=0; /* Clone queues own spindash pitch */
            else if(op==0xE2)encoded=rb(c,pc+1)==0xFF?2:0;
            else if(op==0xE5)encoded=3;
            else if(op==0xE6&&n->kind==2)encoded=3;
            else if(op==0xFF){
                unsigned sub=rb(c,pc+1);
                if(sub==0){length=3;encoded=3;}
                else if(sub==7){length=2;encoded=0;} /* queue reset is internal */
                else {c->bad=1;break;}
            }else if(op==0xEB || op==0xF1 || op==0xFD || op==0xFE){c->bad=1;break;}
            if(op==0xEF){unsigned voice=rb(c,pc+1);if(voice>=128){c->bad=1;break;}if((int)voice>c->voices)c->voices=(int)voice;}
        }
        if(pc+length>c->input_bank+0x8000){c->bad=1;break;}
        n->length=(uint8_t)length;n->encoded=(uint8_t)encoded;
        /* The verified November FBZ1 FM5 stream has a stray nA0 between
         * nA6 and its duration at $BBD73. It adds twelve ticks every loop.
         * Omit only that source byte, as in the corrected retail stream. */
        if(c->source.prototype && pc==0xBBD73 && op==0x8A)n->encoded=0;
        if(op==0xF6 || op==0xF7 || op==0xF8 || op==0xFC){n->target=(uint16_t)rw(c,pc+length-2);node(c,n->target,n->kind);}
        if(n->kind==0 && op==0xF6 && n->target==competition_loop)n->encoded+=12;
        if(op!=0xE3 && op!=0xF2 && op!=0xF9 && op!=0xF6)node(c,n->pc+length,n->kind);
    }
    if(c->bad)return 0;
    qsort(c->nodes,(size_t)c->count,sizeof c->nodes[0],order);
    unsigned size=header;
    for(int i=0;i<c->count;i++){
        Node *n=&c->nodes[i];if(size>0x7FFF){c->bad=1;return 0;}n->output=(uint16_t)size;size+=n->encoded;c->map[(n->pc-0x8000)*KINDS+n->kind]=i;
    }
    unsigned voices=(unsigned)(c->voices+1);
    if(size+voices*25>0x7FFF || size+voices*25>c->capacity-dest){c->bad=1;return 0;}
    for(unsigned i=0;i<root_count;i++)ww(c,dest+headers[i],c->nodes[find_node(c,roots[i],kinds[i])].output);
    static const uint8_t commands[32]={0,1,4,0x18,0x1C,6,6,0,0x1D,0,0x19,0,0x0C,0x1B,0x20,0x0D,0x21,0,0x10,0x11,0x22,0x13,0x14,0x15,0x16,3,0x12,9,0x1E,0,0,0};
    for(int i=0;i<c->count && !c->bad;i++){
        Node *n=&c->nodes[i];uint32_t pc=c->input_bank+(n->pc&0x7FFFu);size_t out=dest+n->output;unsigned op=rb(c,pc);
        if(!n->encoded)continue;
        if(n->kind==0 && op==0xF6 && n->target==competition_loop){
            static const uint8_t setup[]={0xFF,0x0D,0x0D,0xFF,0x01,0x03,0xFF,0x21,0x0F,0x01,0x06,0x06};
            memcpy(c->bank+out,setup,sizeof setup);out+=sizeof setup;
        }
        if(op<0xE0){
            c->bank[out]=(uint8_t)op;
            continue;
        }
        if(op==0xE7){c->bank[out]=0xFE;continue;}
        if(n->kind==2&&(op==0xE5||op==0xE6)){
            /* Ignored FM volume commands still separate an implicit-duration
             * PSG note from the following duration-only event. Preserve that
             * boundary with a panning command, which is inert on PSG. */
            c->bank[out]=0xFF;c->bank[out+1]=0;c->bank[out+2]=0;continue;
        }
        c->bank[out]=0xFF;c->bank[out+1]=commands[op-0xE0];
        if(op==0xF2 && n->kind==3)c->bank[out+1]=4;
        if(op==0xE2)continue;
        if(op==0xFF){c->bank[out+1]=0x0A;c->bank[out+2]=(uint8_t)rb(c,pc+2);continue;}
        if(op==0xE5){
            /* Retail discards the first parameter, then changes FM volume
             * using the second. The PSG form was handled above. */
            c->bank[out+1]=6;c->bank[out+2]=(uint8_t)rb(c,pc+2);continue;
        }
        for(unsigned j=1;j<n->length;j++)c->bank[out+j+1]=(uint8_t)rb(c,pc+j);
        /* Retail Set Key encodes its transpose with a $40 bias; Clone
         * takes the decoded byte. Preserve the original key on all tracks. */
        if(op==0xED)c->bank[out+2]=(uint8_t)(rb(c,pc+1)-0x40u);
        if(op==0xF5)c->bank[out+2]=(uint8_t)psg_id(c,rb(c,pc+1));
        if(op==0xF4)c->bank[out+2]=(uint8_t)mod_id(c,rb(c,pc+1));
        if(op==0xEC){unsigned v=rb(c,pc+1);c->bank[out+2]=(uint8_t)(((v<<3)&0x7F)|(v&0x80));}
        if(n->target){
            int target=c->nodes[find_node(c,n->target,n->kind)].output;
            int relative=target-(n->output+n->encoded-(op==0xF8?0:1));
            if(relative<-32768 || relative>32767){c->bad=1;return 0;}
            ww(c,dest+n->output+n->encoded-2,(unsigned)relative);
        }
    }
    /* Retail uses operators 4/3/2/1 with TL last; Clone uses 4/2/3/1
     * with TL second. Preserve the original S3 carrier bits verbatim. */
    unsigned vp=rw(c,input);ww(c,dest,size);
    static const unsigned reorder[]={0,2,1,3};
    for(unsigned v=0;v<voices;v++){
        uint32_t addr=vp+v*25;
        if(vp>=0x8000)addr=c->input_bank+(addr&0x7FFFu);
        else if(addr+25>0x1C00){c->bad=1;return 0;}
        uint8_t voice[25];for(unsigned k=0;k<25;k++)voice[k]=(uint8_t)(vp<0x8000?zb(c,addr+k):rb(c,addr+k));
        c->bank[dest+size]=voice[0];
        for(unsigned k=0;k<4;k++){
            c->bank[dest+size+1+k]=voice[1+reorder[k]];
            c->bank[dest+size+5+k]=voice[21+reorder[k]];
            for(unsigned group=0;group<4;group++)c->bank[dest+size+9+group*4+k]=voice[5+group*4+reorder[k]];
        }
        size+=25;
    }
    return (int)size;
}
static int prepare_envelopes(const S3SoundSource *source,S2DonorSound *profile)
{
    unsigned end=source->music_pointers;
    if(source->volume_count>S2_DONOR_PSG_ENVELOPES || source->modulation_count>S2_DONOR_MOD_ENVELOPES ||
       end>sizeof source->z80)return 0;
    for(unsigned i=0;i<source->volume_count;++i) {
        const uint8_t *p=source->z80+source->volume_pointers+2*i;
        unsigned start=p[0]|(unsigned)p[1]<<8;
        S2SoundEnvelope *env=&profile->envelope[i];
        if(start>=end)return 0;
        unsigned bytes=end-start;
        if(bytes>sizeof env->data)bytes=sizeof env->data;
        memcpy(env->data,source->z80+start,bytes);
        env->bytes=(uint16_t)bytes;
    }
    for(unsigned i=0;i<source->modulation_count;++i) {
        const uint8_t *p=source->z80+source->modulation_pointers+2*i;
        unsigned start=p[0]|(unsigned)p[1]<<8;
        S2SoundEnvelope *env=&profile->modulation[i];
        for(unsigned j=0;j<sizeof env->data && start+j<sizeof source->z80;++j) {
            unsigned value=source->z80[start+j];
            env->data[j]=(uint8_t)value;
            if(value==0x83) {
                env->bytes=(uint16_t)(j+1);
                break;
            }
            if(value==0x82) {
                if(++j>=sizeof env->data || start+j>=sizeof source->z80)return 0;
                unsigned target=source->z80[start+j];
                /* Preserve the original retail envelope's observed wrap. */
                if(!source->prototype && i==3 && start==0x1338 && j==13 &&
                   target==0 && source->z80[13]==9)target=9;
                if(target>=j)return 0;
                env->data[j]=(uint8_t)target;
                env->bytes=(uint16_t)(j+1);
                break;
            }
        }
        if(!env->bytes)return 0;
    }
    if(source->prototype) {
        /* The prototype Game Over cue addresses envelope $FF. The original
         * driver reads past its pointer table and uses a fixed ROM-backed
         * curve. Decode that curve into a bounded slot for playback. */
        unsigned slot=source->modulation_pointers+2u*(0xFFu-1u);
        if(slot+1u>=sizeof source->z80)return 0;
        const uint8_t *p=source->z80+slot;
        unsigned start=p[0]|(unsigned)p[1]<<8;
        S2SoundEnvelope *env=&profile->modulation[S2_DONOR_MOD_ENVELOPES-1];
        for(unsigned j=0;j<sizeof env->data && start+j<sizeof source->z80;++j) {
            unsigned value=source->z80[start+j];
            env->data[j]=(uint8_t)value;
            if(value==0x81 || value==0x83) {
                env->bytes=(uint16_t)(j+1);
                break;
            }
            if(value==0x84) {
                if(++j>=sizeof env->data)return 0;
                /* The unfixed driver uses the curve index as the operand
                 * address for this command. Store the byte it actually reads. */
                env->data[j]=source->z80[j];
            }
        }
        if(!env->bytes)return 0;
    }
    return 1;
}

int s3_sound_prepare_cue(const uint8_t *rom,size_t size,unsigned donor,unsigned song,
                         S3SoundSource *source,S2DonorSound *profile,S2MusicCue *cue,
                         char *error,size_t error_size)
{
    if(!rom || !source || !profile || !cue || donor>=S3_ROM_DONOR_COUNT)return 0;
    enum { CUE_CAPACITY=0x8000 };
    uint8_t *data=malloc(CUE_CAPACITY);
    S3LegacySource *legacy=NULL;
    Convert *convert=NULL;
    uint8_t drums[128]={0};
    int ok=0,bytes=0;
    unsigned tempo=1,force_pal=0;
    if(!data)goto done;
    if(donor<=S3_DONOR_WAI || donor==S3_DONOR_SEP14) {
        legacy=malloc(sizeof *legacy);
        if(!legacy || !s3_legacy_music_source(rom,size,donor,song,legacy))goto done;
        bytes=s3_legacy_song(legacy,data,CUE_CAPACITY,0,NULL,drums,error,error_size);
        if(bytes<=0 || legacy->env_count>S2_DONOR_PSG_ENVELOPES)goto done;
        if(!profile->envelope[0].bytes)for(unsigned i=0;i<legacy->env_count;++i) {
            S2SoundEnvelope *env=&profile->envelope[i];
            int n=s3_legacy_envelope(legacy,i+1,env->data);
            if(n<=0)goto done;
            env->bytes=(uint16_t)n;
        }
        tempo=legacy->fast_tempo;
        force_pal=legacy->force_pal;
    } else {
        if(!source->rom_size) {
            int decoded=donor==S3_DONOR_PROTO?s3_sound_read_prototype(rom,size,source,error,error_size):
                         s3_sound_read_source(rom,size,source,error,error_size);
            if(!decoded || !prepare_envelopes(source,profile))goto done;
        }
        unsigned start=s3_sound_music_source(source,rom,song);
        if(!start)goto done;
        convert=calloc(1,sizeof *convert);
        if(!convert)goto done;
        convert->source=*source;
        convert->rom=rom;
        convert->bank=data;
        convert->capacity=CUE_CAPACITY;
        convert->drums=drums;
        bytes=convert_song(convert,start,0,1);
        if(bytes<=0)goto done;
    }

    for(unsigned i=1;i<128;++i)if(drums[i]) {
        if(i>S2_DONOR_PCM_SLOTS)goto done;
        S2SoundWave *wave=&profile->pcm[i-1];
        if(wave->data)continue;
        if(!legacy) {
            if(!s3_sound_decode_drum(rom,source,i|0x80,wave))goto done;
            continue;
        }
        uint8_t *samples=malloc(0x10000);
        unsigned rate=0;
        int count=samples?s3_legacy_sample(legacy,i|0x80,samples,0x10000,&rate):0;
        if(count<=0 || !rate) {free(samples);goto done;}
        *wave=(S2SoundWave){samples,(uint32_t)count,(rate*25100u+128u)/256u};
    }
    *cue=(S2MusicCue){data,profile,(uint16_t)bytes,(uint8_t)tempo,(uint8_t)force_pal};
    data=NULL;
    ok=1;
done:
    if(!ok && error && error_size && !*error)
        snprintf(error,error_size,"Unable to decode music cue %u from donor %u",song,donor);
    free(data);
    free(legacy);
    free(convert);
    return ok;
}
