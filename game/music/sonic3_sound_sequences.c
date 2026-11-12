/* Reused ROM music decoder from Sonic3AndKnucklesRecomp 70d69e6. */
#include "sonic3_sound_assets.h"
#include "sonic3_sound_legacy.h"
#include "sonic3_sound_themes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { BASE=0x2A0000, MAX_NODES=16384, KINDS=4 };
typedef struct Node {
    uint16_t pc,target,output;
    uint8_t kind,length,encoded;
} Node;
typedef struct Convert {
    S3SoundSource source;
    const uint8_t *rom;
    uint8_t *bank;
    size_t capacity,cursor;
    uint32_t input_bank;
    int bad,count,voices;
    unsigned psg_base,mod_base;
    const uint8_t *dac_map;
    uint8_t *drums;
    uint32_t last_pc,last_op;
    int map[0x8000*KINDS];
    Node nodes[MAX_NODES];
    S3SoundAssetStats stats;
} Convert;
static unsigned rb(Convert *c,uint32_t p)
{if(p>=c->source.rom_size){c->bad=1;return 0;}return c->rom[p];}
static unsigned rw(Convert *c,uint32_t p){return rb(c,p)|(rb(c,p+1)<<8);}
static unsigned zb(Convert *c,uint32_t p)
{if(p>=sizeof c->source.z80){c->bad=1;return 0;}return c->source.z80[p];}
static unsigned zw(Convert *c,uint32_t p){return zb(c,p)|(zb(c,p+1)<<8);}
static void ww(Convert *c,size_t p,unsigned v)
{if(p>c->capacity || c->capacity-p<2){c->bad=1;return;}c->bank[p]=(uint8_t)(v>>8);c->bank[p+1]=(uint8_t)v;}
static void wl(Convert *c,size_t p,uint32_t v){ww(c,p,v>>16);ww(c,p+2,v);}
static int table(Convert *c,uint32_t address,size_t bytes)
{return address>=BASE && address-BASE<=c->cursor && bytes<=c->cursor-(address-BASE);}
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
    return id?id+c->psg_base:0;
}
static unsigned mod_id(Convert *c,unsigned id)
{
    unsigned env=id&63u;
    if(!(id&0xC0) && env){
        if(env>c->source.modulation_count){c->bad=1;return 0;}
        return env+c->mod_base;
    }
    return id;
}
static int convert_song(Convert *c,uint32_t input,size_t dest,int music)
{
    unsigned roots[10],kinds[10],headers[10],root_count=0,header;
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
        if(n->kind==1 && op>=0x81 && op<0xE0){
            if(c->drums)c->drums[op&127]=1;
            unsigned banks=c->source.standalone_sonic3?0xDC:0xD6;
            if(c->source.prototype ? op>0x9B : op>0xC4 || ((uint32_t)c->source.z80[banks+(op&0x7F)]<<15)>=c->source.rom_size){c->bad=1;break;}
        }
        if(op>=0xE0){
            length=1+parameter_length(op);encoded=length+1;
            if(op==0xE7)encoded=1;
            else if(op==0xE9)encoded=0; /* Clone queues own spindash pitch */
            else if(op==0xE2)encoded=rb(c,pc+1)==0xFF?2:0;
            else if(op==0xE5)encoded=n->kind==2?0:3;
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
        if(op<0xE0){
            c->bank[out]=(uint8_t)(n->kind==1 && op>=0x81 && c->dac_map?c->dac_map[op&127]:op+(c->source.prototype && n->kind==1 && op>=0x81?70:0));
            continue;
        }
        if(op==0xE7){c->bank[out]=0xFE;continue;}
        c->bank[out]=0xFF;c->bank[out+1]=commands[op-0xE0];
        if(op==0xF2 && n->kind==3)c->bank[out+1]=4;
        if(op==0xE2)continue;
        if(op==0xFF){c->bank[out+1]=0x0A;c->bank[out+2]=(uint8_t)rb(c,pc+2);continue;}
        if(op==0xE5){
            /* Retail discards the first parameter, then changes FM volume
             * using the second. PSG ignores both parameters entirely. */
            c->bank[out+1]=6;c->bank[out+2]=(uint8_t)rb(c,pc+2);continue;
        }
        for(unsigned j=1;j<n->length;j++)c->bank[out+j+1]=(uint8_t)rb(c,pc+j);
        if(op==0xF5)c->bank[out+2]=(uint8_t)psg_id(c,rb(c,pc+1));
        if(op==0xF4)c->bank[out+2]=(uint8_t)mod_id(c,rb(c,pc+1));
        if(op==0xEC){unsigned v=rb(c,pc+1);c->bank[out+2]=(uint8_t)(((v<<3)&0x7F)|(v&0x80));}
        if(n->target){
            int target=c->nodes[find_node(c,n->target,n->kind)].output;
            int relative=target-(n->output+n->encoded-(op==0xF8?0:1));
            if(relative<-32768 || relative>32767){c->bad=1;return 0;}
            ww(c,out+n->encoded-2,(unsigned)relative);
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
    c->stats.nodes+=(uint32_t)c->count;return (int)size;
}
static int envelopes(Convert *c,const S3SoundLayout *l)
{
    unsigned first=0x2000;
    for(unsigned i=0;i<c->source.volume_count;i++){unsigned ptr=zw(c,c->source.volume_pointers+i*2);if(ptr<first)first=ptr;}
    unsigned end=c->source.music_pointers;
    if(first>=end || end>0x2000 || end-first>c->capacity-c->cursor)return 0;
    memcpy(c->bank+c->cursor,c->source.z80+first,end-first);
    for(unsigned i=0;i<c->source.volume_count;i++){
        unsigned ptr=zw(c,c->source.volume_pointers+i*2);if(ptr<first || ptr>=end)return 0;
        wl(c,l->psg_index-BASE+(i+c->psg_base)*4,BASE+(uint32_t)c->cursor+ptr-first);
    }
    c->cursor+=end-first;
    for(unsigned i=0;i<c->source.modulation_count;i++){
        unsigned src=zw(c,c->source.modulation_pointers+i*2);
        uint8_t out[128];unsigned n=2;out[0]=0x84;out[1]=1;int complete=0;
        for(unsigned j=0;j<120;j++){
            unsigned value=zb(c,src+j);
            if(value==0x83){out[n++]=0x81;complete=1;break;}
            out[n++]=(uint8_t)value;
            if(value==0x82){unsigned target=zb(c,src+(++j));if(target>=j)return 0;out[n++]=(uint8_t)(target+2);complete=1;break;}
        }
        if(!complete || n>c->capacity-c->cursor)return 0;
        memcpy(c->bank+c->cursor,out,n);wl(c,l->modulation_index-BASE+(i+c->mod_base)*4,BASE+(uint32_t)c->cursor);c->cursor+=n;
    }
    return !c->bad;
}
int s3_sound_build_sequences(const uint8_t *rom,size_t size,const S3SoundLayout *l,
                             uint8_t *bank,size_t capacity,S3SoundAssetStats *stats,char *error,size_t error_size)
{
    if(!l || !bank || capacity>0x400000 || (capacity&1) || l->program_size>capacity || l->program_size<84){if(error && error_size)snprintf(error,error_size,"Invalid sound output layout");return 0;}
    uintptr_t a=(uintptr_t)rom,b=(uintptr_t)bank;
    if((b>=a && b-a<size) || (a>=b && a-b<capacity)){if(error && error_size)snprintf(error,error_size,"Sound output aliases ROM");return 0;}
    Convert *c=(Convert*)calloc(1,sizeof *c);if(!c)return 0;
    c->rom=rom;c->bank=bank;c->capacity=capacity;c->cursor=l->program_size;
    int ok=s3_sound_read_source(rom,size,&c->source,error,error_size);
    if(ok)ok=table(c,l->music_index,51*4) && table(c,l->sound_index,169*4) && table(c,l->psg_index,51*4) && table(c,l->modulation_index,8*4);
    c->cursor=(c->cursor+1)&~(size_t)1;
    unsigned failed_id=0;int music=1;
    for(unsigned i=1;ok && i<=c->source.music_count;i++){
        uint32_t source=s3_sound_music_source(&c->source,rom,i);if(!source)continue;
        failed_id=i;int bytes=convert_song(c,source,c->cursor,1);if(bytes<=0){ok=0;break;}
        /* Clone's metadata uses tempo 1 for speed shoes, and keeps the
         * drowning countdown at PAL speed when running on PAL hardware. */
        wl(c,l->music_index-BASE+(i-1)*4,0x01000000u|(BASE+(uint32_t)c->cursor)|(i==0x31?1u:0u));c->cursor=((c->cursor+(unsigned)bytes)+1)&~(size_t)1;++c->stats.music;
    }
    if(ok)music=0;
    for(unsigned i=0x33;ok && i<=0xDB;i++){
        uint32_t source=s3_sound_sfx_source(&c->source,rom,i);failed_id=i;
        int bytes=convert_song(c,source,c->cursor,0);if(bytes<=0){ok=0;break;}
        wl(c,l->sound_index-BASE+(i-0x33)*4,BASE+(uint32_t)c->cursor);c->cursor=((c->cursor+(unsigned)bytes)+1)&~(size_t)1;++c->stats.sfx;
    }
    if(ok)ok=envelopes(c,l);
    if(c->bad)ok=0;
    if(ok){c->stats.bytes=(uint32_t)c->cursor;if(stats)*stats=c->stats;if(error && error_size)*error=0;}
    else if(error && error_size)snprintf(error,error_size,"Unable to convert %s $%02X at $%06X, command $%02X",music?"music":"SFX",failed_id,c->last_pc,c->last_op);
    free(c);return ok;
}
int s3_sound_build_prototype_music(const uint8_t *rom,size_t size,const S3SoundLayout *l,
                                   uint8_t *bank,size_t capacity,uint32_t *used,int standalone_s3,
                                   char *error,size_t error_size)
{
    if(!l || !bank || !used || capacity>0x400000 || *used>capacity ||
       *used<l->program_size || l->program_size<84)return 0;
    uintptr_t a=(uintptr_t)rom,b=(uintptr_t)bank;
    if((b>=a && b-a<size) || (a>=b && a-b<capacity))return 0;
    Convert *c=(Convert*)calloc(1,sizeof *c);if(!c)return 0;
    c->rom=rom;c->bank=bank;c->capacity=capacity;c->cursor=*used;
    c->psg_base=64;c->mod_base=8;
    int ok=s3_sound_read_prototype(rom,size,&c->source,error,error_size);
    if(ok)ok=table(c,l->music_index,51*4) && table(c,l->psg_index,103*4) && table(c,l->modulation_index,18*4);
    unsigned failed_id=0;
    for(unsigned i=1;ok && i<=51;i++){
        /* The prototype's $2E is Super, while retail $2E is a miniboss.
         * Standalone S3 uses the spare $32 entry for its separate Super
         * override. S&K uses $32 for Ending and native $DC (entry $33) for
         * Credits; its Ending has no prototype counterpart. */
        unsigned donor=i==0x32&&standalone_s3?0x2E:i==0x33?0x26:i==0x2E?0x18:i;
        if(standalone_s3 && i>0x32)continue;
        uint32_t source=s3_sound_music_source(&c->source,rom,donor);
        if(!source)continue;
        failed_id=i;
        c->cursor=(c->cursor+1)&~(size_t)1;
        int bytes=convert_song(c,source,c->cursor,1);
        if(bytes<=0){ok=0;break;}
        wl(c,l->music_index-BASE+(i-1)*4,0x01000000u|(BASE+(uint32_t)c->cursor)|(i==0x31?1u:0u));
        c->cursor+=bytes;
    }
    if(ok)ok=envelopes(c,l);
    if(c->bad)ok=0;
    if(ok){*used=(uint32_t)((c->cursor+1)&~(size_t)1);if(error && error_size)*error=0;}
    else if(error && error_size && (!*error || failed_id))
        snprintf(error,error_size,"Unable to convert prototype music $%02X at $%06X, command $%02X",failed_id,c->last_pc,c->last_op);
    free(c);return ok;
}

static uint32_t read32(const uint8_t *p)
{return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];}
static void write32(uint8_t *p,uint32_t n)
{p[0]=(uint8_t)(n>>24);p[1]=(uint8_t)(n>>16);p[2]=(uint8_t)(n>>8);p[3]=(uint8_t)n;}
static unsigned append_sample(uint8_t *bank,size_t cap,uint32_t *used,
                              const S3SoundPCMLayout *l,unsigned *next,
                              const uint8_t *bytes,size_t length,unsigned rate,unsigned flags)
{
    if(!length||rate<1||rate>255)return 0;
    for(unsigned i=0;i<*next;i++){
        const uint8_t *d=bank+l->sample_table-BASE+i*10;
        uint32_t start=read32(d+2),end=read32(d+6);
        if(d[0]!=flags||d[1]!=rate||start<BASE||end<start||end-BASE>*used||end-start!=length)continue;
        if(!memcmp(bank+start-BASE,bytes,length))return i+0x81;
    }
    if(*next>=l->sample_count||*next>=126||*used>cap||length>cap-*used)return 0;
    uint8_t *d=bank+l->sample_table-BASE+(*next)*10;memset(d,0,10);d[0]=(uint8_t)flags;d[1]=(uint8_t)rate;
    write32(d+2,BASE+*used);memcpy(bank+*used,bytes,length);*used+=(uint32_t)length;write32(d+6,BASE+*used);
    unsigned slot=(*next)++;
    return slot+0x81;
}
typedef struct PreparedMusicDonor {
    const uint8_t *rom;size_t size;
    uint8_t *samples,*bank;uint32_t sample_used;
    S3SoundSource source;
    uint8_t map[128];int source_valid,envelopes_ready;
} PreparedMusicDonor;
void s3_sound_music_cache_free(S3SoundMusicCache *cache)
{
    if(!cache)return;
    for(unsigned d=0;d<S3_ROM_DONOR_COUNT;d++){
        PreparedMusicDonor *p=cache->donors[d];if(p){free(p->samples);free(p);cache->donors[d]=NULL;}
    }
}
int s3_sound_build_track(const uint8_t *rom,size_t size,unsigned donor,unsigned song,unsigned target,
                         const S3SoundLayout *l,const S3SoundPCMLayout *pcm,const uint8_t *program,
                         uint8_t *bank,size_t cap,uint32_t *used,unsigned *next_sample,
                         S3SoundMusicCache *cache,
                         char *error,size_t error_size)
{
    if(!rom||!l||!pcm||!program||!bank||!used||!next_sample||!cache||donor>=S3_ROM_DONOR_COUNT||
       !target||target>51||*used<l->program_size||*used>cap||cap>0x400000||
       l->psg_index<BASE||l->psg_index-BASE+256*4>l->program_size||pcm->sample_count<126)return 0;
    uint32_t dest=(*used+1)&~1u;if(dest>cap||cap-dest<0x8000)return 0;
    /* Retail PSG envelope $26 differs between Sonic 3 and S&K. Keep their
     * tables separate when several events use different cartridges. */
    static const unsigned bases[]={103,112,125,138,64,151,202};
    unsigned psg_base=bases[donor];uint8_t drums[128]={0},map[128]={0};int bytes=0,ok=0;
    Convert *c=NULL;S3LegacySource *legacy=NULL;
    PreparedMusicDonor *prepared=cache->donors[donor];
    if(!prepared){prepared=calloc(1,sizeof *prepared);if(!prepared)return 0;cache->donors[donor]=prepared;prepared->rom=rom;prepared->size=size;prepared->bank=bank;}
    if(prepared->rom!=rom||prepared->size!=size||prepared->bank!=bank)return 0;
    if(donor<=S3_DONOR_WAI){
        legacy=malloc(sizeof *legacy);if(!legacy)goto done;
        if(!s3_legacy_music_source(rom,size,donor,song,legacy))goto done;
        bytes=s3_legacy_song(legacy,bank+dest,cap-dest,psg_base,NULL,drums,error,error_size);
    }else{
        c=calloc(1,sizeof *c);if(!c)goto done;
        c->rom=rom;c->bank=bank;c->capacity=cap;c->cursor=dest;c->psg_base=psg_base;c->mod_base=donor==S3_DONOR_PROTO?8:18;c->drums=drums;
        if(!prepared->source_valid){
            prepared->source_valid=donor==S3_DONOR_PROTO?s3_sound_read_prototype(rom,size,&prepared->source,error,error_size):s3_sound_read_source(rom,size,&prepared->source,error,error_size);
            if(!prepared->source_valid)goto done;
        }
        c->source=prepared->source;
        unsigned src=s3_sound_music_source(&c->source,rom,song);if(!src)goto done;
        bytes=convert_song(c,src,dest,1);
    }
    if(bytes<=0)goto done;*used=dest+(uint32_t)bytes;
    for(unsigned i=1;i<128;i++)if(drums[i]){
        if(prepared->map[i]){map[i]=prepared->map[i];continue;}
        if(!prepared->samples){
            prepared->samples=calloc(1,0x400000);if(!prepared->samples)goto done;
            memcpy(prepared->samples,program,l->program_size);prepared->sample_used=l->program_size;
            if(!legacy&&!(donor==S3_DONOR_PROTO?s3_sound_build_prototype_pcm(rom,size,pcm,prepared->samples,0x400000,&prepared->sample_used,error,error_size):
                                       s3_sound_build_pcm(rom,size,pcm,prepared->samples,0x400000,&prepared->sample_used,error,error_size)))goto done;
        }
        unsigned rate,length,flags=2;const uint8_t *wave;
        if(legacy){
            int n=s3_legacy_sample(legacy,i+0x80,prepared->samples,0x400000,&rate);if(n<=0)goto done;length=(unsigned)n;wave=prepared->samples;
        }else{
            unsigned slot=i-1+(donor==S3_DONOR_PROTO?70:0);if(slot>=pcm->sample_count)goto done;
            const uint8_t *d=prepared->samples+pcm->sample_table-BASE+slot*10;uint32_t start=read32(d+2),end=read32(d+6);
            if((d[0]&15)!=2||start<BASE||end<start||end-BASE>prepared->sample_used)goto done;
            flags=d[0];rate=d[1];length=end-start;wave=prepared->samples+start-BASE;
        }
        unsigned id=append_sample(bank,cap,used,pcm,next_sample,wave,length,rate,flags);if(!id)goto done;map[i]=prepared->map[i]=(uint8_t)id;
    }
    if(legacy){
        bytes=s3_legacy_song(legacy,bank+dest,cap-dest,psg_base,map,drums,error,error_size);if(bytes<=0)goto done;
        if(!prepared->envelopes_ready)for(unsigned env=1;env<=legacy->env_count;env++){
            uint8_t values[256];int n=s3_legacy_envelope(legacy,env,values);if(n<=0||(size_t)n>cap-*used)goto done;
            memcpy(bank+*used,values,(size_t)n);write32(bank+l->psg_index-BASE+(psg_base+env-1)*4,BASE+*used);*used+=(unsigned)n;
        }
    }else{
        c->dac_map=map;c->drums=NULL;
        bytes=convert_song(c,s3_sound_music_source(&c->source,rom,song),dest,1);if(bytes<=0)goto done;
        if(!prepared->envelopes_ready){c->cursor=*used;if(!envelopes(c,l))goto done;*used=(uint32_t)c->cursor;}
    }
    prepared->envelopes_ready=1;
    write32(bank+l->music_index-BASE+(target-1)*4,((legacy?legacy->fast_tempo:1u)<<24)|(BASE+dest)|((target==0x1F)||(legacy&&legacy->force_pal)));*used=(*used+1)&~1u;ok=1;
done:
    if(!ok&&error&&error_size&&!*error)snprintf(error,error_size,"Unable to prepare the selected event theme");
    free(legacy);free(c);return ok;
}
int s3_sound_build_event(const uint8_t *rom,size_t size,unsigned donor,unsigned theme,unsigned target,
                         const S3SoundLayout *l,const S3SoundPCMLayout *pcm,const uint8_t *program,
                         uint8_t *bank,size_t cap,uint32_t *used,unsigned *next_sample,char *error,size_t error_size)
{
    static const unsigned ids[3][S3_THEME_COUNT]={{7,14,15,13,6},{23,26,27,25,22},{0x2A,0x27,0x28,0x29,0x2C}};
    if(theme>=S3_THEME_COUNT||donor>=S3_ROM_DONOR_COUNT)return 0;
    S3SoundMusicCache cache={0};
    int ok=s3_sound_build_track(rom,size,donor,ids[donor==S3_DONOR_S1?0:donor<=S3_DONOR_WAI?1:2][theme],target,l,pcm,program,bank,cap,used,next_sample,&cache,error,error_size);
    s3_sound_music_cache_free(&cache);return ok;
}
