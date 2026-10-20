#include "sonic2_shadow.h"
#include "sonic2_party.h"
#include "sonic2_resources.h"
static int available(void)
{ return s2_party.shadow_enabled && s2_resource_shadow_bank()!=NULL; }
static const S2Character shadow={"shadow","SHADOW EXP",1,available};
void s2_shadow_register(void) { s2_character_register(&shadow); }
