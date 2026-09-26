#include "items.hpp"
#include "player.hpp"
#include "icons.hpp"
#include <cmath>
#include <algorithm>
#include <raymath.h>

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
        { "Vampyrtann",     "+0.15 HP for hvert drap",                               Color{ 190, 20, 40, 255 } },
        { "Piggkrage",      "Fiender som treffer deg tar 60 skade (+60 per nivaa)",  Color{ 150, 150, 160, 255 } },
        { "Kronjuvel",      "+15% gull og +5% XP",                                   Color{ 80, 230, 170, 255 } },
        { "Skyggekappe",    "+4% sjanse for aa unnvike angrep",                      Color{ 110, 80, 160, 255 } },
        { "Kikkert",        "+10% prosjektilfart og rekkevidde",                     Color{ 200, 160, 90, 255 } },
        { "Evighetslys",    "+12% varighet paa flekker, virvler og effekter",        Color{ 255, 220, 150, 255 } },
        { "Trollspeil",     "+1 reroll i level-up og kister",                        Color{ 170, 220, 255, 255 } },
        { "Firklover",      "+15% flaks: oftere kister, mat og magneter",            Color{ 90, 200, 80, 255 } },
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

    // Trekant uansett hjørnerekkefølge (raylib vil ha dem mot klokka)
    void tri(Vector2 a, Vector2 b, Vector2 c, Color col) {
        float cross = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
        if (cross < 0.0f) DrawTriangle(a, b, c, col);
        else DrawTriangle(a, c, b, col);
    }
    Vector2 P(Vector2 c, float s, float x, float y) { return { c.x + x * s, c.y + y * s }; }

    // Vampyrtann: to hoggtenner med en bloddråpe
    void fang(Vector2 c, float s) {
        for (int k = -1; k <= 1; k += 2) {
            tri(P(c, s, k * 0.5f - 0.36f, -0.7f), P(c, s, k * 0.5f + 0.36f, -0.7f), P(c, s, k * 0.45f, 0.75f), INK);
            tri(P(c, s, k * 0.5f - 0.26f, -0.62f), P(c, s, k * 0.5f + 0.26f, -0.62f), P(c, s, k * 0.45f, 0.6f), { 245, 240, 225, 255 });
        }
        DrawCircleV(P(c, s, 0.45f, 0.85f), s * 0.16f, { 200, 20, 40, 255 });
        tri(P(c, s, 0.31f, 0.8f), P(c, s, 0.59f, 0.8f), P(c, s, 0.45f, 0.55f), { 200, 20, 40, 255 });
    }

    // Piggkrage: ring med pigger
    void collar(Vector2 c, float s) {
        for (int i = 0; i < 8; i++) {
            float a = i * PI / 4.0f;
            Vector2 d = { cosf(a), sinf(a) }, n = { -d.y, d.x };
            Vector2 b = { c.x + d.x * s * 0.55f, c.y + d.y * s * 0.55f };
            Vector2 tip = { c.x + d.x * s * 1.0f, c.y + d.y * s * 1.0f };
            tri({ b.x + n.x * s * 0.2f, b.y + n.y * s * 0.2f }, { b.x - n.x * s * 0.2f, b.y - n.y * s * 0.2f }, tip, INK);
            tri({ b.x + n.x * s * 0.13f, b.y + n.y * s * 0.13f }, { b.x - n.x * s * 0.13f, b.y - n.y * s * 0.13f }, { tip.x - d.x * 3, tip.y - d.y * 3 }, { 210, 215, 225, 255 });
        }
        DrawRing(c, s * 0.38f, s * 0.62f, 0, 360, 24, INK);
        DrawRing(c, s * 0.42f, s * 0.58f, 0, 360, 24, { 120, 70, 40, 255 });
    }

    // Kronjuvel: slipt edelstein
    void jewel(Vector2 c, float s) {
        Vector2 a = P(c, s, -0.75f, -0.2f), b = P(c, s, 0.75f, -0.2f), top1 = P(c, s, -0.4f, -0.65f), top2 = P(c, s, 0.4f, -0.65f), bot = P(c, s, 0.0f, 0.85f);
        const float g = 1.18f;
        auto big = [&](Vector2 v) { return Vector2{ c.x + (v.x - c.x) * g, c.y + (v.y - c.y) * g }; };
        tri(big(a), big(b), big(bot), INK); tri(big(top1), big(top2), big(b), INK); tri(big(top1), big(b), big(a), INK);
        tri(a, b, bot, { 40, 180, 130, 255 });
        tri(top1, top2, b, { 130, 250, 200, 255 }); tri(top1, b, a, { 130, 250, 200, 255 });
        tri(P(c, s, -0.3f, -0.2f), P(c, s, 0.3f, -0.2f), bot, { 80, 220, 170, 255 });
        DrawCircleV(P(c, s, -0.25f, -0.42f), s * 0.1f, { 255, 255, 255, 200 });
    }

    // Skyggekappe: mørk kappe med hette
    void cloak(Vector2 c, float s) {
        tri(P(c, s, 0.0f, -0.95f), P(c, s, -0.85f, 0.95f), P(c, s, 0.85f, 0.95f), INK);
        tri(P(c, s, 0.0f, -0.82f), P(c, s, -0.72f, 0.85f), P(c, s, 0.72f, 0.85f), { 90, 60, 140, 255 });
        DrawCircleV(P(c, s, 0.0f, -0.35f), s * 0.3f, INK);
        DrawCircleV(P(c, s, -0.1f, -0.35f), s * 0.05f, { 255, 220, 120, 255 });
        DrawCircleV(P(c, s, 0.1f, -0.35f), s * 0.05f, { 255, 220, 120, 255 });
    }

    // Kikkert: messingrør
    void spyglass(Vector2 c, float s) {
        Vector2 a = P(c, s, -0.85f, 0.55f), b = P(c, s, 0.85f, -0.55f), m = P(c, s, 0.0f, 0.0f);
        DrawLineEx(a, b, s * 0.5f, INK);
        DrawLineEx(a, m, s * 0.34f, { 140, 90, 50, 255 });
        DrawLineEx(m, b, s * 0.42f, { 220, 180, 90, 255 });
        DrawCircleV(b, s * 0.26f, INK);
        DrawCircleV(b, s * 0.19f, { 170, 220, 255, 255 });
        DrawCircleV(P(c, s, 0.8f, -0.62f), s * 0.06f, WHITE);
    }

    // Evighetslys: stearinlys med flamme
    void candle(Vector2 c, float s) {
        DrawRectangleRec({ c.x - s * 0.36f, c.y - s * 0.25f, s * 0.72f, s * 1.1f }, INK);
        DrawRectangleRec({ c.x - s * 0.28f, c.y - s * 0.18f, s * 0.56f, s * 0.98f }, { 250, 240, 215, 255 });
        float f = sinf((float)GetTime() * 12.0f) * 0.04f;
        DrawCircleV(P(c, s, f, -0.5f), s * 0.3f, { 255, 150, 40, 120 });
        tri(P(c, s, -0.16f, -0.42f), P(c, s, 0.16f, -0.42f), P(c, s, f, -0.95f), { 255, 170, 50, 255 });
        DrawCircleV(P(c, s, 0.0f, -0.45f), s * 0.16f, { 255, 170, 50, 255 });
        DrawCircleV(P(c, s, 0.0f, -0.45f), s * 0.08f, { 255, 250, 200, 255 });
    }

    // Trollspeil: ovalt speil med gullramme
    void mirror(Vector2 c, float s) {
        DrawEllipse((int)c.x, (int)(c.y - s * 0.15f), s * 0.68f + 2, s * 0.85f + 2, INK);
        DrawEllipse((int)c.x, (int)(c.y - s * 0.15f), s * 0.68f, s * 0.85f, { 230, 180, 60, 255 });
        DrawEllipse((int)c.x, (int)(c.y - s * 0.15f), s * 0.52f, s * 0.68f, { 150, 210, 250, 255 });
        DrawLineEx(P(c, s, -0.25f, -0.4f), P(c, s, 0.05f, -0.7f), s * 0.1f, { 255, 255, 255, 180 });
        DrawRectangleRec({ c.x - s * 0.1f, c.y + s * 0.65f, s * 0.2f, s * 0.35f }, { 230, 180, 60, 255 });
    }

    // Firkløver
    void clover(Vector2 c, float s) {
        DrawLineEx(P(c, s, 0.0f, 0.1f), P(c, s, 0.35f, 0.95f), s * 0.14f, { 50, 120, 40, 255 });
        for (int i = 0; i < 4; i++) {
            float a = i * PI / 2.0f + PI / 4.0f;
            Vector2 p = { c.x + cosf(a) * s * 0.38f, c.y + sinf(a) * s * 0.38f - s * 0.1f };
            DrawCircleV(p, s * 0.36f, INK);
        }
        for (int i = 0; i < 4; i++) {
            float a = i * PI / 2.0f + PI / 4.0f;
            Vector2 p = { c.x + cosf(a) * s * 0.38f, c.y + sinf(a) * s * 0.38f - s * 0.1f };
            DrawCircleV(p, s * 0.29f, { 90, 200, 80, 255 });
            DrawCircleV({ p.x - s * 0.08f, p.y - s * 0.08f }, s * 0.08f, { 200, 255, 190, 150 });
        }
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
        case ItemId::VAMPIRE_FANG:  fang(c, s); break;
        case ItemId::THORN_COLLAR:  collar(c, s); break;
        case ItemId::CROWN_JEWEL:   jewel(c, s); break;
        case ItemId::SHADOW_CLOAK:  cloak(c, s); break;
        case ItemId::SPYGLASS:      spyglass(c, s); break;
        case ItemId::CANDLE:        candle(c, s); break;
        case ItemId::MAGIC_MIRROR:  mirror(c, s); break;
        case ItemId::CLOVER:        clover(c, s); break;
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
        case ItemId::VAMPIRE_FANG: player.lifePerKill += 0.15f; break;
        case ItemId::THORN_COLLAR: player.thorns += 60.0f; break;
        case ItemId::CROWN_JEWEL:  player.goldMultiplier *= 1.15f; player.xpMultiplier *= 1.05f; break;
        case ItemId::SHADOW_CLOAK: player.evasion += 0.04f; break;
        case ItemId::SPYGLASS:     player.projectileSpeedMult *= 1.10f; break;
        case ItemId::CANDLE:       player.durationMult *= 1.12f; break;
        case ItemId::MAGIC_MIRROR: player.bonusRerolls += 1; break;
        case ItemId::CLOVER:       player.luck *= 1.15f; break;
        default: break;
    }
}
