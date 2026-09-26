#include "player.hpp"
#include "castle.hpp"
#include "render3d.hpp"
#include <cmath>
#include <algorithm>
#include <cstdlib>
#include <vector>
#include <memory>

void Player::update(float cameraRotation)
{
    float deltaTime = GetFrameTime();

    if (invulnerableTimer > 0.0f) invulnerableTimer -= deltaTime;
    if (slowTimer > 0.0f) slowTimer -= deltaTime;
    if (hpRegen > 0.0f && hp > 0.0f) hp = std::min(maxHp, hp + hpRegen * deltaTime);

    // 1. Les inn tastetrykk basert på SKJERM-retninger
    Vector2 screenInput = { 0.0f, 0.0f };
    if (IsKeyDown(KEY_W)) screenInput.y -= 1.0f; // Opp på skjermen
    if (IsKeyDown(KEY_S)) screenInput.y += 1.0f; // Ned på skjermen
    if (IsKeyDown(KEY_A)) screenInput.x -= 1.0f; // Venstre på skjermen
    if (IsKeyDown(KEY_D)) screenInput.x += 1.0f; // Høyre på skjermen

    // Normaliser input om man trykker f.eks. W og D samtidig
    float length = std::sqrt(screenInput.x * screenInput.x + screenInput.y * screenInput.y);
    if (length > 0.0f) {
        screenInput.x /= length;
        screenInput.y /= length;

        // Klovnen peker i den retningen du trykker på skjermen (0 deg = rett opp)
        facingRotation = std::atan2(screenInput.x, -screenInput.y) * RAD2DEG;
        if (screenInput.x < 0.0f) facingLeft = true;
        else if (screenInput.x > 0.0f) facingLeft = false;
    }

    // 2. Transformer skjerm-bevegelsen til verdens-bevegelse basert på KAMERAROTASJONEN.
    // Dette gjør at W ALLTID går rett opp på skjermen!
    float rad = cameraRotation * DEG2RAD;
    Vector2 worldMovement = {
        screenInput.x * cosf(rad) + screenInput.y * sinf(rad),
        -screenInput.x * sinf(rad) + screenInput.y * cosf(rad)
    };

    // 3. Oppdater posisjonen i verden
    float currentSpeed = speed * (slowTimer > 0.0f ? (1.0f - slowAmount) : 1.0f);
    position.x += worldMovement.x * currentSpeed * deltaTime;
    position.y += worldMovement.y * currentSpeed * deltaTime;
}

namespace {
    constexpr float PLAYER_SPRITE_HEIGHT = 56.0f;
}

void Player::drawShadow() const {
    DrawShadow({ position.x + 4.0f, position.y + 4.0f }, 16.0f, 9.0f);
}

void Player::drawSprite(const Camera3D& camera) const {
    // Blålig når man er slowet, blinker rødt mens man er udødelig etter et treff
    Color tint = (slowTimer > 0.0f) ? SKYBLUE : WHITE;
    if (invulnerableTimer > 0.0f && ((int)(invulnerableTimer * 20.0f) % 2 == 0)) tint = RED;

    DrawSpriteStanding(camera, texture, position, PLAYER_SPRITE_HEIGHT, facingLeft, tint);
}

// Beregn skade basert på Armor og Evasion
float Player::takeDamage(float rawDamage)
{
    // 1. Sjekk Evasion (f.eks. random tall mellom 0.0 og 1.0)
    float roll = static_cast<float>(rand()) / RAND_MAX;
    if (roll < evasion) {
        // Unngikk skade helt! (Dodge)
        return 0.0f;
    }

    // 2. Beregn skadereduksjon basert på Armor
    // Eksempel på avtagende skadereduksjons-formel: Damage = RawDamage * (100 / (100 + armor))
    float damageTaken = rawDamage * (100.0f / (100.0f + armor));

    hp -= damageTaken;
    if (hp < 0.0f) hp = 0.0f;

    return damageTaken;
}

CombatModifiers Player::combatModifiers() const {
    CombatModifiers mods;
    mods.extraProjectiles = projectileCount - 1;
    mods.damageMult = spellAmp * damageMult;
    mods.cooldownMult = cooldownMult;
    mods.areaMult = areaMult;
    return mods;
}

void Player::addWeapon(std::unique_ptr<Weapon> newWeapon) {
    if (newWeapon && (int)weapons.size() < MAX_ABILITY_SLOTS) {
        weapons.push_back(std::move(newWeapon));
    }
}

Weapon* Player::findAbility(AbilityId id) const {
    for (const auto& w : weapons) {
        if (w->id == id) return w.get();
    }
    return nullptr;
}

// Behandle oppsamling av XP
void Player::addXP(int amount)
{
    currentXp += amount;
    if (currentXp >= xpToNextLevel) {
        currentXp -= xpToNextLevel;
        level++;
        xpToNextLevel = static_cast<int>(xpToNextLevel * 1.25f); // 25% økning per level
        
        // Øk gjerne noen basestats ved Level Up
        maxHp += 10.0f;
        hp = maxHp; // Full heal på Level Up
    }
}