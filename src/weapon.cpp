#include "weapon.hpp"
#include <raymath.h>
#include <algorithm>

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