#include "sonic2_mighty.h"
#include "sonic2_shadow.h"
#include "sonic2_resources.h"
#include "sonic2_party.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int available(void) { return 1; }
static const S2Character knuckles={"knuckles","KNUCKLES",1,available};
static const S2Character amy={"amy","AMY",1,available};
int main(int argc,char **argv)
{
    s2_party_defaults(); s2_character_register(&knuckles); s2_character_register(&amy);
    s2_shadow_register(); s2_mighty_register(); s2_mighty_register();
    assert(s2_character_count()==6 && !strcmp(s2_character_at(5)->id,"mighty"));
    assert(s2_character_at(5)->native_object_id==1 && !s2_character_at(5)->available());
    assert(!s2_roster_set(&s2_party.roster,0,"mighty"));
    assert(!s2_mighty_frame(0) && !s2_mighty_frame(214) && !s2_mighty_frame(~0u));
    for(unsigned i=1;i<214;++i) assert(s2_mighty_frame(i)>0 && s2_mighty_frame(i)<176);
    if(argc>=2) {
        assert(s2_resource_set(S2_RESOURCE_MEGAMIX,argv[1]));
        const S2DonorBank *b=s2_resource_bank(S2_RESOURCE_MEGAMIX);
        assert(b && b->count==176 && b->animation_count==38);
        const S2DonorBank *shadow=s2_resource_shadow_bank();
        assert(shadow && shadow->count==176 && shadow->super_animation_count==38);
        assert(s2_character_find("shadow")->available());
        s2_party.mighty_enabled=1;
        assert(s2_character_at(5)->available() && s2_roster_set(&s2_party.roster,0,"mighty"));
        assert(!s2_roster_validate(&s2_party.roster));
        assert(s2_roster_cycle(&s2_party.roster,0,-1) && !strcmp(s2_party.roster.character[0],"shadow"));
        FILE *report=argc>=3?fopen(argv[2],"wb"):NULL;
        assert(argc<3 || report);
        for(unsigned i=0;i<b->count;++i) {
            const S2DonorFrame *f=&b->frames[i];
            uint32_t hash=2166136261u;
            for(unsigned n=0;n<(unsigned)f->width*f->height;++n) hash=(hash^f->pixels[n])*16777619u;
            if(report)fprintf(report,"%u,%d,%d,%u,%u,%08x\n",i,f->x,f->y,f->width,f->height,hash);
        }
        if(report)fclose(report);
        if(argc>=4) {
            report=fopen(argv[3],"wb"); assert(report);
            for(unsigned i=0;i<shadow->count;++i) {
                const S2DonorFrame *f=&shadow->frames[i]; uint32_t hash=2166136261u;
                for(unsigned n=0;n<(unsigned)f->width*f->height;++n) hash=(hash^f->pixels[n])*16777619u;
                fprintf(report,"%u,%d,%d,%u,%u,%08x\n",i,f->x,f->y,f->width,f->height,hash);
            }
            fclose(report);
        }
        assert(!s2_resource_set(S2_RESOURCE_MEGAMIX,"absent-mighty-donor.bin"));
        assert(s2_resource_bank(S2_RESOURCE_MEGAMIX)==b && b->count==176);
        s2_party.mighty_enabled=0;
        assert(!s2_character_at(5)->available());
        assert(s2_resource_set(S2_RESOURCE_MEGAMIX,"")==0 && !s2_resource_verified(S2_RESOURCE_MEGAMIX));
    }
    s2_resources_shutdown();
    puts("PASS Mighty sixth entry, native Sonic identity, frame semantics and donor availability/atomic replacement");
    return 0;
}
