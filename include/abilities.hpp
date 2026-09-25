#ifndef ABILITIES_HPP
#define ABILITIES_HPP

#include <raylib.h>
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include "ability_types.hpp"
#include "weapon.hpp"

struct Player;

// En forhåndsbestemt oppgradering, f.eks. "+1 sprett" eller "2x skade"
struct AbilityLevel {
    std::string description;
    std::function<void(AbilityStats&)> apply;
};

struct AbilityDefinition {
    AbilityId id;
    std::string name;
    std::string description;
    Color color;
    AbilityStats baseStats;            // Stats på level 1
    std::vector<AbilityLevel> levels;  // levels[0] = level 2, ..., levels[7] = level 9
};

const AbilityDefinition& GetAbilityDefinition(AbilityId id);

// Lager en ny ability på level 1
std::unique_ptr<Weapon> CreateAbility(AbilityId id);

// Går opp ett level og påfører neste oppgradering fra tabellen
void LevelUpAbility(Weapon& ability);

// Abilities som kan dukke opp i level-up-menyen (alle unntatt karakterenes innate abilities)
std::vector<AbilityId> GetSharedAbilityPool();

// --- Level-up-meny ---
enum class ChoiceType {
    NEW_ABILITY,
    UPGRADE_ABILITY,
    HEAL // Reserve når alt er fullt og maks-level
};

struct AbilityChoice {
    ChoiceType type;
    AbilityId ability;
    std::string title;
    std::string description;
    Color color;
};

std::vector<AbilityChoice> GenerateLevelUpChoices(const Player& player, int count = 3);
void ApplyAbilityChoice(Player& player, const AbilityChoice& choice);

// --- HUD med de 5 ability-slotsene ---
void DrawAbilityHud(const Player& player, int screenWidth, int screenHeight);

#endif // ABILITIES_HPP
