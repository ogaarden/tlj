#ifndef ABILITY_TYPES_HPP
#define ABILITY_TYPES_HPP

// Grunnleggende typer for abilities. Ligger i egen fil slik at både
// character.hpp, weapon.hpp og abilities.hpp kan bruke dem uten sirkulære includes.

constexpr int MAX_ABILITY_LEVEL = 9;
constexpr int MAX_ABILITY_SLOTS = 5; // 1 innate + 4 som låses opp via level up

enum class AbilityId {
    // Innate (unike for hver karakter)
    TREFORK,
    GROUND_SLAM,
    RICOCHET,
    PIE,        // Pierrot: kremkaker som lobbes og spruter

    // Felles pool
    MAGIC_MISSILE,
    ROT,
    DAGGER,
    ORBIT_BLADES,
    LIGHTNING,

    COUNT
};

// Items: passive gjenstander man plukker i level-up. Nullstilles hver runde.
// Hver har 5 nivåer, og man kan bare bære MAX_ITEM_SLOTS forskjellige – så man må velge.
// Flere items er nøkkelen til en evolusjon (se abilities.cpp).
enum class ItemId {
    JUGGLING_BALL,  // Sjonglørball: +1 prosjektil
    ROYAL_CAPE,     // Kongens kappe: +område
    JESTER_SHOES,   // Narreskoene: +fart
    WHETSTONE,      // Slipestein: +skade
    HOURGLASS,      // Timeglass: -cooldown
    HEART_AMULET,   // Hjerteamulett: +maks HP og regen
    CHAINMAIL,      // Ringbrynje: +armor
    LODESTONE,      // Magnetstein: +pickup-radius
    OWL_FEATHER,    // Uglefjær: +XP
    LUCKY_DIE,      // Heldig terning: +kritisk treff
    COUNT
};
constexpr int MAX_ITEM_LEVEL = 5;
constexpr int MAX_ITEM_SLOTS = 6;

// Alle tall en ability kan ha. Hver ability bruker bare de feltene som er relevante for den.
// Level-tabellene i abilities.cpp endrer disse verdiene.
struct AbilityStats {
    float damage = 0.0f;        // Skade per treff (Rot: skade per sekund)
    float cooldown = 1.0f;      // Sekunder mellom hvert angrep (Orbit: tid før samme fiende kan treffes igjen)
    float speed = 0.0f;         // Prosjektilfart (Orbit: grader per sekund)
    int projectiles = 1;        // Antall prosjektiler / blader / lyn
    int pierce = 0;             // Hvor mange fiender et prosjektil går gjennom
    int bounces = 0;            // Antall sprett
    float bounceRange = 0.0f;   // Hvor langt et sprett kan gå
    float bounceFalloff = 1.0f; // Skade-multiplikator per sprett (0.7 = -30% per sprett)
    float radius = 0.0f;        // AOE-radius / orbit-radius / rekkevidde
    float area = 0.0f;          // Treffområde for f.eks. lyn
};

#endif // ABILITY_TYPES_HPP
