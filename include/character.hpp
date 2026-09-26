#ifndef CHARACTER_HPP
#define CHARACTER_HPP

#include <string>
#include <vector>
#include "ability_types.hpp"

// Hvilken 3D-klovn karakteren tegnes som (se clowns.cpp)
enum class ClownStyle {
    JESTER, // Helt vanlig sirkusklovn
    WESTER, // Feit, Wario-aktig klovn
    GEEK    // Lang og tynn nerde-klovn med briller
};

struct CharacterData {
    std::string name;
    std::string description;
    std::string texturePath;

    // Unike basestats for karakterene
    float speed;
    float maxHp;
    float spellAmp;
    float armor;
    float lootRadius;
    float size;
    float xpMultiplier; // Unik trait (1.0f er normal, 1.5f er +50% XP osv.)

    // Unik ability som bare denne karakteren kan ha. Den tar alltid slot 1 av 5,
    // og kan aldri dukke opp som valg for de andre karakterene.
    // Stats og level-oppgraderinger for abilityen ligger i abilities.cpp.
    AbilityId innateAbility;

    ClownStyle clown; // Hvilken 3D-modell som brukes
};
// Enkel hjelpefunksjon som returnerer alle karakterene
inline std::vector<CharacterData> GetAvailableCharacters() {
    return {
        // --- JESTER ---
        CharacterData{
            .name         = "Jester",
            .description  = "Balansert og smidig",
            .texturePath  = "assets/jester_real.png",
            .speed        = 210.0f,
            .maxHp        = 100.0f,
            .spellAmp     = 1.0f,
            .armor        = 2.0f,
            .lootRadius   = 120.0f,
            .size         = 1.0f,
            .xpMultiplier = 1.0f,
            .innateAbility = AbilityId::TREFORK,
            .clown = ClownStyle::JESTER,
        },

        // --- WESTER ---
        CharacterData{
            .name         = "Wester",
            .description  = "AOE damage",
            .texturePath  = "assets/jester_real.png",
            .speed        = 150.0f,
            .maxHp        = 160.0f,
            .spellAmp     = 0.9f,
            .armor        = 8.0f,
            .lootRadius   = 100.0f,
            .size         = 2.3f, // Feitere/større
            .xpMultiplier = 1.0f,
            .innateAbility = AbilityId::GROUND_SLAM,
            .clown = ClownStyle::WESTER,
        },

        // --- TOK GEEK ---
        CharacterData{
            .name         = "tok geek",
            .description  = "Høyere XP rate",
            .texturePath  = "assets/jester_real.png",
            .speed        = 260.0f,
            .maxHp        = 70.0f,
            .spellAmp     = 1.4f,
            .armor        = 0.0f,
            .lootRadius   = 150.0f,
            .size         = 0.9f,
            .xpMultiplier = 1.5f,  // Høyere XP-rate som sin unike trait
            .innateAbility = AbilityId::RICOCHET,
            .clown = ClownStyle::GEEK,
        }
    };
}

#endif // CHARACTER_HPP