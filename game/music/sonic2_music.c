#include "sonic2_music.h"
#include "sonic2_cd_themes.h"
#include "sonic2_sound_driver.h"
#include "host_atomic_file.h"
#include "sha256.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

S2MusicSelection s2_themes[S2_MUSIC_COUNT];
#define TRACK(id,...) id,
const char *const s2_theme_ids[]={
#include "sonic2_music_catalog.inc"
};
#undef TRACK
#define TRACK(id,name,...) name,
const char *const s2_theme_names[]={
#include "sonic2_music_catalog.inc"
};
#undef TRACK
typedef struct Row {unsigned group,native,wai;const char *section;} Row;
#define TRACK(id,name,group,section,native,wai) {group,native,wai,section},
static const Row rows[]={
#include "sonic2_music_catalog.inc"
};
#undef TRACK
const char *const s2_donor_ids[]={"s1","s2","s2-aug","s2-wai","s3-proto","s3","sk","cd-us"};
const char *const s2_donor_names[]={"Sonic 1","Sonic 2","Sonic 2 (Aug 21, 1992 prototype)","Sonic 2 (Simon Wai prototype)","Sonic 3 (Nov 3, 1993 prototype)","Sonic 3","Sonic & Knuckles","Sonic CD (US)"};
static const char *const hashes[]={
 "46160baa06362c711c9f1a5017cb7371026444936c8af5e93a78996cf32ff2a6",
 "193bc4064ce0daf27ea9e908ed246d87ec576cc294833badebb590b6ad8e8f6b",
 "cd1f37fb244f200531b5c93378115fcf2f665fa78ba41548a10bff0ee64f0473",
 "c2e63635703e4dac7b0a2bb383abe5a001c128ab8ab98d33519c31d75b59cc8d",
 "ace6af3187c1c2da4e71a1c24fb81f2529cb25057f093f5cb5389842a90d4abf",
 "d6404843c5ba32486f4c8c744acb7e1932069376960d886b1de37969787d3f9c",
 "6e12e6b33c26ebfcd0be433251d21cf6284eafe9f71b027bda3767ae59affec1"};
static const size_t sizes[]={0x80000,0x100000,0x100000,0x100000,0x200000,0x200000,0x200000};
static S2MusicChoice choices[S2_MUSIC_COUNT][10];
static unsigned counts[S2_MUSIC_COUNT];
static int group_on[S2_MUSIC_GROUPS],verified[S2_DONOR_COUNT];
static uint8_t identities[S2_DONOR_COUNT][32];
static char file[1200],library_settings[1200],paths[S2_DONOR_COUNT][1024],error[256];

const char *s2_themes_error(void){return error;}
void s2_themes_clear_error(void){*error=0;}
static void add(unsigned t,unsigned donor,unsigned song)
{
 if(counts[t]<10)choices[t][counts[t]++]=(S2MusicChoice){s2_donor_ids[donor],s2_donor_names[donor],donor,song};
}
static void catalog(void)
{
 memset(counts,0,sizeof counts);
 for(unsigned t=0;t<S2_MUSIC_COUNT;t++){
  if(rows[t].wai)add(t,S2_DONOR_WAI,rows[t].wai-0x81);
  add(t,S2_DONOR_AUG,rows[t].native-0x81);
  if(rows[t].group!=S2_MUSIC_SHARED && rows[t].native!=0x92)continue;
  switch(rows[t].native){
   case 0x98:for(unsigned d=4;d<=6;d++)add(t,d,0x2A);break;
   case 0x9B:add(t,S2_DONOR_SK,0x27);add(t,S2_DONOR_CD_US,0);break;
   case 0x9C:add(t,S2_DONOR_S3,0x28);add(t,S2_DONOR_SK,0x28);break;
   case 0x9A:add(t,S2_DONOR_PROTO,0x29);add(t,S2_DONOR_SK,0x29);add(t,S2_DONOR_CD_US,0);break;
   case 0x97:add(t,S2_DONOR_S1,6);add(t,S2_DONOR_S3,0x2C);add(t,S2_DONOR_SK,0x2C);add(t,S2_DONOR_CD_US,0);break;
   case 0x9F:add(t,S2_DONOR_PROTO,0x31);break;
   case 0x93:add(t,S2_DONOR_S1,11);for(unsigned d=4;d<=6;d++)add(t,d,0x19);break;
   case 0x94:add(t,S2_DONOR_S1,12);for(unsigned d=4;d<=6;d++)add(t,d,0x30);break;
   case 0x99:add(t,S2_DONOR_S1,9);for(unsigned d=4;d<=6;d++)add(t,d,0x25);break;
   case 0x9E:add(t,S2_DONOR_S3,0x26);add(t,S2_DONOR_SK,0x33);add(t,S2_DONOR_PROTO,0x26);break;
   case 0x96:add(t,S2_DONOR_PROTO,0x2E);break;
   case 0x92:add(t,S2_DONOR_PROTO,0x1C);break;
  }
 }
}
unsigned s2_music_group(unsigned t){return t<S2_MUSIC_COUNT?rows[t].group:S2_MUSIC_GROUPS;}
const char *s2_music_section(unsigned t){return t<S2_MUSIC_COUNT?rows[t].section:"";}
unsigned s2_music_target(unsigned t){return t<S2_MUSIC_COUNT?rows[t].native-0x80:0;}
int s2_music_available(unsigned t){return t<S2_MUSIC_COUNT&&s2_sound_available();}
int s2_theme_choices(unsigned t){return t<S2_MUSIC_COUNT?(int)counts[t]:0;}
const S2MusicChoice *s2_theme_choice(unsigned t,unsigned c){return t<S2_MUSIC_COUNT&&c<counts[t]?&choices[t][c]:NULL;}
int s2_theme_donor(unsigned t){const S2MusicChoice *c=t<S2_MUSIC_COUNT?s2_theme_choice(t,(unsigned)s2_themes[t].choice):NULL;return c?(int)c->donor:-1;}
int s2_music_donor(unsigned t){return s2_theme_donor(t);}
const char *s2_donor_path(unsigned d){return d<S2_DONOR_COUNT?paths[d]:"";}
int s2_donor_verified(unsigned d){return d<S2_DONOR_COUNT&&verified[d];}
int s2_theme_ready(unsigned t){int d=s2_theme_donor(t);return d>=0&&s2_donor_verified((unsigned)d);}
int s2_music_active(unsigned t){return t<S2_MUSIC_COUNT&&s2_themes[t].enabled&&group_on[rows[t].group];}
int s2_themes_enabled(void){for(unsigned t=0;t<S2_MUSIC_COUNT;t++)if(s2_music_active(t))return 1;return 0;}
int s2_music_group_enabled(unsigned g){for(unsigned t=0;t<S2_MUSIC_COUNT;t++)if(rows[t].group==g&&s2_music_active(t))return 1;return 0;}
uint8_t *s2_donor_read(unsigned d,size_t *size)
{
 if(size)*size=0;
 if(d>=S2_ROM_DONOR_COUNT){
  if(d<S2_DONOR_COUNT){
   verified[d]=s2_cd_donor_verify(paths[d],d);
   if(verified[d]){FILE *f=fopen(paths[d],"rb");char raw[8192];size_t n=f?fread(raw,1,sizeof raw,f):0;int ok=f&&n<sizeof raw&&!ferror(f);if(f)fclose(f);if(ok)recompui_sha256_compute(raw,n,identities[d]);else verified[d]=0;}
  }
  return NULL;
 }
 verified[d]=0;FILE *f=fopen(paths[d],"rb");if(!f)return NULL;
 uint8_t *rom=malloc(sizes[d]);int ok=rom&&fread(rom,1,sizes[d],f)==sizes[d]&&fgetc(f)==EOF;fclose(f);
 if(ok){
  uint8_t digest[32];char actual[65];recompui_sha256_compute(rom,sizes[d],digest);
  for(unsigned i=0;i<32;i++)snprintf(actual+2*i,3,"%02x",digest[i]);
  ok=!strcmp(actual,hashes[d]);if(ok)memcpy(identities[d],digest,32);
 }
 if(!ok){free(rom);return NULL;}
 verified[d]=1;if(size)*size=sizes[d];return rom;
}
/* Replay metadata restores resource paths before its track selections. Adopt
 * only verified, already imported content; never re-import a CD manifest as a
 * disc or overwrite a cache while restoring a recording. */
int s2_music_donor_use(unsigned d,const char *path)
{
 *error=0;
 if(d>=S2_DONOR_COUNT||!path||!*path||strlen(path)>=sizeof paths[d]||strpbrk(path,"\t\r\n"))return 0;
 char incoming[1024],previous[1024];uint8_t identity[32];int ready=verified[d];
 snprintf(incoming,sizeof incoming,"%s",path);memcpy(previous,paths[d],sizeof previous);memcpy(identity,identities[d],32);
 memcpy(paths[d],incoming,strlen(incoming)+1);size_t size;free(s2_donor_read(d,&size));
 if(verified[d])return 1;
 memcpy(paths[d],previous,sizeof previous);verified[d]=ready;memcpy(identities[d],identity,32);
 snprintf(error,sizeof error,"Provide verified %s content for this recording.",s2_donor_names[d]);return 0;
}
int s2_music_group_enable(unsigned g,int on)
{
 *error=0;if(g>=S2_MUSIC_GROUPS)return 0;
 if(!on){group_on[g]=0;return 1;}
 unsigned selected=0;int seen[S2_DONOR_COUNT]={0};
 for(unsigned t=0;t<S2_MUSIC_COUNT;t++)if(rows[t].group==g&&s2_music_available(t)&&s2_themes[t].enabled){
  ++selected;int d=s2_theme_donor(t);if(d>=0&&!seen[d]++){size_t size;free(s2_donor_read((unsigned)d,&size));}
  if(!s2_theme_ready(t)){group_on[g]=0;snprintf(error,sizeof error,"Provide %s for %s.",s2_donor_names[s2_theme_donor(t)],s2_theme_names[t]);return 0;}
 }
 if(!selected){snprintf(error,sizeof error,"Choose a replacement track first.");return 0;}
 if(!s2_sound_select(1)){group_on[g]=0;snprintf(error,sizeof error,"%s",s2_sound_error());return 0;}
 group_on[g]=1;return 1;
}
int s2_music_pick(unsigned t,const char *id)
{
 *error=0;if(t>=S2_MUSIC_COUNT||!id)return 0;
 if(!strcmp(id,"base")){s2_themes[t].enabled=0;return 1;}
 for(unsigned c=0;c<counts[t];c++)if(!strcmp(id,choices[t][c].id)){
  s2_themes[t]=(S2MusicSelection){1,(int)c};
  if(!s2_music_group_enable(rows[t].group,1)){group_on[rows[t].group]=0;*error=0;}
  return 1;
 }
 snprintf(error,sizeof error,"This track does not offer that replacement.");return 0;
}
void s2_music_load(const char *settings)
{
 memset(s2_themes,0,sizeof s2_themes);memset(verified,0,sizeof verified);*error=0;catalog();
 for(unsigned g=0;g<S2_MUSIC_GROUPS;g++)group_on[g]=1;
 snprintf(file,sizeof file,"%s",settings?settings:"settings.ini");
 char *a=strrchr(file,'/'),*b=strrchr(file,'\\');if(!a||(b&&b>a))a=b;
 size_t prefix=a?(size_t)(a-file)+1:0;snprintf(file+prefix,sizeof file-prefix,"sonic2-themes.ini");
 snprintf(library_settings,sizeof library_settings,"%s",settings?settings:"settings.ini");
 const char *shared=getenv("SONIC_SOUND_DONOR_LIBRARY");
 if(shared&&*shared&&strlen(shared)<1024&&!strpbrk(shared,"\r\n"))snprintf(library_settings,sizeof library_settings,"%s/settings.ini",shared);
 else if(!getenv("GENESIS_USER_DIR")){
  char *root=SDL_GetPrefPath("GenesisRecomp","SonicSoundDonors");
  if(root){snprintf(library_settings,sizeof library_settings,"%ssettings.ini",root);SDL_free(root);}
 }
 a=strrchr(library_settings,'/');b=strrchr(library_settings,'\\');if(!a||(b&&b>a))a=b;
 prefix=a?(size_t)(a-library_settings)+1:0;
 for(unsigned d=0;d<S2_DONOR_COUNT;d++){
  int n=snprintf(paths[d],sizeof paths[d],"%.*ssonic-sound-donors/%s.%s",(int)prefix,library_settings,s2_donor_ids[d],d<S2_ROM_DONOR_COUNT?"bin":"ini");
  if(n<0||n>=(int)sizeof paths[d]){paths[d][0]=0;continue;}
  size_t bytes;free(s2_donor_read(d,&bytes));
 }
 FILE *f=fopen(file,"rb");if(f){
  char line[256];
  while(fgets(line,sizeof line,f)){
   line[strcspn(line,"\r\n")]=0;char *v=strchr(line,'=');if(!v)continue;*v++=0;
   for(unsigned g=0;g<S2_MUSIC_GROUPS;g++){char key[64];snprintf(key,sizeof key,"group.%u.enabled",g);if(!strcmp(line,key))group_on[g]=!strcmp(v,"1");}
   for(unsigned t=0;t<S2_MUSIC_COUNT;t++)if(!strcmp(line,s2_theme_ids[t])){
    for(unsigned c=0;c<counts[t];c++)if(!strcmp(v,choices[t][c].id))s2_themes[t]=(S2MusicSelection){1,(int)c};
   }
  }
  fclose(f);
 }
 for(unsigned t=0;t<S2_MUSIC_COUNT;t++)if(s2_themes[t].enabled&&!s2_theme_ready(t))group_on[rows[t].group]=0;
 if(s2_themes_enabled()&&!s2_sound_select(1))for(unsigned g=0;g<S2_MUSIC_GROUPS;g++)group_on[g]=0;
}
int s2_themes_save(void)
{
 char text[S2_MUSIC_COUNT*100+128];size_t used=0;
 for(unsigned g=0;g<S2_MUSIC_GROUPS;g++)used+=(size_t)snprintf(text+used,sizeof text-used,"group.%u.enabled=%d\n",g,group_on[g]);
 for(unsigned t=0;t<S2_MUSIC_COUNT;t++)used+=(size_t)snprintf(text+used,sizeof text-used,"%s=%s\n",s2_theme_ids[t],s2_themes[t].enabled?s2_theme_choice(t,(unsigned)s2_themes[t].choice)->id:"base");
 if(!genesis_atomic_file_store(file,text,used)){snprintf(error,sizeof error,"Unable to save music choices.");return 0;}return 1;
}
int s2_themes_validate(void)
{
 *error=0;
 for(unsigned t=0;t<S2_MUSIC_COUNT;t++)if(s2_music_active(t)&&!s2_theme_ready(t)){snprintf(error,sizeof error,"Provide the matching ROM for %s.",s2_theme_names[t]);return 0;}
 return !s2_themes_enabled()||s2_sound_select(1);
}
int s2_theme_import(unsigned t,const char *path,void (*progress)(void *,float,const char *),void *user)
{
 *error=0;int d=s2_theme_donor(t);if(d<0||!path||!*path||strpbrk(path,"\r\n"))return 0;
 char imported[1024];
 if(!s2_music_import_file(path,s2_donor_ids[d],library_settings,imported,error,sizeof error,progress,user))return 0;
 for(unsigned i=0;i<S2_DONOR_COUNT;i++){size_t bytes;free(s2_donor_read(i,&bytes));}
 if(!s2_theme_ready(t)){snprintf(error,sizeof error,"The selected track requires %s.",s2_donor_names[d]);return 0;}
 if(!s2_music_group_enable(rows[t].group,1)){group_on[rows[t].group]=0;*error=0;}return 1;
}

void s2_music_config_identity(uint8_t out[32])
{
 uint8_t image[S2_MUSIC_COUNT*36]={0};
 for(unsigned t=0;t<S2_MUSIC_COUNT;t++)if(s2_music_active(t)){
  const S2MusicChoice *c=s2_theme_choice(t,(unsigned)s2_themes[t].choice);
  image[t*36]=1;image[t*36+1]=(uint8_t)c->donor;image[t*36+2]=(uint8_t)c->song;
  memcpy(image+t*36+4,identities[c->donor],32);
 }
 recompui_sha256_compute(image,sizeof image,out);
}
