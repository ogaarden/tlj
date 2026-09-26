#ifndef ITEMS_HPP
#define ITEMS_HPP

#include <raylib.h>
#include "ability_types.hpp"

struct Player;

// Items er passive gjenstander man velger i level-up (se ability_types.hpp for listen).
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

#endif // ITEMS_HPP
