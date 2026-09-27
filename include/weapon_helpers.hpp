#ifndef WEAPON_HELPERS_HPP
#define WEAPON_HELPERS_HPP

// Felles hjelpefunksjoner for alle våpen (weapon.cpp og weapons_extra.cpp)

#include <raylib.h>
#include <raymath.h>
#include <algorithm>
#include <vector>
#include <memory>
#include "enemy.hpp"
#include "render3d.hpp"
#include "audio.hpp"
#include "vfx.hpp"

namespace weapon_detail {

// Dropp loot (XP/gull), tell drapet og fjern død fiende
inline void removeDeadEnemy(std::vector<std::unique_ptr<Enemy>>& enemies, size_t index, std::vector<Pickup>& pickups) {
    enemies[index]->dropLoot(pickups);
    VfxDeath(enemies[index]->position, Color{ 255, 210, 150, 255 });
    enemies[index]->onDeath();
    Enemy::killCount++;
    PlaySfx(Sfx::KILL);
    KeepCorpse(enemies[index]);
    enemies.erase(enemies.begin() + index);
}

// Gjør skade på alle fiender innenfor radius. Returnerer antall fiender som ble truffet.
inline int damageEnemiesInRadius(Vector2 center, float radius, int damage, Color color, bool isDamageOverTime,
                          std::vector<std::unique_ptr<Enemy>>& enemies, std::vector<Pickup>& pickups,
                          std::vector<int>* hitIds = nullptr) {
    int hits = 0;
    for (size_t j = 0; j < enemies.size(); ) {
        if (Vector2Distance(center, enemies[j]->position) <= radius) {
            enemies[j]->takeDamage(damage, color, isDamageOverTime);
            if (hitIds) hitIds->push_back(enemies[j]->id);
            hits++;
            if (enemies[j]->isDead()) {
                removeDeadEnemy(enemies, j, pickups);
                continue;
            }
        }
        j++;
    }
    return hits;
}

// De N nærmeste fiendene, sortert med nærmeste først
inline std::vector<Enemy*> nearestEnemies(Vector2 from, const std::vector<std::unique_ptr<Enemy>>& enemies, int count) {
    std::vector<Enemy*> sorted;
    for (const auto& e : enemies) sorted.push_back(e.get());

    int n = std::min(count, (int)sorted.size());
    std::partial_sort(sorted.begin(), sorted.begin() + n, sorted.end(), [&](Enemy* a, Enemy* b) {
        return Vector2DistanceSqr(from, a->position) < Vector2DistanceSqr(from, b->position);
    });
    sorted.resize(n);
    return sorted;
}

inline bool containsId(const std::vector<int>& ids, int id) {
    return std::find(ids.begin(), ids.end(), id) != ids.end();
}

inline Enemy* findEnemyById(const std::vector<std::unique_ptr<Enemy>>& enemies, int id) {
    for (const auto& e : enemies) {
        if (e->id == id) return e.get();
    }
    return nullptr;
}

// Nærmeste fiende innenfor range som ikke er i `exclude`
inline Enemy* nearestUnhitEnemy(Vector2 from, float range, const std::vector<int>& exclude, const std::vector<std::unique_ptr<Enemy>>& enemies) {
    Enemy* best = nullptr;
    float bestDist = range;
    for (const auto& e : enemies) {
        if (containsId(exclude, e->id)) continue;
        float d = Vector2Distance(from, e->position);
        if (d < bestDist) {
            bestDist = d;
            best = e.get();
        }
    }
    return best;
}

inline Vector2 rotateDegrees(Vector2 v, float degrees) {
    return Vector2Rotate(v, degrees * DEG2RAD);
}

constexpr float PROJECTILE_HIT_RADIUS = 5.0f;
constexpr float SLASH_ROTATION_OFFSET = 180.0f; // Snur slash-teksturen så buen følger bladets retning
constexpr float PROJECTILE_HEIGHT = 18.0f; // Hvor høyt over bakken prosjektiler flyr (3D)

// Liten skygge under noe som svever
inline void smallShadow(Vector2 pos, float size) {
    DrawEllipse((int)pos.x + 2, (int)pos.y + 2, size, size * 0.6f, Fade(BLACK, 0.3f));
}


} // namespace weapon_detail

#endif // WEAPON_HELPERS_HPP
