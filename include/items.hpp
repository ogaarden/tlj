#ifndef ITEMS_HPP
#define ITEMS_HPP

#include <raylib.h>
#include "ability_types.hpp"

struct Player;

// Items er passive gjenstander man finner i skattekister (se ability_types.hpp for listen).
// De nullstilles hver runde og har 5 nivåer hver.

struct ItemDef {
    const char* name;
    const char* description; // Hva ETT nivå gir
    Color color;
};

const ItemDef& GetItemDef(ItemId id);
void DrawItemIcon(ItemId id, Vector2 center, float size);

// Går opp ett nivå (eller plukker opp itemet) og legger effekten på spilleren
void ApplyItemLevel(Player& player, ItemId id);
bool HasItemSlotFree(const Player& player);
int UsedItemSlots(const Player& player); // Items + kombinerte items

// --- Kombinasjoner: to items på maks nivå -> ett sterkere item ---
struct ItemCombo {
    ItemId a, b;
    const char* name;
    const char* description; // Bonusen man får I TILLEGG til de to itemene
    Color color;
};
const ItemCombo& GetCombo(ComboId id);
ItemId GetComboPartner(ItemId item);                // Hvilket item dette kombineres med
bool CanCombine(const Player& player, ComboId id);  // Begge på maks nivå og ikke kombinert ennå
void ApplyCombo(Player& player, ComboId id);        // Fjerner de to fra item-plassene og legger til kombinasjonen
void DrawComboIcon(ComboId id, Vector2 center, float size);

#endif // ITEMS_HPP
