#ifndef SHOP_HPP
#define SHOP_HPP

#include <raylib.h>
#include <vector>
#include <string>
#include <functional>
#include "character.hpp"
#include "player.hpp"

struct ShopItem {
    std::string name;
    std::string description;
    int cost;
    int currentLevel;
    int maxLevel;
    std::function<void()> applyUpgrade; 
};

class Shop {
public:
    Shop();

    void handleInput(int& totalGold);
    void draw(int totalGold);

    // Kalles når spillet starter for å gange opp statsene til Player
    void applyToPlayer(const CharacterData& baseChar, Player& player) const;

    // Bonuser som shoppen øker
    float hpMult = 1.0f;
    float speedMult = 1.0f;
    float armorMult = 1.0f;
    int lootRadiusAdd = 0;
    float evasionAdd = 0.0f;
    int extraProjectile = 0;
    int aegisBonus = 0;

private:
    std::vector<ShopItem> items;
    int selectedOption;
    int scrollOffset;
};

#endif