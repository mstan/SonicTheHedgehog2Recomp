#include "sonic2_stage_layouts.h"
#include <stdlib.h>
#include <string.h>

static unsigned word(const uint8_t *p) { return (unsigned)p[0]<<8|p[1]; }
static uint32_t longword(const uint8_t *p)
{ return (uint32_t)word(p)<<16|word(p+2); }

int s2_stage_layouts_decode(const uint8_t *rom,size_t size,S2StageLayouts *out)
{
    const size_t table=0xDF370;
    if(!rom || !out || size<table+S2_STAGE_LAYOUT_COUNT*4) return 0;
    S2StageLayouts *candidate=calloc(1,sizeof *candidate);
    if(!candidate) return 0;
    for(unsigned i=0;i<S2_STAGE_LAYOUT_COUNT;++i) {
        size_t start=longword(rom+table+i*4),end=start;
        unsigned count=0;
        if((start&1) || start>=size) goto invalid;
        for(;;) {
            if(end>size || size-end<6 || count++>1024) goto invalid;
            unsigned x=word(rom+end);
            if(x==0xFFFF) { end+=6; break; }
            /* ARZ2 contains out-of-order records in the original cartridge.
             * Retain the exact list order consumed by its scrolling loader. */
            end+=6;
        }
        size_t bytes=end-start;
        if(bytes>S2_STAGE_LAYOUT_BYTES-candidate->size) goto invalid;
        candidate->offset[i]=(uint32_t)candidate->size;
        memcpy(candidate->data+candidate->size,rom+start,bytes);
        candidate->size+=bytes;
    }
    candidate->offset[S2_STAGE_LAYOUT_COUNT]=(uint32_t)candidate->size;
    if(size>=0x3071D2+28){
        memcpy(candidate->special_ring_requirements,rom+0x3071D2,28);
        candidate->special_ring_requirements_ready=1;
    }
    *out=*candidate;
    free(candidate);
    return 1;
invalid:
    free(candidate);
    return 0;
}
S2StagePlacementView s2_stage_layout_view(const S2StageLayouts *layouts,unsigned zone,unsigned act)
{
    if(!layouts || !layouts->size || act>1 || zone>=S2_STAGE_LAYOUT_COUNT/2)
        return (S2StagePlacementView){0};
    unsigned index=zone*2+act;
    size_t start=layouts->offset[index],end=layouts->offset[index+1];
    if(start>=end || end>layouts->size) return (S2StagePlacementView){0};
    return (S2StagePlacementView){layouts->data+start,end-start};
}

enum {
    RECORD_BYTES=6, CAMERA_X=0xEE00, CAMERA_COARSE=0xF7DA,
    MANAGER_ROUTINE=0xF76C, CAMERA_LAST=0xF76E, CAMERA_LAST_P2=0xF78C,
    CURSOR_RIGHT=0xF770, CURSOR_LEFT=0xF774, CURSOR_RIGHT_P2=0xF778, CURSOR_LEFT_P2=0xF77C,
    RESPAWN_RIGHT=0xFC00, RESPAWN_LEFT=0xFC01, RESPAWN_FLAGS=0xFC02, RESPAWN_BYTES=0xBE,
    DYNAMIC_FIRST=0xB400, DYNAMIC_END=0xD000, OBJECT_BYTES=64
};

static void write_word(uint8_t *p,unsigned value)
{
    p[0]=(uint8_t)(value>>8);
    p[1]=(uint8_t)value;
}
static void write_cursor(uint8_t *p,uint32_t offset)
{
    write_word(p,offset>>16);
    write_word(p+2,offset);
}
static unsigned placement_x(S2StagePlacementView placements,size_t position)
{
    return position<=placements.bytes && placements.bytes-position>=RECORD_BYTES
        ? word(placements.data+position):0xFFFFu;
}
static size_t skip_until(S2StagePlacementView placements,size_t position,unsigned x,uint8_t *index)
{
    while(placement_x(placements,position)<x) {
        *index+=(placements.data[position+2]&0x80)!=0;
        position+=RECORD_BYTES;
    }
    return position;
}

void s2_stage_layout_prepare(S2StagePlacementView placements,uint8_t ram[65536])
{
    unsigned camera=word(ram+CAMERA_X);
    unsigned start=camera>=128?(camera-128)&0xFF80u:0;
    ram[MANAGER_ROUTINE]=2;
    ram[RESPAWN_RIGHT]=ram[RESPAWN_LEFT]=1;
    memset(ram+RESPAWN_FLAGS,0,RESPAWN_BYTES);
    size_t right=skip_until(placements,0,start,ram+RESPAWN_RIGHT);
    size_t left=start>=128?skip_until(placements,0,start-128,ram+RESPAWN_LEFT):0;
    write_cursor(ram+CURSOR_RIGHT,(uint32_t)right);
    write_cursor(ram+CURSOR_LEFT,(uint32_t)left);
    write_cursor(ram+CURSOR_RIGHT_P2,(uint32_t)right);
    write_cursor(ram+CURSOR_LEFT_P2,(uint32_t)left);
    write_word(ram+CAMERA_LAST,0xFFFFu);
    write_word(ram+CAMERA_LAST_P2,0xFFFFu);
}

static int load_object(const uint8_t *record,uint8_t index,uint8_t *ram)
{
    unsigned y=word(record+2);
    if(y&0x8000u) {
        uint8_t *flag=ram+RESPAWN_FLAGS+index;
        int already_loaded=(*flag&0x80)!=0;
        *flag|=0x80;
        if(already_loaded) return 1;
    }
    unsigned slot=DYNAMIC_FIRST;
    while(slot<DYNAMIC_END && ram[slot]) slot+=OBJECT_BYTES;
    if(slot==DYNAMIC_END) return 0;
    uint8_t *object=ram+slot;
    write_word(object+8,word(record));
    write_word(object+12,y&0x0FFFu);
    object[1]=object[0x22]=(uint8_t)((y>>13)&3);
    if(y&0x8000u) object[0x23]=index;
    object[0x28]=record[5];
    object[0]=record[4];
    return 1;
}

void s2_stage_layout_stream(S2StagePlacementView placements,uint8_t ram[65536])
{
    unsigned camera=word(ram+CAMERA_X)&0xFF80u;
    unsigned left_bound=(camera-128)&0xFFFFu;
    unsigned right_bound=(camera+640)&0xFFFFu;
    int last=(int16_t)word(ram+CAMERA_LAST);
    write_word(ram+CAMERA_COARSE,left_bound);
    if((int16_t)camera==last) return;
    write_word(ram+CAMERA_LAST,camera);

    /* Match the campaign's native directional cursors and respawn flags.
     * These are byte offsets into the host list, sealed in guest WRAM. */
    size_t right=longword(ram+CURSOR_RIGHT),left=longword(ram+CURSOR_LEFT);
    if(right>=placements.bytes || left>=placements.bytes ||
       right%RECORD_BYTES || left%RECORD_BYTES) return;
    if((int16_t)camera>last) {
        while(placement_x(placements,right)<right_bound) {
            const uint8_t *record=placements.data+right;
            uint8_t index=0;
            if(record[2]&0x80) index=ram[RESPAWN_RIGHT]++;
            if(!load_object(record,index,ram)) break;
            right+=RECORD_BYTES;
        }
        if(right_bound>=768) left=skip_until(placements,left,right_bound-768,ram+RESPAWN_LEFT);
    } else {
        while(left>=RECORD_BYTES && camera>=128 &&
              (int16_t)placement_x(placements,left-RECORD_BYTES)>(int16_t)left_bound) {
            const uint8_t *record=placements.data+left-RECORD_BYTES;
            int tracked=(record[2]&0x80)!=0;
            if(tracked) --ram[RESPAWN_LEFT];
            if(!load_object(record,ram[RESPAWN_LEFT],ram)) {
                if(tracked) ++ram[RESPAWN_LEFT];
                break;
            }
            left-=RECORD_BYTES;
        }
        while(right>=RECORD_BYTES &&
              (int16_t)placement_x(placements,right-RECORD_BYTES)>=(int16_t)right_bound) {
            right-=RECORD_BYTES;
            ram[RESPAWN_RIGHT]-=(placements.data[right+2]&0x80)!=0;
        }
    }
    write_cursor(ram+CURSOR_RIGHT,(uint32_t)right);
    write_cursor(ram+CURSOR_LEFT,(uint32_t)left);
}
