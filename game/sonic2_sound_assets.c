#include "sonic2_sound_assets.h"
#include "sha256.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* REV01 uses banked Z80 pointers and Saxman-compressed songs. Conversion
 * follows the stock parser, not a second copy of the game's arrangements. */
enum { ROM_SIZE=0x100000, BANK_SIZE=0x8000, MAX_NODES=8192 };
typedef struct Node { unsigned pc,target,output; uint8_t kind,length,encoded; } Node;
typedef struct Convert {
    const uint8_t *src; size_t size; unsigned base;
    uint8_t *memory; int bad,count,voices,pitch[8];
    int map[BANK_SIZE*8]; Node nodes[MAX_NODES];
} Convert;
static unsigned rb(Convert *c,unsigned p)
{ if(p>=c->size){c->bad=1;return 0;}return c->src[p]; }
static unsigned rw(Convert *c,unsigned p) { return rb(c,p)|(rb(c,p+1)<<8); }
static unsigned pointer(Convert *c,unsigned p)
{ unsigned v=rw(c,p);if(v<c->base || v-c->base>=c->size)c->bad=1;return v-c->base; }
static void ww(uint8_t *m,unsigned p,unsigned v) { m[p]=(uint8_t)(v>>8);m[p+1]=(uint8_t)v; }
static void wl(uint8_t *m,unsigned p,unsigned v) { ww(m,p,v>>16);ww(m,p+2,v); }
static unsigned le(const uint8_t *m,unsigned p) { return m[p]|(m[p+1]<<8); }

/* Both stock decompressors use the same 4 KiB dictionary, initially zero.
 * Input length is compressed bytes (the music stream has a two-byte prefix).
 * A reference can overlap its output; every read/write is checked. */
static unsigned saxman(const uint8_t *in,size_t size,uint8_t *out,unsigned cap)
{
    size_t p=0;unsigned used=0;
    while(p<size) {
        unsigned flags=in[p++];
        for(unsigned bit=0;bit<8 && p<size;bit++) {
            unsigned a=in[p++];
            if(flags&(1u<<bit)) { if(used==cap)return 0;out[used++]=(uint8_t)a; }
            else {
                /* The stock get-byte routine exits immediately at end of
                 * stream, including a final incomplete reference token. */
                if(p==size)return used;
                unsigned b=in[p++],count=(b&15)+3;
                int from=(int)((a|((b&240)<<4))+0x12)&4095;
                from+=(int)(used&~4095u);if(from>(int)used)from-=4096;
                if(count>cap-used)return 0;
                while(count--) {
                    if(from>=(int)used)return 0;
                    out[used++]=from<0?0:out[from];++from;
                }
            }
        }
    }
    return used;
}
static int node(Convert *c,unsigned pc,unsigned kind)
{
    if(pc>=c->size || pc>=BANK_SIZE || kind>=8 || c->count>=MAX_NODES){c->bad=1;return 0;}
    unsigned key=pc*8+kind;if(c->map[key]>=0)return c->map[key];
    int n=c->count++;c->map[key]=n;c->nodes[n]=(Node){pc,0,0,(uint8_t)kind,0,0};return n;
}
static int order_nodes(const void *a,const void *b)
{
    const Node *x=a,*y=b;
    if(x->kind!=y->kind)return x->kind<y->kind?-1:1;
    return x->pc<y->pc?-1:x->pc>y->pc;
}
static unsigned target_output(Convert *c,unsigned pc,unsigned kind)
{
    if(pc>=BANK_SIZE || kind>=8){c->bad=1;return 0;}
    int n=c->map[pc*8+kind];if(n<0){c->bad=1;return 0;}return c->nodes[n].output;
}
static unsigned convert_song(Convert *c,unsigned source,unsigned dest,unsigned capacity,int music)
{
    static const uint8_t lengths[]={2,2,2,1,1,2,2,1,2,2,2,2,2,1,1,2,5,1,1,2,1,2,3,5,3,1};
    static const uint8_t commands[]={0,1,2,3,4,5,6,0,8,9,10,11,12,31,7,13,14,15,16,17,18,19,20,21,22,0xFF};
    unsigned roots[10],kinds[10],headers[10],root_count=0,header;
    unsigned fm=music?rb(c,source+2):0,psg=music?rb(c,source+3):0;
    c->count=c->voices=0;memset(c->map,0xFF,sizeof c->map);
    if(music) {
        if(!fm || fm>7 || psg>3){c->bad=1;return 0;}
        header=8+fm*4+psg*6;
        c->memory[dest+2]=(uint8_t)fm;c->memory[dest+3]=(uint8_t)psg;
        c->memory[dest+6]=(uint8_t)rb(c,source+4);
        c->memory[dest+7]=(uint8_t)(256-rb(c,source+5));
        for(unsigned i=0;i<fm+psg;i++) {
            unsigned old=source+6+(i<fm?i*4:fm*4+(i-fm)*6),out=dest+old-source+2;
            unsigned kind=i==0?1:i>=fm?i-fm+2:0;
            roots[root_count]=pointer(c,old);kinds[root_count]=kind;headers[root_count++]=out;
            unsigned n=i<fm?4:6;for(unsigned k=0;k<n;k++)c->memory[out+k]=(uint8_t)rb(c,old+k);
            if(kind>=2){c->pitch[kind]=(int8_t)rb(c,old+2);c->memory[out+2]+=12;c->memory[out+3]<<=3;c->memory[out+4]=0;}
        }
        if(fm>1)c->voices=1;
    } else {
        unsigned tracks=rb(c,source+3);
        if(!tracks || tracks>6){c->bad=1;return 0;}
        header=4+tracks*6;
        for(unsigned k=0;k<header;k++)c->memory[dest+k]=(uint8_t)rb(c,source+k);
        for(unsigned i=0;i<tracks;i++) {
            unsigned old=source+4+i*6,out=dest+4+i*6,id=rb(c,old+1);
            unsigned kind=id&128?i+2:0;
            roots[root_count]=pointer(c,old+2);kinds[root_count]=kind;headers[root_count++]=out+2;
            if(kind>=2){c->pitch[kind]=(int8_t)rb(c,old+4);c->memory[out+4]+=12;c->memory[out+5]<<=3;}
            else c->voices=1;
        }
    }
    for(unsigned i=0;i<root_count;i++)node(c,roots[i],kinds[i]);
    for(int i=0;i<c->count && !c->bad;i++) {
        Node *n=&c->nodes[i];unsigned op=rb(c,n->pc),length=1;
        if(op>=0xE0) {
            if(op>0xF9){c->bad=1;break;}
            length=lengths[op-0xE0];n->encoded=(uint8_t)(op==0xE7?1:op==0xF9?8:length+1);
            if(op==0xEF && rb(c,n->pc+1)+1>(unsigned)c->voices)c->voices=(int)rb(c,n->pc+1)+1;
        } else n->encoded=1;
        n->length=(uint8_t)length;
        if(op==0xF6 || op==0xF7 || op==0xF8) {
            n->target=pointer(c,n->pc+length-2);node(c,n->target,n->kind);
        }
        if(op!=0xE3 && op!=0xE4 && op!=0xEE && op!=0xF2 && op!=0xF6)node(c,n->pc+length,n->kind);
    }
    if(c->bad)return 0;
    qsort(c->nodes,(size_t)c->count,sizeof c->nodes[0],order_nodes);
    unsigned size=header;
    for(int i=0;i<c->count;i++){Node *n=&c->nodes[i];n->output=size;size+=n->encoded;c->map[n->pc*8+n->kind]=i;}
    if(size+25*c->voices>capacity || c->voices>128){c->bad=1;return 0;}
    for(unsigned i=0;i<root_count;i++)ww(c->memory,headers[i],target_output(c,roots[i],kinds[i]));
    for(int i=0;i<c->count;i++) {
        Node *n=&c->nodes[i];unsigned out=dest+n->output,op=rb(c,n->pc);
        if(op<0xE0) {
            /* Clone's PSG table includes one extra maximum-frequency slot. */
            if(n->kind>=2 && !c->pitch[n->kind] && op==0xC6)++op;
            c->memory[out]=(uint8_t)op;continue;
        }
        if(op==0xE7){c->memory[out]=0xFE;continue;}
        if(op==0xF9){static const uint8_t release[]={0xFF,0x20,0x88,0x0F,0xFF,0x20,0x8C,0x0F};memcpy(c->memory+out,release,8);continue;}
        c->memory[out]=0xFF;c->memory[out+1]=commands[op-0xE0];
        for(unsigned k=1;k<n->length;k++)c->memory[out+1+k]=(uint8_t)rb(c,n->pc+k);
        if(op==0xEA)c->memory[out+2]=(uint8_t)(256-rb(c,n->pc+1));
        /* Z80 accepts E6 on PSG too; Clone has a separate PSG command. */
        if(op==0xEC || (op==0xE6 && n->kind>=2)) {
            unsigned v=rb(c,n->pc+1);c->memory[out+1]=0x0C;c->memory[out+2]=(uint8_t)(((v<<3)&127)|(v&128));
        }
        if(op==0xF6 || op==0xF7 || op==0xF8) {
            int relative=(int)target_output(c,n->target,n->kind)-(int)(n->output+n->encoded-(op==0xF8?0:1));
            if(relative < -32768 || relative>32767){c->bad=1;return 0;}
            ww(c->memory,out+n->encoded-2,(unsigned)relative);
        }
    }
    unsigned voices=c->voices?pointer(c,source):0;ww(c->memory,dest,size);
    /* Z80 and Clone use the same operator order. Move TL before RS/AR and
     * mark the carriers as the original algorithm's gain mask does. */
    for(int v=0;v<c->voices;v++) {
        unsigned alg=rb(c,voices)&7;c->memory[dest+size]=(uint8_t)rb(c,voices);
        for(unsigned k=0;k<4;k++) {
            unsigned carrier=k==0?alg==7:k==1?alg>=5:k==2?alg>=4:1;
            c->memory[dest+size+1+k]=(uint8_t)rb(c,voices+1+k);
            c->memory[dest+size+5+k]=(uint8_t)((rb(c,voices+21+k)&127)|(carrier?128:0));
            for(unsigned group=0;group<4;group++)c->memory[dest+size+9+group*4+k]=(uint8_t)rb(c,voices+5+group*4+k);
        }
        size+=25;voices+=25;
    }
    return size;
}
int s2_sound_build_assets(const uint8_t *rom,size_t rom_size,uint8_t *memory,size_t memory_size,
                         const S2SoundAssetLayout *layout,char *error,size_t error_size)
{
    const char *failure="Unsupported ROM: Clone Driver requires Sonic 2 JUE REV01.";
    if(error_size)error[0]=0;
    if(!rom || !memory || !layout || rom_size!=ROM_SIZE || memory_size<S2_SOUND_ASSETS_END)goto fail;
    static const uint8_t expected[32]={0x19,0x3B,0xC4,0x06,0x4C,0xE0,0xDA,0xF2,0x7E,0xA9,0xE9,0x08,0xED,0x24,0x6D,0x87,0xEC,0x57,0x6C,0xC2,0x94,0x83,0x3B,0xAD,0xEB,0xB5,0x90,0xB6,0xAD,0x8E,0x8F,0x6B};
    uint8_t digest[32];recompui_sha256_compute(rom,rom_size,digest);
    if(memcmp(digest,expected,32))goto fail;
    unsigned tables[]={layout->music_index,layout->sfx_index,layout->special_index,layout->psg_index,layout->sample_table};
    for(unsigned i=0;i<5;i++)if(tables[i]<ROM_SIZE || tables[i]>S2_SOUND_MUSIC_BASE-1024)goto fail;
    Convert *c=calloc(1,sizeof *c);failure="Unable to allocate sound conversion workspace.";
    if(!c)goto fail;c->memory=memory;
    uint8_t z80[8192],song[8192];unsigned zsize=saxman(rom+0xEC0E8,0xF64,z80,sizeof z80);
    if(zsize!=0x1308){fprintf(stderr,"[sound-driver] Invalid decompressed driver length %u\n",zsize);free(c);failure="Invalid ROM sound driver stream.";goto fail;}
    memset(memory+S2_SOUND_MUSIC_BASE,0,S2_SOUND_ASSETS_END-S2_SOUND_MUSIC_BASE);
    for(unsigned i=0;i<31 && !c->bad;i++) {
        unsigned meta=z80[0x11F5+i],bank=meta&128?0xF8000:0xF0000;
        unsigned offset=le(rom,bank+(meta&31)*2)&32767;
        if(meta&32){c->src=rom+bank;c->size=BANK_SIZE;c->base=0x8000;}
        else {
            unsigned length=le(rom,bank+offset);
            if(length>BANK_SIZE-offset-2){c->bad=1;break;}
            c->size=saxman(rom+bank+offset+2,length,song,sizeof song);
            if(!c->size){c->bad=1;break;}
            c->src=song;c->base=0x1380;offset=0;
        }
        unsigned dest=S2_SOUND_MUSIC_BASE+i*S2_SOUND_MUSIC_STRIDE;
        convert_song(c,offset,dest,S2_SOUND_MUSIC_STRIDE,1);
        if(c->bad)fprintf(stderr,"[sound-driver] Music conversion failed at ID $%02X (base $%04X, offset $%04X, nodes %d)\n",i+0x81,c->base,offset,c->count);
        wl(memory,layout->music_index+4*i,((256u-z80[0x1214+i])<<24)|dest|((meta>>6)&1));
    }
    c->src=rom+0xF8000;c->size=BANK_SIZE;c->base=0x8000;
    for(unsigned i=0;i<81 && !c->bad;i++) {
        unsigned source=le(rom,0xFEE91+2*i)&32767,dest=S2_SOUND_SFX_BASE+i*S2_SOUND_SFX_STRIDE;
        convert_song(c,source,dest,S2_SOUND_SFX_STRIDE,0);
        if(c->bad)fprintf(stderr,"[sound-driver] SFX conversion failed at ID $%02X (offset $%04X, nodes %d)\n",i+0xA0,source,c->count);
        wl(memory,layout->sfx_index+4*i,((unsigned)z80[0xFD8+i]<<24)|dest);
    }
    for(unsigned i=0;i<13 && !c->bad;i++) {
        unsigned p=le(z80,0x1029+2*i),dest=S2_SOUND_PSG_BASE+256*i,j;
        wl(memory,layout->psg_index+4*i,dest);
        for(j=0;j<256;j++) {
            if(p+j>=zsize){c->bad=1;break;}
            unsigned v=z80[p+j];memory[dest+j]=(uint8_t)(v==0x80?0x81:v);
            if(v==0x80)break;
        }
        if(j==256)c->bad=1;
    }
    for(unsigned i=0;i<7 && !c->bad;i++) {
        unsigned p=0xE8000+(le(z80,0x1233+4*i)&32767),size=le(z80,0x1235+4*i),sample=128;
        if(size>0x2000 || p+size>ROM_SIZE){c->bad=1;break;}
        unsigned dest=S2_SOUND_PCM_BASE+0x4000*i;
        for(unsigned j=0;j<size;j++)for(unsigned n=0;n<2;n++) {
            sample=(sample+z80[0x1B3+((rom[p+j]>>(n?0:4))&15)])&255;
            memory[dest++]=(uint8_t)sample;
        }
    }
    for(unsigned i=0;i<17 && !c->bad;i++) {
        unsigned id=z80[0x124F+2*i]-0x81,delay=z80[0x1250+2*i];
        if(id>=7){c->bad=1;break;}
        unsigned dest=S2_SOUND_PCM_BASE+0x4000*id,size=le(z80,0x1235+4*id)*2;
        unsigned record=layout->sample_table+10*i;
        /* 295 cycles per decoded byte, plus two DJNZ delay loops. */
        unsigned rate=7159090u/(295+26*((delay?delay:256)-1));
        memory[record]=0x12;memory[record+1]=(uint8_t)(rate*256/25208);
        wl(memory,record+2,dest);wl(memory,record+6,dest+size);
    }
    memcpy(memory+S2_SOUND_SEGA_BASE,rom+0xF1E8C,0x6174);
    int ok=!c->bad;free(c);if(ok)return 1;
    failure="The ROM's sound data could not be converted for this build.";
fail:
    if(error_size)snprintf(error,error_size,"%s",failure);return 0;
}
