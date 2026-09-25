#ifndef PLAYER_HPP
#define PLAYER_HPP

#include <raylib.h>
#include <vector>
#include <memory>
#include "enemy.hpp"
#include "weapon.hpp"

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

    int projectileCount = 1;
    int aegis = 0;


    int level = 1;
    int currentXp = 0;
    int xpToNextLevel = 100;

    // Abilities: weapons[0] er alltid karakterens innate ability, resten låses opp via level up
    std::vector<std::unique_ptr<Weapon>> weapons;
    AbilityId innateAbility = AbilityId::TREFORK;

    float facingRotation = 0.0f;

    void update(float cameraRotation = 0.0f);
    void draw(float rotation = 0.0f);
    bool takeDamage(float rawDamage);
    void addXP(int amount);

    void addWeapon(std::unique_ptr<Weapon> newWeapon);
    Weapon* findAbility(AbilityId id) const;
};

#endif // PLAYER_HPP