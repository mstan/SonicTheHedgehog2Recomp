#include "sonic2_resources.h"
#include "sonic2_party.h"
#include "sha256.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static S2DonorBank banks[S2_RESOURCE_COUNT];
static S2DonorBank shadow_bank;
static int owner_verified[S2_RESOURCE_COUNT];
static S2SaveAssets *save_assets;
static S2KnucklesArt knuckles_art;
static S2StageLayouts knuckles_stages;
static S2KnucklesEnding knuckles_ending;
static S2SpecialStageArt amy_special;
static S2ItemArt amy_items;
int s2_resources_prepare_stage_images(const unsigned char *rom,unsigned size)
{ return s2_stage_images_decode(rom,size,save_assets); }
static char errors[S2_RESOURCE_COUNT][192];
static void prepare_knuckles_water(void)
{
    S2DonorBank *b=&banks[S2_RESOURCE_SK];
    b->water_palette_count=0;
    if(!owner_verified[S2_RESOURCE_SK] || !owner_verified[S2_RESOURCE_KIS2])return;
    /* S3&K body pixels and KiS2 item pixels use different palette indices. */
    s2_donor_water_palette(b->palette,knuckles_art.palette,knuckles_art.water_palette[0],b->water_palette);
    s2_donor_water_palette(b->palette,knuckles_art.palette,knuckles_art.water_palette[1],b->water_palette_alt);
    b->water_palette_count=2;
}
static int read_palette(const uint8_t *rom,size_t size,unsigned entry,uint16_t out[16])
{
    if(size<32 || entry>size-4)return 0;
    unsigned at=(unsigned)rom[entry]<<24|(unsigned)rom[entry+1]<<16|
        (unsigned)rom[entry+2]<<8|rom[entry+3];
    if(at>size-32)return 0;
    for(unsigned c=0;c<16;++c)out[c]=(uint16_t)((rom[at+c*2]<<8)|rom[at+c*2+1]);
    return 1;
}
static const char *hashes[S2_RESOURCE_COUNT] = {
    "9c028944730128f6b9999fc74babf69694b0edab50e3f42cc6b60a185d0b1457",
    "fba0677fde9f76df93f3e98d6310d8af68b9847bde16e253d73cd4dd8134ed23",
    "85d9e9ba79f0035d5854618666ff198871abe1931558496e6563b1cd90e5cdbe",
    "3da827d319a60b6180e327c37e1fe5d448746c1644cce01ada1f2788d1e182f4"
};
static char *resource_path(unsigned i) { return i==S2_RESOURCE_KIS2?s2_party.kis2_path:i==S2_RESOURCE_MEGAMIX?s2_party.mighty_path:i==S2_RESOURCE_AMY?s2_party.amy_path:s2_party.s3k_path; }
static int fail(unsigned i, const char *error) { snprintf(errors[i], sizeof errors[i], "%s", error); return 0; }
int s2_resource_set(unsigned i, const char *path)
{
    if (i >= S2_RESOURCE_COUNT || !path) return 0;
    if (strlen(path) >= sizeof s2_party.amy_path) return fail(i, "Donor path is too long");
    if (!*path) {
        s2_donor_free(&banks[i]); resource_path(i)[0] = 0;
        if (i==S2_RESOURCE_MEGAMIX) s2_donor_free(&shadow_bank);
        owner_verified[i]=0;
        if(i==S2_RESOURCE_SK || i==S2_RESOURCE_KIS2)prepare_knuckles_water();
        if (i==S2_RESOURCE_AMY) {
            memset(&amy_special,0,sizeof amy_special);
            memset(&amy_items,0,sizeof amy_items);
        }
        if (i==S2_RESOURCE_SK) s2_save_assets_free(&save_assets);
        if (i==S2_RESOURCE_KIS2) {
            memset(&knuckles_art,0,sizeof knuckles_art);
            memset(&knuckles_stages,0,sizeof knuckles_stages);
            memset(&knuckles_ending,0,sizeof knuckles_ending);
            snprintf(errors[i],sizeof errors[i],"Select the original Knuckles in Sonic 2 ROM for a Knuckles-led campaign");
            return 1;
        }
        return fail(i, "Select the required owner-supplied ROM");
    }
    size_t size = i==S2_RESOURCE_KIS2?0x340000:i==S2_RESOURCE_MEGAMIX?1283224:i==S2_RESOURCE_AMY?0x200000:0x400000;
    FILE *f = fopen(path, "rb");
    if (!f) return fail(i, "Cannot open donor ROM; previous verified resource is unchanged");
    uint8_t *rom = malloc(size);
    if (!rom) { fclose(f); return fail(i, "Cannot allocate donor verification buffer"); }
    size_t got = fread(rom, 1, size, f); int extra = fgetc(f), io = ferror(f); fclose(f);
    if (got != size || extra != EOF || io) {
        free(rom); return fail(i, "Wrong donor size; expected an unheadered, big-endian .bin/.md ROM");
    }
    /* Qualified Sega Classics Steam image: only the locked-on S3 region
     * byte differs. Verify the complete canonical buffer after normalizing
     * that known alias; never write to the owner's cartridge file. */
    if(i==S2_RESOURCE_SK && rom[0x2001F0]=='J') rom[0x2001F0]='U';
    uint8_t hash[32]; char hex[65]; recompui_sha256_compute(rom, size, hash);
    for (unsigned n = 0; n < 32; ++n) snprintf(hex + n * 2, 3, "%02x", hash[n]);
    if (strcmp(hex, hashes[i])) { free(rom); return fail(i, "Wrong donor revision (SHA-256 mismatch)"); }
    if (i==S2_RESOURCE_KIS2) {
        S2KnucklesArt *art=calloc(1,sizeof *art);
        S2StageLayouts *stages=calloc(1,sizeof *stages);
        S2KnucklesEnding ending={0};
        int ok=art && stages && s2_knuckles_art_decode(rom,size,art) &&
            s2_stage_layouts_decode(rom,size,stages) && s2_knuckles_ending_decode(rom,size,&ending);
        free(rom);
        if(ok) {knuckles_art=*art;knuckles_stages=*stages;knuckles_ending=ending;}
        free(art);free(stages);
        if (!ok) return fail(i,"Invalid original Knuckles artwork or stages");
        owner_verified[i]=1; memmove(resource_path(i),path,strlen(path)+1);
        prepare_knuckles_water();
        errors[i][0]=0; return 1;
    }
    S2DonorBank candidate={0}, shadow={0}; S2SaveAssets *menu=NULL;
    S2SpecialStageArt special_candidate={0};
    S2ItemArt item_candidate={0};
    int ok = s2_donor_decode(rom, size, i==S2_RESOURCE_MEGAMIX?&s2_megamix_mighty_layout:i==S2_RESOURCE_AMY?&s2_amy_171_layout:&s2_sk_knuckles_layout,
        &candidate, errors[i], sizeof errors[i]);
    if (ok && i==S2_RESOURCE_SK) ok=s2_save_assets_decode(rom,size,&menu,errors[i],sizeof errors[i]);
    if(ok && i==S2_RESOURCE_AMY) {
        /* Amy 1.7.1 retains REV01 PalLoad_Water's descriptor table and
         * provides its own CPZ/ARZ character colours in entries $16/$17. */
        ok=read_palette(rom,size,0x2782+0x16*8,candidate.water_palette) &&
            read_palette(rom,size,0x2782+0x17*8,candidate.water_palette_alt);
        candidate.water_palette_count=2;
        if(!ok)snprintf(errors[i],sizeof errors[i],"Invalid donor water palettes");
        if(ok) {
            ok=s2_amy_special_decode(rom,size,&special_candidate) &&
                s2_amy_special_ui_decode(rom,size,&special_candidate) &&
                s2_amy_item_art_decode(rom,size,&item_candidate);
            if(!ok)snprintf(errors[i],sizeof errors[i],"Invalid donor presentation artwork");
        }
    }
    if (ok && i==S2_RESOURCE_MEGAMIX) {
        ok=s2_donor_decode(rom,size,&s2_megamix_shadow_layout,&shadow,errors[i],sizeof errors[i]);
        if (ok) {
            /* Exact v3 normal/water palette and four-shade Super cycles. */
            /* v3 Level initialization selects $19 for Mighty, $1A for Shadow. */
            ok=read_palette(rom,size,0x22D0+0x19*8,candidate.water_palette) &&
                read_palette(rom,size,0x22D0+0x1A*8,shadow.water_palette);
            candidate.water_palette_count=shadow.water_palette_count=1;
            if(!ok)snprintf(errors[i],sizeof errors[i],"Invalid donor water palettes");
            for (unsigned wet=0;wet<2;++wet) for (unsigned f=0;f<16;++f) for (unsigned c=0;c<4;++c) {
                unsigned p=(wet?0x11CCFC:0x11CB7C)+f*8+c*2;
                shadow.cycle_palette[wet][f][c]=(uint16_t)((rom[p]<<8)|rom[p+1]);
            }
        }
    }
    free(rom);
    if (!ok) { s2_donor_free(&candidate); s2_donor_free(&shadow); s2_save_assets_free(&menu); return 0; }
    /* A failed replacement must not destroy a previously verified resource. */
    s2_donor_free(&banks[i]); banks[i]=candidate; owner_verified[i]=1;
    if (i==S2_RESOURCE_AMY) { amy_special=special_candidate; amy_items=item_candidate; }
    if (i==S2_RESOURCE_MEGAMIX) { s2_donor_free(&shadow_bank); shadow_bank=shadow; }
    if (i==S2_RESOURCE_SK) { s2_save_assets_free(&save_assets); save_assets=menu; }
    if (i==S2_RESOURCE_SK) prepare_knuckles_water();
    memmove(resource_path(i), path, strlen(path) + 1);
    errors[i][0] = 0;
    return 1;
}
void s2_resources_shutdown(void)
{
    for (unsigned i = 0; i < S2_RESOURCE_COUNT; ++i) s2_donor_free(&banks[i]);
    s2_donor_free(&shadow_bank);
    memset(owner_verified,0,sizeof owner_verified);
    s2_save_assets_free(&save_assets);
    memset(&knuckles_art,0,sizeof knuckles_art);
    memset(&knuckles_stages,0,sizeof knuckles_stages);
    memset(&knuckles_ending,0,sizeof knuckles_ending);
    memset(&amy_special,0,sizeof amy_special);
    memset(&amy_items,0,sizeof amy_items);
}
void s2_resources_load(const char *settings_path)
{
    s2_resources_shutdown();
    for (unsigned i = 0; i < S2_RESOURCE_COUNT; ++i) {
        errors[i][0] = 0;
        if (*resource_path(i)) s2_resource_set(i, resource_path(i));
        else if (i == S2_RESOURCE_AMY || i==S2_RESOURCE_MEGAMIX) {
            /* Private import tool stages this next to settings.ini. No owner
             * machine paths or copyrighted binary payloads enter the source. */
            char path[1200]; snprintf(path, sizeof path, "%s", settings_path ? settings_path : "");
            char *slash = strrchr(path, '/'), *back = strrchr(path, '\\');
            if (back && (!slash || back > slash)) slash = back;
            if (slash) slash[1] = 0; else path[0] = 0;
            size_t n = strlen(path);
            snprintf(path + n, sizeof path - n, "assets/characters/%s",i==S2_RESOURCE_AMY?"amy-1.7.1.bin":"megamix-v3.bin");
            s2_resource_set(i, path);
        } else fail(i, i==S2_RESOURCE_KIS2?"Select the original Knuckles in Sonic 2 ROM for a Knuckles-led campaign":"Select your supported Sonic 3 & Knuckles combined ROM");
    }
}
int s2_resource_verified(unsigned i) { return i==S2_RESOURCE_KIS2?owner_verified[i]:i < S2_RESOURCE_COUNT && banks[i].count != 0; }
int s2_resource_owner_verified(unsigned i) { return i<S2_RESOURCE_COUNT && owner_verified[i]; }
const S2SaveAssets *s2_resource_save_assets(void) { return owner_verified[S2_RESOURCE_SK]?save_assets:NULL; }
const S2DonorBank *s2_resource_bank(unsigned i) { return i!=S2_RESOURCE_KIS2 && s2_resource_verified(i) ? &banks[i] : NULL; }
const S2KnucklesArt *s2_resource_knuckles_art(void) { return owner_verified[S2_RESOURCE_KIS2]?&knuckles_art:NULL; }
const S2StageLayouts *s2_resource_knuckles_stages(void) { return owner_verified[S2_RESOURCE_KIS2]?&knuckles_stages:NULL; }
const S2KnucklesEnding *s2_resource_knuckles_ending(void) { return owner_verified[S2_RESOURCE_KIS2]?&knuckles_ending:NULL; }
const S2SpecialStageArt *s2_resource_amy_special(void) { return owner_verified[S2_RESOURCE_AMY]?&amy_special:NULL; }
const S2ItemArt *s2_resource_amy_items(void) { return owner_verified[S2_RESOURCE_AMY]?&amy_items:NULL; }
const S2DonorBank *s2_resource_shadow_bank(void) { return s2_resource_verified(S2_RESOURCE_MEGAMIX) && shadow_bank.count?&shadow_bank:NULL; }
const char *s2_resource_error(unsigned i) { return i < S2_RESOURCE_COUNT ? errors[i] : "Unknown donor"; }
