#include "shop.hpp"
#include "settings.hpp"
#include <cmath>
#include <algorithm>

// =====================================================================
// BALANSE
// Gull er metaprogresjon: målet er at det tar ~30-40 gode runs å kjøpe alt,
// og at spillet da er ganske enkelt. Prisen øker med COST_GROWTH per nivå.
// =====================================================================
namespace {
    constexpr float COST_GROWTH = 1.4f;
}

int ShopItem::cost() const {
    return (int)std::round(baseCost * std::pow(COST_GROWTH, (float)currentLevel));
}

Shop::Shop() {
    //            oppgradering              lagringsnøkkel  navn                      beskrivelse (per nivå)          pris  maks
    items.push_back({ ShopUpgrade::VITALITY,     "vitality",   "Vitality",               "+10% maks HP",                  60,  5 });
    items.push_back({ ShopUpgrade::ARMOR,        "armor",      "Reinforced Armor",       "+3 armor",                      50,  5 });
    items.push_back({ ShopUpgrade::REGEN,        "regen",      "Regeneration",           "+0.4 HP per sekund",            60,  5 });
    items.push_back({ ShopUpgrade::EVASION,      "evasion",    "Float like a butterfly", "+4% evasion",                   60,  5 });
    items.push_back({ ShopUpgrade::SPEED,        "speed",      "Swift Boots",            "+5% fart",                      50,  5 });
    items.push_back({ ShopUpgrade::MIGHT,        "might",      "Might",                  "+10% skade",                    80,  5 });
    items.push_back({ ShopUpgrade::HASTE,        "haste",      "Haste",                  "-5% cooldown",                  80,  5 });
    items.push_back({ ShopUpgrade::AREA,         "area",       "Area",                   "+10% radius og treffomraade",   60,  5 });
    items.push_back({ ShopUpgrade::PROJECTILE,   "projectile", "Projectile count",       "+1 prosjektil",                200,  3 });
    items.push_back({ ShopUpgrade::MAGNET,       "magnet",     "Loot Magnet",            "+30 pickup-radius",             40,  5 });
    items.push_back({ ShopUpgrade::GROWTH,       "growth",     "Growth",                 "+8% XP",                        60,  5 });
    items.push_back({ ShopUpgrade::GREED,        "greed",      "Greed",                  "+10% gull",                     70,  5 });
    items.push_back({ ShopUpgrade::EXTRA_CHOICE, "choice",     "Flere valg",             "+1 valg ved level up",         400,  1 });
    items.push_back({ ShopUpgrade::AEGIS,        "aegis",      "Aegis Protection",       "+1 ekstra liv",                500,  1 });
}

int Shop::level(ShopUpgrade upgrade) const {
    for (const auto& item : items) {
        if (item.upgrade == upgrade) return item.currentLevel;
    }
    return 0;
}

float Shop::hpMult() const { return 1.0f + 0.10f * level(ShopUpgrade::VITALITY); }
float Shop::speedMult() const { return 1.0f + 0.05f * level(ShopUpgrade::SPEED); }
float Shop::armorBonus() const { return 3.0f * level(ShopUpgrade::ARMOR); }
int Shop::aegisBonus() const { return level(ShopUpgrade::AEGIS); }

void Shop::applyToPlayer(const CharacterData& baseChar, Player& player) const {
    // 1. Player arver basestats fra Character og legger på shop-bonusene
    player.maxHp = baseChar.maxHp * hpMult();
    player.hp = player.maxHp;
    player.speed = baseChar.speed * speedMult();
    player.armor = baseChar.armor + armorBonus();
    player.spellAmp = baseChar.spellAmp;
    player.lootRadius = baseChar.lootRadius + 30.0f * level(ShopUpgrade::MAGNET);
    player.xpMultiplier = baseChar.xpMultiplier * (1.0f + 0.08f * level(ShopUpgrade::GROWTH));

    // 2. Universelle shop-stats
    player.evasion = 0.05f + 0.04f * level(ShopUpgrade::EVASION);
    player.hpRegen = 0.4f * level(ShopUpgrade::REGEN);
    player.damageMult = 1.0f + 0.10f * level(ShopUpgrade::MIGHT);
    player.cooldownMult = std::pow(0.95f, (float)level(ShopUpgrade::HASTE));
    player.areaMult = 1.0f + 0.10f * level(ShopUpgrade::AREA);
    player.projectileCount = 1 + level(ShopUpgrade::PROJECTILE);
    player.goldMultiplier = 1.0f + 0.10f * level(ShopUpgrade::GREED);
    player.levelUpChoices = 3 + level(ShopUpgrade::EXTRA_CHOICE);
    player.aegis = aegisBonus();
}

void Shop::handleInput(int& totalGold) {
    int count = static_cast<int>(items.size());
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) {
        selectedOption = (selectedOption + 1) % count;
    }
    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) {
        selectedOption = (selectedOption - 1 + count) % count;
    }

    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
        ShopItem& item = items[selectedOption];
        if (!item.isMaxed() && totalGold >= item.cost()) {
            totalGold -= item.cost();
            item.currentLevel++;
        }
    }

    // DEBUG: gi deg selv gull for å teste shoppen. Fjern før release!
    if (IsKeyPressed(KEY_F9)) totalGold += 500;
}

void Shop::draw(int totalGold) {
    DrawText("POWER-UP SHOP", Settings::SCREEN_WIDTH / 2 - 140, 30, 32, GOLD);
    DrawText(TextFormat("Ditt gull: %d g", totalGold), Settings::SCREEN_WIDTH / 2 - 80, 72, 22, WHITE);

    const int visibleCount = 11;
    const int rowHeight = 40;
    const int startY = 115;
    const int left = 80;
    const int right = Settings::SCREEN_WIDTH - 80;

    if (selectedOption < scrollOffset) scrollOffset = selectedOption;
    if (selectedOption >= scrollOffset + visibleCount) scrollOffset = selectedOption - visibleCount + 1;

    for (int i = 0; i < visibleCount && (scrollOffset + i) < static_cast<int>(items.size()); i++) {
        int idx = scrollOffset + i;
        const ShopItem& item = items[idx];
        bool isSelected = (idx == selectedOption);
        bool canAfford = totalGold >= item.cost();
        int y = startY + i * rowHeight;

        if (isSelected) {
            DrawRectangle(left - 10, y - 6, right - left + 20, rowHeight - 4, Fade(DARKGRAY, 0.6f));
            DrawRectangleLines(left - 10, y - 6, right - left + 20, rowHeight - 4, YELLOW);
        }

        Color nameColor = item.isMaxed() ? GRAY : (isSelected ? YELLOW : WHITE);
        DrawText(item.name.c_str(), left, y, 20, nameColor);
        DrawText(item.description.c_str(), left + 280, y + 3, 16, LIGHTGRAY);

        // Nivå-prikker
        for (int l = 0; l < item.maxLevel; l++) {
            Color pip = (l < item.currentLevel) ? GOLD : Fade(DARKGRAY, 0.9f);
            DrawRectangle(left + 620 + l * 16, y + 5, 12, 12, pip);
        }

        // Pris
        const char* priceText = item.isMaxed() ? "MAKS" : TextFormat("%d g", item.cost());
        Color priceColor = item.isMaxed() ? GRAY : (canAfford ? GOLD : MAROON);
        int priceWidth = MeasureText(priceText, 20);
        DrawText(priceText, right - priceWidth, y, 20, priceColor);
    }

    // Scroll-piler hvis det finnes flere rader
    if (scrollOffset > 0) DrawText("^", Settings::SCREEN_WIDTH / 2, startY - 22, 20, GRAY);
    if (scrollOffset + visibleCount < (int)items.size()) DrawText("v", Settings::SCREEN_WIDTH / 2, startY + visibleCount * rowHeight - 8, 20, GRAY);

    DrawText("[W/S] naviger   [ENTER] kjoep   [ESC/B] tilbake", Settings::SCREEN_WIDTH / 2 - 250, Settings::SCREEN_HEIGHT - 60, 18, LIGHTGRAY);
}

void Shop::writeLevels(std::ostream& out) const {
    for (const auto& item : items) {
        out << item.saveKey << " " << item.currentLevel << "\n";
    }
}

bool Shop::readLevel(const std::string& key, int value) {
    for (auto& item : items) {
        if (item.saveKey == key) {
            item.currentLevel = std::max(0, std::min(value, item.maxLevel));
            return true;
        }
    }
    return false;
}

int Shop::totalCostOfEverything() const {
    int total = 0;
    for (ShopItem item : items) {
        for (item.currentLevel = 0; item.currentLevel < item.maxLevel; item.currentLevel++) {
            total += item.cost();
        }
    }
    return total;
}
