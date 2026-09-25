#include "weapon.hpp"
#include <raymath.h>
#include <algorithm>
#include <cmath>

// --- Implementasjon av ProjectileWeapon ---

ProjectileWeapon::ProjectileWeapon(std::string weaponName, float rate, float speed, float dmg) {
    name = weaponName;
    level = 1;
    fireTimer = 0.0f;
    fireRate = rate;         // Dette er cooldownen mellom hvert skudd
    projectileSpeed = speed;
    damage = dmg;
}

void ProjectileWeapon::update(float deltaTime, Vector2 playerPos, std::vector<std::unique_ptr<Enemy>>& enemies, std::vector<XPorb>& xpOrbs, int projectileCount)
{
    fireTimer += deltaTime;

    // Sjekk om cooldownen (fireRate) er nådd, og om det finnes fiender
    if (fireTimer >= fireRate && !enemies.empty()) {
        struct EnemyDistance {
            Enemy* enemy;
            float distance;
        };

        std::vector<EnemyDistance> nearbyEnemies;
        for (const auto& enemy : enemies) {
            float dist = Vector2Distance(playerPos, enemy->position);
            nearbyEnemies.push_back({ enemy.get(), dist });
        }

        // Sorter fiendene slik at den nærmeste kommer først
        std::sort(nearbyEnemies.begin(), nearbyEnemies.end(), [](const auto& a, const auto& b) {
            return a.distance < b.distance;
        });

        // Skyt mot de N nærmeste fiendene basert på projectileCount
        int shotsToFire = std::min(projectileCount, (int)nearbyEnemies.size());

        for (int i = 0; i < shotsToFire; i++) {
            Enemy* targetEnemy = nearbyEnemies[i].enemy;
            
            Vector2 dir = Vector2Subtract(targetEnemy->position, playerPos);
            dir = Vector2Normalize(dir);

            projectiles.push_back({
                .position = playerPos,
                .direction = dir,
                .speed = projectileSpeed,
                .damage = damage,
                .lifetime = 2.0f
            });
        }

        fireTimer = 0.0f; // Nullstill cooldown-timeren
    }

    // Oppdater eksisterende prosjektiler
    for (size_t i = 0; i < projectiles.size(); ) {
        auto& p = projectiles[i];

        p.position.x += p.direction.x * p.speed * deltaTime;
        p.position.y += p.direction.y * p.speed * deltaTime;
        p.lifetime -= deltaTime;

        bool hit = false;

        for (size_t j = 0; j < enemies.size(); ) {
            if (CheckCollisionCircles(p.position, 5.0f, enemies[j]->position, 16.0f)) {
                enemies[j]->takeDamage(p.damage);
                hit = true;

                if (enemies[j]->isDead()) {
                    xpOrbs.push_back({
                        enemies[j]->position,  
                        enemies[j]->xpValue,   
                        enemies[j]->orbColor,  
                        enemies[j]->orbRadius, 
                        15.0f                  
                    });

                    enemies.erase(enemies.begin() + j);
                } else {
                    j++;
                }
                break;
            } else {
                j++;
            }
        }

        if (hit || p.lifetime <= 0.0f) {
            projectiles.erase(projectiles.begin() + i);
        } else {
            i++;
        }
    }
}

void ProjectileWeapon::draw() const {
    for (const auto& p : projectiles) {
        DrawCircleV(p.position, 4.0f, YELLOW);
    }
}

MeleeWeapon::MeleeWeapon(std::string weaponName, float rate, float hitRadius, float dmg) {
    name = weaponName;
    level = 1;
    fireTimer = 0.0f;
    fireRate = rate;        // Cooldown mellom svinger
    radius = hitRadius;     // Sverdets rekkevidde
    damage = dmg;
    lastPlayerPos = {0, 0};
}

void MeleeWeapon::update(float deltaTime, Vector2 playerPos, std::vector<std::unique_ptr<Enemy>>& enemies, std::vector<XPorb>& xpOrbs, int projectileCount)
{
    fireTimer += deltaTime;
    lastPlayerPos = playerPos; // Oppdaterer posisjonen hver frame slik at draw() vet hvor spilleren er

    // Sjekk om cooldown er nådd
    if (fireTimer >= fireRate && !enemies.empty()) {
        for (size_t j = 0; j < enemies.size(); ) {
            float dist = Vector2Distance(playerPos, enemies[j]->position);

            if (dist <= radius) {
                enemies[j]->takeDamage(damage);

                if (enemies[j]->isDead()) {
                    xpOrbs.push_back({
                        enemies[j]->position,  
                        enemies[j]->xpValue,   
                        enemies[j]->orbColor,  
                        enemies[j]->orbRadius, 
                        15.0f                  
                    });

                    enemies.erase(enemies.begin() + j);
                } else {
                    j++;
                }
            } else {
                j++;
            }
        }

        fireTimer = 0.0f; // Nullstill cooldown
    }
}

void MeleeWeapon::draw() const {
    // Tegner en rød sirkel rundt spilleren som viser sverdets rekkevidde
    DrawCircleLines(lastPlayerPos.x, lastPlayerPos.y, radius, RED);
}

// --- Felles hjelpefunksjon: dropp XP og fjern død fiende ---
static void removeDeadEnemy(std::vector<std::unique_ptr<Enemy>>& enemies, size_t index, std::vector<XPorb>& xpOrbs) {
    xpOrbs.push_back({
        enemies[index]->position,
        enemies[index]->xpValue,
        enemies[index]->orbColor,
        enemies[index]->orbRadius,
        15.0f
    });
    enemies.erase(enemies.begin() + index);
}

// --- Implementasjon av RicochetWeapon ---

RicochetWeapon::RicochetWeapon(std::string weaponName, float rate, float speed, float dmg, int bounces, float range) {
    name = weaponName;
    level = 1;
    fireTimer = 0.0f;
    fireRate = rate;
    projectileSpeed = speed;
    damage = dmg;
    maxBounces = bounces;
    bounceRange = range;
    damageFalloff = 0.7f;
    rangeFalloff = 0.8f;
}

void RicochetWeapon::update(float deltaTime, Vector2 playerPos, std::vector<std::unique_ptr<Enemy>>& enemies, std::vector<XPorb>& xpOrbs, int projectileCount)
{
    fireTimer += deltaTime;

    // Skyt mot de N nærmeste fiendene
    if (fireTimer >= fireRate && !enemies.empty()) {
        std::vector<Enemy*> sorted;
        for (const auto& e : enemies) sorted.push_back(e.get());
        std::sort(sorted.begin(), sorted.end(), [&](Enemy* a, Enemy* b) {
            return Vector2Distance(playerPos, a->position) < Vector2Distance(playerPos, b->position);
        });

        int shotsToFire = std::min(projectileCount, (int)sorted.size());
        for (int i = 0; i < shotsToFire; i++) {
            projectiles.push_back({
                .position = playerPos,
                .direction = Vector2Normalize(Vector2Subtract(sorted[i]->position, playerPos)),
                .speed = projectileSpeed,
                .damage = (float)damage,
                .lifetime = 2.0f,
                .bouncesLeft = maxBounces,
                .bounceRange = bounceRange,
                .hitEnemyIds = {}
            });
        }
        fireTimer = 0.0f;
    }

    for (size_t i = 0; i < projectiles.size(); ) {
        auto& p = projectiles[i];

        p.position.x += p.direction.x * p.speed * deltaTime;
        p.position.y += p.direction.y * p.speed * deltaTime;
        p.lifetime -= deltaTime;

        bool destroyed = false;

        for (size_t j = 0; j < enemies.size(); j++) {
            Enemy* enemy = enemies[j].get();
            bool alreadyHit = std::find(p.hitEnemyIds.begin(), p.hitEnemyIds.end(), enemy->id) != p.hitEnemyIds.end();
            if (alreadyHit || !CheckCollisionCircles(p.position, 5.0f, enemy->position, 16.0f)) continue;

            // Første treff er gult, sprett blir mer og mer oransje
            Color numberColor = p.hitEnemyIds.empty() ? YELLOW : ORANGE;
            enemy->takeDamage((int)p.damage, numberColor);
            p.hitEnemyIds.push_back(enemy->id);
            Vector2 hitPos = enemy->position;

            if (enemy->isDead()) {
                removeDeadEnemy(enemies, j, xpOrbs);
            }

            // Finn neste mål: nærmeste fiende innenfor rekkevidde som ikke er truffet
            Enemy* nextTarget = nullptr;
            if (p.bouncesLeft > 0) {
                float bestDist = p.bounceRange;
                for (const auto& other : enemies) {
                    if (std::find(p.hitEnemyIds.begin(), p.hitEnemyIds.end(), other->id) != p.hitEnemyIds.end()) continue;
                    float d = Vector2Distance(hitPos, other->position);
                    if (d < bestDist) {
                        bestDist = d;
                        nextTarget = other.get();
                    }
                }
            }

            if (nextTarget) {
                // Sprett videre – svakere og med kortere rekkevidde for hvert sprett
                p.bouncesLeft--;
                p.damage *= damageFalloff;
                p.bounceRange *= rangeFalloff;
                p.position = hitPos;
                p.direction = Vector2Normalize(Vector2Subtract(nextTarget->position, hitPos));
                p.lifetime = 1.0f;
                if ((int)p.damage <= 0) destroyed = true;
            } else {
                destroyed = true;
            }
            break;
        }

        if (destroyed || p.lifetime <= 0.0f) {
            projectiles.erase(projectiles.begin() + i);
        } else {
            i++;
        }
    }
}

void RicochetWeapon::draw() const {
    for (const auto& p : projectiles) {
        // Mindre og mørkere jo flere ganger den har sprettet
        int bounced = (int)p.hitEnemyIds.size();
        float size = std::max(2.0f, 5.0f - bounced * 0.7f);
        float t = std::min(1.0f, bounced / 4.0f);
        Color c = {
            (unsigned char)(SKYBLUE.r + (ORANGE.r - SKYBLUE.r) * t),
            (unsigned char)(SKYBLUE.g + (ORANGE.g - SKYBLUE.g) * t),
            (unsigned char)(SKYBLUE.b + (ORANGE.b - SKYBLUE.b) * t),
            255
        };
        DrawCircleV(p.position, size, c);
        DrawLineV(p.position, Vector2Subtract(p.position, Vector2Scale(p.direction, 10.0f)), Fade(c, 0.5f));
    }
}

void RicochetWeapon::upgrade() {
    level++;
    damage += 8;
    maxBounces += 1;       // +1 sprett per level
    bounceRange += 25.0f;
    fireRate *= 0.95f;
}

// --- Implementasjon av RotWeapon ---

RotWeapon::RotWeapon(std::string weaponName, float dps, float auraRadius) {
    name = weaponName;
    level = 1;
    damagePerSecond = dps;
    radius = auraRadius;
    damage = (int)dps;
}

void RotWeapon::update(float deltaTime, Vector2 playerPos, std::vector<std::unique_ptr<Enemy>>& enemies, std::vector<XPorb>& xpOrbs, int projectileCount)
{
    lastPlayerPos = playerPos;
    pulseTimer += deltaTime;

    // Samle opp skade hver frame. Når vi har minst 1 hel skade, del den ut til alle i radius.
    // Ved 60 FPS og f.eks. 60 DPS blir dette 1 skade per fiende per frame.
    damageAccumulator += damagePerSecond * deltaTime;
    int tickDamage = (int)damageAccumulator;
    if (tickDamage <= 0) return;
    damageAccumulator -= tickDamage;

    for (size_t j = 0; j < enemies.size(); ) {
        if (Vector2Distance(playerPos, enemies[j]->position) <= radius) {
            enemies[j]->takeDamage(tickDamage, LIME, true);
            if (enemies[j]->isDead()) {
                removeDeadEnemy(enemies, j, xpOrbs);
                continue;
            }
        }
        j++;
    }
}

void RotWeapon::draw() const {
    // Pulserende grønn giftsky rundt spilleren
    float pulse = 0.5f + 0.5f * sinf(pulseTimer * 4.0f);
    DrawCircleV(lastPlayerPos, radius, Fade(DARKGREEN, 0.15f + 0.08f * pulse));
    DrawCircleLines((int)lastPlayerPos.x, (int)lastPlayerPos.y, radius - 2.0f * pulse, Fade(LIME, 0.6f));
}

void RotWeapon::upgrade() {
    level++;
    damagePerSecond += 20.0f;
    radius += 15.0f;
    damage = (int)damagePerSecond;
}
