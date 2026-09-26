#ifndef ABILITIES_HPP
#define ABILITIES_HPP

#include <raylib.h>
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include "ability_types.hpp"
#include "weapon.hpp"
#include "items.hpp"

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
    ITEM,       // Nytt item eller neste nivå av et item (bare fra skattekister)
    COMBINE,    // Fra skattekiste: to items på maks nivå smeltes sammen til ett
    SCEPTER,    // Fra Kongens septer (miniboss): gir en ability sin septer-oppgradering
    HEAL        // Reserve når alt er fullt og maks-level
};

struct AbilityChoice {
    ChoiceType type;
    AbilityId ability;
    std::string title;
    std::string description;
    Color color;
    ItemId item = ItemId::COUNT;    // Bare for ChoiceType::ITEM
    ComboId combo = ComboId::COUNT; // Bare for ChoiceType::COMBINE
};

// --- Septer-oppgraderinger (Kongens septer fra minibossene) ---
struct ScepterUpgrade {
    AbilityId ability;
    const char* name;
    const char* description;
    Color color;
};
const ScepterUpgrade* GetScepterUpgrade(AbilityId ability); // nullptr hvis abilityen ikke har en
void ApplyScepter(Weapon& weapon);

// Level-up: bare abilities (nye og oppgraderinger)
std::vector<AbilityChoice> GenerateLevelUpChoices(const Player& player, int count = 3);
// Skattekiste: item-kombinasjon hvis mulig, ellers items
std::vector<AbilityChoice> GenerateChestChoices(const Player& player, int count = 3);
// Kongens septer: velg hvilken ability som får septer-oppgraderingen
std::vector<AbilityChoice> GenerateScepterChoices(const Player& player);
void ApplyAbilityChoice(Player& player, const AbilityChoice& choice);

// --- HUD med de 5 ability-slotsene (i et panel, sentrert på centerX, med underkant på bottom) ---
void DrawAbilityHud(const Player& player, float centerX, float bottom, float scale);

#endif // ABILITIES_HPP
