#include "sonic2_resources.h"
#include "sonic2_party.h"
#include "sonic2_mods.h"
#include "sonic2_campaign_file.h"
#include "recomp_launcher.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int delegated;
static int playback, online;
int genesis_netplay_active(void) { return online; }
int genesis_replay_playing(void) { return playback; }
static int count(void *ctx) { assert(ctx == &delegated); return 1; }
static int enable(void *ctx, const char *p, const char *f, int on)
{ assert(ctx == &delegated && !strcmp(p,"base") && !strcmp(f,"wide") && on); ++delegated; return 1; }
static void amy_item_art_bounds_test(void)
{
    uint8_t *rom=calloc(1,0x200000);assert(rom);
    static const unsigned at[]={0x96002,0x98D78};
    for(unsigned n=0;n<2;++n) {
        uint8_t *p=rom+at[n];
        p[1]=12;p[2]=(uint8_t)(0x81+n);
        p[3]=1;p[4]=0;p[5]=0x71;p[6]=1;p[7]=0xFF;
        /* One pixel, then eight-pixel runs. Native NemDec stops after
         * exactly twelve tiles, leaving one final run pixel unused. */
        memset(p+8,0xFF,13);p[8]=0x7F;
    }
    S2ItemArt art;memset(&art,0xA5,sizeof art);
    assert(s2_amy_item_art_decode(rom,0x200000,&art));
    for(unsigned i=0;i<sizeof art.life;++i)assert(art.life[i]==0x11);
    for(unsigned i=0;i<sizeof art.continue_icons;++i)assert(art.continue_icons[i]==0x22);
    S2ItemArt before=art;
    assert(!s2_amy_item_art_decode(rom,0x1FFFFF,&art));
    rom[0x96003]=11;assert(!s2_amy_item_art_decode(rom,0x200000,&art));rom[0x96003]=12;
    rom[0x98D79]=13;assert(!s2_amy_item_art_decode(rom,0x200000,&art));
    assert(!s2_amy_item_art_decode(NULL,0x200000,&art));
    assert(!s2_amy_item_art_decode(rom,0x200000,NULL));
    assert(!memcmp(&art,&before,sizeof art));free(rom);
}
int main(int argc, char **argv)
{
    amy_item_art_bounds_test();
    assert(argc == 2 || argc == 3 || argc == 4);
    s2_party_load(argv[1]); s2_resources_load(argv[1]);
    s2_campaign_file_load(argv[1]);
    assert(s2_party.amy_enabled && !s2_party.s3k_enabled && !s2_party.save_menu_enabled);
    RecompLauncherCModProvider base = {0}; base.ctx=&delegated; base.package_count=count; base.feature_count=count; base.feature_enable=enable;
    const RecompLauncherCModProvider *p=s2_mods(&base);
    assert(p->package_count(p->ctx)==5 && p->feature_count(p->ctx)==8);
    assert(!s2_party.infinite_lives && !s2_party.infinite_time);
    assert(p->feature_get(p->ctx,5,&(RecompLauncherCModFeature){0}));
    assert(p->feature_enable(p->ctx,"sonic2.practice","infinite-lives",1));
    assert(s2_party.infinite_lives && !s2_party.infinite_time);
    assert(p->feature_resource_count(p->ctx,"sonic2.practice","infinite-lives")==0);
    assert(p->set_enabled(p->ctx,"sonic2.practice",1) && s2_party.infinite_time);
    assert(p->set_enabled(p->ctx,"sonic2.practice",0) && !s2_party.infinite_lives && !s2_party.infinite_time);
    online=1;
    assert(!p->feature_enable(p->ctx,"sonic2.practice","infinite-lives",1) && !s2_party.infinite_lives);
    assert(p->feature_enable(p->ctx,"sonic2.practice","infinite-lives",0));
    online=0;
    assert(p->feature_enable(p->ctx,"base","wide",1) && delegated==1);
    assert(!p->feature_enable(p->ctx,"sonic2.s3k","knuckles",1));
    assert(!s2_party.s3k_enabled && *p->last_error(p->ctx));
    assert(!p->feature_enable(p->ctx,"sonic2.s3k","save-menu",1));
    assert(!p->feature_enable(p->ctx,"sonic2.s3k","unknown-feature",1));
    assert(delegated==1 && !s2_party.save_menu_enabled);
    assert(!s2_resource_set(2,"missing") && !s2_resource_set(0,"missing"));
    RecompLauncherCModFeature f;
    assert(p->feature_get(p->ctx,0,&f) && f.enabled && !strcmp(f.id,"amy"));
    assert(p->feature_get(p->ctx,1,&f) && !f.enabled && !strcmp(f.package_name,"S3&K"));
    assert(p->feature_get(p->ctx,2,&f) && !f.enabled && !strcmp(f.id,"save-menu"));
    assert(p->feature_get(p->ctx,3,&f) && !f.enabled && !strcmp(f.id,"mighty"));
    assert(p->feature_get(p->ctx,4,&f) && f.enabled && !strcmp(f.id,"shadow"));
    assert(!p->feature_enable(p->ctx,"sonic2.megamix","mighty",1));
    assert(!p->set_enabled(p->ctx,"sonic2.megamix",1));
    assert(!p->feature_enable(p->ctx,"sonic2.megamix","shadow",1));
    playback=1;
    assert(p->feature_enable(p->ctx,"sonic2.megamix","shadow",1));
    playback=0;
    assert(!p->feature_enable(p->ctx,"sonic2.megamix","save-menu",1));
    assert(p->feature_resource_count(p->ctx,"sonic2.s3k","knuckles")==2);
    assert(p->feature_resource_count(p->ctx,"sonic2.s3k","save-menu")==2);
    RecompLauncherCModResource resource;
    assert(p->feature_resource_get(p->ctx,"sonic2.s3k","save-menu",1,&resource));
    assert(!strcmp(resource.id,"campaign-sram") && !resource.required && !resource.path[0]);
    assert(p->feature_resource_get(p->ctx,"sonic2.s3k","knuckles",1,&resource));
    assert(!strcmp(resource.id,"original-knuckles-art") && !resource.required && !resource.verified);
    assert(!p->feature_resource_set_path(p->ctx,"sonic2.s3k","knuckles",resource.id,"missing-kis2.bin"));
    assert(!s2_resource_knuckles_art() && !s2_party.kis2_path[0]);
    assert(!s2_resource_amy_special());
    assert(!p->feature_resource_set_path(p->ctx,"sonic2.s3k","save-menu","campaign-sram","missing-campaign.srm"));
    assert(*p->last_error(p->ctx) && !s2_party.campaign_path[0]);
    assert(p->feature_resource_set_path(p->ctx,"sonic2.s3k","save-menu","campaign-sram",""));
    if (argc==3) {
        assert(p->feature_resource_set_path(p->ctx,"sonic2.s3k","knuckles","original-knuckles-art",argv[2]));
        const S2KnucklesArt *art=s2_resource_knuckles_art();
        assert(art && art->palette[4]==0x080 && art->palette[6]==0xEEE);
        for(unsigned frame=0;frame<S2_SPECIAL_FRAMES;++frame) {
            const S2SpecialArtFrame *special=&art->special[frame];
            assert(special->count>=2 && special->count<=S2_SPECIAL_PIECES);
            unsigned ink=0;
            for(unsigned i=0;i<sizeof special->tiles;++i)ink+=special->tiles[i]!=0;
            assert(ink>200);
        }
        for(unsigned frame=0;frame<S2_SS_RESULT_FRAMES;++frame)
            assert(art->special_results[frame].count>0 && art->special_results[frame].count<=S2_SS_RESULT_PIECES);
        assert((art->special_results[29].pieces[0].attr&2047)==0x3E);
        S2KnucklesArt before=*art;
        assert(!s2_resource_set(S2_RESOURCE_KIS2,"missing-kis2.bin"));
        assert(!memcmp(&before,s2_resource_knuckles_art(),sizeof before));
        assert(!strcmp(s2_party.kis2_path,argv[2]));
        assert(p->feature_resource_get(p->ctx,"sonic2.s3k","knuckles",1,&resource) && resource.verified);
        assert(p->feature_resource_set_path(p->ctx,"sonic2.s3k","knuckles",resource.id,""));
        assert(!s2_resource_knuckles_art() && !s2_party.kis2_path[0]);
    } else if (argc==4) {
        assert(s2_resource_set(S2_RESOURCE_AMY,argv[2]));
        assert(s2_resource_set(S2_RESOURCE_SK,argv[3]));
        assert(s2_resource_bank(S2_RESOURCE_AMY)->count==253);
        assert(s2_resource_bank(S2_RESOURCE_AMY)->animation_count==44);
        const S2SpecialStageArt *special=s2_resource_amy_special();
        const S2ItemArt *items=s2_resource_amy_items();assert(items);
        S2ItemArt items_before=*items;
        assert(special && special->frames[11].count==6 && special->palette[2]==0x00C);
        for(unsigned frame=0;frame<S2_SPECIAL_FRAMES;++frame)
            assert(special->frames[frame].count>=3 && special->frames[frame].count<=6);
        assert(special->results[1].count==7 && special->results[13].count==5 && !special->results[29].count);
        assert(special->results[1].pieces[6].x==-24 && special->results[1].pieces[6].attr==0x5D8);
        S2SpecialStageArt special_before;memcpy(&special_before,special,sizeof special_before);
        uint8_t *rom=malloc(0x200000);assert(rom);FILE *donor=fopen(argv[2],"rb");assert(donor);
        assert(fread(rom,1,0x200000,donor)==0x200000);fclose(donor);
        S2SpecialStageArt unchanged;memcpy(&unchanged,special,sizeof unchanged);
        assert(!s2_amy_special_ui_decode(rom,0x1FFFFF,&unchanged));
        unsigned mp=0x15262+((unsigned)rom[0x15264]<<8|rom[0x15265]);
        uint8_t count=rom[mp];rom[mp]=1;
        assert(!s2_amy_special_ui_decode(rom,0x200000,&unchanged));rom[mp]=count;
        uint8_t header=rom[0x9BDEB];rom[0x9BDEB]=1;
        assert(!s2_amy_special_ui_decode(rom,0x200000,&unchanged));rom[0x9BDEB]=header;
        assert(!memcmp(&unchanged,special,sizeof unchanged));free(rom);
        assert(s2_resource_bank(S2_RESOURCE_SK)->count==251);
        assert(s2_resource_bank(S2_RESOURCE_SK)->animation_count==37);
        assert(s2_resource_owner_verified(S2_RESOURCE_SK) && s2_resource_save_assets());
        const S2SaveAssets *menu=s2_resource_save_assets();
        assert(!s2_resource_set(S2_RESOURCE_SK,argv[2]));
        assert(s2_resource_save_assets()==menu && s2_resource_owner_verified(S2_RESOURCE_SK));
        const S2DonorBank *before=s2_resource_bank(S2_RESOURCE_AMY);
        uint8_t *pixels=before->frames[1].pixels;
        assert(!s2_resource_set(S2_RESOURCE_AMY,argv[3]));
        assert(s2_resource_verified(S2_RESOURCE_AMY) && before->frames[1].pixels==pixels);
        assert(!memcmp(&special_before,s2_resource_amy_special(),sizeof special_before));
        assert(!memcmp(&items_before,s2_resource_amy_items(),sizeof items_before));
        assert(!strcmp(s2_party.amy_path,argv[2]));
        assert(p->feature_enable(p->ctx,"sonic2.s3k","knuckles",1));
        assert(s2_party.s3k_enabled);
        assert(!strcmp(s2_party.roster.character[0],"sonic"));
        assert(!strcmp(s2_party.roster.character[1],"tails"));
        assert(p->commit_netplay(p->ctx,"unused") && s2_party.s3k_enabled);
        assert(p->feature_enable(p->ctx,"sonic2.s3k","knuckles",0));
        assert(p->feature_enable(p->ctx,"sonic2.s3k","save-menu",1));
        assert(s2_party.save_menu_enabled && !s2_party.s3k_enabled);
        assert(!p->commit_netplay(p->ctx,"unused") && *p->last_error(p->ctx));
        assert(p->feature_get(p->ctx,1,&f) && !f.enabled);
        assert(p->feature_get(p->ctx,2,&f) && f.enabled);
        assert(p->feature_resource_count(p->ctx,"sonic2.s3k","save-menu")==2);
        assert(p->feature_enable(p->ctx,"sonic2.s3k","save-menu",0));
        assert(p->set_enabled(p->ctx,"sonic2.s3k",1));
        assert(s2_party.s3k_enabled && s2_party.save_menu_enabled);
        assert(p->set_enabled(p->ctx,"sonic2.s3k",0));
        assert(!s2_party.s3k_enabled && !s2_party.save_menu_enabled);
    }
    s2_resources_shutdown();
    assert(!s2_resource_verified(0) && !s2_resource_verified(1));
    assert(!s2_resource_owner_verified(1) && !s2_resource_save_assets());
    assert(!s2_resource_amy_special());
    assert(!s2_resource_amy_items());
    puts("Owner-resource identity, atomic replacement and composed Mods provider passed");
    return 0;
}
