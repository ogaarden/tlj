#include "shop.hpp"
#include "settings.hpp"

Shop::Shop() : selectedOption(0), scrollOffset(0) {
    // Øker HP med +10% per nivå (1.10x, 1.20x ...)
    items.push_back({ "Health Boost", "+10% Max HP", 100, 0, 5, [this]() { hpMult += 0.10f; } });
    
    // Øker Speed med +10% per nivå
    items.push_back({ "Swift Boots", "+5% Speed", 110, 0, 5, [this]() { speedMult += 0.10f; } });
    
    // Øker Armor med +15% per nivå
    items.push_back({ "Reinforced Armor", "+10% Armor", 120, 0, 5, [this]() { armorMult += 0.15f; } });
    
    // Universell ekstra-stat som gjelder Player uavhengig av karakter
    items.push_back({ "Aegis Protection", "+1 Ekstra liv (Aegis)", 1000, 0, 1, [this]() { aegisBonus += 1; } });

    // Øker loot radius med 50
    items.push_back({"Loot Magnet", "+50 ekstra radius", 100, 0 ,5, [this]() {lootRadiusAdd += 50;} });

    items.push_back({"Float like a butterfly", "+5% evasion", 100, 0 ,5, [this](){evasionAdd += 0.05f;} });

    items.push_back({"Projecile count", "+1 projectile", 250, 0 ,3, [this](){extraProjectile += 1;} });
   
}

void Shop::applyToPlayer(const CharacterData& baseChar, Player& player) const {
    // 1. Player arver basestats fra Character og ganger med Shop-multiplikatorene
    player.maxHp = baseChar.maxHp * hpMult;
    player.hp = player.maxHp;
    player.speed = baseChar.speed * speedMult;
    player.armor = baseChar.armor * armorMult;
    player.spellAmp = baseChar.spellAmp;
    player.lootRadius = baseChar.lootRadius + lootRadiusAdd;
    player.evasion = player.evasion + evasionAdd;
    player.projectileCount = player.projectileCount + extraProjectile;


    // 2. Universelle shop-stats
    player.aegis = aegisBonus;
}

void Shop::handleInput(int& totalGold) {
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) {
        selectedOption = (selectedOption + 1) % static_cast<int>(items.size());
    }
    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) {
        selectedOption = (selectedOption - 1 + static_cast<int>(items.size())) % static_cast<int>(items.size());
    }

    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
        ShopItem& item = items[selectedOption];
        if (totalGold >= item.cost && item.currentLevel < item.maxLevel) {
            totalGold -= item.cost;
            item.currentLevel++;
            item.applyUpgrade(); // Øker multiplikatoren
            item.cost = static_cast<int>(item.cost * 1.5f);
        }
    }
}

void Shop::draw(int totalGold) {
    DrawText("POWER-UP SHOP", Settings::SCREEN_WIDTH / 2 - 140, 40, 32, GOLD);
    DrawText(TextFormat("Ditt Gull: %d g", totalGold), Settings::SCREEN_WIDTH / 2 - 80, 80, 22, WHITE);

    int visibleCount = 8;
    int startY = 120;

    if (selectedOption < scrollOffset) scrollOffset = selectedOption;
    if (selectedOption >= scrollOffset + visibleCount) scrollOffset = selectedOption - visibleCount + 1;

    for (int i = 0; i < visibleCount && (scrollOffset + i) < static_cast<int>(items.size()); i++) {
        int idx = scrollOffset + i;
        bool isSelected = (idx == selectedOption);
        
        bool isMax = (items[idx].currentLevel >= items[idx].maxLevel);
        Color textColor = isMax ? GRAY : (isSelected ? YELLOW : WHITE);
        const char* prefix = isSelected ? "> " : "  ";

        std::string priceText = isMax ? "MAX" : TextFormat("%d g", items[idx].cost);

        std::string line = TextFormat("%s%s (%s) - Nivaa %d/%d - Pris: %s",
            prefix,
            items[idx].name.c_str(),
            items[idx].description.c_str(),
            items[idx].currentLevel,
            items[idx].maxLevel,
            priceText.c_str()
        );

        DrawText(line.c_str(), 100, startY + (i * 42), 18, textColor);
    }

    DrawText("Bruk WASD/Piltaster for a navigere, [ENTER] for a kjoope", Settings::SCREEN_WIDTH / 2 - 270, 480, 18, LIGHTGRAY);
    DrawText("Trykk [ESC] eller [B] for a ga tilbake", Settings::SCREEN_WIDTH / 2 - 180, 510, 18, GRAY);
}