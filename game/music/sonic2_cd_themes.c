/* Native stereo CD event playback. Clone owns the cartridge queues and SFX;
 * a silent music entry gives the selected CD recording the music channels.
 * Only sample positions, pause and fade clocks belong in execution snapshots. */
#include "sonic2_cd_themes.h"
#include "sonic2_music.h"
#include "sonic2_custom_music.h"
#include <gme/gme.h>
#include "sonic2_state_io.h"
#include "audio.h"
#include "sha256.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Song {
 uint8_t *pcm;uint32_t frames,gain,loop;uint8_t hash[32];
 Music_Emu *vgm;int mute;uint32_t cache_at[2];short cache[2][4096*2];
} Song;
static Song songs[S2_MUSIC_COUNT];
static unsigned audition_track,audition_solo;static int audition_stopped;
typedef struct CDClock {uint64_t position;uint32_t theme,active,paused,volume,fade;} CDClock;
static CDClock clock_state;
static CDClock render_clock,backup_clock;
static unsigned custom_oneup;
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
static int32_t sample(Song *s,uint32_t frame,unsigned channel)
{
 if(!s->vgm)return (int16_t)le16(s->pcm+(size_t)frame*4+channel*2);
 unsigned block=frame/4096,slot=block&1u,start=block*4096;
 if(s->cache_at[slot]!=start){
  if(gme_tell_samples(s->vgm)!=(int)(start*2)&&gme_seek_samples(s->vgm,(int)(start*2)))return 0;
  if(gme_play(s->vgm,4096*2,s->cache[slot]))return 0;
  s->cache_at[slot]=start;
 }
 return s->cache[slot][(frame-start)*2+channel];
}
unsigned s2_cd_theme_channel_count(unsigned t)
{return t>=S2_MUSIC_COUNT?1:songs[t].vgm?1+(unsigned)gme_voice_count(songs[t].vgm):songs[t].pcm?1:11;}
const char *s2_cd_theme_channel_name(unsigned t,unsigned solo)
{return !solo?"ALL CHANNELS":t<S2_MUSIC_COUNT&&songs[t].vgm&&solo<=(unsigned)gme_voice_count(songs[t].vgm)?gme_voice_name(songs[t].vgm,(int)solo-1):"";}
void s2_cd_theme_audition(unsigned t,unsigned solo,int stopped)
{
 audition_track=t;audition_solo=solo;audition_stopped=stopped;
 for(unsigned i=0;i<S2_MUSIC_COUNT;i++)if(songs[i].vgm){
  int mask=i==t&&solo?~(1<<((int)solo-1)):0;
  if(mask!=songs[i].mute){gme_mute_voices(songs[i].vgm,mask);songs[i].mute=mask;songs[i].cache_at[0]=songs[i].cache_at[1]=~0u;}
 }
}
static void mix(int16_t *out,size_t frames,double rate)
{
    CDClock clock=render_clock;
    if(!clock.active||clock.paused||clock.theme>=S2_MUSIC_COUNT||rate<1||rate>384000)return;
    Song *s=&songs[clock.theme];if((!s->pcm&&!s->vgm)||!s->frames||audition_stopped)return;
    if(s->pcm&&audition_solo&&audition_track==clock.theme)return;
    uint64_t step=(uint64_t)(44100.0*4294967296.0/rate+0.5);
    for(size_t i=0;i<frames;i++){
        uint32_t at=(uint32_t)(clock.position>>32);
        if(at>=s->frames){
            if(s->loop==~0u)break;
            clock.position=((uint64_t)s->loop<<32)+(clock.position-((uint64_t)s->frames<<32))%((uint64_t)(s->frames-s->loop)<<32);at=(uint32_t)(clock.position>>32);
        }
        unsigned next=at+1<s->frames?at+1:s->loop!=~0u?s->loop:at;
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
    for(unsigned t=0;t<S2_MUSIC_COUNT;t++){if(songs[t].vgm)gme_delete(songs[t].vgm);free(songs[t].pcm);memset(&songs[t],0,sizeof songs[t]);}
    memset(&clock_state,0,sizeof clock_state);
    memset(&render_clock,0,sizeof render_clock);memset(&backup_clock,0,sizeof backup_clock);custom_oneup=0;
    audition_solo=audition_stopped=0;audition_track=~0u;
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
    songs[t].loop=t==S2_THEME_INVINCIBLE?0:~0u;
    songs[t].gain=t==S2_THEME_OVER?23795u:t==S2_THEME_CLEAR?15541u:17437u;
    be32(bank+l->music_index-0x2A0000+(events[t]-1)*4,0x01000000u|(0x2A0000u+dest));*used=dest+86;
    audio_set_extra_source(mix);return 1;
}
void s2_cd_theme_request(unsigned native)
{
    if(native==0x18){
        if(!songs[0].pcm&&!songs[0].vgm)return;
        if(!custom_oneup)backup_clock=clock_state;custom_oneup=1;
        memset(&clock_state,0,sizeof clock_state);clock_state.active=1;clock_state.volume=65536;return;
    }
    if(native>=1&&native<=0x1F){
        for(unsigned t=0;t<S2_MUSIC_COUNT;t++)if(native==s2_music_target(t)&&(songs[t].pcm||songs[t].vgm)){
            custom_oneup=0;
            memset(&clock_state,0,sizeof clock_state);clock_state.theme=t;clock_state.active=1;clock_state.volume=65536;return;
        }
        clock_state.active=0;custom_oneup=0;
    }else if(native==0xFB)clock_state.fade=1;
    else if(native==0xFC||native==0xFF){clock_state.active=0;custom_oneup=0;memset(&backup_clock,0,sizeof backup_clock);}
}
void s2_cd_theme_frame(int paused,int oneup)
{
    if(custom_oneup&&!oneup){clock_state=backup_clock;custom_oneup=0;}
    clock_state.paused=paused||(oneup&&!custom_oneup);
    if(clock_state.active&&clock_state.fade&&!clock_state.paused){
        if(clock_state.volume<=1457){clock_state.volume=0;clock_state.active=0;}else clock_state.volume-=1457;
    }
}
void s2_cd_theme_tick(uint64_t tick)
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
        if(s->loop!=~0u)clock_state.position=((uint64_t)s->loop<<32)+(clock_state.position-end)%((uint64_t)(s->frames-s->loop)<<32);
        else {clock_state.position=end;clock_state.active=0;}
    }
}
static int clock_valid(const CDClock *clock)
{
    if(clock->theme>=S2_MUSIC_COUNT||clock->active>1||clock->paused>1||clock->volume>65536||clock->fade>1)return 0;
    const Song *song=&songs[clock->theme];
    return !clock->active||((song->pcm||song->vgm)&&song->frames&&clock->position<((uint64_t)song->frames<<32));
}
void s2_cd_theme_state(struct S2StateIO *io)
{
    CDClock incoming=clock_state,backup=backup_clock;unsigned oneup=custom_oneup;
    if(io->mode){
        size_t bytes=sizeof incoming+sizeof backup+sizeof oneup;
        if(!io->data||io->pos>io->size||bytes>io->size-io->pos){io->ok=0;return;}
        memcpy(&incoming,io->data+io->pos,sizeof incoming);
        memcpy(&backup,io->data+io->pos+sizeof incoming,sizeof backup);
        memcpy(&oneup,io->data+io->pos+sizeof incoming+sizeof backup,sizeof oneup);
    }
    if(!clock_valid(&incoming)||!clock_valid(&backup)||oneup>1){io->ok=0;return;}
    S2_STATE(io,clock_state);S2_STATE(io,backup_clock);S2_STATE(io,custom_oneup);
    if(io->mode==2&&io->ok)render_clock=clock_state;
}
void s2_cd_theme_identity(uint8_t out[32])
{
    uint8_t hashes[S2_MUSIC_COUNT*36]={0};for(unsigned t=0;t<S2_MUSIC_COUNT;t++){memcpy(hashes+t*36,songs[t].hash,32);be32(hashes+t*36+32,songs[t].gain);}
    recompui_sha256_compute(hashes,sizeof hashes,out);
}
void s2_cd_theme_debug(unsigned *active,unsigned *theme,uint64_t *position)
{if(active)*active=clock_state.active;if(theme)*theme=clock_state.theme;if(position)*position=clock_state.position;}

static int custom_loops(unsigned t)
{
 const char *id=s2_theme_ids[t];
 return strcmp(id,"extra-life")&&strcmp(id,"game-over")&&strcmp(id,"continue")&&strcmp(id,"act-clear")&&strcmp(id,"title")&&strcmp(id,"credits")&&strcmp(id,"emerald")&&strcmp(id,"drowning");
}
int s2_cd_custom_build(unsigned t,unsigned kind,const uint8_t *data,size_t bytes,unsigned frames,unsigned loop,const uint8_t hash[32],const S3SoundLayout *l,uint8_t *bank,size_t cap,uint32_t *used,char *error,size_t capacity)
{
 if(t>=S2_MUSIC_COUNT||!frames||!used||*used>cap||!data||!bytes)return 0;
 Song song={0};song.frames=frames;song.gain=65536;memcpy(song.hash,hash,32);song.loop=~0u;
 if(kind==S2_CUSTOM_PCM){song.pcm=malloc(bytes);if(!song.pcm)return 0;memcpy(song.pcm,data,bytes);if(custom_loops(t))song.loop=0;}
 else if(kind==S2_CUSTOM_VGM){
  if(gme_open_data(data,(long)bytes,&song.vgm,44100))goto fail;
  gme_ignore_silence(song.vgm,1);gme_set_autoload_playback_limit(song.vgm,0);
  if(gme_start_track(song.vgm,0))goto fail;
  song.cache_at[0]=song.cache_at[1]=~0u;
  if(loop&&loop<=frames&&custom_loops(t)){
   /* GME already follows the file's intro/loop. Keep its timeline advancing
    * so every loop does not seek/re-emulate the intro on the audio thread.
    * Bound the counter below GME's signed stereo-sample API limit. */
   song.loop=0;song.frames=0x3fffffffu;
  }
 }else return 0;
 unsigned ticks=(unsigned)(((uint64_t)frames*53693175u+44100ULL*895780u-1)/(44100ULL*895780u));
 unsigned rests=(ticks+126)/127;size_t length=song.loop==~0u?54+rests*2+2:60;
 uint32_t dest=(*used+1)&~1u;if(dest>cap||length>cap-dest)goto fail;
 uint8_t *p=bank+dest;memset(p,0,length);p[2]=7;p[3]=3;p[6]=p[7]=1;
 unsigned end=(unsigned)length;p[0]=(uint8_t)(end>>8);p[1]=(uint8_t)end;
 for(unsigned i=0;i<10;i++){unsigned at=i<7?8+i*4:36+(i-7)*6;p[at+1]=54;}
 if(song.loop!=~0u){p[54]=0x80;p[55]=1;p[56]=0xFF;p[57]=0x14;p[58]=0xFF;p[59]=0xFB;}
 else{for(unsigned i=0;i<rests;i++){p[54+i*2]=0x80;unsigned duration=ticks>127?127:ticks;p[55+i*2]=(uint8_t)duration;ticks-=duration;}p[length-2]=0xFF;p[length-1]=t==S2_THEME_LIFE?0x04:0x10;}
 be32(bank+l->music_index-0x2A0000+(s2_music_target(t)-1)*4,0x01000000u|(0x2A0000u+dest));*used=dest+(uint32_t)length;
 if(songs[t].vgm)gme_delete(songs[t].vgm);free(songs[t].pcm);songs[t]=song;audio_set_extra_source(mix);return 1;
fail:
 if(song.vgm)gme_delete(song.vgm);free(song.pcm);if(error&&capacity)snprintf(error,capacity,"Unable to prepare the custom recording.");return 0;
}
