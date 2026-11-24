/* Build tool only. Load the original ROM read-only, then map the
 * separately assembled driver software into the unused cartridge range. */
#define main genesis_recompile
#define rom_parse sonic2_parse_rom_and_programs
#include "main_genesis.c"
#undef rom_parse
#undef main

#include "sonic2_clone_config.h"
#include "sonic2_clone_metadata.h"
#include "sonic2_clone_program.h"
#include "sonic2_music_clone_metadata.h"
#include "sonic2_music_clone_program.h"

bool rom_parse(const char *path, GenesisRom *out);

bool sonic2_parse_rom_and_programs(const char *path, GenesisRom *rom)
{
    if (!rom_parse(path, rom))
        return false;

    const size_t clone_end = S2_CLONE_LOAD_ADDRESS + sizeof s2_clone_program;
    const size_t music_end = S2_MUSIC_CLONE_LOAD_ADDRESS + sizeof s2_music_clone_program;
    if (rom->rom_size != S2_CLONE_LOAD_ADDRESS ||
        clone_end > S2_MUSIC_CLONE_LOAD_ADDRESS ||
        music_end > GENESIS_ROM_MAX_SIZE) {
        fprintf(stderr, "[GenesisRecomp] Sonic 2 driver programs do not fit the cartridge layout\n");
        rom_free(rom);
        return false;
    }

    uint8_t *image = realloc(rom->rom_data, music_end);
    if (!image) {
        rom_free(rom);
        return false;
    }
    rom->rom_data = image;
    rom->rom_size = (uint32_t)music_end;
    memcpy(image + S2_CLONE_LOAD_ADDRESS, s2_clone_program, sizeof s2_clone_program);
    memset(image + clone_end, 0, S2_MUSIC_CLONE_LOAD_ADDRESS - clone_end);
    memcpy(image + S2_MUSIC_CLONE_LOAD_ADDRESS, s2_music_clone_program,
           sizeof s2_music_clone_program);
    return true;
}

int main(int argc, char **argv)
{
    for (int i = 2; i + 1 < argc; ++i) {
        if (strcmp(argv[i], "--game") == 0 && strcmp(argv[i + 1], "game.toml") == 0) {
            argv[i + 1] = SONIC2_CLONE_CONFIG;
            break;
        }
    }
    return genesis_recompile(argc, argv);
}
