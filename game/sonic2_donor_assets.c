#include "sonic2_donor_assets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* See docs/SONIC2_DONOR_PORT.md. These describe verified donor revisions,
 * never offsets in the Sonic 2 host image. Callers must verify donor identity. */
const S2DonorLayout s2_amy_171_layout = {253,0x8B8C0,0x8D6CE,0x60000,0x29E2,8,0x1C96E,44};
/* PalCycle_SuperHyperKnuckles, byte-matched combined S3&K listing. */
const S2DonorLayout s2_sk_knuckles_layout = {251,0x14A8D6,0x14BD0A,0x1200E0,0xA8AFC,6,0x17EF4,37,0x3AAE,10};
/* Same verified v3 bindings as Sonic 3's tools/megamix/v3_rom.py. S1
 * mappings/DPLCs have byte counts, five-byte pieces and signed byte X. */
const S2DonorLayout s2_megamix_mighty_layout = {176,0x2A9D6,0x2C3D2,0xA9364,0x2C60,5,0x1AA1C,38};
const S2DonorLayout s2_megamix_shadow_layout = {176,0x2B6A4,0x2CA1C,0xBE2E4,0x2C80,5,0x1AA1C,38,0,0,0x1AB5E,38};
static unsigned word(const uint8_t *p) { return (unsigned)p[0]*256+p[1]; }
void s2_donor_water_palette(const uint16_t donor[16],const uint16_t normal[16],
                            const uint16_t water[16],uint16_t out[16])
{
    out[0]=water[0];
    for(unsigned c=1;c<16;++c) {
        unsigned best=1,distance=~0u;
        for(unsigned n=1;n<16;++n) {
            unsigned delta=0;
            for(unsigned shift=0;shift<=8;shift+=4) {
                int d=(int)((donor[c]>>shift)&14)-(int)((normal[n]>>shift)&14);
                delta+=(unsigned)(d*d);
            }
            if(delta<distance){distance=delta;best=n;}
        }
        out[c]=water[best];
    }
}
static int inside(size_t size,size_t offset,size_t length) { return offset<=size && length<=size-offset; }
int s2_amy_special_decode(const uint8_t *rom,size_t size,S2SpecialStageArt *out)
{
    if(!rom || !out || size!=0x200000)return 0;
    S2SpecialStageArt candidate={0};
    /* The original Obj09 uses the stock 18-frame animation sequence, but
     * supplies its own geometry and standard DPLCs into raw ROM art. */
    for(unsigned frame=0;frame<S2_SPECIAL_FRAMES;++frame) {
        size_t mp=0x36490+word(rom+0x36490+frame*2);
        size_t dp=0x36878+word(rom+0x36878+frame*2);
        if(!inside(size,mp,2) || !inside(size,dp,2))return 0;
        unsigned pieces=word(rom+mp),cues=word(rom+dp),tiles=0;
        if(!pieces || pieces>S2_SPECIAL_PIECES || !cues || cues>S2_SPECIAL_PIECES ||
           !inside(size,mp+2,pieces*8) || !inside(size,dp+2,cues*2))return 0;
        S2SpecialArtFrame *f=&candidate.frames[frame];f->count=(uint8_t)pieces;
        for(unsigned i=0;i<cues;++i) {
            unsigned code=word(rom+dp+2+i*2),count=(code>>12)+1;
            size_t source=0x106DC2+(size_t)(code&4095)*32;
            if(tiles+count>S2_SPECIAL_TILES || !inside(size,source,count*32))return 0;
            memcpy(f->tiles+tiles*32,rom+source,count*32);tiles+=count;
        }
        for(unsigned i=0;i<pieces;++i) {
            const uint8_t *p=rom+mp+2+i*8;S2SpecialPiece *piece=&f->pieces[i];
            piece->y=(int8_t)p[0];piece->size=p[1];piece->attr=(uint16_t)word(p+2);
            piece->x=(int16_t)word(p+6);
            unsigned cells=(((piece->size>>2)&3)+1)*((piece->size&3)+1);
            if(piece->size>15 || (piece->attr&0x6000) ||
               (piece->attr&2047)+cells>tiles || piece->x < -128 || piece->x>127)return 0;
        }
    }
    /* PalLoad_ForFade entry $18 ($2842) selects $3142; Obj09 uses line1. */
    for(unsigned i=0;i<16;++i)candidate.palette[i]=(uint16_t)word(rom+0x3162+i*2);
    *out=candidate;return 1;
}
void s2_donor_free(S2DonorBank *bank)
{
    if (!bank) return;
    for (unsigned i=0;i<S2_DONOR_MAX_FRAMES;++i) free(bank->frames[i].pixels);
    memset(bank,0,sizeof *bank);
}
int s2_donor_decode(const uint8_t *rom,size_t size,const S2DonorLayout *l,
                    S2DonorBank *out,char *error,size_t error_size)
{
    if (error && error_size) error[0]=0;
    if (!rom || !l || !out || !l->frames || l->frames>S2_DONOR_MAX_FRAMES ||
        (l->mapping_stride!=5 && l->mapping_stride!=6 && l->mapping_stride!=8) ||
        !inside(size,l->mappings,l->frames*2) || !inside(size,l->dplc,l->frames*2) ||
        !inside(size,l->palette,32) || l->animation_count>64 || l->super_frames>10 ||
        (l->super_frames && !inside(size,l->super_palette,l->super_frames*6)) ||
        !inside(size,l->animations,l->animation_count*2) || l->super_animation_count>64 ||
        !inside(size,l->super_animations,l->super_animation_count*2)) {
        if (error && error_size) snprintf(error,error_size,"Invalid donor asset layout");
        return 0;
    }
    /* Build transactionally; never destroy a previously loaded bank on error.
     * Caller initializes *out to zero before its first decode. */
    S2DonorBank *bank=(S2DonorBank *)calloc(1,sizeof *bank);
    if (!bank) return 0;
    bank->count=l->frames;
    for (unsigned c=0;c<16;++c) bank->palette[c]=(uint16_t)word(rom+l->palette+c*2);
    bank->super_frames=l->super_frames;
    for (unsigned f=0;f<l->super_frames;++f) for (unsigned c=0;c<3;++c)
        bank->super_palette[f][c]=(uint16_t)word(rom+l->super_palette+f*6+c*2);
    unsigned frame=0;
    for (;frame<l->frames;++frame) {
        size_t mp=(size_t)l->mappings+word(rom+l->mappings+frame*2);
        size_t dp=(size_t)l->dplc+word(rom+l->dplc+frame*2);
        unsigned count_size=l->mapping_stride==5?1:2;
        if (!inside(size,mp,count_size) || !inside(size,dp,count_size)) goto bad;
        unsigned pieces=count_size==1?rom[mp]:word(rom+mp), cues=count_size==1?rom[dp]:word(rom+dp);
        mp+=count_size; dp+=count_size;
        if (pieces>S2_DONOR_MAX_PIECES || cues>64 ||
            !inside(size,mp,pieces*l->mapping_stride) || !inside(size,dp,cues*2)) goto bad;
        uint8_t tiles[1024*32]; unsigned tile_count=0;
        for (unsigned cue=0;cue<cues;++cue) {
            unsigned code=word(rom+dp+cue*2), count=(code>>12)+1;
            size_t source=(size_t)l->art+(code&4095)*32;
            if (tile_count+count>1024 || !inside(size,source,count*32)) goto bad;
            memcpy(tiles+tile_count*32,rom+source,count*32);
            tile_count+=count;
        }
        if (!pieces) continue;
        int minx=32767,miny=32767,maxx=-32768,maxy=-32768;
        for (unsigned piece=0;piece<pieces;++piece) {
            const uint8_t *p=rom+mp+piece*l->mapping_stride;
            int y=(int8_t)p[0],x=l->mapping_stride==5?(int8_t)p[4]:(int16_t)word(p+l->mapping_stride-2);
            unsigned w=((p[1]>>2)&3)+1,h=(p[1]&3)+1,attr=word(p+2);
            if ((attr&2047)+w*h>tile_count || (attr&0x6000)) goto bad;
            if (x<minx) minx=x;
            if (y<miny) miny=y;
            if (x+(int)w*8>maxx) maxx=x+w*8;
            if (y+(int)h*8>maxy) maxy=y+h*8;
        }
        if (minx < -128 || miny < -128 || maxx>128 || maxy>128 || maxx<=minx || maxy<=miny) goto bad;
        S2DonorFrame *f=&bank->frames[frame];
        f->x=(int16_t)minx; f->y=(int16_t)miny;
        f->width=(uint16_t)(maxx-minx); f->height=(uint16_t)(maxy-miny);
        f->pixels=(uint8_t *)calloc((size_t)f->width*f->height,1);
        if (!f->pixels) goto bad;
        /* Native SAT priority: earlier pieces win overlapping nonzero pixels. */
        for (unsigned piece=0;piece<pieces;++piece) {
            const uint8_t *p=rom+mp+piece*l->mapping_stride;
            int y=(int8_t)p[0],x=l->mapping_stride==5?(int8_t)p[4]:(int16_t)word(p+l->mapping_stride-2);
            unsigned w=(((p[1]>>2)&3)+1)*8,h=((p[1]&3)+1)*8,attr=word(p+2);
            for (unsigned py=0;py<h;++py) for (unsigned px=0;px<w;++px) {
                unsigned sx=attr&0x0800?w-1-px:px,sy=attr&0x1000?h-1-py:py;
                unsigned tile=(attr&2047)+(sx/8)*(h/8)+sy/8;
                unsigned byte=tiles[tile*32+(sy%8)*4+(sx%8)/2];
                unsigned nibble=sx&1?byte&15:byte>>4;
                size_t dest=(size_t)(y-miny+py)*f->width+x-minx+px;
                if (!f->pixels[dest]) f->pixels[dest]=(uint8_t)nibble;
            }
        }
    }
    bank->animation_count=l->animation_count;
    bank->super_animation_count=l->super_animation_count;
    for (unsigned n=0;n<l->animation_count+l->super_animation_count;++n) {
        unsigned super=n>=l->animation_count,i=super?n-l->animation_count:n;
        unsigned base=super?l->super_animations:l->animations;
        /* ADDA.W sign-extends relative animation offsets. Super scripts
         * intentionally refer back into the normal sequence bank. */
        size_t at=(size_t)((int64_t)base+(int16_t)word(rom+base+i*2));
        unsigned length=1;
        if (!inside(size,at,2)) goto bad;
        for (;;) {
            if (length>=255 || !inside(size,at,length+1)) goto bad;
            unsigned code=rom[at+length++];
            if (code>=0xFC) {
                if (code==0xFD || code==0xFE) {
                    if (!inside(size,at,length+1)) goto bad;
                    unsigned argument=rom[at+length++];
                    if ((code==0xFD && argument>=(super?l->super_animation_count:l->animation_count)) ||
                        (code==0xFE && (!argument || argument>length-3))) goto bad;
                }
                break;
            }
            if (code>=l->frames) goto bad;
        }
        if (super) {
            bank->super_animation_length[i]=(uint16_t)length;
            memcpy(bank->super_animations[i],rom+at,length);
        } else {
            bank->animation_length[i]=(uint16_t)length;
            memcpy(bank->animations[i],rom+at,length);
        }
    }
    s2_donor_free(out); *out=*bank; free(bank);
    return 1;
bad:
    if (error && error_size) snprintf(error,error_size,"Invalid mapping or DPLC at donor frame %u",frame);
    s2_donor_free(bank); free(bank); return 0;
}
