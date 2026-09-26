#include "items.hpp"
#include "player.hpp"
#include "icons.hpp"
#include <cmath>
#include <algorithm>

const ItemDef& GetItemDef(ItemId id) {
    static const ItemDef defs[(int)ItemId::COUNT] = {
        { "Sjonglorball",   "+1 prosjektil paa nivaa 1, 3 og 5 (ellers +5% skade)", Color{ 255, 120, 80, 255 } },
        { "Kongens kappe",  "+12% radius og treffomraade",                           Color{ 200, 40, 60, 255 } },
        { "Narreskoene",    "+8% fart",                                              Color{ 190, 130, 70, 255 } },
        { "Slipestein",     "+10% skade",                                            Color{ 210, 215, 230, 255 } },
        { "Timeglass",      "-7% cooldown",                                          Color{ 150, 200, 255, 255 } },
        { "Hjerteamulett",  "+15% maks HP og +0.3 HP/s regen",                       Color{ 230, 60, 80, 255 } },
        { "Ringbrynje",     "+5 armor",                                              Color{ 170, 180, 200, 255 } },
        { "Magnetstein",    "+40 radius for aa plukke opp XP og gull",               Color{ 230, 60, 60, 255 } },
        { "Uglefjaer",      "+10% XP",                                               Color{ 90, 170, 255, 255 } },
        { "Heldig terning", "+5% sjanse for kritisk treff (2x skade)",               Color{ 255, 200, 40, 255 } },
    };
    return defs[(int)id];
}

namespace {
    const Color INK = { 12, 10, 16, 255 };

    // Sjonglørball: tre baller i en bue
    void jugglingBalls(Vector2 c, float s) {
        const Color cols[3] = { { 230, 60, 60, 255 }, { 255, 210, 60, 255 }, { 80, 160, 255, 255 } };
        DrawRing(c, s * 0.7f, s * 0.78f, 200.0f, 340.0f, 20, { 255, 255, 255, 120 });
        for (int i = 0; i < 3; i++) {
            float a = PI * (1.15f + i * 0.35f) + (float)GetTime() * 0.8f * 0.0f;
            Vector2 p = { c.x + cosf(a) * s * 0.72f, c.y + sinf(a) * s * 0.72f + s * 0.25f };
            DrawCircleV(p, s * 0.33f, INK);
            DrawCircleV(p, s * 0.28f, cols[i]);
            DrawCircleV({ p.x - s * 0.09f, p.y - s * 0.09f }, s * 0.09f, { 255, 255, 255, 170 });
        }
    }

    // Heldig terning: hvit terning med prikker
    void die(Vector2 c, float s) {
        Rectangle r = { c.x - s * 0.62f, c.y - s * 0.62f, s * 1.24f, s * 1.24f };
        DrawRectangleRounded({ r.x - 2, r.y - 2, r.width + 4, r.height + 4 }, 0.3f, 6, INK);
        DrawRectangleRounded(r, 0.3f, 6, { 245, 240, 230, 255 });
        const Vector2 pips[5] = { { -0.3f, -0.3f }, { 0.3f, -0.3f }, { 0.0f, 0.0f }, { -0.3f, 0.3f }, { 0.3f, 0.3f } };
        for (Vector2 p : pips) DrawCircleV({ c.x + p.x * s, c.y + p.y * s }, s * 0.11f, { 200, 30, 40, 255 });
    }
}

void DrawItemIcon(ItemId id, Vector2 c, float s) {
    switch (id) {
        case ItemId::JUGGLING_BALL: jugglingBalls(c, s); break;
        case ItemId::ROYAL_CAPE:    DrawUpgradeIcon(ShopUpgrade::AREA, c, s); break;
        case ItemId::JESTER_SHOES:  DrawUpgradeIcon(ShopUpgrade::SPEED, c, s); break;
        case ItemId::WHETSTONE:     DrawUpgradeIcon(ShopUpgrade::MIGHT, c, s); break;
        case ItemId::HOURGLASS:     DrawUpgradeIcon(ShopUpgrade::HASTE, c, s); break;
        case ItemId::HEART_AMULET:  DrawUpgradeIcon(ShopUpgrade::REGEN, c, s); break;
        case ItemId::CHAINMAIL:     DrawUpgradeIcon(ShopUpgrade::ARMOR, c, s); break;
        case ItemId::LODESTONE:     DrawUpgradeIcon(ShopUpgrade::MAGNET, c, s); break;
        case ItemId::OWL_FEATHER:   DrawUpgradeIcon(ShopUpgrade::GROWTH, c, s); break;
        case ItemId::LUCKY_DIE:     die(c, s); break;
        default: break;
    }
}

bool HasItemSlotFree(const Player& player) {
    return (int)player.items.size() < MAX_ITEM_SLOTS;
}

void ApplyItemLevel(Player& player, ItemId id) {
    int& level = player.itemLevels[(int)id];
    if (level >= MAX_ITEM_LEVEL) return;
    if (level == 0) player.items.push_back(id);
    level++;

    switch (id) {
        case ItemId::JUGGLING_BALL:
            if (level % 2 == 1) player.projectileCount += 1;
            else player.damageMult *= 1.05f;
            break;
        case ItemId::ROYAL_CAPE:   player.areaMult *= 1.12f; break;
        case ItemId::JESTER_SHOES: player.speed *= 1.08f; break;
        case ItemId::WHETSTONE:    player.damageMult *= 1.10f; break;
        case ItemId::HOURGLASS:    player.cooldownMult *= 0.93f; break;
        case ItemId::HEART_AMULET: {
            float gain = player.maxHp * 0.15f;
            player.maxHp += gain;
            player.hp += gain;
            player.hpRegen += 0.3f;
        } break;
        case ItemId::CHAINMAIL:    player.armor += 5.0f; break;
        case ItemId::LODESTONE:    player.lootRadius += 40.0f; break;
        case ItemId::OWL_FEATHER:  player.xpMultiplier *= 1.10f; break;
        case ItemId::LUCKY_DIE:    player.critChance += 0.05f; break;
        default: break;
    }
}
