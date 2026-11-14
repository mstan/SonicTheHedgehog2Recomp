#include "sonic2_custom_music.h"
#include "sonic2_music.h"
#include "sonic2_cd_themes.h"
#include "sonic3_sound_legacy.h"
#include "host_atomic_file.h"
#include "sha256.h"
#include <SDL2/SDL.h>
#include <gme/gme.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#define make_directory(p) _mkdir(p)
#else
#include <sys/stat.h>
#define make_directory(p) mkdir(p,0700)
#endif
enum { LIMIT=64*1024*1024,HEADER=24 };
static char root[1024],settings_file[1200],paths[S2_MUSIC_COUNT][1024];
static uint8_t identities[S2_MUSIC_COUNT][32];
static int ready[S2_MUSIC_COUNT];
static unsigned le32(const uint8_t *p)
{return p[0]|((unsigned)p[1]<<8)|((unsigned)p[2]<<16)|((unsigned)p[3]<<24);}
static void put32(uint8_t *p,unsigned v)
{for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(i*8));}
/* Genesis VGM commands only. Verify every boundary, DAC read and loop before
 * asking a synthesizer to run untrusted data. A loop must advance audio time. */
static int vgm_info(const uint8_t *data,size_t size,unsigned *frames,unsigned *loop)
{
 if(size<=0x40||memcmp(data,"Vgm ",4))return 0;
 unsigned version=le32(data+8);if(version<0x100||version>0x171)return 0;
 size_t end=(size_t)le32(data+4)+4;if(end>size||end<=0x40)return 0;
 size_t pc=version>=0x150&&le32(data+0x34)?(size_t)le32(data+0x34)+0x34:0x40;
 size_t repeat=le32(data+0x1c)?(size_t)le32(data+0x1c)+0x1c:0;
 if(pc<0x40||pc>=end||(repeat&&(repeat<pc||repeat>=end)))return 0;
 uint64_t samples=0,loop_time=0;unsigned bank=0,at=0;int loop_seen=0;
 while(pc<end){
  if(pc==repeat){loop_seen=1;loop_time=samples;}
  unsigned op=data[pc++],bytes=0,wait=0;
  if(op==0x4f||op==0x50)bytes=1;
  else if(op==0x52||op==0x53)bytes=2;
  else if(op==0x61){if(end-pc<2)return 0;wait=data[pc]|((unsigned)data[pc+1]<<8);bytes=2;}
  else if(op==0x62)wait=735;
  else if(op==0x63)wait=882;
  else if(op>=0x70&&op<=0x7f)wait=(op&15)+1;
  else if(op>=0x80&&op<=0x8f){if(at>=bank)return 0;++at;wait=op&15;}
  else if(op==0xe0){if(end-pc<4)return 0;at=le32(data+pc);if(at>bank)return 0;bytes=4;}
  else if(op==0x67){
   if(end-pc<6||data[pc]!=0x66||data[pc+1]!=0)return 0;
   unsigned count=le32(data+pc+2);pc+=6;if(count>end-pc||count>LIMIT-bank)return 0;
   bank+=count;pc+=count;
  }else if(op==0x66){
   if(!samples||(repeat&&(!loop_seen||samples==loop_time)))return 0;
   *frames=(unsigned)samples;*loop=repeat?(unsigned)loop_time+1:0;return 1;
  }else return 0;
  if(bytes>end-pc)return 0;pc+=bytes;samples+=wait;
  if(samples>44100u*1800u)return 0;
 }
 return 0;
}
static uint8_t *read_file(const char *path,size_t *size)
{
 FILE *f=fopen(path,"rb");if(!f)return NULL;
 if(fseek(f,0,SEEK_END)){fclose(f);return NULL;}long n=ftell(f);rewind(f);
 if(n<6||n>LIMIT+HEADER){fclose(f);return NULL;}
 uint8_t *data=malloc((size_t)n);int ok=data&&fread(data,1,(size_t)n,f)==(size_t)n&&!ferror(f);
 fclose(f);if(!ok){free(data);return NULL;}*size=(size_t)n;return data;
}
static int convert_smps(unsigned track,const uint8_t *data,size_t bytes,uint8_t *out,size_t capacity,char *error,size_t error_size)
{
 if(bytes<6||bytes>=0x8000)return 0;
 S3LegacySource *source=calloc(1,sizeof *source);if(!source)return 0;
 source->rom=data;source->size=bytes;source->high=(uint32_t)bytes;source->env_count=13;source->require_restore=track==S2_THEME_LIFE;
 uint8_t drums[128]={0};int n=s3_legacy_song(source,out,capacity,0,NULL,drums,error,error_size);
 for(unsigned i=18;i<128;i++)if(drums[i])n=0;
 free(source);return n;
}
static int valid(unsigned track,const uint8_t *p,size_t n)
{
 if(n<HEADER||memcmp(p,"S2MUSIC1",8)||le32(p+12)!=n-HEADER)return 0;
 unsigned kind=le32(p+8),frames=le32(p+16),loop=le32(p+20);
 if(kind==S2_CUSTOM_SMPS){uint8_t *out=malloc(0x8000);int ok=out&&!frames&&!loop&&convert_smps(track,p+HEADER,n-HEADER,out,0x8000,NULL,0)>0;free(out);return ok;}
 if(!frames||frames>44100u*1800u||loop>frames)return 0;
 if(kind==S2_CUSTOM_PCM)return (uint64_t)frames*4==n-HEADER;
 unsigned actual=0,repeat=0;
 return kind==S2_CUSTOM_VGM&&vgm_info(p+HEADER,n-HEADER,&actual,&repeat)&&frames==actual&&loop==repeat;
}
int s2_custom_ready(unsigned t){return t<S2_MUSIC_COUNT&&ready[t];}
const char *s2_custom_path(unsigned t){return t<S2_MUSIC_COUNT?paths[t]:"";}
void s2_custom_identity(unsigned t,uint8_t out[32])
{if(t<S2_MUSIC_COUNT)memcpy(out,identities[t],32);else memset(out,0,32);}
int s2_custom_use(unsigned t,const char *path,char *error,size_t capacity)
{
 size_t n=0;uint8_t *p=NULL;
 if(t<S2_MUSIC_COUNT&&path&&strlen(path)<sizeof paths[t]&&!strpbrk(path,"\r\n"))p=read_file(path,&n);
 int ok=p&&valid(t,p,n);
 if(ok){recompui_sha256_compute(p,n,identities[t]);snprintf(paths[t],sizeof paths[t],"%s",path);ready[t]=1;}
 free(p);if(!ok&&error&&capacity)snprintf(error,capacity,"Choose an imported, complete custom music file.");return ok;
}
void s2_custom_load(const char *settings)
{
 memset(ready,0,sizeof ready);memset(identities,0,sizeof identities);memset(paths,0,sizeof paths);
 snprintf(settings_file,sizeof settings_file,"%s",settings?settings:"settings.ini");
 snprintf(root,sizeof root,"%s",settings_file);char *a=strrchr(root,'/'),*b=strrchr(root,'\\');if(!a||(b&&b>a))a=b;
 if(a)*a=0;else strcpy(root,".");size_t prefix=strlen(root);
 if(prefix+20>=sizeof root){root[0]=0;return;}strcat(root,"/sonic-custom-music");
 for(unsigned t=0;t<S2_MUSIC_COUNT;t++){
  char path[1024];int n=snprintf(path,sizeof path,"%s/%s.s2music",root,s2_theme_ids[t]);
  if(n>0&&(size_t)n<sizeof path)s2_custom_use(t,path,NULL,0);
 }
}
int s2_custom_import(unsigned t,const char *source,char *error,size_t capacity,
                     void (*progress)(void *,float,const char *),void *user)
{
 if(t>=S2_MUSIC_COUNT||!source||!*root)return 0;
 char compiled[1024];const char *extension=strrchr(source,'.');
 if(extension&&!SDL_strcasecmp(extension,".s2music")){snprintf(error,capacity,"Select the original WAV, VGM, BIN or ASM source.");return 0;}
 if(extension&&!SDL_strcasecmp(extension,".asm")){
  if(!s2_custom_assemble(source,s2_theme_ids[t],settings_file,compiled,error,capacity,progress,user))return 0;
  source=compiled;
 }
 size_t n=0;uint8_t *raw=read_file(source,&n),*payload=NULL;size_t bytes=0;
 unsigned kind=0,frames=0,loop=0;int ok=0;
 if(!raw){snprintf(error,capacity,"Unable to read music (64 MiB maximum).");return 0;}
 if(progress)progress(user,0.25f,"Preparing your custom music...");
 if(n>=12&&!memcmp(raw,"RIFF",4)&&!memcmp(raw+8,"WAVE",4)){
  SDL_AudioSpec spec;Uint8 *wave=NULL;Uint32 length=0;
  if(SDL_LoadWAV_RW(SDL_RWFromConstMem(raw,(int)n),1,&spec,&wave,&length)){
   SDL_AudioCVT cvt;int convert=SDL_BuildAudioCVT(&cvt,spec.format,spec.channels,spec.freq,AUDIO_S16LSB,2,44100);
   if(convert>=0&&cvt.len_mult>0&&length<=LIMIT/(unsigned)cvt.len_mult){
    cvt.buf=malloc((size_t)length*cvt.len_mult);cvt.len=(int)length;
    if(cvt.buf){memcpy(cvt.buf,wave,length);if(SDL_ConvertAudio(&cvt)==0){payload=cvt.buf;bytes=(size_t)cvt.len_cvt;frames=(unsigned)(bytes/4);kind=S2_CUSTOM_PCM;}else free(cvt.buf);}
   }
   SDL_FreeWAV(wave);
  }
 }else if(n>=0x40&&!memcmp(raw,"Vgm ",4)&&vgm_info(raw,n,&frames,&loop)){
  Music_Emu *emu=NULL;
  if(!gme_open_data(raw,(long)n,&emu,44100)){
   gme_ignore_silence(emu,1);gme_set_autoload_playback_limit(emu,0);
   short probe[64];if(!gme_start_track(emu,0)&&!gme_play(emu,64,probe)){payload=raw;raw=NULL;bytes=n;kind=S2_CUSTOM_VGM;}
  }
  if(emu)gme_delete(emu);
 }else if(n<0x8000){
  uint8_t *out=malloc(0x8000);
  int count=out?convert_smps(t,raw,n,out,0x8000,error,capacity):0;free(out);
  if(count>0){kind=S2_CUSTOM_SMPS;payload=raw;raw=NULL;bytes=n;}

 }
 if(kind&&bytes&&bytes<=LIMIT){
  uint8_t *pack=calloc(1,HEADER+bytes);
  if(pack){
   memcpy(pack,"S2MUSIC1",8);put32(pack+8,kind);put32(pack+12,(unsigned)bytes);put32(pack+16,frames);put32(pack+20,loop);memcpy(pack+HEADER,payload,bytes);
   char path[1024];int count=snprintf(path,sizeof path,"%s/%s.s2music",root,s2_theme_ids[t]);
   make_directory(root);
   if(valid(t,pack,HEADER+bytes)&&count>0&&(size_t)count<sizeof path&&genesis_atomic_file_store(path,pack,HEADER+bytes))ok=s2_custom_use(t,path,error,capacity);
   free(pack);
  }
 }
 free(payload);free(raw);
 if(!ok&&error&&capacity&&!*error)snprintf(error,capacity,"Use WAV, VGM, or Sonic 1 68K SMPS BIN/ASM with Sonic 2 PSG envelopes and DAC notes.");
 if(ok&&progress)progress(user,1,"Custom music imported. Restart to apply.");return ok;
}
int s2_custom_build(unsigned t,const S3SoundLayout *l,uint8_t *bank,size_t cap,uint32_t *used,char *error,size_t capacity)
{
 size_t n=0;uint8_t *p=t<S2_MUSIC_COUNT?read_file(paths[t],&n):NULL;int ok=0;
 if(!p||!valid(t,p,n))goto done;
 uint8_t digest[32];recompui_sha256_compute(p,n,digest);if(memcmp(digest,identities[t],32))goto done;
 if(le32(p+8)!=S2_CUSTOM_SMPS){ok=s2_cd_custom_build(t,le32(p+8),p+HEADER,n-HEADER,le32(p+16),le32(p+20),digest,l,bank,cap,used,error,capacity);goto done;}
 uint32_t dest=(*used+1)&~1u;size_t length=0;
 if(dest>cap||cap-dest<0x8000||l->music_index<0x2A0000)goto done;
 int converted=convert_smps(t,p+HEADER,n-HEADER,bank+dest,cap-dest,error,capacity);if(converted<=0)goto done;length=(size_t)converted;*used=dest+(uint32_t)length;
 unsigned target=s2_music_target(t);uint8_t *entry=bank+l->music_index-0x2A0000+(target-1)*4;
 unsigned address=0x01000000u|(0x2A0000u+dest);for(unsigned i=0;i<4;i++)entry[i]=(uint8_t)(address>>(24-i*8));ok=1;
done:
 free(p);if(!ok&&error&&capacity&&!*error)snprintf(error,capacity,"Unable to prepare custom music for %s.",s2_theme_names[t]);return ok;
}
