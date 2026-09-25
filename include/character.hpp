#ifndef CHARACTER_HPP
#define CHARACTER_HPP

#include <string>
#include <vector>

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

    std::string weaponName;
    float cooldown;
    float weaponSpeed;
    float weaponDamage;
};
//Jester går fra pinne til trefork
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

            .weaponName = "Trefork",
            .cooldown = 1.0f, //i sekunder
            .weaponSpeed = 500.0f,
            .weaponDamage = 200.0f,
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

            .weaponName = "Trefork",
            .cooldown = 2.0f, //i sekunder
            .weaponSpeed = 500.0f,
            .weaponDamage = 200.0f,
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
            
            .weaponName = "Trefork",
            .cooldown = 1.0f, //i sekunder
            .weaponSpeed = 500.0f,
            .weaponDamage = 200.0f,
        }
    };
}

#endif // CHARACTER_HPP