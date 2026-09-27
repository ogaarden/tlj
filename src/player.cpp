#include "player.hpp"
#include "gametime.hpp"
#include "castle.hpp"
#include "render3d.hpp"
#include "clowns.hpp"
#include <cmath>
#include <rlgl.h>
#include <algorithm>
#include <cstdlib>
#include <vector>
#include <memory>

void Player::update(float cameraRotation)
{
    float deltaTime = GameDt();

    if (invulnerableTimer > 0.0f) invulnerableTimer -= deltaTime;
    if (dashCooldown > -1.0f) dashCooldown -= deltaTime; // Går litt under 0, så klar-blinket vises kort
    justDashed = false;
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
    }

    // 2. Transformer skjerm-bevegelsen til verdens-bevegelse basert på KAMERAROTASJONEN.
    // Dette gjør at W ALLTID går rett opp på skjermen!
    float rad = cameraRotation * DEG2RAD;
    Vector2 worldMovement = {
        screenInput.x * cosf(rad) + screenInput.y * sinf(rad),
        -screenInput.x * sinf(rad) + screenInput.y * cosf(rad)
    };

    // Gangeanimasjon: klovnen snur seg mot der den går
    isMoving = length > 0.0f;
    if (isMoving) {
        walkTime += deltaTime;
        facingDir = worldMovement;
    }

    // Unnvikelsesrull: bykser i gangretningen (eller dit klovnen ser) og er udødelig imens
    if (dashTimer <= 0.0f && dashCooldown <= 0.0f && (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_LEFT_SHIFT))) {
        dashDir = isMoving ? worldMovement : facingDir;
        if (Vector2Length(dashDir) < 0.01f) dashDir = { 0.0f, 1.0f };
        dashDir = Vector2Normalize(dashDir);
        dashTimer = DASH_TIME;
        dashCooldown = DASH_COOLDOWN;
        justDashed = true;
    }
    if (dashTimer > 0.0f) {
        dashTimer -= deltaTime;
        invulnerableTimer = std::max(invulnerableTimer, 0.08f);
        facingDir = dashDir;
        walkTime += deltaTime * 2.0f;
        position.x += dashDir.x * speed * DASH_SPEED_MULT * deltaTime;
        position.y += dashDir.y * speed * DASH_SPEED_MULT * deltaTime;
        return;
    }

    // 3. Oppdater posisjonen i verden
    float currentSpeed = speed * (slowTimer > 0.0f ? (1.0f - slowAmount) : 1.0f);
    position.x += worldMovement.x * currentSpeed * deltaTime;
    position.y += worldMovement.y * currentSpeed * deltaTime;
}

void Player::drawShadow() const {
    float w = ClownShadowWidth(clown);
    DrawShadow({ position.x + 4.0f, position.y + 4.0f }, w, w * 0.6f);
}

void Player::drawModel() const {
    // Blålig når man er slowet, blinker rødt mens man er udødelig etter et treff
    Color tint = (slowTimer > 0.0f) ? SKYBLUE : WHITE;
    if (dashTimer <= 0.0f && invulnerableTimer > 0.0f && ((int)(invulnerableTimer * 20.0f) % 2 == 0)) tint = RED;

    ClownPose pose;
    pose.position = position;
    pose.facing = facingDir;
    pose.walkTime = walkTime;
    pose.moving = isMoving;
    pose.tint = tint;
    // Angreps-animasjon: den første tredjedelen av cooldownen etter at standardvåpenet ble brukt
    if (!weapons.empty()) {
        float p = weapons[0]->cooldownProgress();
        pose.attack = p < 0.3f ? 1.0f - p / 0.3f : 0.0f;
    }
    if (dashTimer > 0.0f) {
        // Rullen: klovnen tar en hel kolbøtte fremover rundt midten av kroppen
        float angle = (1.0f - dashTimer / DASH_TIME) * 360.0f;
        const float mid = 22.0f;
        rlPushMatrix();
            rlTranslatef(position.x, mid, position.y);
            rlRotatef(angle, dashDir.y, 0.0f, -dashDir.x);
            rlTranslatef(-position.x, -mid, -position.y);
            DrawClown(clown, pose);
        rlPopMatrix();
        return;
    }
    DrawClown(clown, pose);
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

    // 2. Skadereduksjon fra armor (se armorReduction)
    float damageTaken = rawDamage * (1.0f - armorReduction());

    hp -= damageTaken;
    if (hp < 0.0f) hp = 0.0f;

    return damageTaken;
}

// Armor med avtagende effekt: armor / (armor + 30).
// 8 armor = 21 %, 23 armor = 43 %, 40 armor = 57 %. Aldri mer enn 75 %.
float Player::armorReduction() const {
    if (armor <= 0.0f) return 0.0f;
    return std::min(0.75f, armor / (armor + 30.0f));
}

CombatModifiers Player::combatModifiers() const {
    CombatModifiers mods;
    mods.extraProjectiles = projectileCount - 1;
    mods.damageMult = spellAmp * damageMult;
    mods.cooldownMult = fmaxf(0.65f, cooldownMult); // Maks -35 % fra items og shop, så abilities aldri blir maskingevær
    mods.areaMult = areaMult;
    mods.speedMult = projectileSpeedMult;
    mods.durationMult = durationMult;
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
// XP-kurve: 35 + 35L + 4L^2. Level 1->2 = 74 XP, 10->11 = 785, 20->21 = 2335.
// Med fiendemengden i spawner.cpp gir det første level-up innen 30 sek,
// ca. level 14 etter 5 min og ca. level 25 etter 10 min.
// Et fullt build (5 abilities på level 9 + 6 items på nivå 5) krever over 70 level-ups,
// så man må velge hva man satser på.
int Player::xpForLevel(int lvl) {
    // Mye XP per level (ca. +40 % mot før): rundt level 18-20 ved 10 min med et vanlig build
    return (int)(60 + 65 * lvl + 8.5f * lvl * lvl + 0.17f * lvl * lvl * lvl); // ca. +40 % mot før
}

void Player::addXP(int amount)
{
    currentXp += amount;
    // while: nok XP på én gang kan gi flere level (hver gir sitt eget valg, se tlj.cpp)
    while (currentXp >= xpToNextLevel) {
        currentXp -= xpToNextLevel;
        level++;
        xpToNextLevel = xpForLevel(level);

        // Litt mer liv og en liten helbredelse (ikke full – det gjorde spillet for lett)
        maxHp += 5.0f;
        hp = std::min(maxHp, hp + maxHp * 0.12f);
    }
}