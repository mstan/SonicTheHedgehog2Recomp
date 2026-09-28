/* Exercise the real menu, roster and rollback stream without ROM/network I/O. */
#include <assert.h>
#define s2_party_save test_party_save
#include "sonic2_options.c"
#undef s2_party_save

uint8_t g_ram[65536];
M68KState g_cpu;
static int net_active = 1, saves, save_ok = 1, save_overlay_calls;
static uint32_t tail_pc;
int genesis_netplay_active(void) { return net_active; }
int test_party_save(void) { ++saves; return save_ok; }
void recomp_tail_call(uint32_t pc) { tail_pc = pc; }
void s2_resources_load(const char *path) { (void)path; }
int s2_resource_verified(unsigned i) { (void)i; return 1; }
const S2SaveAssets *s2_resource_save_assets(void) { return NULL; }
void s2_save_menu_load(const char *path) { (void)path; }
void s2_runtime_load(void) {}
void s2_runtime_overlay(const GVDP *v, int line, uint32_t *out, int width)
{ (void)v; (void)line; (void)out; (void)width; }
int s2_save_menu_overlay(int line, uint32_t *out, int width)
{ (void)line; (void)out; (void)width; ++save_overlay_calls; return 0; }
static int available(void) { return 1; }
static const S2Character knuckles = {"knuckles", "KNUCKLES", 1, available};
static const S2Character amy = {"amy", "AMY", 1, available};

static void press(unsigned port, unsigned value)
{
    g_ram[0xF605] = port == 0 ? (uint8_t)value : 0;
    g_ram[0xF607] = port == 1 ? (uint8_t)value : 0;
    assert(s2_options_hook(0x90E0) == 1);
}
static size_t snapshot(uint8_t *out, size_t cap)
{
    S2StateIO io = {out, cap, 0, 0, 1};
    s2_options_rb_state(&io);
    assert(io.ok);
    return io.pos;
}
static void restore(uint8_t *data, size_t size)
{
    S2StateIO io = {data, size, 0, 1, 1};
    s2_options_rb_state(&io);
    assert(io.ok && io.pos == size);
    io.pos = 0; io.mode = 2;
    s2_options_rb_state(&io);
    assert(io.ok);
}
int main(void)
{
    uint8_t before[256], first[256], replay[256];
    s2_party_defaults();
    assert(s2_character_register(&knuckles) && s2_character_register(&amy));
    g_ram[0xF600] = 0x24;
    assert(!s2_options_hook(0x8FCC));
    press(0, 8); press(1, 8); /* Both synchronized native ports can edit: 2 -> 4. */
    assert(s2_party.roster.slots == 4);
    press(0, 2); press(0, 2); press(0, 2); /* P3 */
    press(0, 8); press(0, 8); press(0, 8);
    assert(!strcmp(s2_party.roster.character[2], "knuckles"));
    size_t len = snapshot(before, sizeof before);
    press(1, 2); press(1, 4); /* P4: NONE -> AMY. */
    assert(!strcmp(s2_party.roster.character[3], "amy"));
    assert(snapshot(first, sizeof first) == len);
    restore(before, len);
    assert(!strcmp(s2_party.roster.character[3], "none"));
    press(1, 2); press(1, 4);
    assert(snapshot(replay, sizeof replay) == len && !memcmp(first, replay, len));

    static GVDP vdp;
    uint32_t pixels[320] = {0};
    s2_options_overlay(&vdp, 16, pixels, 320);
    assert(pixels[16] == 0xFF708ADCu && !save_overlay_calls); /* Online menu visible. */
    save_ok = 0; /* Online APPLY must work even if local disk writes would fail. */
    assert(s2_options_hook(0x909A) == 1 && g_ram[0xF600] == 4 && saves == 0 && !tail_pc);

    net_active = 0;
    g_ram[0xF600] = 0x24;
    assert(s2_options_hook(0x909A) == 1 && saves == 1 && tail_pc == 0x9060);
    save_ok = 1;
    assert(s2_options_hook(0x909A) == 1 && saves == 2 && g_ram[0xF600] == 4);
    puts("PASS online four-character Options, visible overlay, rollback replay, no online disk writes, offline save");
    return 0;
}
