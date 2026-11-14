#pragma once
#include <stddef.h>
#include <stdint.h>

enum { S2_MAX_PLAYERS = 6, S2_NETPLAY_PLAYERS = S2_MAX_PLAYERS, S2_MAX_CHARACTERS = 16, S2_CHARACTER_ID_SIZE = 32 };
typedef struct S2Character {
    const char *id;
    const char *label;
    unsigned native_object_id;
    int (*available)(void);
} S2Character;

typedef struct S2Roster {
    unsigned slots;
    char character[S2_MAX_PLAYERS][S2_CHARACTER_ID_SIZE];
} S2Roster;

typedef struct S2PartyConfig {
    S2Roster roster;
    int infinite_lives; /* opt-in native practice options */
    int infinite_time;
    int easy_debug; /* optional native Debug Mode, off by default */
    int amy_enabled;
    int s3k_enabled;
    int save_menu_enabled; /* independent default-OFF S3&K feature */
    int mighty_enabled; /* default-OFF, verified Megamix v3 donor */
    int shadow_enabled; /* requires the same verified Megamix donor */
    int knuckles_campaign_enabled; /* verified local capability; session-scoped online */
    int knuckles_flow; /* optional landing-to-spindash input, original behavior by default */
    int presentation_enabled;
    int save_entry_warp,save_card_act,title_card_colors,options_panel_fade,options_slow_backdrop;
    char amy_path[1024];
    char s3k_path[1024];
    char mighty_path[1024];
    char kis2_path[1024]; /* original KiS2 artwork and Knuckles-led campaign stages */
    char campaign_path[1024]; /* empty until selected or first save; exe-relative */
} S2PartyConfig;

extern S2PartyConfig s2_party;
void s2_party_defaults(void);
int s2_character_register(const S2Character *definition);
const S2Character *s2_character_find(const char *id);
const S2Character *s2_character_at(unsigned index);
unsigned s2_character_count(void);
int s2_roster_set(S2Roster *roster, unsigned player, const char *character);
int s2_roster_cycle(S2Roster *roster, unsigned player, int direction);
void s2_roster_slots(S2Roster *roster, unsigned slots);
int s2_roster_validate(S2Roster *roster);
int s2_roster_vs_ready(const S2Roster *roster);
/* Validated original P1/P2 roles; inactive capacity does not add players. */
int s2_roster_is_classic(const S2Roster *roster);
void s2_party_load(const char *settings_path);
int s2_party_save(void);
const char *s2_party_error(void);
