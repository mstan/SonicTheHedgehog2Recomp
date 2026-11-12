#include "sonic2_music.h"
#include "sonic2_cd_themes.h"
#include "sonic3_sound_assets.h"
#include "sha256.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef SONIC2_MUSIC_DRIVER
#include "sonic2_music_clone_metadata.h"
#include "sonic2_music_clone_program.h"
#endif
int s2_music_prepare(uint8_t *rom,size_t size,uint32_t music,uint32_t sfx,uint32_t psg,uint32_t samples,char *error,size_t error_size)
{
#ifdef SONIC2_MUSIC_DRIVER
 enum { BASE=0x2A0000,CAPACITY=0x400000-BASE };
 if(!rom||size<0x400000||sizeof s2_music_clone_program>CAPACITY)return 0;
 uint8_t *bank=rom+BASE;
 memset(bank,0,CAPACITY);memcpy(bank,s2_music_clone_program,sizeof s2_music_clone_program);
 const S3SoundLayout l={sizeof s2_music_clone_program,S2_MUSIC_CLONE_MUSIC_INDEX,S2_MUSIC_CLONE_SOUND_INDEX,S2_MUSIC_CLONE_PSG_INDEX,S2_MUSIC_CLONE_MODENV_INDEX};
 const S3SoundPCMLayout pcm={S2_MUSIC_CLONE_SAMPLE_TABLE,S2_MUSIC_CLONE_DAC_MAP_S3,S2_MUSIC_CLONE_DAC_MAP_SK,S2_MUSIC_CLONE_SEGA_ID,S2_MUSIC_CLONE_SAMPLE_COUNT};
 memcpy(rom+l.music_index,rom+music,31*4);
 memcpy(rom+l.sound_index,rom+sfx,81*4);
 memcpy(rom+l.psg_index,rom+psg,13*4);
 /* Original Sonic 2 notes and logo keep their exact PCM descriptors. New
  * donor notes use separate slots, preserving native effects and volume. */
 memcpy(rom+pcm.sample_table,rom+samples,17*10);
 memcpy(rom+pcm.sample_table+(pcm.sega_id-0x81)*10,rom+samples+17*10,10);
 s2_cd_themes_reset();
 S3SoundMusicCache cache={0};uint8_t *inputs[S2_ROM_DONOR_COUNT]={0};
 size_t sizes[S2_ROM_DONOR_COUNT]={0};uint32_t used=sizeof s2_music_clone_program;unsigned next=70;int ok=1;
 for(unsigned t=0;t<S2_MUSIC_COUNT&&ok;t++)if(s2_music_active(t)){
  const S2MusicChoice *choice=s2_theme_choice(t,(unsigned)s2_themes[t].choice);
  unsigned d=choice->donor,target=s2_music_target(t);
  if(d==S2_DONOR_CD_US)ok=s2_cd_theme_build(t,d,s2_donor_path(d),&l,bank,CAPACITY,&used,error,error_size);
  else{
   if(!inputs[d])inputs[d]=s2_donor_read(d,&sizes[d]);
   ok=inputs[d]&&s3_sound_build_track(inputs[d],sizes[d],d,choice->song,target,&l,&pcm,
          s2_music_clone_program,bank,CAPACITY,&used,&next,&cache,error,error_size);
  }
  if(ok){
   uint8_t *entry=rom+l.music_index+(target-1)*4;
   uint32_t address=(((uint32_t)entry[1]<<16)|((uint32_t)entry[2]<<8)|entry[3])&0xFFFFFEu;
   uint32_t end=address+((unsigned)rom[address]<<8)+rom[address+1];
   fprintf(stderr,"[music] %s: %s; native=$%02X data=$%06X..$%06X\n",s2_theme_names[t],choice->label,target+0x80,address,end);
  }
  else if(error&&error_size&&!*error)snprintf(error,error_size,"Unable to prepare %s.",s2_theme_names[t]);
 }
 for(unsigned d=0;d<S2_ROM_DONOR_COUNT;d++)free(inputs[d]);
 s3_sound_music_cache_free(&cache);
 if(ok){
  uint8_t digest[32];char hex[65];recompui_sha256_compute(bank,used,digest);
  for(unsigned i=0;i<32;i++)snprintf(hex+2*i,3,"%02x",digest[i]);
  fprintf(stderr,"[music] Prepared immutable bank: %u bytes, %u sample slots, SHA256 %s\n",used,next,hex);
 }
 return ok;
#else
 (void)rom;(void)size;(void)music;(void)sfx;(void)psg;(void)samples;
 if(error&&error_size)snprintf(error,error_size,"Music customization is unavailable in this build.");
 return 0;
#endif
}
