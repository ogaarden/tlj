#include "weapon.hpp"
#include <raymath.h>
#include <algorithm>
#include <cmath>
#include "render3d.hpp"
#include "castle.hpp"
#include "audio.hpp"
#include "vfx.hpp"

// --- Felles hjelpefunksjoner ---
namespace {

// Dropp loot (XP/gull), tell drapet og fjern død fiende
void removeDeadEnemy(std::vector<std::unique_ptr<Enemy>>& enemies, size_t index, std::vector<Pickup>& pickups) {
    enemies[index]->dropLoot(pickups);
    VfxDeath(enemies[index]->position, Color{ 255, 210, 150, 255 });
    enemies[index]->onDeath();
    Enemy::killCount++;
    PlaySfx(Sfx::KILL);
    enemies.erase(enemies.begin() + index);
}

// Gjør skade på alle fiender innenfor radius. Returnerer antall fiender som ble truffet.
int damageEnemiesInRadius(Vector2 center, float radius, int damage, Color color, bool isDamageOverTime,
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
constexpr float SLASH_ROTATION_OFFSET = 180.0f; // Snur slash-teksturen så buen følger bladets retning
constexpr float PROJECTILE_HEIGHT = 18.0f; // Hvor høyt over bakken prosjektiler flyr (3D)

// Liten skygge under noe som svever
void smallShadow(Vector2 pos, float size) {
    DrawEllipse((int)pos.x + 2, (int)pos.y + 2, size, size * 0.6f, Fade(BLACK, 0.3f));
}

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
        VfxTrail(ToWorld3D(p.position, PROJECTILE_HEIGHT), color, spread ? 11.0f : 9.0f, 0.14f);

        bool destroyed = false;
        for (size_t j = 0; j < enemies.size() && !destroyed; ) {
            Enemy* enemy = enemies[j].get();
            if (containsId(p.hitEnemyIds, enemy->id) ||
                !CheckCollisionCircles(p.position, PROJECTILE_HIT_RADIUS, enemy->position, enemy->hitRadius)) {
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

void ProjectileWeapon::drawVfx() const {
    for (const auto& p : projectiles) {
        Vector3 pos = ToWorld3D(p.position, PROJECTILE_HEIGHT);
        VfxBillboard(VfxTex::GLOW, pos, spread ? 26.0f : 20.0f, Fade(color, 0.85f));
        VfxBillboard(VfxTex::SPARK, pos, spread ? 16.0f : 12.0f, Color{ 255, 255, 255, 200 }, (float)GetTime() * 360.0f);
    }
}

void ProjectileWeapon::draw() const {
    for (const auto& p : projectiles) smallShadow(p.position, spread ? 4.0f : 5.0f);
}

void ProjectileWeapon::draw3D() const {
    for (const auto& p : projectiles) {
        if (spread) {
            // Trefork-prong: gullkule med en liten spiss
            ShadedSphere(ToWorld3D(p.position, PROJECTILE_HEIGHT), 4.5f, color, 5, 8);
            Vector2 tip = Vector2Add(p.position, Vector2Scale(p.direction, 9.0f));
            ShadedCylinder(ToWorld3D(p.position, PROJECTILE_HEIGHT), ToWorld3D(tip, PROJECTILE_HEIGHT), 3.0f, 0.0f, color, 6);
        } else {
            // Dolk: skaft + blad i fartsretningen
            Vector2 hilt = Vector2Subtract(p.position, Vector2Scale(p.direction, 8.0f));
            Vector2 tip = Vector2Add(p.position, Vector2Scale(p.direction, 8.0f));
            ShadedCylinder(ToWorld3D(hilt, PROJECTILE_HEIGHT), ToWorld3D(p.position, PROJECTILE_HEIGHT), 1.8f, 1.8f, Color{ 90, 60, 40, 255 }, 6);
            ShadedCylinder(ToWorld3D(p.position, PROJECTILE_HEIGHT), ToWorld3D(tip, PROJECTILE_HEIGHT), 2.8f, 0.0f, color, 6);
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
    VfxShockwave(playerPos, radius(), color);
    effectTimer = 0.3f;
    fireTimer = 0.0f;
}

void MeleeWeapon::draw() const {
    // Svak ring som viser rekkevidden
    DrawCircleLines((int)lastPlayerPos.x, (int)lastPlayerPos.y, radius(), Fade(color, 0.3f));

    // Mørk sprekk-skygge på gulvet rett etter slaget (selve sjokkbølgen er VFX, se VfxShockwave)
    if (effectTimer > 0.0f) {
        float t = 1.0f - effectTimer / 0.3f; // 0 -> 1
        DrawCircleV(lastPlayerPos, radius() * 0.5f, Fade(BLACK, 0.25f * (1.0f - t)));
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
        if (homing) {
            VfxTrail(ToWorld3D(p.position, PROJECTILE_HEIGHT + 6.0f), color, 16.0f, 0.3f);
            if (GetRandomValue(0, 3) == 0) VfxTrail(ToWorld3D(p.position, PROJECTILE_HEIGHT + 6.0f + GetRandomValue(-6, 6)), WHITE, 5.0f, 0.4f);
        } else {
            VfxTrail(ToWorld3D(p.position, PROJECTILE_HEIGHT), Color{ 255, 190, 120, 255 }, 8.0f, 0.16f);
        }

        bool destroyed = false;

        for (size_t j = 0; j < enemies.size(); j++) {
            Enemy* enemy = enemies[j].get();
            if (containsId(p.hitEnemyIds, enemy->id) ||
                !CheckCollisionCircles(p.position, PROJECTILE_HIT_RADIUS, enemy->position, enemy->hitRadius)) continue;

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

void BouncingProjectileWeapon::drawVfx() const {
    float t = (float)GetTime();
    for (const auto& p : projectiles) {
        if (homing) {
            Vector3 pos = ToWorld3D(p.position, PROJECTILE_HEIGHT + 6.0f);
            VfxBillboard(VfxTex::GLOW, pos, 44.0f, Fade(color, 0.6f));
            VfxBillboard(VfxTex::MAGIC_ORB, pos, 38.0f, WHITE, t * 240.0f);
        } else {
            // Ricochet: blå gnist som blir oransje for hvert sprett
            float k = std::min(1.0f, p.hitEnemyIds.size() / 4.0f);
            Color c = { (unsigned char)(color.r + (255 - color.r) * k), (unsigned char)(color.g + (150 - color.g) * k), (unsigned char)(color.b + (40 - color.b) * k), 255 };
            Vector3 pos = ToWorld3D(p.position, PROJECTILE_HEIGHT);
            VfxBillboard(VfxTex::GLOW, pos, 24.0f, c);
            VfxBillboard(VfxTex::SPARK, pos, 22.0f, WHITE, t * 500.0f);
        }
    }
}

void BouncingProjectileWeapon::draw() const {
    for (const auto& p : projectiles) smallShadow(p.position, homing ? 6.0f : 4.0f);
}

void BouncingProjectileWeapon::draw3D() const {
    for (const auto& p : projectiles) {
        int bounced = (int)p.hitEnemyIds.size();
        Vector3 pos = ToWorld3D(p.position, PROJECTILE_HEIGHT + (homing ? 6.0f : 0.0f));

        if (homing) {
            // Magisk missil: lysende kjerne med glorie og hale
            ShadedSphere(pos, 4.0f, Color{ 240, 220, 255, 255 }, 6, 8); // Lys kjerne – gløden er VFX
        } else {
            // Ricochet: mindre og mer oransje for hvert sprett
            float size = std::max(2.5f, 5.0f - bounced * 0.6f);
            float t = std::min(1.0f, bounced / 4.0f);
            Color c = {
                (unsigned char)(color.r + (ORANGE.r - color.r) * t),
                (unsigned char)(color.g + (ORANGE.g - color.g) * t),
                (unsigned char)(color.b + (ORANGE.b - color.b) * t),
                255
            };
            ShadedSphere(pos, size, c, 5, 8);
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

    // Giftbobler som stiger opp fra tåka
    bubbleTimer += deltaTime;
    while (bubbleTimer > 0.05f) {
        bubbleTimer -= 0.05f;
        float a = GetRandomValue(0, 628) / 100.0f;
        float d = radius() * sqrtf(GetRandomValue(0, 100) / 100.0f) * 0.9f;
        VfxBubble({ playerPos.x + cosf(a) * d, playerPos.y + sinf(a) * d }, 3.0f, Color{ 120, 255, 90, 255 });
    }

    // Samle opp skade hver frame. Når vi har minst 1 hel skade, del den ut til alle i radius.
    // Ved 60 FPS og f.eks. 60 DPS blir dette 1 skade per fiende per frame.
    damageAccumulator += stats.damage * mods.damageMult * deltaTime;
    int tickDamage = (int)damageAccumulator;
    if (tickDamage <= 0) return;
    damageAccumulator -= tickDamage;

    damageEnemiesInRadius(playerPos, radius(), tickDamage, color, true, enemies, pickups);
}

void RotWeapon::draw() const {
    // Pulserende giftsky på gulvet rundt spilleren
    float pulse = 0.5f + 0.5f * sinf(pulseTimer * 4.0f);
    DrawCircleV(lastPlayerPos, radius(), Fade(Color{ 10, 30, 10, 255 }, 0.12f));
    DrawCircleLines((int)lastPlayerPos.x, (int)lastPlayerPos.y, radius() - 2.0f * pulse, Fade(color, 0.5f));
}

void RotWeapon::drawVfx() const {
    // To lag gifttåke som roterer hver sin vei og pulserer
    // Dempet så klovnen og fiendene fortsatt synes gjennom tåka
    float pulse = 0.8f + 0.2f * sinf(pulseTimer * 4.0f);
    VfxDecal(VfxTex::POISON_MIST, lastPlayerPos, radius() * 2.2f, Color{ (unsigned char)(70 * pulse), (unsigned char)(110 * pulse), (unsigned char)(60 * pulse), 255 }, pulseTimer * 25.0f, 1.2f);
    VfxDecal(VfxTex::POISON_MIST, lastPlayerPos, radius() * 1.5f, Color{ 30, 60, 30, 255 }, -pulseTimer * 40.0f, 1.4f);

    // Glødende sporer som svever rundt i auraen
    const int spores = 10;
    for (int i = 0; i < spores; i++) {
        float a = pulseTimer * (0.6f + 0.1f * (i % 3)) + i * (2.0f * PI / spores);
        float dist = radius() * (0.35f + 0.5f * (0.5f + 0.5f * sinf(pulseTimer * 0.7f + i)));
        Vector2 pos = { lastPlayerPos.x + cosf(a) * dist, lastPlayerPos.y + sinf(a) * dist };
        float height = 10.0f + 12.0f * (0.5f + 0.5f * sinf(pulseTimer * 2.0f + i * 1.7f));
        VfxBillboard(VfxTex::GLOW, ToWorld3D(pos, height), 14.0f, Color{ 140, 255, 100, 255 });
    }
}

void RotWeapon::draw3D() const {
    // Alt det synlige er VFX (gifttåke og sporer), se drawVfx()
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

    // Lysende ribbe bak hvert blad
    for (int b = 0; b < bladeCount; b++) VfxTrail(ToWorld3D(bladePosition(b), 16.0f), color, 14.0f, 0.18f);

    for (int b = 0; b < bladeCount; b++) {
        Vector2 bladePos = bladePosition(b);

        for (size_t j = 0; j < enemies.size(); ) {
            Enemy* enemy = enemies[j].get();
            if (!CheckCollisionCircles(bladePos, 10.0f, enemy->position, enemy->hitRadius)) { j++; continue; }

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

void OrbitWeapon::drawVfx() const {
    for (int b = 0; b < bladeCount; b++) {
        Vector2 pos = bladePosition(b);
        float deg = angle + 360.0f / bladeCount * b;
        // Hugg-bue som følger bladet rundt spilleren
        VfxDecal(VfxTex::SLASH, pos, 46.0f, Color{ 255, 200, 230, 255 }, deg + SLASH_ROTATION_OFFSET, 14.0f);
        VfxBillboard(VfxTex::GLOW, ToWorld3D(pos, 16.0f), 26.0f, Fade(color, 0.7f));
    }
}

void OrbitWeapon::draw() const {
    for (int b = 0; b < bladeCount; b++) smallShadow(bladePosition(b), 7.0f);
}

void OrbitWeapon::draw3D() const {
    for (int b = 0; b < bladeCount; b++) {
        Vector2 pos = bladePosition(b);
        // Bladet ligger langs bevegelsesretningen (tangenten til sirkelen)
        float a = (angle + 360.0f / bladeCount * b) * DEG2RAD;
        Vector2 tangent = { -sinf(a), cosf(a) };
        Vector2 back = Vector2Subtract(pos, Vector2Scale(tangent, 6.0f));
        Vector2 front = Vector2Add(pos, Vector2Scale(tangent, 12.0f));
        ShadedCylinder(ToWorld3D(back, 16.0f), ToWorld3D(front, 16.0f), 4.5f, 0.0f, color, 6);
        ShadedSphere(ToWorld3D(back, 16.0f), 3.0f, Color{ 110, 110, 120, 255 }, 4, 6);
    }
}

// =====================================================================
// LightningWeapon
// =====================================================================

namespace {
    constexpr float BOLT_TIME = 0.25f;
    constexpr float ARC_TIME = 0.22f;
    constexpr float CHAIN_DELAY = 0.07f; // Tid mellom hvert hopp i kjeden
    constexpr float ARC_HEIGHT = 26.0f;  // Høyden buene går i (ca. brysthøyde på fiendene)

    // Hakkete bue mellom to punkter i 3D
    std::vector<Vector3> jaggedLine(Vector3 from, Vector3 to, int segments, float jitter) {
        std::vector<Vector3> points;
        for (int s = 0; s <= segments; s++) {
            float t = (float)s / segments;
            Vector3 p = Vector3Lerp(from, to, t);
            if (s != 0 && s != segments) {
                p.x += (float)GetRandomValue((int)-jitter, (int)jitter);
                p.y += (float)GetRandomValue((int)(-jitter * 0.5f), (int)(jitter * 0.5f));
                p.z += (float)GetRandomValue((int)-jitter, (int)jitter);
            }
            points.push_back(p);
        }
        return points;
    }
}

void LightningWeapon::queueNextJump(Vector2 from, float damage, int jumpsLeft, const std::vector<int>& hitIds,
                                    const std::vector<std::unique_ptr<Enemy>>& enemies) {
    if (jumpsLeft <= 0 || (int)damage <= 0) return;
    Enemy* next = nearestUnhitEnemy(from, stats.bounceRange * mods.areaMult, hitIds, enemies);
    if (!next) return;
    pendingJumps.push_back({ from, next->id, damage, jumpsLeft - 1, CHAIN_DELAY, hitIds });
}

void LightningWeapon::tick(float deltaTime, Vector2 playerPos, std::vector<std::unique_ptr<Enemy>>& enemies, std::vector<Pickup>& pickups)
{
    fireTimer += deltaTime;

    for (size_t i = 0; i < bolts.size(); ) {
        bolts[i].timer -= deltaTime;
        if (bolts[i].timer <= 0.0f) bolts.erase(bolts.begin() + i);
        else i++;
    }

    // --- Kjede-hopp som står i kø ---
    std::vector<ChainJump> ready;
    for (size_t i = 0; i < pendingJumps.size(); ) {
        pendingJumps[i].delay -= deltaTime;
        if (pendingJumps[i].delay <= 0.0f) {
            ready.push_back(pendingJumps[i]);
            pendingJumps.erase(pendingJumps.begin() + i);
        } else {
            i++;
        }
    }
    for (ChainJump& jump : ready) {
        // Målet kan ha dødd mens vi ventet – ta i så fall nærmeste andre
        Enemy* target = findEnemyById(enemies, jump.targetId);
        if (!target) target = nearestUnhitEnemy(jump.from, stats.bounceRange * mods.areaMult, jump.hitIds, enemies);
        if (!target) continue;

        Vector2 targetPos = target->position;
        int targetId = target->id;
        jump.hitIds.push_back(targetId);

        bolts.push_back({ targetPos, 0.0f, ARC_TIME, ARC_TIME,
                          jaggedLine(ToWorld3D(jump.from, ARC_HEIGHT), ToWorld3D(targetPos, ARC_HEIGHT), 6, 10.0f) });
        PlaySfx(Sfx::ZAP);

        target->takeDamage((int)jump.damage, Color{ 180, 220, 255, 255 });
        for (size_t j = 0; j < enemies.size(); j++) {
            if (enemies[j]->id == targetId && enemies[j]->isDead()) {
                removeDeadEnemy(enemies, j, pickups);
                break;
            }
        }

        queueNextJump(targetPos, jump.damage * stats.bounceFalloff, jump.jumpsLeft, jump.hitIds, enemies);
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
        // Nedslaget: AOE-skade der lynet treffer
        std::vector<int> hitIds;
        damageEnemiesInRadius(pos, area(), dmg, color, false, enemies, pickups, &hitIds);

        // Hakkete lynstrek fra himmelen rett ned til treffpunktet
        Vector3 sky = { pos.x + (float)GetRandomValue(-20, 20), 420.0f, pos.y + (float)GetRandomValue(-20, 20) };
        bolts.push_back({ pos, area(), BOLT_TIME, BOLT_TIME, jaggedLine(sky, ToWorld3D(pos, 0.0f), 8, 16.0f) });
        VfxLightningStrike(pos, area());

        // Kjeden: hopper videre til nye fiender, svakere for hvert hopp
        queueNextJump(pos, dmg * stats.bounceFalloff, stats.bounces, hitIds, enemies);
    }
    PlaySfx(Sfx::THUNDER);

    fireTimer = 0.0f;
}

void LightningWeapon::draw() const {
    for (const auto& bolt : bolts) {
        float alpha = bolt.timer / bolt.maxTimer;
        if (bolt.radius > 0.0f) {
            // Brent merke på gulvet der lynet slo ned
            DrawCircleV(bolt.target, bolt.radius, Fade(color, 0.25f * alpha));
            DrawCircleLines((int)bolt.target.x, (int)bolt.target.y, bolt.radius, Fade(WHITE, 0.6f * alpha));
        } else {
            // Lite lysglimt under fienden kjeden traff
            DrawCircleV(bolt.target, 16.0f, Fade(SKYBLUE, 0.35f * alpha));
        }
    }
}

void LightningWeapon::draw3D() const {
    // Alt det synlige er VFX, se drawVfx()
}

void LightningWeapon::drawVfx() const {
    for (const auto& bolt : bolts) {
        float alpha = bolt.timer / bolt.maxTimer;
        // Flimrer litt de første øyeblikkene, som ekte lyn
        float flicker = (alpha > 0.6f && GetRandomValue(0, 3) == 0) ? 0.5f : 1.0f;
        unsigned char a = (unsigned char)(255 * alpha * flicker);
        bool isArc = bolt.radius <= 0.0f;
        if (!isArc) {
            // Nedslag: lyn-teksturen fra himmelen og ned, med blå glød rundt
            Vector3 sky = bolt.points.front(), ground = bolt.points.back();
            VfxBeam(VfxTex::LIGHTNING_BOLT, sky, ground, 150.0f, Color{ a, a, a, 255 });
            VfxBeam(VfxTex::GLOW, sky, ground, 60.0f, Color{ (unsigned char)(60 * alpha), (unsigned char)(110 * alpha), (unsigned char)(220 * alpha), 255 });
            VfxBillboard(VfxTex::GLOW, Vector3Add(ground, { 0, 12, 0 }), 90.0f * alpha + 30.0f, Color{ (unsigned char)(160 * alpha), (unsigned char)(210 * alpha), a, 255 });
        } else {
            // Kjede-bue mellom fiender: hakkete stråle med hvit kjerne
            for (size_t i = 1; i < bolt.points.size(); i++) {
                VfxBeam(VfxTex::GLOW, bolt.points[i - 1], bolt.points[i], 18.0f, Color{ (unsigned char)(90 * alpha), (unsigned char)(160 * alpha), a, 255 });
                VfxBeam(VfxTex::GLOW, bolt.points[i - 1], bolt.points[i], 6.0f, Color{ a, a, a, 255 });
            }
            VfxBillboard(VfxTex::SPARK, bolt.points.back(), 40.0f * alpha + 10.0f, Color{ a, a, a, 255 }, alpha * 180.0f);
        }
    }
}
