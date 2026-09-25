#include "player.hpp"
#include <cmath>
#include <cstdlib>
#include <vector>
#include <memory>

void Player::update(float cameraRotation)
{
    float deltaTime = GetFrameTime();

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
    }

    // 2. Transformer skjerm-bevegelsen til verdens-bevegelse basert på KAMERAROTASJONEN.
    // Dette gjør at W ALLTID går rett opp på skjermen!
    float rad = cameraRotation * DEG2RAD;
    Vector2 worldMovement = {
        screenInput.x * cosf(rad) + screenInput.y * sinf(rad),
        -screenInput.x * sinf(rad) + screenInput.y * cosf(rad)
    };

    // 3. Oppdater posisjonen i verden
    position.x += worldMovement.x * speed * deltaTime;
    position.y += worldMovement.y * speed * deltaTime;
}

void Player::draw(float cameraRotation)
{
    // Legg til cameraRotation her slik at figuren roterer i takt med skjermen/kameraet
    float finalDrawAngle = facingRotation + cameraRotation;

    Rectangle source = { 0.0f, 0.0f, (float)texture.width, (float)texture.height };
    Rectangle dest = { position.x, position.y, (float)texture.width, (float)texture.height };
    Vector2 origin = { (float)texture.width / 2.0f, (float)texture.height / 2.0f };

    DrawTexturePro(texture, source, dest, origin, -finalDrawAngle, WHITE);
}

// Beregn skade basert på Armor og Evasion
bool Player::takeDamage(float rawDamage)
{
    // 1. Sjekk Evasion (f.eks. random tall mellom 0.0 og 1.0)
    float roll = static_cast<float>(rand()) / RAND_MAX;
    if (roll < evasion) {
        // Unngikk skade helt! (Dodge)
        return false;
    }

    // 2. Beregn skadereduksjon basert på Armor
    // Eksempel på avtagende skadereduksjons-formel: Damage = RawDamage * (100 / (100 + armor))
    float damageTaken = rawDamage * (100.0f / (100.0f + armor));

    hp -= damageTaken;
    if (hp < 0.0f) hp = 0.0f;

    return true; // Tok skade
}

void Player::addWeapon(std::unique_ptr<Weapon> newWeapon) {
    if (weapons.size() < maxWeapons) {
        weapons.push_back(std::move(newWeapon));
    }
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