#ifdef NDEBUG
#undef NDEBUG
#endif
#include "sonic2_music.h"
#include "sonic3_sound_legacy.h"
#include "sha256.h"
#include "sonic2_sound_assets.h"
#include "sonic2_cd_themes.h"
#include "sonic2_custom_music.h"
#include "sonic2_vgm.h"
#include "sonic2_state_io.h"
#include "audio.h"
#include "app_config.h"
AppConfig g_app_config;
#include "recomp_launcher.h"
#include <SDL2/SDL.h>
#include <gme/gme.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sonic2_sound_sequence.h"
static AudioExtraSource extra;
void audio_set_extra_source(AudioExtraSource source){extra=source;}
static void envset(const char *name,const char *value)
{
#ifdef _WIN32
 _putenv_s(name,value);
#else
 setenv(name,value,1);
#endif
}
static unsigned track(const char *id)
{for(unsigned t=0;t<S2_MUSIC_COUNT;t++)if(!strcmp(id,s2_theme_ids[t]))return t;assert(0);return 0;}
#ifdef SONIC2_MUSIC_DRIVER
static S2StockSound *stock_from_file(const char *path)
{
 FILE *file=fopen(path,"rb");assert(file);
 uint8_t *rom=malloc(0x100000);assert(rom);
 assert(fread(rom,1,0x100000,file)==0x100000 && fgetc(file)==EOF);
 fclose(file);
 uint8_t before[32],after[32];
 recompui_sha256_compute(rom,0x100000,before);
 char error[256]={0};
 S2StockSound *stock=s2_sound_decode_stock(rom,0x100000,error,sizeof error);
 if(!stock)fprintf(stderr,"%s\n",error);
 assert(stock);
 recompui_sha256_compute(rom,0x100000,after);
 assert(!memcmp(before,after,sizeof before));
 free(rom);
 return stock;
}
static void count_events(void *context,uint32_t cycle,uint8_t chip,uint8_t address,uint8_t value)
{
 (void)chip;(void)address;(void)value;
 assert(cycle<895780);
 ++*(unsigned *)context;
}
static void play_cue(const S2StockSound *stock,unsigned target)
{
 S2SoundSequence playback;
 s2_sequence_reset(&playback);
 unsigned events=0;
 for(unsigned tick=0;tick<1800;++tick) {
  uint8_t queue[5]={0};
  if(!tick)queue[0]=(uint8_t)(target+0x80);
  if(tick%61==11)queue[1]=0xA0;
  if(tick==48)queue[0]=0xFE;
  if(tick==52)queue[0]=0xFF;
  if(tick==150)queue[0]=0xFB;
  if(tick==250)queue[0]=0xFC;
  if(tick==400 && target!=0x18)queue[0]=0x98;
  int ok=s2_sequence_frame(&playback,stock,queue,0,53693175,895780,count_events,&events);
  if(!ok){
   unsigned channel=playback.fault_channel;
   const S2SoundTrack *fault=playback.fault_cue>=0xA0?&playback.effect[channel]:&playback.music[channel];
   S2MusicCue cue=s2_sound_music_cue(stock,playback.fault_cue);
   unsigned pc=playback.fault_offset;
   fprintf(stderr,"Queue $%02X tick %u: unsupported $%02X channel %u at $%04X; "
                  "envelope %u at %u, modulation %u at %u, phrase depth %u\n",
           target+0x80,tick,playback.fault_cue,channel,pc,fault->voice,fault->envelope_position,
           fault->mod_envelope,fault->mod_position,fault->depth);
   if(pc>=4 && pc<=cue.bytes)fprintf(stderr,"Previous cue bytes: %02X %02X %02X %02X\n",
                                     cue.data[pc-4],cue.data[pc-3],cue.data[pc-2],cue.data[pc-1]);
  }
  assert(ok && !queue[0] && !queue[1]);
 }
 assert(events);
}
static void competition_loops(S2MusicCue cue,unsigned channel)
{
 /* Follow the converted stream through an actual backward jump. Both the
  * main voice and its delayed echo must begin each pass with the same setup. */
 const uint8_t *rom=cue.data;
 unsigned pc=((unsigned)rom[8+4*channel]<<8)|rom[9+4*channel];
 unsigned pass=0,heard=0,voice=255,detune=255;uint8_t modulation[4]={0};
 static const uint8_t expected[]={15,1,6,6};
 for(unsigned guard=0;guard<4096;guard++){
  assert(pc+6<cue.bytes);unsigned op=rom[pc++];
  if(op==0xFF){
   unsigned command=rom[pc++];
   if(command==0x0D)voice=rom[pc++];
   else if(command==1)detune=rom[pc++];
   else if(command==0x21){memcpy(modulation,rom+pc,4);pc+=4;}
   else if(command==0x14){
    unsigned target=(unsigned)((int)pc+1+(int16_t)(((unsigned)rom[pc]<<8)|rom[pc+1]));
    if(target<pc){assert(heard);pass++;heard=0;}
    pc=target;
   }else assert(!"Unexpected Competition Menu coordination command");
  }else if(op>=0x81&&!heard){
   assert(op==0xBB&&voice==0x0D&&detune==3&&!memcmp(modulation,expected,4));
   if(pass==1)return;
   heard=1;
  }
 }
 assert(!"Competition Menu did not complete a loop");
}
#endif
static void vgm_dac_stereo(void)
{
 enum { WRITES=128, WINDOW=WRITES*16 };
 for(int encoded=0;encoded<=1;encoded++){
  uint8_t vgm[2048]={0};size_t pos=0x40;
  memcpy(vgm,"Vgm ",4);vgm[8]=0x50;vgm[9]=1;
  vgm[0x2c]=0xb6;vgm[0x2d]=0x0a;vgm[0x2e]=0x75;
  if(encoded){
   const uint8_t block[]={0x67,0x66,0,WRITES,0,0,0};
   memcpy(vgm+pos,block,sizeof block);pos+=sizeof block;
   for(unsigned i=0;i<WRITES;i++)vgm[pos++]=i&4?202:54;
  }
  vgm[pos++]=0x52;vgm[pos++]=0x2b;vgm[pos++]=0x80;
  for(unsigned window=0;window<3;window++){
   const uint8_t pan[]={0x80,0x40,0xc0};
   vgm[pos++]=0x53;vgm[pos++]=0xb6;vgm[pos++]=pan[window];
   if(window==2){vgm[pos++]=0x52;vgm[pos++]=0x2b;vgm[pos++]=0;}
   if(encoded){
    const uint8_t seek[]={0xe0,0,0,0,0};
    memcpy(vgm+pos,seek,sizeof seek);pos+=sizeof seek;
    for(unsigned i=0;i<WRITES;i++){
     vgm[pos++]=0x8f;vgm[pos++]=0x70;
    }
   }else for(unsigned i=0;i<WRITES;i++){
    vgm[pos++]=0x52;vgm[pos++]=0x2a;vgm[pos++]=i&4?202:54;vgm[pos++]=0x7f;
   }
  }
  vgm[pos++]=0x66;
  vgm[4]=(uint8_t)(pos-4);vgm[5]=(uint8_t)((pos-4)>>8);
  for(unsigned mute=0;mute<2;mute++){
   Music_Emu *emu=NULL;int16_t pcm[WINDOW*3*2];
   assert(!gme_open_data(vgm,(long)pos,&emu,44100));
   gme_ignore_silence(emu,1);gme_set_autoload_playback_limit(emu,0);
   assert(!gme_start_track(emu,0));gme_mute_voices(emu,mute?1<<6:0);
   assert(!gme_play(emu,WINDOW*3*2,pcm));gme_delete(emu);
   for(unsigned window=0;window<3;window++)for(unsigned ch=0;ch<2;ch++){
    unsigned energy=0;
    for(unsigned i=window*WINDOW+256;i<(window+1)*WINDOW-256;i++)
     energy+=(unsigned)abs(pcm[i*2+ch]-pcm[(i-1)*2+ch]);
    assert((!mute&&window==ch)?energy>10000:energy<1000);
   }
  }
 }
 puts("PASS direct and banked DAC stereo routing, disable gate and voice mute");
}
int main(int argc,char **argv)
{
 if(argc==2&&!strcmp(argv[1],"--vgm-dac")){vgm_dac_stereo();return 0;}
 /* Mixed-chip VGMs expose sounding voices, including separate PSG tones
  * and noise. Scanning malformed input must fail. */
 {
  uint8_t vgm[128]={0};memcpy(vgm,"Vgm ",4);vgm[8]=0x50;vgm[9]=1;
  const uint8_t commands[]={0x52,0x28,0xf0,0x52,0x28,0xf6,0x52,0x2b,0x80,0x52,0x2a,0x80,
   0x50,0x90,0x50,0xb0,0x50,0xd0,0x50,0xf0,0x62,0x66};
  size_t bytes=0x40+sizeof commands;vgm[4]=(uint8_t)(bytes-4);memcpy(vgm+0x40,commands,sizeof commands);
  assert(s2_custom_vgm_channels(vgm,bytes)==((1u<<0)|(1u<<5)|(1u<<6)|0x780u));
  vgm[0x40]=0xff;assert(!s2_custom_vgm_channels(vgm,bytes));
 }
#ifdef SONIC2_MUSIC_DRIVER
 if(argc==6&&(!strcmp(argv[1],"--custom-import")||!strcmp(argv[1],"--custom-reject")||!strcmp(argv[1],"--vgm-voices")||!strcmp(argv[1],"--vgm-transport"))){
  int reject=!strcmp(argv[1],"--custom-reject");char settings[1200],error[256]={0};
  envset("GENESIS_USER_DIR",argv[2]);snprintf(settings,sizeof settings,"%s/settings.ini",argv[2]);s2_music_load(settings);
  unsigned t=track(argv[3]);assert(s2_music_pick(t,"custom"));
  uint8_t before[32],after[32];s2_custom_identity(t,before);
  const RecompLauncherCModProvider *provider=s2_music_mods(NULL);
  const char *package="sonic2.audio.event-themes";
  const char *group=s2_music_group(t)==0?"shared-themes":s2_music_group(t)==1?"sonic2-campaign":"competition";
  RecompLauncherCModResource resource;
  assert(provider->feature_resource_get(provider->ctx,package,group,0,&resource));
  assert(strstr(resource.file_patterns,"*.vgz"));
  int imported=provider->feature_resource_import(provider->ctx,package,group,resource.id,argv[4],argv[5],NULL,NULL,error,sizeof error);
  if(!imported&&!reject)fprintf(stderr,"Custom music import: %s\n",error);
  if(reject){assert(!imported&&s2_custom_ready(t));s2_custom_identity(t,after);assert(!memcmp(before,after,32));puts("PASS rejected music; previous selection retained");return 0;}
  assert(imported&&s2_music_active(t)&&s2_theme_ready(t)&&s2_themes_save());
  s2_custom_identity(t,before);s2_music_load(settings);assert(s2_music_active(t)&&s2_theme_ready(t));
  s2_custom_identity(t,after);assert(!memcmp(before,after,32));
  assert(provider->feature_resource_count(provider->ctx,package,group)==1);
  assert(provider->feature_resource_get(provider->ctx,package,group,0,&resource)&&resource.verified);
  assert(strstr(resource.label,"custom music")&&!strncmp(resource.id,"custom-",7));
  S2StockSound *stock=stock_from_file(argv[5]);S2MusicLibrary library={0};
  assert(s2_music_prepare(stock,&library,error,sizeof error));
  S2MusicCue cue=s2_sound_music_cue(stock,s2_music_target(t)+0x80);
  if(extra && s2_cd_theme_channel_count(t)!=11)
   assert(cue.bytes>=14 && cue.data[2]==1 && cue.data[3]==0 && cue.data[9]==12);
  play_cue(stock,s2_music_target(t));
  if(!strcmp(argv[1],"--vgm-transport")){
   unsigned dac=0;
   for(unsigned ch=1;ch<s2_cd_theme_channel_count(t);ch++)
    if(!strcmp(s2_cd_theme_channel_name(t,ch),"DAC"))dac=ch;
   assert(extra&&dac);
   uint64_t start=SDL_GetPerformanceCounter();
   s2_cd_theme_request(s2_music_target(t));
   for(unsigned i=0;i<3600;i++){s2_cd_theme_frame(0,0);s2_cd_theme_tick(i);}
   uint64_t sixty=SDL_GetPerformanceCounter();
   S2StateIO measured={0};s2_cd_theme_state(&measured);
   uint8_t *bytes=malloc(measured.pos);assert(bytes);
   S2StateIO save={bytes,measured.pos,0,0,1};s2_cd_theme_state(&save);assert(save.ok);
   S2StateIO check={bytes,save.pos,0,1,1};s2_cd_theme_state(&check);assert(check.ok);
   s2_cd_theme_frame(0,0);s2_cd_theme_tick(3600);
   int16_t normal[1400]={0},back[1400]={0},faded[1400]={0};
   extra(normal,700,44100);
   uint64_t restore_from=SDL_GetPerformanceCounter();
   S2StateIO restore={bytes,save.pos,0,2,1};s2_cd_theme_state(&restore);assert(restore.ok);
   s2_cd_theme_frame(0,0);s2_cd_theme_tick(3600);extra(back,700,44100);
   uint64_t restore_end=SDL_GetPerformanceCounter();
   assert(!memcmp(normal,back,sizeof normal));
   restore.pos=0;s2_cd_theme_state(&restore);assert(restore.ok);
   uint64_t paused,resume;
   s2_cd_theme_debug(NULL,NULL,&paused);
   s2_cd_theme_frame(0,1);s2_cd_theme_tick(3600);
   s2_cd_theme_debug(NULL,NULL,&resume);assert(paused==resume);
   s2_cd_theme_frame(0,0);s2_cd_theme_tick(3601);extra(faded,700,44100);
   unsigned energy=0;
   for(unsigned i=0;i<1400;i++){
    energy+=(unsigned)abs(normal[i]);
    assert(faded[i]==(int32_t)normal[i]*2072/65536);
   }
   assert(energy);
   uint64_t channel_from=SDL_GetPerformanceCounter();
   s2_cd_theme_audition(t,dac,0);s2_cd_theme_frame(0,0);s2_cd_theme_tick(3602);
   memset(back,0,sizeof back);extra(back,700,44100);
   uint64_t channel_end=SDL_GetPerformanceCounter();
   /* Repeat the restart with the same solo selection. Then confirm All is
    * a different signal over that same prefix. */
   enum { RESTART_TICKS=120 };
   int16_t (*prefix)[1400]=calloc(RESTART_TICKS,sizeof *prefix);assert(prefix);
   int different=0;
   for(unsigned pass=0;pass<3;pass++){
    if(pass!=1)s2_cd_theme_audition(t,pass==2?0:dac,0);
    s2_cd_theme_request(s2_music_target(t));
    for(unsigned i=0;i<RESTART_TICKS;i++){
     s2_cd_theme_frame(0,0);s2_cd_theme_tick(i);
     memset(back,0,sizeof back);extra(back,700,44100);
     if(pass==0)memcpy(prefix[i],back,sizeof back);
     else if(pass==1)assert(!memcmp(prefix[i],back,sizeof back));
     else different|=memcmp(prefix[i],back,sizeof back)!=0;
    }
   }
   assert(different);free(prefix);
   double tick=1000.0/SDL_GetPerformanceFrequency();
   printf("VGM transport: sixty seconds %.3f ms, restore and render %.3f ms, channel switch and render %.3f ms, snapshot %zu bytes\n",
      (sixty-start)*tick,(restore_end-restore_from)*tick,(channel_end-channel_from)*tick,save.pos);
   free(bytes);s2_music_release(stock,&library);free(stock);return 0;
  }
  if(!strcmp(argv[1],"--vgm-voices")){
   static const char *names[]={"ALL CHANNELS","FM 1","PSG 1","PSG 2","PSG 3","PSG NOISE"};
   static const unsigned ids[]={0,1,8,9,10,11};
   int16_t voices[6][2048]={{0}},restored[2048]={0};assert(extra&&s2_cd_theme_channel_count(t)==6&&!s2_cd_theme_native_channels(t));
   for(unsigned solo=0;solo<6;solo++){
    assert(!strcmp(s2_cd_theme_channel_name(t,solo),names[solo]));
    assert(s2_cd_theme_channel_id(t,solo)==ids[solo]);
    s2_cd_theme_audition(t,solo,0);
    s2_cd_theme_request(s2_music_target(t));s2_cd_theme_frame(0,0);s2_cd_theme_tick(0);extra(voices[solo],1024,44100);
    int nonzero=0;for(unsigned i=0;i<2048;i++)nonzero|=voices[solo][i];assert(nonzero);
    for(unsigned i=0;i<solo;i++)assert(memcmp(voices[i],voices[solo],sizeof voices[solo]));
   }
   s2_cd_theme_audition(t,0,0);s2_cd_theme_request(s2_music_target(t));s2_cd_theme_frame(0,0);s2_cd_theme_tick(0);extra(restored,1024,44100);
   assert(!memcmp(voices[0],restored,sizeof restored));
   puts("PASS mixed VGM: active FM/individual PSG isolation and exact All restoration");s2_music_release(stock,&library);free(stock);return 0;
  }
  if(extra&&s2_cd_theme_channel_count(t)!=11){
   s2_cd_theme_request(s2_music_target(t));s2_cd_theme_frame(0,t==0);s2_cd_theme_tick(0);
   int16_t pcm[2048]={0},again[2048]={0};extra(pcm,1024,44100);
   int nonzero=0;for(unsigned i=0;i<2048;i++)nonzero|=pcm[i];assert(nonzero);
   S2StateIO size={0};s2_cd_theme_state(&size);
   uint8_t *state=malloc(size.pos);assert(state);
   S2StateIO save={state,size.pos,0,0,1};s2_cd_theme_state(&save);assert(save.ok);
   s2_cd_theme_tick(1);memset(pcm,0,sizeof pcm);extra(pcm,1024,44100);s2_cd_theme_tick(2);
   S2StateIO load={state,save.pos,0,2,1};s2_cd_theme_state(&load);assert(load.ok);s2_cd_theme_tick(1);
   extra(again,1024,44100);assert(!memcmp(pcm,again,sizeof pcm));free(state);
   s2_cd_theme_audition(t,0,1);memset(again,0,sizeof again);extra(again,1024,44100);for(unsigned i=0;i<2048;i++)assert(!again[i]);
   if(s2_cd_theme_channel_count(t)>1&&t==track("ehz")){
    for(unsigned i=0;i<180;i++)s2_cd_theme_tick(i);
    uint64_t position=0;s2_cd_theme_debug(NULL,NULL,&position);assert(position>(44100ULL<<32));
   }
  }
  printf("PASS custom import, persistence, bank and deterministic replay: %s\n",argv[4]);s2_music_release(stock,&library);free(stock);return 0;
 }
#endif
 if(argc==4&&!strcmp(argv[1],"--source-map")){
  unsigned donor=(unsigned)strtoul(argv[2],NULL,10);size_t size=donor?0x100000:0x80000;
  uint8_t *data=malloc(size),*song=malloc(0x8000);S3LegacySource *source=malloc(sizeof *source);
  FILE *file=fopen(argv[3],"rb");assert(data&&song&&source&&file&&fread(data,1,size,file)==size&&fgetc(file)==EOF);fclose(file);
  for(unsigned i=0;i<(donor?31u:19u);++i){
   uint8_t drums[128]={0},digest[32];char error[256]={0},hash[65];
   assert(s3_legacy_music_source(data,size,donor,i,source));
   int n=s3_legacy_song(source,song,0x8000,0,NULL,drums,error,sizeof error);assert(n>0);
   song[7]=0; /* Match composition independently of the source tempo rule. */
   recompui_sha256_compute(song,(size_t)n,digest);for(unsigned k=0;k<32;k++)snprintf(hash+2*k,3,"%02x",digest[k]);
   printf("SOURCE %u %u %06X %d %s\n",donor,i,source->source,n,hash);
  }
  free(source);free(song);free(data);return 0;
 }
 assert(argc==2||argc==4);
 envset("GENESIS_USER_DIR",argv[1]);
 if(argc==2)envset("SONIC_SOUND_DONOR_LIBRARY",argv[1]);
 char settings[1200];snprintf(settings,sizeof settings,"%s/settings.ini",argv[1]);
 s2_music_load(settings);
 assert(!s2_themes_enabled());
 assert(!s2_music_group_enable(S2_MUSIC_CAMPAIGN,1));
 unsigned ehz=track("ehz"),cpz=track("cpz"),boss=track("boss"),inv=track("invincibility");
 assert(s2_theme_choices(ehz)==2);
 assert(!s2_music_pick(ehz,"s3"));
 for(unsigned t=0;t<S2_MUSIC_COUNT;t++){
  assert(s2_music_target(t)>=1&&s2_music_target(t)<=31);
  for(unsigned j=0;j<t;j++)assert(s2_music_target(j)!=s2_music_target(t));
  if(!strcmp(s2_music_section(t),"Zones"))
   for(unsigned c=0;c<(unsigned)s2_theme_choices(t);c++)assert(s2_theme_choice(t,c)->donor==S2_DONOR_CUSTOM||s2_theme_choice(t,c)->donor==S2_DONOR_WAI||s2_theme_choice(t,c)->donor==S2_DONOR_AUG||(!strcmp(s2_theme_ids[t],"ehz-2p")&&s2_theme_choice(t,c)->donor==S2_DONOR_SEP14)||(!strcmp(s2_theme_ids[t],"ooz")&&!strcmp(s2_theme_choice(t,c)->id,"s2-aug-oil-ocean")));
 }
 assert(s2_theme_choices(inv)==9);
 assert(s2_music_available(track("ending"))&&!s2_music_available(track("hidden-palace")));
 unsigned emerald=track("emerald");
 assert(s2_theme_choices(emerald)==1);
 assert(!strcmp(s2_theme_choice(emerald,0)->id,"sk")&&s2_theme_choice(emerald,0)->song==0x2B);
 assert(!s2_music_pick(emerald,"custom")&&!s2_music_pick(emerald,"s2-aug"));
 const char *new_tracks[]={"game-over","continue","competition-menu","ending"};
 const char *new_choices[]={"s3-proto","s3-proto","s3-proto-continue","s3-proto-sky-sanctuary"};
 const unsigned new_songs[]={0x27,0x28,0x28,0x15};
 for(unsigned i=0;i<4;i++){
  unsigned t=track(new_tracks[i]);assert(s2_music_pick(t,new_choices[i]));
  assert(s2_theme_choice(t,(unsigned)s2_themes[t].choice)->song==new_songs[i]);
  assert(s2_themes_save());s2_music_load(settings);
  assert(s2_themes[t].enabled&&!strcmp(s2_theme_choice(t,(unsigned)s2_themes[t].choice)->id,new_choices[i]));
  assert(s2_music_pick(t,"base"));
 }
 for(unsigned t=0;t<S2_MUSIC_COUNT;t++)for(unsigned c=0;c<(unsigned)s2_theme_choices(t);c++){
  const S2MusicChoice *choice=s2_theme_choice(t,c);
  for(unsigned j=0;j<c;j++){
   assert(strcmp(choice->id,s2_theme_choice(t,j)->id));
   assert(strcmp(choice->label,s2_theme_choice(t,j)->label));
  }
 }
 const struct RecompLauncherCModProvider *provider=s2_music_mods(NULL);
 assert(provider&&provider->package_count(provider->ctx)==1&&provider->feature_count(provider->ctx)==3);
 unsigned covered=0;
 for(int g=0;g<3;g++){
  RecompLauncherCModFeature feature;assert(provider->feature_get(provider->ctx,g,&feature));
  assert(feature.option_count==(g==0?13:g==1?13:4));
  for(int i=0;i<feature.option_count;i++){
   RecompLauncherCModOption option;assert(provider->feature_option_get(provider->ctx,feature.package_id,feature.id,i,&option));
   unsigned t=track(option.id);assert(s2_music_group(t)==(unsigned)g&&!(covered&(1u<<t)));covered|=1u<<t;
   assert(!strcmp(option.value,"base")&&option.choice_count==s2_theme_choices(t)+1);
   for(int c=0;c<option.choice_count;c++){
    RecompLauncherCModChoice choice;assert(provider->feature_choice_get(provider->ctx,feature.package_id,feature.id,option.id,c,&choice));
    assert(!strcmp(choice.value,c?s2_theme_choice(t,(unsigned)c-1)->id:"base"));
   }
  }
 }
 assert(covered==(0x7FFFFFFFu&~(1u<<track("hidden-palace"))));
 assert(s2_music_pick(inv,"s3-proto-unused"));
 assert(s2_theme_choice(inv,(unsigned)s2_themes[inv].choice)->song==0x2E);
 assert(s2_themes_save());s2_music_load(settings);
 assert(!strcmp(s2_theme_choice(inv,(unsigned)s2_themes[inv].choice)->id,"s3-proto-unused"));
 assert(s2_music_pick(inv,"base"));
 unsigned title=track("title");
 assert(s2_music_title_frames()==640);
 assert(s2_music_pick(title,"s3")&&s2_music_title_frames()==(s2_donor_verified(S2_DONOR_S3)?1800u:640u));
 assert(s2_music_group_enable(S2_MUSIC_SHARED,0)&&s2_music_title_frames()==640);
 assert(s2_music_pick(title,"base"));
 unsigned oil=track("ooz");
 assert(s2_music_pick(oil,"s2-aug-oil-ocean")&&s2_theme_choice(oil,(unsigned)s2_themes[oil].choice)->song==7);
 assert(s2_music_native_choice(oil)&&s2_theme_ready(oil)&&s2_theme_donor(oil)==-1);
 assert(s2_music_group_enable(S2_MUSIC_CAMPAIGN,1)&&s2_music_active(oil));
 assert(provider->feature_resource_count(provider->ctx,"sonic2.audio.event-themes","sonic2-campaign")==0);
 assert(s2_themes_save());s2_music_load(settings);assert(s2_music_native_choice(oil)&&s2_theme_ready(oil));
 assert(s2_music_pick(oil,"base"));
 unsigned credits=track("credits");assert(s2_music_pick(credits,"s3c-may17"));
 assert(s2_theme_choice(credits,(unsigned)s2_themes[credits].choice)->donor==S2_DONOR_MAY17&&s2_theme_choice(credits,(unsigned)s2_themes[credits].choice)->song==0x26);
 assert(s2_themes_save());s2_music_load(settings);assert(!strcmp(s2_theme_choice(credits,(unsigned)s2_themes[credits].choice)->id,"s3c-may17"));
 assert(!s2_music_pick(track("drowning"),"s3c-may17"));assert(s2_music_pick(credits,"base"));
 unsigned final=track("final-boss");assert(s2_music_pick(final,"s3"));
 const S2MusicChoice *final_choice=s2_theme_choice(final,(unsigned)s2_themes[final].choice);
 assert(final_choice->song==0x30&&!strcmp(final_choice->label,"Sonic 3 & Knuckles"));assert(s2_music_pick(final,"base"));
 if(argc==2){
  assert(s2_music_pick(ehz,"s2-wai"));
  assert(s2_music_pick(cpz,"s2-wai"));
  assert(provider->feature_resource_count(provider->ctx,"sonic2.audio.event-themes","sonic2-campaign")==1);
  assert(s2_themes[ehz].enabled&&!s2_music_group_enabled(S2_MUSIC_CAMPAIGN));
  assert(!s2_music_group_enable(S2_MUSIC_CAMPAIGN,1));
  assert(s2_themes_save());s2_music_load(settings);
  assert(s2_themes[ehz].enabled&&!s2_music_group_enabled(S2_MUSIC_CAMPAIGN));
  puts("PASS catalog, stage isolation, missing donor, pending selection and persistence");return 0;
 }
#ifdef SONIC2_MUSIC_DRIVER
 envset("SONIC_SOUND_DONOR_LIBRARY",argv[3]);s2_music_load(settings);
 assert(s2_donor_verified(S2_DONOR_WAI)&&s2_donor_verified(S2_DONOR_AUG));
 /* Resource rows precede options in recorded setup, including CD manifests.
  * Valid paths can be adopted read-only before any track requires them. */
 assert(provider->feature_resource_set_path(provider->ctx,"sonic2.audio.event-themes","shared-themes","cd-us",s2_donor_path(S2_DONOR_CD_US)));
 char wai_path[1024];snprintf(wai_path,sizeof wai_path,"%s",s2_donor_path(S2_DONOR_WAI));
 assert(!s2_music_donor_use(S2_DONOR_WAI,s2_donor_path(S2_DONOR_AUG)));
 assert(s2_donor_verified(S2_DONOR_WAI)&&!strcmp(wai_path,s2_donor_path(S2_DONOR_WAI)));
 assert(s2_music_pick(cpz,"s2-wai")&&s2_theme_choice(cpz,(unsigned)s2_themes[cpz].choice)->song==12);
 assert(s2_music_pick(boss,"s2-wai")&&s2_theme_choice(boss,(unsigned)s2_themes[boss].choice)->song==13);
 assert(s2_themes_enabled());
 assert(s2_music_group_enable(S2_MUSIC_SHARED,0)&&s2_themes[boss].enabled);
 assert(s2_themes_save());s2_music_load(settings);
 assert(s2_themes[boss].enabled&&!s2_music_group_enabled(S2_MUSIC_SHARED));
 S2StockSound *stock=stock_from_file(argv[2]);S2MusicLibrary library={0};
 char error[256]={0};
 uint8_t baseline[32],unchanged[32];
 recompui_sha256_compute((const uint8_t *)stock+offsetof(S2StockSound,music_bytes),
                         sizeof *stock-offsetof(S2StockSound,music_bytes),baseline);
 for(unsigned d=0;d<S2_DONOR_COUNT;d++){
  assert(s2_donor_verified(d));
  unsigned selected=0;memset(s2_themes,0,sizeof s2_themes);
  for(unsigned g=0;g<S2_MUSIC_GROUPS;g++)assert(s2_music_group_enable(g,0));
  for(unsigned t=0;t<S2_MUSIC_COUNT;t++)
   for(unsigned c=0;c<(unsigned)s2_theme_choices(t);c++)if(s2_theme_choice(t,c)->donor==d){
    assert(s2_music_pick(t,s2_theme_choice(t,c)->id));
    if(d==S2_DONOR_CD_US&&(t==S2_MUSIC_BOSS||t==S2_MUSIC_FINAL_BOSS)&&!s2_theme_ready(t)){assert(s2_music_pick(t,"base"));break;}
    ++selected;break;
   }
  if(d==S2_DONOR_S3)assert(s2_music_pick(boss,"s3-miniboss"));
  if(d==S2_DONOR_PROTO)assert(s2_music_pick(track("options"),"s3-proto-competition"));
  if(!selected)continue;
  uint8_t before[32],after[32];s2_music_config_identity(before);
  if(!s2_music_prepare(stock,&library,error,sizeof error)){
   fprintf(stderr,"FAIL donor %s: %s\n",s2_donor_ids[d],error);return 1;
  }
  fprintf(stderr,"Exercising donor %s with %u selected cues\n",s2_donor_ids[d],selected);
  s2_music_config_identity(after);assert(!memcmp(before,after,32));
  recompui_sha256_compute((const uint8_t *)stock+offsetof(S2StockSound,music_bytes),
                          sizeof *stock- offsetof(S2StockSound,music_bytes),unchanged);
  assert(!memcmp(baseline,unchanged,32));
  if(d==S2_DONOR_PROTO){
   S2MusicCue cue=s2_sound_music_cue(stock,s2_music_target(track("options"))+0x80);
   competition_loops(cue,2);competition_loops(cue,5);
   S2SoundSequence opening;
   s2_sequence_reset(&opening);
   uint8_t queue[5]={0x9B};
   unsigned events=0;
   assert(s2_sequence_frame(&opening,stock,queue,0,53693175,895780,count_events,&events));
   assert(opening.music[6].mod_envelope==3 && opening.music[7].mod_envelope==3);
  }
  if(d==S2_DONOR_S3||d==S2_DONOR_SK||d==S2_DONOR_MAY17){
   const S2SoundEnvelope *envelope=&library.donors[d]->modulation[3];
   assert(envelope->bytes==14 && envelope->data[12]==0x82 && envelope->data[13]==9);
  }
  if(d==S2_DONOR_SK||d==S2_DONOR_MAY17){
   S2MusicCue cue=s2_sound_music_cue(stock,s2_music_target(track("credits"))+0x80);
   unsigned fm1=((unsigned)cue.data[12]<<8)|cue.data[13];
   assert(fm1+2<cue.bytes);
   assert(cue.data[fm1]==0xFF&&cue.data[fm1+1]==0x1B&&cue.data[fm1+2]==0x0C);
  }
  for(unsigned t=0;t<S2_MUSIC_COUNT;t++)if(s2_music_active(t)) {
   unsigned target=s2_music_target(t);
   S2MusicCue cue=s2_sound_music_cue(stock,target+0x80);
   assert(cue.data==stock->selected_music[target-1].data && cue.data!=stock->music[target-1]);
   if(s2_music_native_choice(t))assert(cue.bytes==stock->music_bytes[7] && !memcmp(cue.data,stock->music[7],cue.bytes));
   play_cue(stock,target);
  }
  if(d==S2_DONOR_CD_US){
   S2MusicCue cue=s2_sound_music_cue(stock,s2_music_target(S2_THEME_CLEAR)+0x80);
   assert(cue.bytes>=14 && cue.data[2]==1 && cue.data[3]==0);
   assert(extra);int16_t pcm[512]={0};s2_cd_theme_request(0x17);s2_cd_theme_frame(0,0);s2_cd_theme_tick(0);extra(pcm,256,44100);
   if(s2_music_active(S2_MUSIC_FINAL_BOSS)){
    int16_t boss_pcm[512]={0},final_pcm[512]={0};unsigned theme=0;
    s2_cd_theme_request(0x13);s2_cd_theme_frame(0,0);s2_cd_theme_tick(0);extra(boss_pcm,256,44100);
    s2_cd_theme_request(0x14);s2_cd_theme_frame(0,0);s2_cd_theme_tick(0);extra(final_pcm,256,44100);
    s2_cd_theme_debug(NULL,&theme,NULL);assert(theme==S2_MUSIC_FINAL_BOSS);
    assert(!memcmp(boss_pcm,final_pcm,sizeof boss_pcm));
    s2_cd_theme_request(0x17);s2_cd_theme_frame(0,0);s2_cd_theme_tick(0);
   }
   unsigned active=0;s2_cd_theme_debug(&active,NULL,NULL);assert(active);
   S2StateIO size={0};s2_cd_theme_state(&size);
   uint8_t *state=malloc(size.pos);assert(state);
   S2StateIO save={state,size.pos,0,0,1};s2_cd_theme_state(&save);assert(save.ok);
   uint64_t position=0;s2_cd_theme_debug(NULL,NULL,&position);
   assert(position>0);extra(pcm,256,44100);uint64_t held;s2_cd_theme_debug(NULL,NULL,&held);assert(position==held);
   s2_cd_theme_frame(1,0);s2_cd_theme_tick(1);memset(pcm,0,sizeof pcm);extra(pcm,256,44100);
   for(unsigned i=0;i<512;i++)assert(!pcm[i]);s2_cd_theme_debug(NULL,NULL,&held);assert(position==held);
   s2_cd_theme_frame(0,1);s2_cd_theme_tick(1);extra(pcm,256,44100);s2_cd_theme_debug(NULL,NULL,&held);assert(position==held);
   s2_cd_theme_frame(0,0);s2_cd_theme_tick(2);extra(pcm,256,44100);s2_cd_theme_debug(NULL,NULL,&held);assert(held>position);
   S2StateIO validate={state,save.pos,0,1,1};s2_cd_theme_state(&validate);assert(validate.ok);
   uint8_t invalid[128];memcpy(invalid,state,save.pos);memset(invalid,255,8);
   S2StateIO bad={invalid,save.pos,0,1,1};s2_cd_theme_state(&bad);assert(!bad.ok);
   S2StateIO load={state,save.pos,0,2,1};s2_cd_theme_state(&load);assert(load.ok);
   s2_cd_theme_debug(NULL,NULL,&held);assert(position==held);
  }
  fprintf(stdout,"PASS %s: %u real-ROM tracks together; native music/SFX and source bytes preserved\n",s2_donor_ids[d],selected);
 }
 /* Independent selectors can combine all donor families in one bounded
  * cartridge bank. Exercise that shared sample/envelope allocation too. */
 for(unsigned mix=0;mix<8;mix++){
  memset(s2_themes,0,sizeof s2_themes);
  for(unsigned g=0;g<S2_MUSIC_GROUPS;g++)assert(s2_music_group_enable(g,0));
  for(unsigned t=0;t<S2_MUSIC_COUNT;t++){
   if(!s2_theme_choices(t))continue;
   unsigned count=(unsigned)s2_theme_choices(t),start=(t*5+mix)%count;
   for(unsigned k=0;k<count;++k) {
    const S2MusicChoice *choice=s2_theme_choice(t,(start+k)%count);
    if(choice->donor==S2_DONOR_CUSTOM || (choice->donor!=S2_DONOR_S2 && !s2_donor_verified(choice->donor)))continue;
    assert(s2_music_pick(t,choice->id));
    if(s2_theme_ready(t)) {assert(s2_music_active(t));break;}
    assert(s2_music_pick(t,"base"));
   }
  }
  if(!s2_music_prepare(stock,&library,error,sizeof error)){
   fprintf(stderr,"FAIL mixed bank %u: %s\n",mix,error);return 1;
  }
  fprintf(stderr,"Exercising mixed bank %u\n",mix);
  recompui_sha256_compute((const uint8_t *)stock+offsetof(S2StockSound,music_bytes),
                          sizeof *stock- offsetof(S2StockSound,music_bytes),unchanged);
  assert(!memcmp(baseline,unchanged,32));
  for(unsigned t=0;t<S2_MUSIC_COUNT;++t)if(s2_music_active(t))play_cue(stock,s2_music_target(t));
 }
 puts("PASS eight mixed-source libraries; stock assets preserved");
 s2_music_release(stock,&library);free(stock);
#endif
 return 0;
}
