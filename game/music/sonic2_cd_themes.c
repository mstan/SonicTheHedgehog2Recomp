/* Native stereo CD event playback. Clone owns the cartridge queues and SFX;
 * a silent music entry gives the selected CD recording the music channels.
 * Only sample positions, pause and fade clocks belong in execution snapshots. */
#include "sonic2_cd_themes.h"
#include "sonic2_music.h"
#include "sonic2_state_io.h"
#include "audio.h"
#include "sha256.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Song {uint8_t *pcm;uint32_t frames,gain;uint8_t hash[32];} Song;
static Song songs[S2_THEME_COUNT];
typedef struct CDClock {uint64_t position;uint32_t theme,active,paused,volume,fade;} CDClock;
static CDClock clock_state;
static CDClock render_clock;
static const unsigned events[]={0x18,0x1B,0x1C,0x1A,0x17};
static unsigned le16(const uint8_t *p){return p[0]|((unsigned)p[1]<<8);}
static uint32_t le32(const uint8_t *p){return le16(p)|((uint32_t)le16(p+2)<<16);}
static void be32(uint8_t *p,uint32_t v){p[0]=(uint8_t)(v>>24);p[1]=(uint8_t)(v>>16);p[2]=(uint8_t)(v>>8);p[3]=(uint8_t)v;}
static int cd_event(unsigned t){return t==S2_THEME_OVER||t==S2_THEME_CLEAR||t==S2_THEME_INVINCIBLE;}
static int hex(const char *p,size_t n)
{if(strlen(p)!=n)return 0;for(size_t i=0;i<n;i++)if(!((p[i]>='0'&&p[i]<='9')||(p[i]>='a'&&p[i]<='f')))return 0;return 1;}
static int wav(const char *path,const char *hash,Song *out)
{
    FILE *f=fopen(path,"rb");if(!f)return 0;
    if(fseek(f,0,SEEK_END)){fclose(f);return 0;}long n=ftell(f);rewind(f);
    if(n<48||n>32*1024*1024+44){fclose(f);return 0;}
    uint8_t *raw=malloc((size_t)n);if(!raw){fclose(f);return 0;}
    int ok=fread(raw,1,(size_t)n,f)==(size_t)n;fclose(f);
    uint8_t digest[32];char actual[65];
    if(ok){recompui_sha256_compute(raw,(size_t)n,digest);for(unsigned i=0;i<32;i++)snprintf(actual+2*i,3,"%02x",digest[i]);}
    ok=ok&&hex(hash,64)&&!strcmp(actual,hash)&&!memcmp(raw,"RIFF",4)&&le32(raw+4)==(uint32_t)n-8&&
        !memcmp(raw+8,"WAVEfmt ",8)&&le32(raw+16)==16&&le16(raw+20)==1&&le16(raw+22)==2&&
        le32(raw+24)==44100&&le32(raw+28)==176400&&le16(raw+32)==4&&le16(raw+34)==16&&
        !memcmp(raw+36,"data",4)&&le32(raw+40)==(uint32_t)n-44&&!(le32(raw+40)&3);
    if(ok&&out){out->frames=((uint32_t)n-44)/4;memcpy(out->hash,digest,32);memmove(raw,raw+44,(size_t)n-44);out->pcm=raw;raw=NULL;}
    free(raw);return ok;
}
static int manifest_read(const char *path,unsigned donor,int wanted,Song *out)
{
    if(!path||donor!=S2_DONOR_CD_US)return 0;
    FILE *f=fopen(path,"rb");if(!f)return 0;
    char line[1400],source[65]="",relative[S2_THEME_COUNT][160]={{0}},hashes[S2_THEME_COUNT][65]={{0}};
    unsigned format=0,region=0,lines=0;int ok=1;
    while(fgets(line,sizeof line,f)){
        if(++lines>16){ok=0;break;}line[strcspn(line,"\r\n")]=0;
        char *value=strchr(line,'=');if(!value){ok=0;break;}*value++=0;
        if(!strcmp(line,"format"))format=!strcmp(value,"1");
        else if(!strcmp(line,"donor"))region=!strcmp(value,s2_donor_ids[donor]);
        else if(!strcmp(line,"source_sha256")&&strlen(value)==64)snprintf(source,sizeof source,"%s",value);
        else for(unsigned t=0;t<S2_THEME_COUNT;t++)if(cd_event(t)){
            char key[64];snprintf(key,sizeof key,"%s.path",s2_theme_ids[t]);
            if(!strcmp(line,key)&&strlen(value)<sizeof relative[t])snprintf(relative[t],sizeof relative[t],"%s",value);
            snprintf(key,sizeof key,"%s.sha256",s2_theme_ids[t]);
            if(!strcmp(line,key)&&strlen(value)==64)snprintf(hashes[t],sizeof hashes[t],"%s",value);
        }
    }
    if(ferror(f))ok=0;fclose(f);if(!ok||!format||!region||!hex(source,64))return 0;
    char root[1200];snprintf(root,sizeof root,"%s",path);char *a=strrchr(root,'/'),*b=strrchr(root,'\\');if(!a||(b&&b>a))a=b;
    size_t prefix=a?(size_t)(a-root)+1:0;
    for(unsigned t=0;t<S2_THEME_COUNT;t++)if(cd_event(t)&&(wanted<0||wanted==(int)t)){
        char expected[160];snprintf(expected,sizeof expected,"%s-%s/%s.wav",s2_donor_ids[donor],source,s2_theme_ids[t]);
        if(strcmp(relative[t],expected)||!hex(hashes[t],64)||prefix+strlen(expected)>=sizeof root)return 0;
        snprintf(root+prefix,sizeof root-prefix,"%s",expected);
        if(!wav(root,hashes[t],wanted==(int)t?out:NULL))return 0;
    }
    return 1;
}
int s2_cd_donor_verify(const char *manifest,unsigned donor){return manifest_read(manifest,donor,-1,NULL);}
static int32_t sample(const Song *s,uint32_t frame,unsigned channel)
{return (int16_t)le16(s->pcm+(size_t)frame*4+channel*2);}
static void mix(int16_t *out,size_t frames,double rate)
{
    CDClock clock=render_clock;
    if(!clock.active||clock.paused||clock.theme>=S2_THEME_COUNT||rate<1||rate>384000)return;
    const Song *s=&songs[clock.theme];if(!s->pcm||!s->frames)return;
    uint64_t step=(uint64_t)(44100.0*4294967296.0/rate+0.5);
    for(size_t i=0;i<frames;i++){
        uint32_t at=(uint32_t)(clock.position>>32);
        if(at>=s->frames){
            if(clock.theme!=S2_THEME_INVINCIBLE)break;
            clock.position%=((uint64_t)s->frames<<32);at=(uint32_t)(clock.position>>32);
        }
        unsigned next=at+1<s->frames?at+1:clock.theme==S2_THEME_INVINCIBLE?0:at;
        unsigned fraction=(unsigned)(clock.position&0xFFFFFFFFu)>>16;
        for(unsigned ch=0;ch<2;ch++){
            int64_t a=sample(s,at,ch),b=sample(s,next,ch);
            int64_t value=a+(((b-a)*fraction)>>16);
            value=(value*clock.volume*s->gain)/4294967296LL;
            value+=out[i*2+ch];out[i*2+ch]=(int16_t)(value>32767?32767:value<-32768?-32768:value);
        }
        clock.position+=step;
    }
}
void s2_cd_themes_reset(void)
{
    audio_set_extra_source(NULL);
    for(unsigned t=0;t<S2_THEME_COUNT;t++){free(songs[t].pcm);memset(&songs[t],0,sizeof songs[t]);}
    memset(&clock_state,0,sizeof clock_state);
    memset(&render_clock,0,sizeof render_clock);
}
int s2_cd_theme_build(unsigned t,unsigned donor,const char *manifest,const S3SoundLayout *l,
                      uint8_t *bank,size_t cap,uint32_t *used,char *error,size_t error_size)
{
    uint32_t dest=used&&*used<=cap?(*used+1)&~1u:0;
    if(!cd_event(t)||!l||!bank||!used||cap>0x400000u||*used>cap||dest>cap||cap-dest<86||!manifest_read(manifest,donor,(int)t,&songs[t])){
        if(error&&error_size)snprintf(error,error_size,"Provide the matching Sonic CD disc with audio tracks");return 0;
    }
    uint8_t *p=bank+dest;memset(p,0,86);p[1]=60;p[2]=7;p[3]=3;p[6]=p[7]=1;
    for(unsigned i=0;i<10;i++){unsigned off=i<7?8+i*4:8+7*4+(i-7)*6;p[off+1]=54;}
    /* Keep a live, silent music entry for native music/1-UP ownership. Every
     * channel rests; the relative jump repeats without keying an instrument. */
    p[54]=0x80;p[55]=1;p[56]=0xFF;p[57]=0x14;p[58]=0xFF;p[59]=0xFB;
    /* Fixed Q16 mixer levels, calibrated from the supported US recordings
     * against the existing CD campaign's -24.2 LUFS music reference. No cold
     * analysis or floating-point gain enters playback or snapshots. */
    songs[t].gain=t==S2_THEME_OVER?23795u:t==S2_THEME_CLEAR?15541u:17437u;
    be32(bank+l->music_index-0x2A0000+(events[t]-1)*4,0x01000000u|(0x2A0000u+dest));*used=dest+86;
    audio_set_extra_source(mix);return 1;
}
void s2_cd_theme_request(unsigned native)
{
    if(native==0x18)return; /* Clone backs up/restores the silent music entry. */
    if(native>=1&&native<=0x1F){
        for(unsigned t=0;t<S2_THEME_COUNT;t++)if(cd_event(t)&&native==events[t]&&songs[t].pcm){
            memset(&clock_state,0,sizeof clock_state);clock_state.theme=t;clock_state.active=1;clock_state.volume=65536;return;
        }
        clock_state.active=0;
    }else if(native==0xFB)clock_state.fade=1;
    else if(native==0xFC||native==0xFF)clock_state.active=0;
}
void s2_cd_theme_frame(int paused,int oneup)
{
    clock_state.paused=paused||oneup;
    if(clock_state.active&&clock_state.fade&&!clock_state.paused){
        if(clock_state.volume<=1457){clock_state.volume=0;clock_state.active=0;}else clock_state.volume-=1457;
    }
}
void s2_cd_theme_tick(uint32_t tick)
{
    (void)tick;render_clock=clock_state;
    if(!clock_state.active||clock_state.paused)return;
    const Song *s=&songs[clock_state.theme];
    /* One sealed NTSC tick is 895780 master clocks. Advance outside the
     * presentation mixer so rollback and muted/core replay share this clock.
     * Quotient/remainder arithmetic keeps the Q32 product within uint64_t. */
    const uint64_t samples=44100ULL*895780u,master=53693175u;
    const uint64_t step=((samples/master)<<32)+((samples%master)*4294967296ULL+master/2)/master;
    clock_state.position+=step;
    uint64_t end=(uint64_t)s->frames<<32;
    if(clock_state.position>=end){
        if(clock_state.theme==S2_THEME_INVINCIBLE)clock_state.position%=end;
        else {clock_state.position=end;clock_state.active=0;}
    }
}
void s2_cd_theme_state(struct S2StateIO *io)
{
    CDClock incoming=clock_state;
    if(io->mode){if(!io->data||io->pos>io->size||sizeof incoming>io->size-io->pos){io->ok=0;return;}memcpy(&incoming,io->data+io->pos,sizeof incoming);}
    if(incoming.theme>=S2_THEME_COUNT||incoming.active>1||incoming.paused>1||incoming.volume>65536||incoming.fade>1||
       (incoming.active && (!songs[incoming.theme].pcm||!songs[incoming.theme].frames||
                            incoming.position>=((uint64_t)songs[incoming.theme].frames<<32))))io->ok=0;
    S2_STATE(io,clock_state);
    if(io->mode==2&&io->ok)render_clock=clock_state;
}
void s2_cd_theme_identity(uint8_t out[32])
{
    uint8_t hashes[S2_THEME_COUNT*36]={0};for(unsigned t=0;t<S2_THEME_COUNT;t++){memcpy(hashes+t*36,songs[t].hash,32);be32(hashes+t*36+32,songs[t].gain);}
    recompui_sha256_compute(hashes,sizeof hashes,out);
}
void s2_cd_theme_debug(unsigned *active,unsigned *theme,uint64_t *position)
{if(active)*active=clock_state.active;if(theme)*theme=clock_state.theme;if(position)*position=clock_state.position;}
