#include "global.h"
#include "pokemon.h"
#include "constants/items.h"

#include "data/pokemon/evolution.h"

#include <assert.h>

static void AssertLevelEvolution(u16 species, u16 level, u16 targetSpecies)
{
    assert(gEvolutionTable[species][0].method == EVO_LEVEL);
    assert(gEvolutionTable[species][0].param == level);
    assert(gEvolutionTable[species][0].targetSpecies == targetSpecies);
}

static void AssertItemEvolution(u16 species, u16 item, u16 targetSpecies)
{
    u8 i;
    bool8 found = FALSE;

    for (i = 0; i < EVOS_PER_MON; i++)
    {
        if (gEvolutionTable[species][i].method == EVO_ITEM
         && gEvolutionTable[species][i].param == item
         && gEvolutionTable[species][i].targetSpecies == targetSpecies)
        {
            found = TRUE;
            break;
        }
    }
    assert(found);
}

int main(void)
{
    AssertLevelEvolution(SPECIES_KADABRA, 40, SPECIES_ALAKAZAM);
    AssertLevelEvolution(SPECIES_MACHOKE, 40, SPECIES_MACHAMP);
    AssertLevelEvolution(SPECIES_GRAVELER, 40, SPECIES_GOLEM);
    AssertLevelEvolution(SPECIES_HAUNTER, 40, SPECIES_GENGAR);

    AssertItemEvolution(SPECIES_ONIX, ITEM_METAL_COAT, SPECIES_STEELIX);
    AssertItemEvolution(SPECIES_SCYTHER, ITEM_METAL_COAT, SPECIES_SCIZOR);
    AssertItemEvolution(SPECIES_SEADRA, ITEM_DRAGON_SCALE, SPECIES_KINGDRA);
    AssertItemEvolution(SPECIES_PORYGON, ITEM_UP_GRADE, SPECIES_PORYGON2);
    AssertItemEvolution(SPECIES_SLOWPOKE, ITEM_KINGS_ROCK, SPECIES_SLOWKING);
    AssertItemEvolution(SPECIES_POLIWHIRL, ITEM_KINGS_ROCK, SPECIES_POLITOED);
    AssertItemEvolution(SPECIES_CLAMPERL, ITEM_DEEP_SEA_TOOTH, SPECIES_HUNTAIL);
    AssertItemEvolution(SPECIES_CLAMPERL, ITEM_DEEP_SEA_SCALE, SPECIES_GOREBYSS);
    return 0;
}
