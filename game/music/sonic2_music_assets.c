#include "sonic2_music.h"
#include "sonic2_custom_music.h"
#include "sonic2_cd_themes.h"
#include "sonic3_sound_assets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void s2_music_release(S2StockSound *stock,S2MusicLibrary *library)
{
    s2_cd_themes_reset();
    if(stock)for(unsigned i=0;i<S2_MUSIC_CUES;++i) {
        free((void *)stock->selected_music[i].data);
        stock->selected_music[i]=(S2MusicCue){0};
    }
    for(unsigned d=0;d<S2_ROM_DONOR_COUNT;++d) {
        S2DonorSound *sound=library->donors[d];
        if(!sound)continue;
        for(unsigned n=0;n<S2_DONOR_PCM_SLOTS;++n)free(sound->pcm[n].data);
        free(sound);
        library->donors[d]=NULL;
    }
}

int s2_music_prepare(S2StockSound *stock,S2MusicLibrary *library,char *error,size_t error_size)
{
    if(!stock || !library)return 0;
    if(error && error_size)*error=0;
    s2_music_release(stock,library);
    uint8_t *roms[S2_ROM_DONOR_COUNT]={0};
    size_t sizes[S2_ROM_DONOR_COUNT]={0};
    S3SoundSource sources[S2_ROM_DONOR_COUNT]={0};
    int ok=1;
    for(unsigned t=0;t<S2_MUSIC_COUNT && ok;++t) {
        if(!s2_music_active(t))continue;
        const S2MusicChoice *choice=s2_theme_choice(t,(unsigned)s2_themes[t].choice);
        unsigned target=s2_music_target(t),donor=choice->donor;
        S2MusicCue *cue=&stock->selected_music[target-1];
        if(s2_music_native_choice(t)) {
            /* The alternative Oil Ocean arrangement already exists in the
             * stock 2P catalogue. Copy only its decoded cue. */
            unsigned i=choice->song;
            uint8_t *data=malloc(stock->music_bytes[i]);
            ok=data!=NULL;
            if(ok) {
                memcpy(data,stock->music[i],stock->music_bytes[i]);
                *cue=(S2MusicCue){data,NULL,stock->music_bytes[i],stock->music_tempo[i],stock->music_pal[i]};
            }
        } else if(donor==S2_DONOR_CUSTOM) {
            ok=s2_custom_prepare(t,cue,error,error_size);
        } else if(donor==S2_DONOR_CD_US) {
            ok=s2_cd_theme_prepare(t,donor,s2_donor_path(donor),cue,error,error_size);
        } else {
            if(!roms[donor]) {
                roms[donor]=s2_donor_read(donor,&sizes[donor]);
                library->donors[donor]=calloc(1,sizeof(S2DonorSound));
            }
            ok=roms[donor] && library->donors[donor] &&
               s3_sound_prepare_cue(roms[donor],sizes[donor],donor,choice->song,
                                    &sources[donor],library->donors[donor],cue,error,error_size);
        }
        if(ok && target==0x1F)cue->force_pal=1;
        if(!ok && error && error_size && !*error)
            snprintf(error,error_size,"Unable to prepare %s.",s2_theme_names[t]);
    }
    for(unsigned d=0;d<S2_ROM_DONOR_COUNT;++d)free(roms[d]);
    if(!ok)s2_music_release(stock,library);
    return ok;
}
