#ifdef NDEBUG
#undef NDEBUG
#endif
#include "sonic2_music.h"
#include "sonic2_sound_assets.h"
#include "sonic2_cd_themes.h"
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
int main(int argc,char **argv)
{
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
   for(unsigned c=0;c<(unsigned)s2_theme_choices(t);c++)assert(s2_theme_choice(t,c)->donor==S2_DONOR_WAI||s2_theme_choice(t,c)->donor==S2_DONOR_AUG);
 }
 for(unsigned c=0;c<(unsigned)s2_theme_choices(inv);c++)assert(s2_theme_choice(inv,c)->donor!=S2_DONOR_WAI);
 const struct RecompLauncherCModProvider *provider=s2_music_mods(NULL);
 assert(provider&&provider->package_count(provider->ctx)==1&&provider->feature_count(provider->ctx)==3);
 unsigned covered=0;
 for(int g=0;g<3;g++){
  RecompLauncherCModFeature feature;assert(provider->feature_get(provider->ctx,g,&feature));
  assert(feature.option_count==(g==0?13:g==1?14:4));
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
 assert(covered==0x7FFFFFFFu);
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
   for(unsigned c=0;c<(unsigned)s2_theme_choices(t);c++)if(s2_theme_choice(t,c)->donor==d){assert(s2_music_pick(t,s2_donor_ids[d]));++selected;break;}
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
   unsigned c=(t*5+mix)%(unsigned)s2_theme_choices(t);
   assert(s2_music_pick(t,s2_theme_choice(t,c)->id));
  }
  assert(s2_music_prepare(rom,0x400000,l.music_index,l.sfx_index,l.psg_index,l.sample_table,error,sizeof error));
  assert(!memcmp(baseline,rom,0x2A0000));
 }
 puts("PASS eight mixed-source 31-track banks; native assets preserved");
 s2_cd_themes_reset();free(baseline);free(rom);
#endif
 return 0;
}
