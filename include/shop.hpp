#ifndef SHOP_HPP
#define SHOP_HPP

#include <raylib.h>
#include <vector>
#include <string>
#include <ostream>
#include "character.hpp"
#include "player.hpp"

// Alle permanente oppgraderinger. Rekkefølgen her er rekkefølgen i shop-menyen.
enum class ShopUpgrade {
    VITALITY,
    ARMOR,
    REGEN,
    EVASION,
    SPEED,
    MIGHT,
    HASTE,
    AREA,
    PROJECTILE,
    MAGNET,
    GROWTH,
    GREED,
    EXTRA_CHOICE,
    AEGIS,
    COUNT
};

struct ShopItem {
    ShopUpgrade upgrade;
    std::string saveKey;     // Navnet som brukes i lagringsfila
    std::string name;
    std::string description; // Hva ETT nivå gir
    int baseCost;
    int maxLevel;
    int currentLevel = 0;

    int cost() const; // Prisen for neste nivå
    bool isMaxed() const { return currentLevel >= maxLevel; }
};

class Shop {
public:
    Shop();

    void handleInput(int& totalGold);
    void draw(int totalGold);

    // Kalles når spillet starter: basestats fra karakteren + alle shop-bonuser
    void applyToPlayer(const CharacterData& baseChar, Player& player) const;

    int level(ShopUpgrade upgrade) const;

    // Bonuser regnet ut fra kjøpte nivåer (brukes også i karaktervalg-skjermen)
    float hpMult() const;
    float speedMult() const;
    float armorBonus() const;
    int aegisBonus() const;

    // Lagring av kjøpte nivåer (brukes av save.cpp)
    void writeLevels(std::ostream& out) const;
    bool readLevel(const std::string& key, int value);

    // Totalpris for å kjøpe ALT (nyttig for balansering)
    int totalCostOfEverything() const;

private:
    static constexpr int GRID_COLUMNS = 4;

    std::vector<ShopItem> items;
    int selectedOption = 0;
    double purchaseTime = -10.0; // Når siste kjøp skjedde (for animasjon)
    double denyTime = -10.0;     // Når man sist prøvde å kjøpe uten nok gull
};

#endif
