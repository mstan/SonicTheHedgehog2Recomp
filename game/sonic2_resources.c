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
static S2SpecialStageArt amy_special;
int s2_resources_prepare_stage_images(const unsigned char *rom,unsigned size)
{ return s2_stage_images_decode(rom,size,save_assets); }
static char errors[S2_RESOURCE_COUNT][192];
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
        if (i==S2_RESOURCE_AMY) memset(&amy_special,0,sizeof amy_special);
        if (i==S2_RESOURCE_SK) s2_save_assets_free(&save_assets);
        if (i==S2_RESOURCE_KIS2) {
            memset(&knuckles_art,0,sizeof knuckles_art);
            snprintf(errors[i],sizeof errors[i],"Optional original Knuckles artwork not selected");
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
    uint8_t hash[32]; char hex[65]; recompui_sha256_compute(rom, size, hash);
    for (unsigned n = 0; n < 32; ++n) snprintf(hex + n * 2, 3, "%02x", hash[n]);
    if (strcmp(hex, hashes[i])) { free(rom); return fail(i, "Wrong donor revision (SHA-256 mismatch)"); }
    if (i==S2_RESOURCE_KIS2) {
        int ok=s2_knuckles_art_decode(rom,size,&knuckles_art); free(rom);
        if (!ok) return fail(i,"Invalid original Knuckles artwork");
        owner_verified[i]=1; memmove(resource_path(i),path,strlen(path)+1);
        errors[i][0]=0; return 1;
    }
    S2DonorBank candidate={0}, shadow={0}; S2SaveAssets *menu=NULL;
    S2SpecialStageArt special_candidate={0};
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
                s2_amy_special_ui_decode(rom,size,&special_candidate);
            if(!ok)snprintf(errors[i],sizeof errors[i],"Invalid donor special-stage artwork");
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
    if (i==S2_RESOURCE_AMY) amy_special=special_candidate;
    if (i==S2_RESOURCE_MEGAMIX) { s2_donor_free(&shadow_bank); shadow_bank=shadow; }
    if (i==S2_RESOURCE_SK) { s2_save_assets_free(&save_assets); save_assets=menu; }
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
    memset(&amy_special,0,sizeof amy_special);
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
        } else fail(i, i==S2_RESOURCE_KIS2?"Optional original Knuckles in Sonic 2 ROM not selected":"Select your stock Sonic 3 & Knuckles combined ROM");
    }
}
int s2_resource_verified(unsigned i) { return i==S2_RESOURCE_KIS2?owner_verified[i]:i < S2_RESOURCE_COUNT && banks[i].count != 0; }
int s2_resource_owner_verified(unsigned i) { return i<S2_RESOURCE_COUNT && owner_verified[i]; }
const S2SaveAssets *s2_resource_save_assets(void) { return owner_verified[S2_RESOURCE_SK]?save_assets:NULL; }
const S2DonorBank *s2_resource_bank(unsigned i) { return i!=S2_RESOURCE_KIS2 && s2_resource_verified(i) ? &banks[i] : NULL; }
const S2KnucklesArt *s2_resource_knuckles_art(void) { return owner_verified[S2_RESOURCE_KIS2]?&knuckles_art:NULL; }
const S2SpecialStageArt *s2_resource_amy_special(void) { return owner_verified[S2_RESOURCE_AMY]?&amy_special:NULL; }
const S2DonorBank *s2_resource_shadow_bank(void) { return s2_resource_verified(S2_RESOURCE_MEGAMIX) && shadow_bank.count?&shadow_bank:NULL; }
const char *s2_resource_error(unsigned i) { return i < S2_RESOURCE_COUNT ? errors[i] : "Unknown donor"; }
