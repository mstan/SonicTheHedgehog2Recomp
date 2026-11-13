#include "sonic2_music.h"
#include "sonic2_sound_driver.h"
#if RECOMP_LAUNCHER
#include "recomp_launcher.h"
#include <stdio.h>
#include <string.h>
#define COPY(d,s) snprintf(d,sizeof(d),"%s",s)
static const RecompLauncherCModProvider *base;
static const char package[]="sonic2.audio.event-themes";
static const char *const group_ids[]={"shared-themes","sonic2-campaign","competition"};
static const char *const group_names[]={"Shared Themes","Sonic 2 Campaign","2-Player Competition"};
static int ours(const char *p){return p&&!strcmp(p,package);}
static int group_id(const char *id){if(id)for(int g=0;g<S2_MUSIC_GROUPS;g++)if(!strcmp(id,group_ids[g]))return g;return -1;}
static int track_id(const char *id){if(id)for(int t=0;t<S2_MUSIC_COUNT;t++)if(!strcmp(id,s2_theme_ids[t]))return t;return -1;}
static int track_at(int group,int index)
{
    static const char *const common[]={"Events","Bosses","Menus","Other music"};
    static const char *const campaign[]={"Zones","Menus","Bonus stages","Other music"};
    const char *const *sections=group==S2_MUSIC_SHARED?common:campaign;
    for(unsigned s=0;s<4;s++)for(unsigned t=0;t<S2_MUSIC_COUNT;t++)
        if(s2_music_group(t)==(unsigned)group&&s2_music_available(t)&&!strcmp(sections[s],s2_music_section(t))&&index--==0)return (int)t;
    return -1;
}
static int group_at(int index)
{for(int g=0;g<S2_MUSIC_GROUPS;g++)if(track_at(g,0)>=0&&index--==0)return g;return -1;}
static int group_count(void){int count=0;while(group_at(count)>=0)++count;return count;}
static int selected(int group)
{for(unsigned t=0;t<S2_MUSIC_COUNT;t++)if(s2_music_available(t)&&s2_music_group(t)==(unsigned)group&&s2_themes[t].enabled)return 1;return 0;}
static int requirement(int group,int index,int *track)
{
    int seen[S2_DONOR_COUNT]={0};
    for(unsigned t=0;t<S2_MUSIC_COUNT;t++)if(s2_music_group(t)==(unsigned)group&&s2_music_available(t)&&s2_themes[t].enabled){
        int d=s2_theme_donor(t);if(d>=0&&!seen[d]++&&index--==0){if(track)*track=(int)t;return d;}
    }
    return -1;
}
static int packages(void *c){(void)c;return 1+(base&&base->package_count?base->package_count(base->ctx):0);}
static int package_get(void *c,int i,RecompLauncherCModPackage *o)
{
    (void)c;if(i>0)return base&&base->package_get&&base->package_get(base->ctx,i-1,o);
    if(i<0||!o)return 0;memset(o,0,sizeof *o);
    COPY(o->id,package);COPY(o->name,"Music Customization");COPY(o->version,"0.2.0");
    COPY(o->author,"SonicTheHedgehog2Recomp contributors");
    COPY(o->description,"Choose individual tracks in three music sections. Matching ROMs are imported once and shared across all choices and games. All tracks default to Stock.");
    o->enabled=s2_themes_enabled();return 1;
}
static int features(void *c){(void)c;return group_count()+(base&&base->feature_count?base->feature_count(base->ctx):0);}
static int feature_get(void *c,int i,RecompLauncherCModFeature *o)
{
    (void)c;int count=group_count();if(i>=count)return base&&base->feature_get&&base->feature_get(base->ctx,i-count,o);
    int g=group_at(i);if(g<0||!o)return 0;memset(o,0,sizeof *o);
    COPY(o->id,group_ids[g]);COPY(o->package_id,package);COPY(o->package_name,"Music Customization");COPY(o->package_version,"0.2.0");
    COPY(o->name,group_names[g]);COPY(o->group,"Sound");COPY(o->author,"SonicTheHedgehog2Recomp contributors");
    COPY(o->description,"Choose each track independently. Stock leaves that track unchanged. Only ROMs needed by your selected replacements appear below, and each is shared automatically. Choosing a ready replacement or importing its ROM enables this section and Clone Driver. Disable the section to restore all its normal tracks while keeping your choices. Restart to apply; local Sonic 2 gameplay only.");
    o->enabled=s2_music_group_enabled((unsigned)g);
    while(track_at(g,o->option_count)>=0)++o->option_count;
    int ready=1;for(unsigned t=0;t<S2_MUSIC_COUNT;t++)if(s2_music_group(t)==(unsigned)g&&s2_music_available(t)&&s2_themes[t].enabled&&!s2_theme_ready(t))ready=0;
    COPY(o->status,!ready?"Provide the matching ROMs below to enable this section":o->enabled?"Enabled; changes apply after restart":selected(g)?"Ready; enable to use the selected replacements":"All tracks use Stock");
    COPY(o->source_name,"Donor ROM details and prototype sources");COPY(o->source_url,"https://github.com/mstan/SonicTheHedgehog2Recomp/blob/master/docs/EVENT_MUSIC.md");
    return 1;
}
static int enable(void *c,const char *p,const char *f,int on)
{(void)c;s2_themes_clear_error();if(ours(p))return group_id(f)>=0&&s2_music_group_enable((unsigned)group_id(f),on);return base&&base->feature_enable&&base->feature_enable(base->ctx,p,f,on);}
static int enable_package(void *c,const char *p,int on)
{
    (void)c;s2_themes_clear_error();if(!ours(p))return base&&base->set_enabled&&base->set_enabled(base->ctx,p,on);
    int any=0;for(int g=0;g<S2_MUSIC_GROUPS;g++)if(!on||selected(g)){any=1;if(!s2_music_group_enable((unsigned)g,on))return 0;}
    return any?1:s2_music_group_enable(0,1);
}
static int option_get(void *c,const char *p,const char *f,int i,RecompLauncherCModOption *o)
{
    (void)c;if(!ours(p))return base&&base->feature_option_get&&base->feature_option_get(base->ctx,p,f,i,o);
    int t=track_at(group_id(f),i);if(t<0||!o)return 0;memset(o,0,sizeof *o);
    COPY(o->id,s2_theme_ids[t]);COPY(o->label,s2_theme_names[t]);COPY(o->group,s2_music_section((unsigned)t));
    COPY(o->description,"Stock keeps the current soundtrack. Matching donor ROMs are shared automatically.");
    COPY(o->value,s2_themes[t].enabled?s2_theme_choice((unsigned)t,(unsigned)s2_themes[t].choice)->id:"base");COPY(o->default_value,"base");
    o->type=RECOMP_MOD_OPTION_CHOICE;o->choice_count=s2_theme_choices((unsigned)t)+1;return 1;
}
static int choice_get(void *c,const char *p,const char *f,const char *opt,int i,RecompLauncherCModChoice *o)
{
    (void)c;if(!ours(p))return base&&base->feature_choice_get&&base->feature_choice_get(base->ctx,p,f,opt,i,o);
    int t=track_id(opt),g=group_id(f);if(t<0||g<0||s2_music_group((unsigned)t)!=(unsigned)g||!s2_music_available((unsigned)t)||!o)return 0;
    memset(o,0,sizeof *o);if(!i){COPY(o->value,"base");COPY(o->label,"Stock");return 1;}
    const S2MusicChoice *choice=s2_theme_choice((unsigned)t,(unsigned)(i-1));if(!choice)return 0;COPY(o->value,choice->id);
    const char *label=choice->label;
    if(g!=S2_MUSIC_SHARED){
        if(choice->donor==S2_DONOR_WAI)label="Simon Wai prototype";
        else if(choice->donor==S2_DONOR_AUG)label="Aug 21, 1992 prototype";
        else if(choice->donor==S2_DONOR_PROTO)label="Sonic 3 prototype";
    }
    COPY(o->label,label);return 1;
}
static int set_option(void *c,const char *p,const char *f,const char *opt,const char *v)
{
    (void)c;s2_themes_clear_error();if(!ours(p))return base&&base->feature_set_option&&base->feature_set_option(base->ctx,p,f,opt,v);
    int t=track_id(opt),g=group_id(f);return t>=0&&g>=0&&s2_music_group((unsigned)t)==(unsigned)g&&s2_music_pick((unsigned)t,v);
}
static int resource_count(void *c,const char *p,const char *f)
{
    (void)c;if(!ours(p))return base&&base->feature_resource_count?base->feature_resource_count(base->ctx,p,f):0;
    int count=0;while(requirement(group_id(f),count,NULL)>=0)++count;return count;
}
static int resource_get(void *c,const char *p,const char *f,int i,RecompLauncherCModResource *o)
{
    (void)c;if(!ours(p))return base&&base->feature_resource_get&&base->feature_resource_get(base->ctx,p,f,i,o);
    int t,d=requirement(group_id(f),i,&t);if(d<0||!o)return 0;memset(o,0,sizeof *o);
    COPY(o->id,s2_donor_ids[d]);snprintf(o->label,sizeof o->label,"%s %s",s2_donor_names[d],d>=S2_DONOR_CD_US?"disc":"ROM");
    const S2MusicChoice *choice=s2_theme_choice((unsigned)t,(unsigned)s2_themes[t].choice);
    COPY(o->description,!strcmp(choice->id,"s1-s2")?"Sonic 1 (JUE rev 0) or Sonic 2 (World rev 1). ZIP or raw ROM; one import covers every compatible choice.":"Provide this unmodified cartridge as a ZIP or raw ROM. Import once; every matching track and game shares it.");
    COPY(o->file_patterns,d>=S2_DONOR_CD_US?"*.cue,*.bin,*.sgcd,*.zip":"*.zip,*.bin,*.md,*.gen,*.68k,*");
    COPY(o->file_description,d>=S2_DONOR_CD_US?"Sonic CD disc or ZIP":"Genesis ROM or ZIP");
    if(d>=S2_DONOR_CD_US)COPY(o->description,"Choose your US disc as a CUE/BIN set, SGCD or ZIP with audio tracks. One import covers Game Over, Act Clear and Invincibility.");
    COPY(o->path,s2_donor_verified((unsigned)d)?s2_donor_path((unsigned)d):"");
    o->required=1;o->verified=s2_donor_verified((unsigned)d);o->import_on_select=1;
    COPY(o->status,o->verified?"Verified; shared with all matching tracks":"No matching ROM provided");return 1;
}
static int resource_import(void *c,const char *p,const char *f,const char *r,const char *path,const char *image,
                           void (*progress)(void *,float,const char *),void *user,char *error,size_t capacity)
{
    (void)c;s2_themes_clear_error();if(!ours(p))return base&&base->feature_resource_import&&base->feature_resource_import(base->ctx,p,f,r,path,image,progress,user,error,capacity);
    int ok=0,g=group_id(f);if(r)for(int i=0;;i++){
        int t,d=requirement(g,i,&t);if(d<0)break;
        if(!strcmp(r,s2_donor_ids[d])){ok=s2_theme_import((unsigned)t,path,progress,user);break;}
    }
    if(!ok&&error&&capacity)snprintf(error,capacity,"%s",s2_themes_error());return ok;
}
static int resource_set(void *c,const char *p,const char *f,const char *r,const char *path)
{
    s2_themes_clear_error();if(!ours(p))return base&&base->feature_resource_set_path&&base->feature_resource_set_path(base->ctx,p,f,r,path);
    if(group_id(f)<0||!r)return 0;
    for(unsigned d=0;d<S2_DONOR_COUNT;d++)if(!strcmp(r,s2_donor_ids[d]))return s2_music_donor_use(d,path);
    return 0;
}
static int commit(void *c,const char *image)
{(void)c;if(!s2_themes_validate())return 0;if(base&&base->commit&&!base->commit(base->ctx,image))return 0;return s2_themes_save();}
static const char *last_error(void *c)
{(void)c;return *s2_themes_error()?s2_themes_error():base&&base->last_error?base->last_error(base->ctx):"";}
const RecompLauncherCModProvider *s2_music_mods(const RecompLauncherCModProvider *common)
{
    static RecompLauncherCModProvider p;base=common;if(base)p=*base;else memset(&p,0,sizeof p);
    p.package_count=packages;p.package_get=package_get;p.set_enabled=enable_package;p.feature_count=features;p.feature_get=feature_get;p.feature_enable=enable;
    p.feature_option_get=option_get;p.feature_choice_get=choice_get;p.feature_set_option=set_option;
    p.feature_resource_count=resource_count;p.feature_resource_get=resource_get;p.feature_resource_set_path=resource_set;p.feature_resource_import=resource_import;
    p.try_commit=NULL;p.preparation_revision=NULL;p.commit_worker_safe=0;
    p.commit=commit;p.last_error=last_error;return &p;
}
#else
const struct RecompLauncherCModProvider *s2_music_mods(const struct RecompLauncherCModProvider *base){return base;}
#endif
