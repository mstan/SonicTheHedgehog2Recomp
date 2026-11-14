#ifdef NDEBUG
#undef NDEBUG
#endif
#include "sonic2_music.h"
#include "sonic3_sound_legacy.h"
#include "sha256.h"
#include "sonic2_sound_assets.h"
#include "sonic2_cd_themes.h"
#include "sonic2_custom_music.h"
#include "sonic2_state_io.h"
#include "audio.h"
#include "recomp_launcher.h"
#include <SDL2/SDL.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef SONIC2_MUSIC_DRIVER
#include "sonic2_clone_metadata.h"
#include "sonic2_clone_program.h"
#include "sonic2_music_clone_metadata.h"
#endif
static int driver;
int s2_sound_select(int on){driver=on;return 1;}
int s2_sound_available(void){return 1;}
const char *s2_sound_error(void){return "";}
static AudioExtraSource extra;
void audio_set_extra_source(AudioExtraSource source){extra=source;}
int s2_music_import_file(const char *a,const char *b,const char *c,char d[1024],char *e,size_t n,void (*p)(void *,float,const char *),void *u)
{(void)a;(void)b;(void)c;(void)d;(void)e;(void)n;(void)p;(void)u;return 0;}
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
static uint32_t read32(const uint8_t *p){return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];}
int s2_custom_assemble(const char *source,const char *track,const char *settings,char output[1024],char *error,size_t capacity,void (*progress)(void *,float,const char *),void *user)
{(void)source;(void)track;(void)settings;(void)output;(void)error;(void)capacity;(void)progress;(void)user;return 0;}
int main(int argc,char **argv)
{
#ifdef SONIC2_MUSIC_DRIVER
 if(argc==6&&(!strcmp(argv[1],"--custom-import")||!strcmp(argv[1],"--custom-reject"))){
  int reject=!strcmp(argv[1],"--custom-reject");char settings[1200],error[256]={0};
  envset("GENESIS_USER_DIR",argv[2]);snprintf(settings,sizeof settings,"%s/settings.ini",argv[2]);s2_music_load(settings);
  unsigned t=track(argv[3]);assert(s2_music_pick(t,"custom"));
  uint8_t before[32],after[32];s2_custom_identity(t,before);
  int imported=s2_theme_import(t,argv[4],NULL,NULL);
  if(reject){assert(!imported&&s2_custom_ready(t));s2_custom_identity(t,after);assert(!memcmp(before,after,32));puts("PASS rejected music; previous selection retained");return 0;}
  assert(imported&&s2_music_active(t)&&s2_theme_ready(t)&&s2_themes_save());
  s2_custom_identity(t,before);s2_music_load(settings);assert(s2_music_active(t)&&s2_theme_ready(t));
  s2_custom_identity(t,after);assert(!memcmp(before,after,32));
  const RecompLauncherCModProvider *provider=s2_music_mods(NULL);const char *group=s2_music_group(t)==0?"shared-themes":s2_music_group(t)==1?"sonic2-campaign":"competition";
  assert(provider->feature_resource_count(provider->ctx,"sonic2.audio.event-themes",group)==1);
  RecompLauncherCModResource resource;assert(provider->feature_resource_get(provider->ctx,"sonic2.audio.event-themes",group,0,&resource)&&resource.verified);
  assert(strstr(resource.label,"custom music")&&!strncmp(resource.id,"custom-",7));
  uint8_t *rom=calloc(1,0x400000);assert(rom);FILE *f=fopen(argv[5],"rb");assert(f&&fread(rom,1,0x100000,f)==0x100000);fclose(f);
  memcpy(rom+S2_CLONE_LOAD_ADDRESS,s2_clone_program,sizeof s2_clone_program);
  const S2SoundAssetLayout l={S2_CLONE_MUSICINDEX,S2_CLONE_SOUNDINDEX,S2_CLONE_SPECSOUNDINDEX,S2_CLONE_PSG_INDEX,S2_CLONE_SAMPLE_TABLE};
  assert(s2_sound_build_assets(rom,0x100000,rom,0x400000,&l,error,sizeof error));
  assert(s2_music_prepare(rom,0x400000,l.music_index,l.sfx_index,l.psg_index,l.sample_table,error,sizeof error));
  if(extra&&s2_cd_theme_channel_count(t)!=11){
   s2_cd_theme_request(s2_music_target(t));s2_cd_theme_frame(0,t==0);s2_cd_theme_tick(0);
   int16_t pcm[2048]={0},again[2048]={0};extra(pcm,1024,44100);
   int nonzero=0;for(unsigned i=0;i<2048;i++)nonzero|=pcm[i];assert(nonzero);
   uint8_t state[128];S2StateIO save={state,sizeof state,0,0,1};s2_cd_theme_state(&save);assert(save.ok);
   s2_cd_theme_tick(1);memset(pcm,0,sizeof pcm);extra(pcm,1024,44100);s2_cd_theme_tick(2);
   S2StateIO load={state,save.pos,0,2,1};s2_cd_theme_state(&load);assert(load.ok);s2_cd_theme_tick(1);
   extra(again,1024,44100);assert(!memcmp(pcm,again,sizeof pcm));
   s2_cd_theme_audition(t,0,1);memset(again,0,sizeof again);extra(again,1024,44100);for(unsigned i=0;i<2048;i++)assert(!again[i]);
   if(s2_cd_theme_channel_count(t)>1&&t==track("ehz")){
    for(unsigned i=0;i<180;i++)s2_cd_theme_tick(i);
    uint64_t position=0;s2_cd_theme_debug(NULL,NULL,&position);assert(position>(44100ULL<<32));
   }
  }
  printf("PASS custom import, persistence, bank and deterministic replay: %s\n",argv[4]);s2_cd_themes_reset();free(rom);return 0;
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
 assert(!s2_music_group_enable(S2_MUSIC_CAMPAIGN,1)&&!driver);
 unsigned ehz=track("ehz"),cpz=track("cpz"),boss=track("boss"),inv=track("invincibility");
 assert(s2_theme_choices(ehz)==2);
 assert(!s2_music_pick(ehz,"s3"));
 for(unsigned t=0;t<S2_MUSIC_COUNT;t++){
  assert(s2_music_target(t)>=1&&s2_music_target(t)<=31);
  for(unsigned j=0;j<t;j++)assert(s2_music_target(j)!=s2_music_target(t));
  if(!strcmp(s2_music_section(t),"Zones"))
   for(unsigned c=0;c<(unsigned)s2_theme_choices(t);c++)assert(s2_theme_choice(t,c)->donor==S2_DONOR_CUSTOM||s2_theme_choice(t,c)->donor==S2_DONOR_WAI||s2_theme_choice(t,c)->donor==S2_DONOR_AUG||(!strcmp(s2_theme_ids[t],"ehz-2p")&&s2_theme_choice(t,c)->donor==S2_DONOR_SEP14));
 }
 assert(s2_theme_choices(inv)==9);
 assert(!s2_music_available(track("ending"))&&!s2_music_available(track("hidden-palace")));
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
  assert(feature.option_count==(g==0?12:g==1?13:4));
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
 assert(covered==(0x7FFFFFFFu&~((1u<<track("ending"))|(1u<<track("hidden-palace")))));
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
 assert(s2_music_pick(oil,"base"));
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
 assert(driver&&s2_themes_enabled());
 assert(s2_music_group_enable(S2_MUSIC_SHARED,0)&&s2_themes[boss].enabled);
 assert(s2_themes_save());s2_music_load(settings);
 assert(s2_themes[boss].enabled&&!s2_music_group_enabled(S2_MUSIC_SHARED));
 uint8_t *rom=calloc(1,0x400000),*baseline=malloc(0x2A0000);assert(rom&&baseline);
 FILE *f=fopen(argv[2],"rb");assert(f&&fread(rom,1,0x100000,f)==0x100000&&fgetc(f)==EOF);fclose(f);
 memcpy(rom+S2_CLONE_LOAD_ADDRESS,s2_clone_program,sizeof s2_clone_program);
 const S2SoundAssetLayout l={S2_CLONE_MUSICINDEX,S2_CLONE_SOUNDINDEX,S2_CLONE_SPECSOUNDINDEX,S2_CLONE_PSG_INDEX,S2_CLONE_SAMPLE_TABLE};
 char error[256];assert(s2_sound_build_assets(rom,0x100000,rom,0x400000,&l,error,sizeof error));
 memcpy(baseline,rom,0x2A0000);
 for(unsigned d=0;d<S2_DONOR_COUNT;d++){
  unsigned selected=0;memset(s2_themes,0,sizeof s2_themes);
  for(unsigned g=0;g<S2_MUSIC_GROUPS;g++)assert(s2_music_group_enable(g,0));
  for(unsigned t=0;t<S2_MUSIC_COUNT;t++)
   for(unsigned c=0;c<(unsigned)s2_theme_choices(t);c++)if(s2_theme_choice(t,c)->donor==d){assert(s2_music_pick(t,s2_theme_choice(t,c)->id));++selected;break;}
  if(!selected)continue;
  uint8_t before[32],after[32];s2_music_config_identity(before);
  if(!s2_music_prepare(rom,0x400000,l.music_index,l.sfx_index,l.psg_index,l.sample_table,error,sizeof error)){
   fprintf(stderr,"FAIL donor %s: %s\n",s2_donor_ids[d],error);return 1;
  }
  s2_music_config_identity(after);assert(!memcmp(before,after,32));
  assert(!memcmp(baseline,rom,0x2A0000));
  assert(!memcmp(rom+S2_MUSIC_CLONE_SOUND_INDEX,rom+l.sfx_index,81*4));
  for(unsigned t=0;t<S2_MUSIC_COUNT;t++)if(s2_music_active(t))
   assert((read32(rom+S2_MUSIC_CLONE_MUSIC_INDEX+(s2_music_target(t)-1)*4)&0xFFFFFF)>=0x2A0000);
  if(d==S2_DONOR_CD_US){
   assert(extra);int16_t pcm[512]={0};s2_cd_theme_request(0x17);s2_cd_theme_frame(0,0);s2_cd_theme_tick(0);extra(pcm,256,44100);
   unsigned active=0;s2_cd_theme_debug(&active,NULL,NULL);assert(active);
   uint8_t state[128];S2StateIO save={state,sizeof state,0,0,1};s2_cd_theme_state(&save);assert(save.ok);
   uint64_t position=0;s2_cd_theme_debug(NULL,NULL,&position);
   assert(position>0);extra(pcm,256,44100);uint64_t held;s2_cd_theme_debug(NULL,NULL,&held);assert(position==held);
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
   unsigned c=(t*5+mix)%(unsigned)s2_theme_choices(t);
   while(s2_theme_choice(t,c)->donor==S2_DONOR_CUSTOM)c=(c+1)%(unsigned)s2_theme_choices(t);
   assert(s2_music_pick(t,s2_theme_choice(t,c)->id));
   assert(s2_music_active(t));
  }
  assert(s2_music_prepare(rom,0x400000,l.music_index,l.sfx_index,l.psg_index,l.sample_table,error,sizeof error));
  assert(!memcmp(baseline,rom,0x2A0000));
 }
 puts("PASS eight mixed-source banks; native assets preserved");
 s2_cd_themes_reset();free(baseline);free(rom);
#endif
 return 0;
}
