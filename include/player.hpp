#ifndef PLAYER_HPP
#define PLAYER_HPP

#include <raylib.h>
#include <vector>
#include <memory>
#include "enemy.hpp"
#include "weapon.hpp"
#include "character.hpp"

struct Player {

    Vector2 position;
    Texture2D texture{};

    float hp = 100.0f;
    float maxHp = 101.0f;
    float armor = 10.0f;
    float evasion = 0.05f;

    float speed = 10.0f;

    float lootRadius = 100.0f;
    float xpMultiplier = 1.0f;
    float goldMultiplier = 1.0f;

    float spellAmp = 1.0f;
    float cooldownReduction = 0.0f;

    // Bonuser fra shoppen (metaprogresjon)
    float damageMult = 1.0f;   // "Might"
    float cooldownMult = 1.0f; // "Haste"
    float areaMult = 1.0f;     // "Area"
    float hpRegen = 0.0f;      // HP per sekund
    int levelUpChoices = 3;    // Antall valg i level-up-menyen

    float invulnerableTimer = 0.0f; // Kort udødelighet etter å ha tatt skade

    // Slow fra fiender (echelon 3+)
    float slowTimer = 0.0f;
    float slowAmount = 0.0f; // 0.4 = 40% tregere

    int projectileCount = 1;
    int aegis = 0;
    float critChance = 0.05f; // Sjanse for kritisk treff (dobbel skade)

    // Stat-oppgraderinger tatt i level-up denne runden (se StatBoost)
    int statBoosts[(int)StatBoost::COUNT] = {};


    int level = 1;
    int currentXp = 0;
    int xpToNextLevel = 100;

    // Abilities: weapons[0] er alltid karakterens innate ability, resten låses opp via level up
    std::vector<std::unique_ptr<Weapon>> weapons;
    AbilityId innateAbility = AbilityId::TREFORK;

    float facingRotation = 0.0f;

    void update(float cameraRotation = 0.0f);
    // 3D-klovnen og gangeanimasjonen
    ClownStyle clown = ClownStyle::JESTER;
    Vector2 facingDir = { 0.0f, 1.0f }; // Retningen klovnen ser på gulvet (starter mot kameraet)
    float walkTime = 0.0f;
    bool isMoving = false;

    void drawShadow() const; // I gulvlaget
    void drawModel() const;  // I 3D-laget
    float takeDamage(float rawDamage); // Returnerer faktisk skade (0 hvis dodge)
    float armorReduction() const;      // Andel skade armor tar bort (0.0 - 0.75)
    CombatModifiers combatModifiers() const;
    void addXP(int amount);

    void addWeapon(std::unique_ptr<Weapon> newWeapon);
    Weapon* findAbility(AbilityId id) const;
};

#endif // PLAYER_HPP