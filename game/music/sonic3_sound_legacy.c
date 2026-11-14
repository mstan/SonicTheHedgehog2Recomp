/* Reused ROM music decoder from Sonic3AndKnucklesRecomp 70d69e6. */
/* SMPS 68k (Sonic 1) and SMPS Z80 v2 (Sonic 2) event music. The 68k
 * conversion follows the qualified Sonic 1 adapter's command contract. */
#include "sonic3_sound_legacy.h"
#include "sonic3_sound_themes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned le(const uint8_t *p){return p[0]|((unsigned)p[1]<<8);}
static unsigned be(const uint8_t *p){return ((unsigned)p[0]<<8)|p[1];}
static uint32_t bl(const uint8_t *p){return ((uint32_t)be(p)<<16)|be(p+2);}
static void ww(uint8_t *p,unsigned v){p[0]=(uint8_t)(v>>8);p[1]=(uint8_t)v;}
static unsigned tempo(unsigned n){unsigned t=n?n:256;return (256-((((t-1)<<8)+(t>>1))/t))&255;}
/* Sonic 1 and Simon Wai delay once per countdown. Later Sonic 2 instead
 * advances on accumulator carry; Clone delays on carry, the opposite sense. */
static unsigned native_tempo(unsigned donor,unsigned n)
{return donor==S3_DONOR_S1||donor==S3_DONOR_WAI?tempo(n):(0u-n)&255;}
typedef struct Kos {const uint8_t *rom;size_t size,pos;unsigned bits,left,bad;} Kos;
static unsigned kb(Kos *k)
{
    unsigned bit=k->bits&1;k->bits>>=1;
    if(!--k->left){if(k->pos+2>k->size){k->bad=1;return 0;}k->bits=le(k->rom+k->pos);k->pos+=2;k->left=16;}
    return bit;
}
static unsigned unpack_s1(S3LegacySource *s)
{
    Kos k={s->rom,s->size,0x72E7E,le(s->rom+0x72E7C),16,0};unsigned n=0;
    for(unsigned guard=0;guard<65536&&!k.bad;guard++){
        if(kb(&k)){if(n>=sizeof s->z80||k.pos>=k.size)return 0;s->z80[n++]=s->rom[k.pos++];continue;}
        int offset;unsigned count;
        if(!kb(&k)){unsigned a=kb(&k);count=(a<<1)|kb(&k);count+=2;if(k.pos>=k.size)return 0;offset=(int)s->rom[k.pos++]-256;}
        else{
            if(k.pos+2>k.size)return 0;unsigned a=s->rom[k.pos++],b=s->rom[k.pos++];offset=(int16_t)(0xE000|((b&0xF8)<<5)|a);count=b&7;
            if(count)count+=2;else{if(k.pos>=k.size)return 0;count=s->rom[k.pos++];if(!count)return n;if(count==1)continue;++count;}
        }
        if(offset>=0||(unsigned)-offset>n||count>sizeof s->z80-n)return 0;
        while(count--){s->z80[n]=s->z80[(int)n+offset];++n;}
    }
    return 0;
}
static unsigned sax(const uint8_t *data,size_t size,size_t p,size_t bytes,uint8_t *out,size_t cap,unsigned base)
{
    if(p>size||bytes>size-p)return 0;size_t end=p+bytes,n=0;unsigned bits=0,mask=0;
    while(p<end){
        if(!mask){bits=data[p++];mask=1;if(p==end)break;}
        int literal=(bits&mask)!=0;mask=(mask<<1)&255;
        if(literal){if(n==cap)return 0;out[n++]=data[p++];}
        else{
            /* The native byte reader exits immediately when its input count
             * reaches zero, including a partially read final match. */
            if(end-p<2)return (unsigned)n;
            unsigned lo=data[p++],hi=data[p++],count=(hi&15)+3;
            int off=(int)((((hi&0xF0)<<4)|lo)+0x12);off=(off&0xFFF)|((int)(n+base)&~0xFFF);
            if(off>=(int)(n+base))off-=0x1000;
            if(count>cap-n)return 0;
            while(count--){out[n++]=off>=(int)base?out[off-(int)base]:0;++off;}
        }
    }
    return (unsigned)n;
}
int s3_legacy_music_source(const uint8_t *rom,size_t size,unsigned donor,unsigned song,S3LegacySource *s)
{
    if(!rom||!s||song>=(donor?31u:19u)||(donor>S3_DONOR_WAI&&donor!=S3_DONOR_SEP14)||size!=(donor?0x100000u:0x80000u))return 0;
    memset(s,0,sizeof *s);s->rom=rom;s->size=size;s->donor=donor;
    if(donor==S3_DONOR_S1){
        unsigned table=bl(rom+0x71998);if(table>size||4*19>size-table)return 0;
        s->source=bl(rom+table+4*song);s->bank=0x70000;s->low=0x70000;s->high=0x80000;
        s->env=bl(rom+0x719A4);if(s->env>size||9*4>size-s->env)return 0;
        s->env_count=9;s->zsize=unpack_s1(s);
        if(!s->zsize||s->source<s->low||s->source>s->high-64)return 0;
        unsigned speed=bl(rom+0x719A0);if(speed>size||8>size-speed)return 0;
        s->fast_tempo=tempo(rom[song<8?speed+song:s->source+5]);s->force_pal=song==15;return 1;
    }
    unsigned loader=donor==S3_DONOR_S2?0xEC04A:0xEC03C;
    if(be(rom+loader)!=0x4DFA||be(rom+loader+4)!=0x3E3C)return 0;
    unsigned src=loader+2+(int16_t)be(rom+loader+2),bytes=be(rom+loader+6);
    s->zsize=sax(rom,size,src,bytes,s->z80,sizeof s->z80,0);if(!s->zsize)return 0;
    static const unsigned playlists[]={0,0x11F5,0x11E7,0x1169,0,0,0,0x11FC};
    static const unsigned envelopes[]={0,0x1029,0x101B,0xFA9,0,0,0,0x1030};
    static const unsigned speeds[]={0,0x1214,0x1205,0x1188,0,0,0,0x121B};
    s->fast_tempo=native_tempo(donor,s->z80[speeds[donor]+song]);
    unsigned meta=s->z80[playlists[donor]+song],index=meta&31;
    s->force_pal=(meta&0x40)!=0;
    s->bank=meta&128?0xF8000:0xF0000;s->source=s->bank+(le(rom+s->bank+2*index)&0x7FFF);
    s->little=1;s->low=s->bank;s->high=s->bank+0x8000;
    s->env=envelopes[donor];s->env_count=13;
    if(donor==S3_DONOR_S2 && !(meta&0x20)){
        s->compressed=1;
        bytes=le(rom+s->source);
        unsigned n=sax(rom,size,s->source+2,bytes,s->song,sizeof s->song,0);if(!n)return 0;
        s->source=0x1380;s->bank=0;s->low=0x1380;s->high=0x1380+n;
    }
    return 1;
}
int s3_legacy_source(const uint8_t *rom,size_t size,unsigned donor,unsigned theme,S3LegacySource *s)
{
    static const unsigned ids[3][S3_THEME_COUNT]={{7,14,15,13,6},{23,26,27,25,22},{23,26,27,25,20}};
    unsigned playlist=donor==S3_DONOR_S1?0:donor==S3_DONOR_WAI?2:1;
    return theme<S3_THEME_COUNT&&s3_legacy_music_source(rom,size,donor,ids[playlist][theme],s);
}
typedef struct Node {uint32_t pc,target;uint16_t output;uint8_t kind,length,encoded;} Node;
typedef struct Convert {
    const S3LegacySource *s;uint8_t *out;size_t capacity;
    int bad,count,voices;unsigned psg_base;int map[0x10000*8];Node nodes[8192];
} Convert;
static unsigned rb(Convert *c,uint32_t p)
{
    const S3LegacySource *s=c->s;
    if(s->compressed && p>=0x1380&&p<s->high)return s->song[p-0x1380];
    if(p>=s->size){c->bad=1;return 0;}return s->rom[p];
}
static unsigned rw(Convert *c,uint32_t p){unsigned a=rb(c,p),b=rb(c,p+1);return c->s->little?a|(b<<8):(a<<8)|b;}
static unsigned ptr(Convert *c,uint32_t p,int root)
{unsigned v=rw(c,p);return c->s->little?c->s->bank+(v&(c->s->bank?0x7FFF:0xFFFF)):root?c->s->source+v:p+1+(int16_t)v;}
static int node(Convert *c,uint32_t p,unsigned kind)
{
    if(p<c->s->low||p>=c->s->high||kind>=8||c->count>=8192){c->bad=1;return 0;}
    unsigned key=(p&0xFFFF)*8+kind;if(c->map[key]>=0)return c->map[key];
    int n=c->count++;c->map[key]=n;c->nodes[n]=(Node){p,0,0,(uint8_t)kind,0,0};return n;
}
static int find(Convert *c,uint32_t p,unsigned kind)
{if(p<c->s->low||p>=c->s->high){c->bad=1;return 0;}int n=c->map[(p&0xFFFF)*8+kind];if(n<0){c->bad=1;return 0;}return n;}
static int order(const void *a,const void *b)
{const Node *x=a,*y=b;return x->kind!=y->kind?(x->kind<y->kind?-1:1):x->pc<y->pc?-1:x->pc>y->pc;}
/* Standalone songs are untrusted. Explore their real call/return stack and
 * reject recursive/zero-time cycles before the native driver can execute one.
 * Timing bytes terminate the current zero-time path, then schedule its next
 * state. The stock ROM adapters do not need this additional input boundary. */
typedef struct MusicGraph {
    uint64_t keys[65536],pending[65536];uint8_t color[65536];unsigned count,queued;
} MusicGraph;
static int safe_music_state(Convert *c,MusicGraph *g,uint64_t key,unsigned depth)
{
    if(depth>512||g->count>=32768)return 0;
    unsigned slot=(unsigned)((key^(key>>29))*2654435761u)&65535u;
    while(g->keys[slot]&&g->keys[slot]!=key)slot=(slot+1)&65535u;
    if(g->keys[slot])return g->color[slot]==2;
    g->keys[slot]=key;g->color[slot]=1;++g->count;
    unsigned index=(unsigned)(key&65535u)-1;if(index>=(unsigned)c->count)return 0;
    Node *n=&c->nodes[index];unsigned op=rb(c,n->pc);uint64_t stack=key&~65535ULL;
    if(op<0xE0){
        if(g->queued>=65536)return 0;
        int next=find(c,n->pc+1,n->kind);if(c->bad)return 0;
        g->pending[g->queued++]=stack|(unsigned)(next+1);
    }else if(op==0xE3){
        unsigned level=3;while(level&&!((key>>(level*16))&65535u))--level;
        if(!level)return 0;
        uint64_t next=(key>>(level*16))&65535u;
        if(!safe_music_state(c,g,(stack&~(65535ULL<<(level*16)))|next,depth+1))return 0;
    }else if(op!=0xE4&&op!=0xEE&&op!=0xF2){
        if(op==0xF6||op==0xF7||op==0xF8){
            uint64_t branch=stack|(unsigned)(find(c,n->target,n->kind)+1);
            if(op==0xF8){
                unsigned level=1;while(level<4&&((stack>>(level*16))&65535u))++level;
                if(level==4)return 0;
                branch|=(uint64_t)(unsigned)(find(c,n->pc+n->length,n->kind)+1)<<(level*16);
            }
            if(c->bad||!safe_music_state(c,g,branch,depth+1))return 0;
        }
        if(op!=0xF6&&op!=0xF8){
            uint64_t next=stack|(unsigned)(find(c,n->pc+n->length,n->kind)+1);
            if(c->bad||!safe_music_state(c,g,next,depth+1))return 0;
        }
    }
    g->color[slot]=2;return 1;
}
static int safe_custom_music(Convert *c,const unsigned *roots,const unsigned *kinds,unsigned count)
{
    MusicGraph *g=calloc(1,sizeof *g);if(!g)return 0;int ok=1;
    for(unsigned i=0;i<count;i++)g->pending[g->queued++]=(unsigned)(find(c,roots[i],kinds[i])+1);
    for(unsigned i=0;i<g->queued&&ok;i++)ok=safe_music_state(c,g,g->pending[i],0);
    free(g);return ok&&!c->bad;
}
int s3_legacy_song(const S3LegacySource *s,uint8_t *out,size_t capacity,unsigned psg_base,
                   const uint8_t dac_map[128],uint8_t drums[128],char *error,size_t error_size)
{
    if(!s||!out||capacity<0x8000||!drums)return 0;
    Convert *c=calloc(1,sizeof *c);if(!c)return 0;c->s=s;c->out=out;c->capacity=capacity;c->psg_base=psg_base;memset(c->map,0xFF,sizeof c->map);
    const unsigned lengths[]={2,2,2,1,1,2,2,1,2,2,2,2,2,1,1,2,5,1,1,2,1,2,3,5,3,1};
    const unsigned commands[]={0,1,2,3,4,5,6,0,8,9,10,11,12,31,7,13,14,15,16,17,18,19,20,21,22,0xFF};
    unsigned roots[10],kinds[10],headers[10],count=0,input=s->source,fm=rb(c,input+2),psg=rb(c,input+3);
    int size=0;
    if(!fm||fm>7||psg>3)goto fail;
    unsigned header=8+fm*4+psg*6;memset(out,0,header);out[2]=(uint8_t)fm;out[3]=(uint8_t)psg;
    out[5]=2; /* Resolve legacy zero-period PSG after the effective transpose. */
    out[6]=(uint8_t)rb(c,input+4);out[7]=(uint8_t)native_tempo(s->donor,rb(c,input+5));
    for(unsigned i=0;i<fm+psg;i++){
        unsigned old=input+6+(i<fm?i*4:fm*4+(i-fm)*6),dest=old-input+2,kind=i==0?1:i>=fm?i-fm+2:0;
        roots[count]=ptr(c,old,1);kinds[count]=kind;headers[count++]=dest;
        out[dest+2]=(uint8_t)(rb(c,old+2)+(kind>=2?12:0));out[dest+3]=(uint8_t)(rb(c,old+3)<<(kind>=2?3:0));
        if(kind>=2){unsigned env=rb(c,old+5);if(env>s->env_count)goto fail;out[dest+5]=(uint8_t)(env?psg_base+env:0);}
    }
    c->voices=fm>1?1:0;
    for(unsigned i=0;i<count;i++)node(c,roots[i],kinds[i]);
    for(int i=0;i<c->count&&!c->bad;i++){
        Node *n=&c->nodes[i];unsigned op=rb(c,n->pc),length=1;
        if(op>=0xE0){
            if(op>0xF9){c->bad=1;break;}length=lengths[op-0xE0];n->encoded=(uint8_t)(op==0xE7?1:op==0xF9?8:length+1);
            if(op==0xEF && rb(c,n->pc+1)+1>(unsigned)c->voices)c->voices=(int)rb(c,n->pc+1)+1;
        }else{n->encoded=1;if(n->kind==1&&op>=0x81)drums[op&127]=1;}
        n->length=(uint8_t)length;
        if(op==0xF6||op==0xF7||op==0xF8){n->target=ptr(c,n->pc+length-2,0);node(c,n->target,n->kind);}
        if(op!=0xE3&&op!=0xE4&&op!=0xEE&&op!=0xF2&&op!=0xF6)node(c,n->pc+length,n->kind);
    }
    if(c->bad)goto fail;
    if(s->require_restore){
        int restore=0;for(int i=0;i<c->count;i++)if(rb(c,c->nodes[i].pc)==0xE4)restore=1;
        if(!restore){
            if(error&&error_size)snprintf(error,error_size,"1-UP SMPS needs smpsFade on its final channel to resume the background song.");
            free(c);return 0;
        }
    }
    if(!s->source&&!s->low&&s->size<0x8000&&!safe_custom_music(c,roots,kinds,count))goto fail;
    qsort(c->nodes,(size_t)c->count,sizeof *c->nodes,order);
    size=(int)header;
    for(int i=0;i<c->count;i++){Node *n=&c->nodes[i];n->output=(uint16_t)size;size+=n->encoded;c->map[(n->pc&0xFFFF)*8+n->kind]=i;}
    if(c->voices>128||size+c->voices*25>0x7FFF)goto fail;
    for(unsigned i=0;i<count;i++)ww(out+headers[i],c->nodes[find(c,roots[i],kinds[i])].output);
    for(int i=0;i<c->count&&!c->bad;i++){
        Node *n=&c->nodes[i];unsigned dest=n->output,op=rb(c,n->pc);
        if(op<0xE0){
            unsigned value=op;
            out[dest]=(uint8_t)(n->kind==1&&op>=0x81&&dac_map?dac_map[op&127]:value);continue;
        }
        if(op==0xE7){out[dest]=0xFE;continue;}
        if(op==0xF9){static const uint8_t release[]={0xFF,0x20,0x88,0x0F,0xFF,0x20,0x8C,0x0F};memcpy(out+dest,release,sizeof release);continue;}
        out[dest]=0xFF;out[dest+1]=(uint8_t)commands[op-0xE0];
        for(unsigned j=1;j<n->length;j++)out[dest+1+j]=(uint8_t)rb(c,n->pc+j);
        if(op==0xEA)out[dest+2]=(uint8_t)native_tempo(s->donor,rb(c,n->pc+1));
        if(op==0xEC){unsigned v=rb(c,n->pc+1);out[dest+2]=(uint8_t)(((v<<3)&0x7F)|(v&0x80));}
        if(op==0xF5){unsigned env=rb(c,n->pc+1);if(env>s->env_count)goto fail;out[dest+2]=(uint8_t)(env?psg_base+env:0);}
        if(n->target){int rel=c->nodes[find(c,n->target,n->kind)].output-(n->output+n->encoded-(op==0xF8?0:1));if(rel<-32768||rel>32767)goto fail;ww(out+dest+n->encoded-2,(unsigned)rel);}
    }
    unsigned voice=s->little?ptr(c,input,1):input+(int16_t)rw(c,input);ww(out,(unsigned)size);
    /* Sonic 1 stores operators 4/3/2/1; Sonic 2 already uses Clone's
     * 4/2/3/1 order. Only the 68K donor needs its middle operators swapped. */
    static const unsigned order_s1[]={0,2,1,3},order_s2[]={0,1,2,3};
    const unsigned *reorder=s->donor==S3_DONOR_S1?order_s1:order_s2;
    for(int v=0;v<c->voices;v++){
        unsigned alg=rb(c,voice)&7;out[size]=(uint8_t)rb(c,voice);
        for(unsigned k=0;k<4;k++){
            unsigned carrier=k==0?alg==7:k==1?alg>=5:k==2?alg>=4:1;
            out[size+1+k]=(uint8_t)rb(c,voice+1+reorder[k]);out[size+5+k]=(uint8_t)((rb(c,voice+21+reorder[k])&127)|(carrier?128:0));
            for(unsigned g=0;g<4;g++)out[size+9+g*4+k]=(uint8_t)rb(c,voice+5+g*4+reorder[k]);
        }
        size+=25;voice+=25;
    }
    if(!c->bad){free(c);return size;}
fail:
    if(error&&error_size)snprintf(error,error_size,"Unable to convert legacy event music near $%06X",c->count?c->nodes[c->count-1].pc:input);
    free(c);return 0;
}
int s3_legacy_envelope(const S3LegacySource *s,unsigned index,uint8_t out[256])
{
    if(!s||!index||index>s->env_count||!out)return 0;
    unsigned p=s->little?le(s->z80+s->env+2*(index-1)):bl(s->rom+s->env+4*(index-1));
    for(unsigned i=0;i<256;i++){
        if(p+i>=(s->little?s->zsize:s->size))return 0;
        unsigned v=s->little?s->z80[p+i]:s->rom[p+i];
        out[i]=(uint8_t)(v==0x80 && (s->little||index!=6)?0x81:v);
        if(v==0x80)return (int)i+1;
    }
    return 0;
}
int s3_legacy_sample(const S3LegacySource *s,unsigned note,uint8_t *out,size_t capacity,unsigned *rate)
{
    if(!s||!out||!rate||note<0x81||note>0x91||
       (s->donor==S3_DONOR_WAI&&note>0x8E))return 0;
    static const unsigned hz[]={8201,23784,17127,15232,7328,13714,7426,9635,8720,7138,6957,22799,18262,15232,15232,13064,9806};
    unsigned start,length,delta,native_hz=hz[note-0x81];const uint8_t *data;
    if(s->donor==S3_DONOR_S1){
        unsigned sample=note==0x81?0:note==0x82?1:note>=0x88&&note<=0x8B?2:3;if(sample>=3)return 0;
        unsigned meta=0xD6+sample*8;start=le(s->z80+meta);length=le(s->z80+meta+2);delta=0x22;data=s->z80;
        if(start>s->zsize||length>s->zsize-start)return 0;
    }else{
        static const unsigned ptrs[]={0,0x1233,0xF93,0xF75,0,0,0,0xFA6},rates[]={0,0x124F,0xFAF,0xF8D,0,0,0,0xFC2},deltas[]={0,0x1B3,0x197,0x179,0,0,0,0x197};
        unsigned entry=rates[s->donor]+(note-0x81)*2,sample=s->z80[entry];if(sample<0x81||sample>0x87)return 0;
        if(s->donor==S3_DONOR_WAI){
            /* This prototype's tom pitches and clap/scratch differ from
             * retail. Its DPCM loop emits two samples in 303 base cycles,
             * plus 13 cycles per additional DJNZ iteration per sample. */
            unsigned wait=s->z80[entry+1];if(!wait)wait=256;
            unsigned cycles=303+26*(wait-1);
            native_hz=(7159090u+cycles/2)/cycles;
        }
        unsigned meta=ptrs[s->donor]+(sample-0x81)*4;
        start=0xE8000+(le(s->z80+meta)&0x7FFF);length=le(s->z80+meta+2);delta=deltas[s->donor];data=s->rom;
        if(start>s->size||length>s->size-start)return 0;
    }
    if(!length||2*length>capacity)return 0;
    unsigned accum=0x80;
    for(unsigned i=0;i<length;i++){
        unsigned v=data[start+i];accum=(accum+s->z80[delta+(v>>4)])&255;out[i*2]=(uint8_t)accum;
        accum=(accum+s->z80[delta+(v&15)])&255;out[i*2+1]=(uint8_t)accum;
    }
    *rate=(native_hz*256u+12550u)/25100u;return (int)(2*length);
}
