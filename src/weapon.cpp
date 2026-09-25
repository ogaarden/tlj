#include "weapon.hpp"
#include <raymath.h>
#include <algorithm>
#include <cmath>

// --- Felles hjelpefunksjoner ---
namespace {

// Dropp loot (XP/gull), tell drapet og fjern død fiende
void removeDeadEnemy(std::vector<std::unique_ptr<Enemy>>& enemies, size_t index, std::vector<Pickup>& pickups) {
    enemies[index]->dropLoot(pickups);
    Enemy::killCount++;
    enemies.erase(enemies.begin() + index);
}

// Gjør skade på alle fiender innenfor radius. Returnerer antall fiender som ble truffet.
int damageEnemiesInRadius(Vector2 center, float radius, int damage, Color color, bool isDamageOverTime,
                          std::vector<std::unique_ptr<Enemy>>& enemies, std::vector<Pickup>& pickups) {
    int hits = 0;
    for (size_t j = 0; j < enemies.size(); ) {
        if (Vector2Distance(center, enemies[j]->position) <= radius) {
            enemies[j]->takeDamage(damage, color, isDamageOverTime);
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
std::vector<Enemy*> nearestEnemies(Vector2 from, const std::vector<std::unique_ptr<Enemy>>& enemies, int count) {
    std::vector<Enemy*> sorted;
    for (const auto& e : enemies) sorted.push_back(e.get());

    int n = std::min(count, (int)sorted.size());
    std::partial_sort(sorted.begin(), sorted.begin() + n, sorted.end(), [&](Enemy* a, Enemy* b) {
        return Vector2DistanceSqr(from, a->position) < Vector2DistanceSqr(from, b->position);
    });
    sorted.resize(n);
    return sorted;
}

bool containsId(const std::vector<int>& ids, int id) {
    return std::find(ids.begin(), ids.end(), id) != ids.end();
}

Enemy* findEnemyById(const std::vector<std::unique_ptr<Enemy>>& enemies, int id) {
    for (const auto& e : enemies) {
        if (e->id == id) return e.get();
    }
    return nullptr;
}

// Nærmeste fiende innenfor range som ikke er i `exclude`
Enemy* nearestUnhitEnemy(Vector2 from, float range, const std::vector<int>& exclude, const std::vector<std::unique_ptr<Enemy>>& enemies) {
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

Vector2 rotateDegrees(Vector2 v, float degrees) {
    return Vector2Rotate(v, degrees * DEG2RAD);
}

constexpr float PROJECTILE_HIT_RADIUS = 5.0f;
constexpr float ENEMY_HIT_RADIUS = 16.0f;

} // namespace

// =====================================================================
// ProjectileWeapon (Trefork, Dagger)
// =====================================================================

ProjectileWeapon::ProjectileWeapon(bool fireInSpread) : spread(fireInSpread) {}

void ProjectileWeapon::tick(float deltaTime, Vector2 playerPos, std::vector<std::unique_ptr<Enemy>>& enemies, std::vector<Pickup>& pickups)
{
    fireTimer += deltaTime;

    if (fireTimer >= cooldown() && !enemies.empty()) {
        int count = stats.projectiles + mods.extraProjectiles;
        int dmg = scaledDamage();

        auto fire = [&](Vector2 dir) {
            projectiles.push_back({
                .position = playerPos,
                .direction = dir,
                .speed = stats.speed,
                .damage = dmg,
                .lifetime = 2.0f,
                .pierceLeft = stats.pierce,
                .hitEnemyIds = {}
            });
        };

        if (spread) {
            // Vifte med prosjektiler sentrert mot nærmeste fiende
            Enemy* target = nearestEnemies(playerPos, enemies, 1)[0];
            Vector2 baseDir = Vector2Normalize(Vector2Subtract(target->position, playerPos));
            float spreadStep = 12.0f;
            for (int i = 0; i < count; i++) {
                float offset = (i - (count - 1) / 2.0f) * spreadStep;
                fire(rotateDegrees(baseDir, offset));
            }
        } else {
            // Én kule mot hver av de N nærmeste fiendene
            std::vector<Enemy*> targets = nearestEnemies(playerPos, enemies, count);
            for (int i = 0; i < count; i++) {
                Enemy* target = targets[i % targets.size()];
                fire(Vector2Normalize(Vector2Subtract(target->position, playerPos)));
            }
        }

        fireTimer = 0.0f;
    }

    // Oppdater eksisterende prosjektiler
    for (size_t i = 0; i < projectiles.size(); ) {
        auto& p = projectiles[i];

        p.position = Vector2Add(p.position, Vector2Scale(p.direction, p.speed * deltaTime));
        p.lifetime -= deltaTime;

        bool destroyed = false;
        for (size_t j = 0; j < enemies.size() && !destroyed; ) {
            Enemy* enemy = enemies[j].get();
            if (containsId(p.hitEnemyIds, enemy->id) ||
                !CheckCollisionCircles(p.position, PROJECTILE_HIT_RADIUS, enemy->position, ENEMY_HIT_RADIUS)) {
                j++;
                continue;
            }

            enemy->takeDamage(p.damage, color);
            p.hitEnemyIds.push_back(enemy->id);

            if (enemy->isDead()) removeDeadEnemy(enemies, j, pickups);
            else j++;

            if (p.pierceLeft <= 0) destroyed = true;
            else p.pierceLeft--;
        }

        if (destroyed || p.lifetime <= 0.0f) {
            projectiles.erase(projectiles.begin() + i);
        } else {
            i++;
        }
    }
}

void ProjectileWeapon::draw() const {
    for (const auto& p : projectiles) {
        if (spread) {
            DrawCircleV(p.position, 4.0f, color);
        } else {
            // Dolk: en kort strek i fartsretningen
            Vector2 tail = Vector2Subtract(p.position, Vector2Scale(p.direction, 12.0f));
            DrawLineEx(tail, p.position, 3.0f, color);
        }
    }
}

// =====================================================================
// MeleeWeapon (Ground Slam)
// =====================================================================

void MeleeWeapon::tick(float deltaTime, Vector2 playerPos, std::vector<std::unique_ptr<Enemy>>& enemies, std::vector<Pickup>& pickups)
{
    fireTimer += deltaTime;
    lastPlayerPos = playerPos; // Oppdaterer posisjonen hver frame slik at draw() vet hvor spilleren er
    if (effectTimer > 0.0f) effectTimer -= deltaTime;

    if (fireTimer < cooldown()) return;

    // Slå bare når minst én fiende er innenfor rekkevidde – ellers holdes angrepet klart
    bool anyInRange = std::any_of(enemies.begin(), enemies.end(), [&](const auto& e) {
        return Vector2Distance(playerPos, e->position) <= radius();
    });
    if (!anyInRange) return;

    damageEnemiesInRadius(playerPos, radius(), scaledDamage(), color, false, enemies, pickups);
    effectTimer = 0.3f;
    fireTimer = 0.0f;
}

void MeleeWeapon::draw() const {
    // Svak ring som viser rekkevidden
    DrawCircleLines((int)lastPlayerPos.x, (int)lastPlayerPos.y, radius(), Fade(color, 0.3f));

    // Sjokkbølge som vokser utover når slaget treffer
    if (effectTimer > 0.0f) {
        float t = 1.0f - effectTimer / 0.3f; // 0 -> 1
        DrawCircleV(lastPlayerPos, radius() * t, Fade(color, 0.35f * (1.0f - t)));
        DrawCircleLines((int)lastPlayerPos.x, (int)lastPlayerPos.y, radius() * t, Fade(color, 1.0f - t));
    }
}

// =====================================================================
// BouncingProjectileWeapon (Ricochet, Magic Missile)
// =====================================================================

BouncingProjectileWeapon::BouncingProjectileWeapon(bool isHoming) : homing(isHoming) {}

void BouncingProjectileWeapon::tick(float deltaTime, Vector2 playerPos, std::vector<std::unique_ptr<Enemy>>& enemies, std::vector<Pickup>& pickups)
{
    fireTimer += deltaTime;

    if (fireTimer >= cooldown() && !enemies.empty()) {
        int count = stats.projectiles + mods.extraProjectiles;
        std::vector<Enemy*> targets = nearestEnemies(playerPos, enemies, count);

        for (int i = 0; i < count; i++) {
            Enemy* target = targets[i % targets.size()];
            Vector2 dir = Vector2Normalize(Vector2Subtract(target->position, playerPos));

            // Homing-missiler skytes ut i en bue og svinger inn mot målet
            if (homing) dir = rotateDegrees(dir, (float)GetRandomValue(-50, 50));

            projectiles.push_back({
                .position = playerPos,
                .direction = dir,
                .speed = stats.speed,
                .damage = stats.damage * mods.damageMult,
                .lifetime = 3.0f,
                .bouncesLeft = stats.bounces,
                .bounceRange = stats.bounceRange,
                .targetId = target->id,
                .hitEnemyIds = {}
            });
        }
        fireTimer = 0.0f;
    }

    for (size_t i = 0; i < projectiles.size(); ) {
        auto& p = projectiles[i];

        if (homing) {
            Enemy* target = findEnemyById(enemies, p.targetId);
            if (!target) {
                // Målet døde før vi rakk frem – finn et nytt i nærheten
                target = nearestUnhitEnemy(p.position, p.bounceRange, p.hitEnemyIds, enemies);
                if (target) p.targetId = target->id;
            }
            if (target) {
                Vector2 desired = Vector2Normalize(Vector2Subtract(target->position, p.position));
                p.direction = Vector2Normalize(Vector2Lerp(p.direction, desired, std::min(1.0f, 8.0f * deltaTime)));
            }
        }

        p.position = Vector2Add(p.position, Vector2Scale(p.direction, p.speed * deltaTime));
        p.lifetime -= deltaTime;

        bool destroyed = false;

        for (size_t j = 0; j < enemies.size(); j++) {
            Enemy* enemy = enemies[j].get();
            if (containsId(p.hitEnemyIds, enemy->id) ||
                !CheckCollisionCircles(p.position, PROJECTILE_HIT_RADIUS, enemy->position, ENEMY_HIT_RADIUS)) continue;

            enemy->takeDamage((int)p.damage, color);
            p.hitEnemyIds.push_back(enemy->id);
            Vector2 hitPos = enemy->position;

            if (enemy->isDead()) removeDeadEnemy(enemies, j, pickups);

            // Finn neste mål: nærmeste fiende innenfor rekkevidde som ikke er truffet
            Enemy* nextTarget = (p.bouncesLeft > 0) ? nearestUnhitEnemy(hitPos, p.bounceRange, p.hitEnemyIds, enemies) : nullptr;

            if (nextTarget) {
                // Sprett videre – bounceFalloff < 1 gjør hvert sprett svakere
                p.bouncesLeft--;
                p.damage *= stats.bounceFalloff;
                p.position = hitPos;
                p.direction = Vector2Normalize(Vector2Subtract(nextTarget->position, hitPos));
                p.targetId = nextTarget->id;
                p.lifetime = 1.5f;
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

void BouncingProjectileWeapon::draw() const {
    for (const auto& p : projectiles) {
        int bounced = (int)p.hitEnemyIds.size();

        if (homing) {
            // Magisk missil med glød og hale
            DrawCircleV(p.position, 9.0f, Fade(color, 0.25f));
            DrawCircleV(p.position, 5.0f, color);
            DrawLineEx(p.position, Vector2Subtract(p.position, Vector2Scale(p.direction, 16.0f)), 3.0f, Fade(color, 0.5f));
        } else {
            // Ricochet: mindre og mer oransje for hvert sprett
            float size = std::max(2.0f, 5.0f - bounced * 0.7f);
            float t = std::min(1.0f, bounced / 4.0f);
            Color c = {
                (unsigned char)(color.r + (ORANGE.r - color.r) * t),
                (unsigned char)(color.g + (ORANGE.g - color.g) * t),
                (unsigned char)(color.b + (ORANGE.b - color.b) * t),
                255
            };
            DrawCircleV(p.position, size, c);
            DrawLineV(p.position, Vector2Subtract(p.position, Vector2Scale(p.direction, 10.0f)), Fade(c, 0.5f));
        }
    }
}

// =====================================================================
// RotWeapon
// =====================================================================

void RotWeapon::tick(float deltaTime, Vector2 playerPos, std::vector<std::unique_ptr<Enemy>>& enemies, std::vector<Pickup>& pickups)
{
    lastPlayerPos = playerPos;
    pulseTimer += deltaTime;

    // Samle opp skade hver frame. Når vi har minst 1 hel skade, del den ut til alle i radius.
    // Ved 60 FPS og f.eks. 60 DPS blir dette 1 skade per fiende per frame.
    damageAccumulator += stats.damage * mods.damageMult * deltaTime;
    int tickDamage = (int)damageAccumulator;
    if (tickDamage <= 0) return;
    damageAccumulator -= tickDamage;

    damageEnemiesInRadius(playerPos, radius(), tickDamage, color, true, enemies, pickups);
}

void RotWeapon::draw() const {
    // Pulserende giftsky rundt spilleren
    float pulse = 0.5f + 0.5f * sinf(pulseTimer * 4.0f);
    DrawCircleV(lastPlayerPos, radius(), Fade(DARKGREEN, 0.15f + 0.08f * pulse));
    DrawCircleLines((int)lastPlayerPos.x, (int)lastPlayerPos.y, radius() - 2.0f * pulse, Fade(color, 0.6f));
}

// =====================================================================
// OrbitWeapon (Orbiting Blades)
// =====================================================================

Vector2 OrbitWeapon::bladePosition(int index) const {
    float a = (angle + 360.0f / bladeCount * index) * DEG2RAD;
    return { lastPlayerPos.x + cosf(a) * radius(), lastPlayerPos.y + sinf(a) * radius() };
}

void OrbitWeapon::tick(float deltaTime, Vector2 playerPos, std::vector<std::unique_ptr<Enemy>>& enemies, std::vector<Pickup>& pickups)
{
    lastPlayerPos = playerPos;
    time += deltaTime;
    angle = fmodf(angle + stats.speed * deltaTime, 360.0f);
    bladeCount = stats.projectiles + mods.extraProjectiles;
    int dmg = scaledDamage();

    for (int b = 0; b < bladeCount; b++) {
        Vector2 bladePos = bladePosition(b);

        for (size_t j = 0; j < enemies.size(); ) {
            Enemy* enemy = enemies[j].get();
            if (!CheckCollisionCircles(bladePos, 10.0f, enemy->position, ENEMY_HIT_RADIUS)) { j++; continue; }

            // Samme fiende kan bare treffes én gang per cooldown
            auto it = lastHitTime.find(enemy->id);
            if (it != lastHitTime.end() && time - it->second < cooldown()) { j++; continue; }

            enemy->takeDamage(dmg, color);
            lastHitTime[enemy->id] = time;

            if (enemy->isDead()) removeDeadEnemy(enemies, j, pickups);
            else j++;
        }
    }

    // Rydd bort gamle oppføringer så mappet ikke vokser for alltid
    if (lastHitTime.size() > 256) {
        for (auto it = lastHitTime.begin(); it != lastHitTime.end(); ) {
            if (time - it->second >= cooldown()) it = lastHitTime.erase(it);
            else ++it;
        }
    }
}

void OrbitWeapon::draw() const {
    for (int b = 0; b < bladeCount; b++) {
        Vector2 pos = bladePosition(b);
        float bladeRotation = angle + 360.0f / bladeCount * b + 90.0f; // Pek i bevegelsesretningen
        DrawPoly(pos, 3, 11.0f, bladeRotation, color);
        DrawPolyLines(pos, 3, 11.0f, bladeRotation, BLACK);
    }
}

// =====================================================================
// LightningWeapon
// =====================================================================

void LightningWeapon::tick(float deltaTime, Vector2 playerPos, std::vector<std::unique_ptr<Enemy>>& enemies, std::vector<Pickup>& pickups)
{
    fireTimer += deltaTime;

    for (size_t i = 0; i < bolts.size(); ) {
        bolts[i].timer -= deltaTime;
        if (bolts[i].timer <= 0.0f) bolts.erase(bolts.begin() + i);
        else i++;
    }

    if (fireTimer < cooldown()) return;

    // Finn fiender innenfor rekkevidde
    std::vector<Vector2> candidates;
    for (const auto& e : enemies) {
        if (Vector2Distance(playerPos, e->position) <= radius()) candidates.push_back(e->position);
    }
    if (candidates.empty()) return; // Hold lynet klart til noen kommer nær nok

    // Velg tilfeldige mål (forskjellige så lenge det finnes nok fiender)
    int count = stats.projectiles + mods.extraProjectiles;
    std::vector<Vector2> strikePositions;
    for (int i = 0; i < count; i++) {
        if (candidates.empty()) break;
        int idx = GetRandomValue(0, (int)candidates.size() - 1);
        strikePositions.push_back(candidates[idx]);
        candidates.erase(candidates.begin() + idx);
    }

    int dmg = scaledDamage();
    for (Vector2 pos : strikePositions) {
        damageEnemiesInRadius(pos, area(), dmg, color, false, enemies, pickups);

        // Lag en hakkete lynstrek fra "himmelen" ned til treffpunktet
        LightningBolt bolt{ pos, area(), 0.25f, {} };
        Vector2 start = { pos.x + (float)GetRandomValue(-40, 40), pos.y - 350.0f };
        const int segments = 7;
        for (int s = 0; s <= segments; s++) {
            float t = (float)s / segments;
            Vector2 point = Vector2Lerp(start, pos, t);
            if (s != 0 && s != segments) point.x += (float)GetRandomValue(-18, 18);
            bolt.points.push_back(point);
        }
        bolts.push_back(bolt);
    }

    fireTimer = 0.0f;
}

void LightningWeapon::draw() const {
    for (const auto& bolt : bolts) {
        float alpha = bolt.timer / 0.25f;
        DrawCircleV(bolt.target, bolt.radius, Fade(color, 0.2f * alpha));
        for (size_t i = 1; i < bolt.points.size(); i++) {
            DrawLineEx(bolt.points[i - 1], bolt.points[i], 4.0f, Fade(color, alpha));
            DrawLineEx(bolt.points[i - 1], bolt.points[i], 1.5f, Fade(WHITE, alpha));
        }
    }
}
