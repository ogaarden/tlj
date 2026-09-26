#include "weapon.hpp"
#include "weapon_helpers.hpp"
#include <cmath>

// =====================================================================
// De nye abilitiene: Ildsluker, Bumerang, Kortstokk, Frostnova, Katapult,
// Narrebjeller, Rampelys, Sabelhugg og Virvelvind.
// Tallene (skade, cooldown osv.) ligger i abilities.cpp – her er bare oppførselen.
// =====================================================================

using namespace weapon_detail;

namespace {

// Skader fienden på plass j. Returnerer true hvis den døde (og ble fjernet fra lista).
bool hitEnemy(std::vector<std::unique_ptr<Enemy>>& enemies, size_t j, int damage, Color color, std::vector<Pickup>& pickups) {
    enemies[j]->takeDamage(damage, color);
    if (enemies[j]->isDead()) {
        removeDeadEnemy(enemies, j, pickups);
        return true;
    }
    return false;
}

bool anyEnemyWithin(Vector2 from, float range, const std::vector<std::unique_ptr<Enemy>>& enemies) {
    float r2 = range * range;
    for (const auto& e : enemies) if (Vector2DistanceSqr(from, e->position) <= r2) return true;
    return false;
}

float frandf(float a, float b) { return a + (b - a) * (GetRandomValue(0, 10000) / 10000.0f); }

// Trekant på gulvet uansett hjørnerekkefølge
void groundTri(Vector2 a, Vector2 b, Vector2 c, Color col) {
    float cross = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    if (cross < 0.0f) DrawTriangle(a, b, c, col);
    else DrawTriangle(a, c, b, col);
}

float headingDeg(Vector2 d) { return atan2f(d.y, d.x) * RAD2DEG; }

} // namespace

// =====================================================================
// Ildsluker (FIRE_BREATH)
//  area     = kjeglens halvvinkel i grader
//  radius   = rekkevidde
//  duration = hvor lenge hvert pust varer
//  damage   = skade per "tikk" (hvert 0.12 sek)
// =====================================================================
namespace { constexpr float FLAME_TICK = 0.12f; }

void FlameWeapon::tick(float deltaTime, Vector2 playerPos, std::vector<std::unique_ptr<Enemy>>& enemies, std::vector<Pickup>& pickups) {
    lastPlayerPos = playerPos;

    if (breathTimer > 0.0f) {
        breathTimer -= deltaTime;
        tickTimer += deltaTime;

        // Kjeglene følger målet litt mens de blåser
        if (!enemies.empty()) {
            std::vector<Enemy*> near = nearestEnemies(playerPos, enemies, (int)directions.size());
            for (size_t i = 0; i < directions.size() && i < near.size(); i++) {
                Vector2 want = Vector2Normalize(Vector2Subtract(near[i]->position, playerPos));
                directions[i] = Vector2Normalize(Vector2Lerp(directions[i], want, std::min(1.0f, 3.0f * deltaTime)));
            }
        }

        // Flammer: mange små ildkuler som skytes ut i kjeglen
        Color hot = hasScepter ? Color{ 110, 170, 255, 255 } : Color{ 255, 150, 50, 255 };
        Color core = hasScepter ? Color{ 220, 240, 255, 255 } : Color{ 255, 230, 140, 255 };
        for (Vector2 d : directions) {
            for (int k = 0; k < 3; k++) {
                Vector2 dir = rotateDegrees(d, frandf(-area(), area()) * 0.8f);
                float v = radius() / 0.42f * frandf(0.8f, 1.1f);
                Vector3 start = ToWorld3D(Vector2Add(playerPos, Vector2Scale(d, 14.0f)), 22.0f);
                VfxParticle(k == 0 ? VfxTex::EXPLOSION : VfxTex::GLOW, start, { dir.x * v, frandf(-20.0f, 25.0f), dir.y * v },
                            0.42f, 8.0f, radius() * 0.45f, k == 2 ? core : hot, 0.0f, 1.2f, frandf(0, 360), 200.0f);
            }
        }

        if (tickTimer >= FLAME_TICK) {
            tickTimer -= FLAME_TICK;
            float cosLimit = cosf(area() * DEG2RAD);
            int dmg = scaledDamage();
            for (size_t j = 0; j < enemies.size(); ) {
                Vector2 to = Vector2Subtract(enemies[j]->position, playerPos);
                float dist = Vector2Length(to);
                bool inside = false;
                if (dist <= radius() + enemies[j]->hitRadius) {
                    Vector2 n = dist > 0.01f ? Vector2Scale(to, 1.0f / dist) : directions.empty() ? Vector2{ 1, 0 } : directions[0];
                    for (Vector2 d : directions) if (Vector2DotProduct(n, d) >= cosLimit || dist < 24.0f) { inside = true; break; }
                }
                if (inside && hitEnemy(enemies, j, dmg, hot, pickups)) continue;
                j++;
            }
        }
        if (breathTimer <= 0.0f) fireTimer = 0.0f;
        return;
    }

    fireTimer += deltaTime;
    if (fireTimer < cooldown() || !anyEnemyWithin(playerPos, radius() * 1.2f, enemies)) return;

    // Én kjegle per prosjektil, rettet mot de nærmeste fiendene (ekstra kjegler spres jevnt)
    int count = stats.projectiles + mods.extraProjectiles;
    std::vector<Enemy*> near = nearestEnemies(playerPos, enemies, count);
    directions.clear();
    Vector2 first = Vector2Normalize(Vector2Subtract(near[0]->position, playerPos));
    for (int i = 0; i < count; i++) {
        if (i < (int)near.size() && Vector2Distance(near[i]->position, playerPos) < radius() * 1.3f)
            directions.push_back(Vector2Normalize(Vector2Subtract(near[i]->position, playerPos)));
        else
            directions.push_back(rotateDegrees(first, 360.0f / count * i));
    }
    breathTimer = stats.duration * mods.durationMult;
    tickTimer = FLAME_TICK;
    PlaySfxPitch(Sfx::EXPLOSION, 1.6f);
}

void FlameWeapon::draw() const {
    if (breathTimer <= 0.0f) return;
    // Oransje glød på gulvet i kjeglen
    for (Vector2 d : directions) {
        float h = headingDeg(d);
        DrawCircleSector(lastPlayerPos, radius(), h - area(), h + area(), 16, Fade(hasScepter ? SKYBLUE : ORANGE, 0.16f));
    }
}

void FlameWeapon::drawVfx() const {
    if (breathTimer <= 0.0f) return;
    for (Vector2 d : directions) {
        Vector3 mouth = ToWorld3D(Vector2Add(lastPlayerPos, Vector2Scale(d, 16.0f)), 22.0f);
        VfxBillboard(VfxTex::GLOW, mouth, 40.0f, hasScepter ? Color{ 120, 170, 255, 255 } : Color{ 255, 170, 70, 255 });
        for (int k = 1; k <= 3; k++) {
            Vector2 g = Vector2Add(lastPlayerPos, Vector2Scale(d, radius() * 0.28f * k));
            VfxDecal(VfxTex::GLOW, g, radius() * 0.35f * k, hasScepter ? Color{ 40, 70, 140, 255 } : Color{ 140, 60, 20, 255 });
        }
    }
}

// =====================================================================
// Bumerang (BOOMERANG)
//  radius = hvor langt den flyr før den snur
// =====================================================================
void BoomerangWeapon::tick(float deltaTime, Vector2 playerPos, std::vector<std::unique_ptr<Enemy>>& enemies, std::vector<Pickup>& pickups) {
    lastPlayerPos = playerPos;
    fireTimer += deltaTime;
    if (fireTimer >= cooldown() && !enemies.empty()) {
        int count = stats.projectiles + mods.extraProjectiles;
        std::vector<Enemy*> targets = nearestEnemies(playerPos, enemies, count);
        for (int i = 0; i < count; i++) {
            Vector2 dir = Vector2Normalize(Vector2Subtract(targets[i % targets.size()]->position, playerPos));
            if (i >= (int)targets.size()) dir = rotateDegrees(dir, 360.0f / count * i);
            boomers.push_back({ playerPos, dir, 0.0f, false, (float)GetRandomValue(0, 360), 6.0f, scaledDamage(), {} });
        }
        fireTimer = 0.0f;
    }

    float range = radius() * mods.speedMult;
    for (size_t i = 0; i < boomers.size(); ) {
        Boomer& b = boomers[i];
        b.lifetime -= deltaTime;
        b.spin += deltaTime * 900.0f;
        if (!b.returning) {
            // Bremser litt ned mot vendepunktet
            float slow = 0.45f + 0.55f * (1.0f - b.traveled / range);
            float step = projSpeed() * slow * deltaTime;
            b.position = Vector2Add(b.position, Vector2Scale(b.direction, step));
            b.traveled += step;
            if (b.traveled >= range) { b.returning = true; b.hitIds.clear(); }
        } else {
            Vector2 back = Vector2Subtract(playerPos, b.position);
            float dist = Vector2Length(back);
            if (dist < 20.0f || b.lifetime <= 0.0f) { boomers[i] = boomers.back(); boomers.pop_back(); continue; }
            b.direction = Vector2Scale(back, 1.0f / dist);
            b.position = Vector2Add(b.position, Vector2Scale(b.direction, projSpeed() * 1.25f * deltaTime));
        }
        VfxTrail(ToWorld3D(b.position, PROJECTILE_HEIGHT), color, 12.0f, 0.18f);

        for (size_t j = 0; j < enemies.size(); ) {
            Enemy* e = enemies[j].get();
            if (containsId(b.hitIds, e->id) || !CheckCollisionCircles(b.position, 14.0f, e->position, e->hitRadius)) { j++; continue; }
            b.hitIds.push_back(e->id);
            e->knockBack(b.direction, 140.0f);
            if (!hitEnemy(enemies, j, b.damage, color, pickups)) j++;
        }
        i++;
    }
}

void BoomerangWeapon::draw() const {
    for (const Boomer& b : boomers) smallShadow(b.position, 8.0f);
}

void BoomerangWeapon::draw3D() const {
    for (const Boomer& b : boomers) {
        // V-form: to armer som snurrer flatt rundt midten
        Vector3 c = ToWorld3D(b.position, PROJECTILE_HEIGHT);
        for (int k = 0; k < 2; k++) {
            float a = (b.spin + k * 110.0f) * DEG2RAD;
            Vector3 tip = { c.x + cosf(a) * 14.0f, c.y, c.z + sinf(a) * 14.0f };
            ShadedCylinder(c, tip, 3.2f, 2.2f, hasScepter ? Color{ 120, 220, 255, 255 } : Color{ 175, 110, 55, 255 }, 5);
        }
        ShadedSphere(c, 3.4f, Color{ 240, 200, 90, 255 }, 4, 6);
    }
}

void BoomerangWeapon::drawVfx() const {
    for (const Boomer& b : boomers) {
        Vector3 c = ToWorld3D(b.position, PROJECTILE_HEIGHT);
        VfxDecal(VfxTex::SLASH, b.position, 44.0f, Fade(color, 0.7f), b.spin, PROJECTILE_HEIGHT - 4.0f);
        VfxBillboard(VfxTex::GLOW, c, 26.0f, Fade(color, 0.6f));
    }
}

// =====================================================================
// Kortstokk (CARDS): en ring av kort som skytes ut i alle retninger
// =====================================================================
void CardWeapon::tick(float deltaTime, Vector2 playerPos, std::vector<std::unique_ptr<Enemy>>& enemies, std::vector<Pickup>& pickups) {
    fireTimer += deltaTime;
    if (fireTimer >= cooldown() && !enemies.empty()) {
        int count = stats.projectiles + mods.extraProjectiles * 2; // Ekstra prosjektiler gir to kort hver
        volleyAngle += 360.0f / count * 0.5f + 7.0f;                // Neste salve skytes mellom de forrige
        for (int i = 0; i < count; i++) {
            float a = (volleyAngle + 360.0f / count * i) * DEG2RAD;
            cards.push_back({ playerPos, { cosf(a), sinf(a) }, 1.4f, scaledDamage(), stats.pierce, (i % 2) == 0, {} });
        }
        PlaySfxPitch(Sfx::UI_MOVE, 1.4f);
        fireTimer = 0.0f;
    }

    for (size_t i = 0; i < cards.size(); ) {
        Card& c = cards[i];
        c.position = Vector2Add(c.position, Vector2Scale(c.direction, projSpeed() * deltaTime));
        c.lifetime -= deltaTime;
        bool destroyed = false;
        for (size_t j = 0; j < enemies.size() && !destroyed; ) {
            Enemy* e = enemies[j].get();
            if (containsId(c.hitIds, e->id) || !CheckCollisionCircles(c.position, 7.0f, e->position, e->hitRadius)) { j++; continue; }
            c.hitIds.push_back(e->id);
            Color hitCol = c.red ? Color{ 255, 90, 110, 255 } : Color{ 235, 235, 245, 255 };
            if (!hitEnemy(enemies, j, c.damage, hitCol, pickups)) j++;
            if (c.pierceLeft-- <= 0) destroyed = true;
        }
        if (destroyed || c.lifetime <= 0.0f) { cards[i] = cards.back(); cards.pop_back(); }
        else i++;
    }
}

void CardWeapon::draw() const {
    for (const Card& c : cards) smallShadow(c.position, 5.0f);
}

void CardWeapon::draw3D() const {
    float t = (float)GetTime();
    for (const Card& c : cards) {
        // Kortet ligger flatt og snurrer litt mens det flyr
        float yaw = -headingDeg(c.direction) + sinf(t * 14.0f + c.position.x * 0.05f) * 25.0f;
        Vector3 p = ToWorld3D(c.position, PROJECTILE_HEIGHT);
        ShadedCube(p, { 9.0f, 1.0f, 13.0f }, yaw, hasScepter ? Color{ 255, 230, 150, 255 } : Color{ 248, 244, 232, 255 });
        ShadedCube({ p.x, p.y + 0.8f, p.z }, { 4.0f, 0.6f, 4.0f }, yaw + 45.0f, c.red ? Color{ 210, 30, 50, 255 } : Color{ 30, 25, 35, 255 });
    }
}

void CardWeapon::drawVfx() const {
    for (const Card& c : cards) {
        VfxBillboard(VfxTex::GLOW, ToWorld3D(c.position, PROJECTILE_HEIGHT), 20.0f,
                     c.red ? Color{ 160, 40, 70, 255 } : Color{ 120, 120, 160, 255 });
    }
}

// =====================================================================
// Frostnova (FROST_NOVA)
//  effect   = hvor mye fiender bremses (0.5 = halv fart)
//  duration = hvor lenge de er bremset
// =====================================================================
void FrostNovaWeapon::tick(float deltaTime, Vector2 playerPos, std::vector<std::unique_ptr<Enemy>>& enemies, std::vector<Pickup>& pickups) {
    lastPlayerPos = playerPos;
    fireTimer += deltaTime;
    if (flashTimer > 0.0f) flashTimer -= deltaTime;
    for (size_t i = 0; i < shards.size(); ) {
        shards[i].timer -= deltaTime;
        if (shards[i].timer <= 0.0f) { shards[i] = shards.back(); shards.pop_back(); }
        else i++;
    }

    if (fireTimer < cooldown() || !anyEnemyWithin(playerPos, radius(), enemies)) return;

    Color ice = { 150, 215, 255, 255 };
    float slowTime = stats.duration * mods.durationMult;
    for (size_t j = 0; j < enemies.size(); ) {
        if (Vector2Distance(playerPos, enemies[j]->position) > radius()) { j++; continue; }
        enemies[j]->applySlow(stats.effect, slowTime);
        if (!hitEnemy(enemies, j, scaledDamage(), ice, pickups)) j++;
    }

    // Isnåler som skyter opp av gulvet i en ring
    int n = 10 + (int)(radius() / 25.0f);
    for (int i = 0; i < n; i++) {
        float a = (float)i / n * 2.0f * PI + frandf(-0.15f, 0.15f);
        float d = radius() * frandf(0.55f, 0.95f);
        shards.push_back({ { playerPos.x + cosf(a) * d, playerPos.y + sinf(a) * d }, 0.8f, frandf(10.0f, 16.0f), frandf(0, 360) });
    }
    VfxFrost(playerPos, radius());
    AddCameraShake(0.15f);
    PlaySfxPitch(Sfx::ZAP, 0.6f);
    flashTimer = 0.35f;
    fireTimer = 0.0f;
}

void FrostNovaWeapon::draw() const {
    DrawCircleLines((int)lastPlayerPos.x, (int)lastPlayerPos.y, radius(), Fade(SKYBLUE, 0.2f));
    if (flashTimer > 0.0f) DrawCircleV(lastPlayerPos, radius(), Fade(Color{ 180, 230, 255, 255 }, 0.35f * flashTimer / 0.35f));
    for (const IceShard& s : shards) DrawCircleV(s.position, s.size * 0.9f, Fade(Color{ 200, 235, 255, 255 }, 0.35f * s.timer / 0.8f));
}

void FrostNovaWeapon::draw3D() const {
    for (const IceShard& s : shards) {
        // Vokser opp fort og synker ned igjen
        float life = s.timer / 0.8f;
        float grow = std::min(1.0f, (1.0f - life) * 6.0f) * std::min(1.0f, life * 3.0f);
        ShadedCrystal(ToWorld3D(s.position, s.size * 1.2f * grow), s.size * 0.5f * grow + 0.1f, s.size * 2.4f * grow + 0.1f, s.spin,
                      hasScepter ? Color{ 230, 245, 255, 255 } : Color{ 150, 210, 250, 255 });
    }
}

void FrostNovaWeapon::drawVfx() const {
    for (const IceShard& s : shards) {
        VfxBillboard(VfxTex::GLOW, ToWorld3D(s.position, s.size * 1.5f), 22.0f, Color{ 70, 130, 190, 255 });
    }
}

// =====================================================================
// Katapult (CATAPULT)
//  radius = hvor langt unna den velger mål
//  area   = eksplosjonsradius
// =====================================================================
namespace {
    constexpr float BOULDER_FALL = 0.85f;
    constexpr float BOULDER_HEIGHT = 520.0f;
}

void CatapultWeapon::tick(float deltaTime, Vector2 playerPos, std::vector<std::unique_ptr<Enemy>>& enemies, std::vector<Pickup>& pickups) {
    fireTimer += deltaTime;
    if (fireTimer >= cooldown()) {
        std::vector<Vector2> candidates;
        for (const auto& e : enemies)
            if (Vector2Distance(playerPos, e->position) <= radius()) candidates.push_back(e->position);
        if (!candidates.empty()) {
            int count = stats.projectiles + mods.extraProjectiles;
            for (int i = 0; i < count; i++) {
                Vector2 t = candidates[GetRandomValue(0, (int)candidates.size() - 1)];
                if (i > 0) t = Vector2Add(t, { frandf(-30, 30), frandf(-30, 30) });
                boulders.push_back({ t, 0.0f, BOULDER_FALL + i * 0.12f, scaledDamage(), frandf(0, 360) });
            }
            fireTimer = 0.0f;
        }
    }

    for (size_t i = 0; i < boulders.size(); ) {
        Boulder& b = boulders[i];
        b.t += deltaTime / b.fallTime;
        b.spin += deltaTime * 200.0f;
        if (b.t >= 1.0f) {
            damageEnemiesInRadius(b.target, area(), b.damage, Color{ 255, 190, 110, 255 }, false, enemies, pickups);
            VfxExplosion(b.target, area() * 0.8f);
            VfxShockwave(b.target, area(), Color{ 200, 170, 130, 255 });
            AddCameraShake(0.22f);
            PlaySfx(Sfx::EXPLOSION);
            boulders[i] = boulders.back();
            boulders.pop_back();
            continue;
        }
        Vector3 p = ToWorld3D(b.target, BOULDER_HEIGHT * (1.0f - b.t * b.t));
        p.x -= (1.0f - b.t) * 120.0f;
        VfxTrail(p, hasScepter ? Color{ 255, 210, 90, 255 } : Color{ 255, 120, 40, 255 }, 26.0f, 0.3f);
        i++;
    }
}

void CatapultWeapon::draw() const {
    // Rødt sikte på gulvet som strammer seg inn før steinen treffer
    for (const Boulder& b : boulders) {
        float r = area() * (1.25f - 0.25f * b.t);
        DrawCircleV(b.target, r, Fade(Color{ 200, 40, 30, 255 }, 0.12f + 0.15f * b.t));
        DrawRing(b.target, r - 2.5f, r, 0, 360, 36, Fade(Color{ 255, 80, 50, 255 }, 0.4f + 0.5f * b.t));
        smallShadow(b.target, 10.0f + 12.0f * b.t);
    }
}

void CatapultWeapon::draw3D() const {
    for (const Boulder& b : boulders) {
        Vector3 p = ToWorld3D(b.target, BOULDER_HEIGHT * (1.0f - b.t * b.t));
        p.x -= (1.0f - b.t) * 120.0f;
        Color rock = hasScepter ? Color{ 230, 190, 80, 255 } : Color{ 140, 128, 118, 255 };
        ShadedSphere(p, 14.0f, rock, 6, 8);
        float a = b.spin * DEG2RAD;
        ShadedSphere({ p.x + cosf(a) * 9.0f, p.y + 5.0f, p.z + sinf(a) * 9.0f }, 7.0f, rock, 4, 6);
    }
}

void CatapultWeapon::drawVfx() const {
    for (const Boulder& b : boulders) {
        Vector3 p = ToWorld3D(b.target, BOULDER_HEIGHT * (1.0f - b.t * b.t));
        p.x -= (1.0f - b.t) * 120.0f;
        VfxBillboard(VfxTex::EXPLOSION, p, 46.0f, Color{ 200, 110, 50, 255 }, b.spin);
    }
}

// =====================================================================
// Narrebjeller (BELLS)
//  speed  = hvor fort ringen brer seg
//  radius = hvor langt ringen går
//  effect = hvor hardt fiender dyttes
// =====================================================================
void BellWeapon::tick(float deltaTime, Vector2 playerPos, std::vector<std::unique_ptr<Enemy>>& enemies, std::vector<Pickup>& pickups) {
    lastPlayerPos = playerPos;
    time += deltaTime;
    fireTimer += deltaTime;
    jingle = std::max(0.0f, jingle - deltaTime * 2.0f);

    if (fireTimer >= cooldown() && anyEnemyWithin(playerPos, radius(), enemies)) {
        int count = stats.projectiles + mods.extraProjectiles;
        for (int i = 0; i < count; i++) rings.push_back({ 10.0f, i * 0.22f, {} });
        fireTimer = 0.0f;
    }

    for (size_t i = 0; i < rings.size(); ) {
        SoundRing& r = rings[i];
        if (r.delay > 0.0f) {
            r.delay -= deltaTime;
            if (r.delay <= 0.0f) { jingle = 1.0f; PlaySfxPitch(Sfx::UI_SELECT, 1.8f + GetRandomValue(0, 20) / 100.0f); }
            i++;
            continue;
        }
        r.radius += projSpeed() * deltaTime;
        for (size_t j = 0; j < enemies.size(); ) {
            Enemy* e = enemies[j].get();
            float d = Vector2Distance(playerPos, e->position);
            if (containsId(r.hitIds, e->id) || fabsf(d - r.radius) > 14.0f + e->hitRadius) { j++; continue; }
            r.hitIds.push_back(e->id);
            e->knockBack(Vector2Subtract(e->position, playerPos), stats.effect);
            if (!hitEnemy(enemies, j, scaledDamage(), GOLD, pickups)) j++;
        }
        if (r.radius >= radius()) { rings[i] = rings.back(); rings.pop_back(); }
        else i++;
    }
}

void BellWeapon::draw() const {
    for (const SoundRing& r : rings) {
        if (r.delay > 0.0f) continue;
        float a = 1.0f - r.radius / radius();
        DrawRing(lastPlayerPos, r.radius - 4.0f, r.radius + 4.0f, 0, 360, 64, Fade(Color{ 255, 220, 120, 255 }, 0.35f * a));
    }
}

void BellWeapon::draw3D() const {
    // To små gullbjeller som svever ved skuldrene og svinger når de ringer
    for (int k = -1; k <= 1; k += 2) {
        float sway = sinf(time * 14.0f + k) * 0.5f * jingle + sinf(time * 2.0f + k) * 0.1f;
        Vector3 top = ToWorld3D({ lastPlayerPos.x + k * 20.0f, lastPlayerPos.y - 6.0f }, 58.0f + sinf(time * 3.0f + k) * 2.0f);
        Vector3 mouth = { top.x + sway * 6.0f, top.y - 9.0f, top.z };
        Color gold = hasScepter ? Color{ 255, 150, 220, 255 } : Color{ 245, 200, 70, 255 };
        ShadedCylinder(top, mouth, 2.0f, 6.0f, gold, 8);
        ShadedSphere(top, 2.4f, gold, 4, 6);
        ShadedSphere({ mouth.x, mouth.y - 1.5f, mouth.z }, 1.6f, Color{ 120, 80, 30, 255 }, 3, 5);
    }
}

void BellWeapon::drawVfx() const {
    for (const SoundRing& r : rings) {
        if (r.delay > 0.0f) continue;
        float a = 1.0f - r.radius / radius();
        unsigned char c = (unsigned char)(200 * a);
        VfxDecal(VfxTex::SHOCKWAVE, lastPlayerPos, r.radius * 2.3f, Color{ c, (unsigned char)(c * 0.85f), (unsigned char)(c * 0.4f), 255 }, time * 40.0f, 2.0f);
    }
    if (jingle > 0.0f) {
        for (int k = -1; k <= 1; k += 2)
            VfxBillboard(VfxTex::GLOW, ToWorld3D({ lastPlayerPos.x + k * 20.0f, lastPlayerPos.y - 6.0f }, 54.0f), 30.0f * jingle, Color{ 255, 220, 120, 255 });
    }
}

// =====================================================================
// Rampelys (SPOTLIGHT)
//  speed    = grader per sekund
//  radius   = strålens lengde
//  area     = strålens halve bredde
//  cooldown = tid før samme fiende kan treffes igjen
// =====================================================================
Vector2 SpotlightWeapon::beamDirection(int index) const {
    float a = (angle + 360.0f / std::max(1, beamCount) * index) * DEG2RAD;
    return { cosf(a), sinf(a) };
}

void SpotlightWeapon::tick(float deltaTime, Vector2 playerPos, std::vector<std::unique_ptr<Enemy>>& enemies, std::vector<Pickup>& pickups) {
    lastPlayerPos = playerPos;
    time += deltaTime;
    angle = fmodf(angle + stats.speed * deltaTime, 360.0f);
    beamCount = stats.projectiles + mods.extraProjectiles;
    int dmg = scaledDamage();
    Color light = hasScepter ? Color{ 255, 160, 240, 255 } : Color{ 255, 240, 170, 255 };

    for (size_t j = 0; j < enemies.size(); ) {
        Enemy* e = enemies[j].get();
        Vector2 to = Vector2Subtract(e->position, playerPos);
        bool inBeam = false;
        for (int b = 0; b < beamCount && !inBeam; b++) {
            Vector2 d = beamDirection(b);
            float along = Vector2DotProduct(to, d);
            if (along < 0.0f || along > radius()) continue;
            float across = fabsf(to.x * d.y - to.y * d.x);
            inBeam = across <= area() * (0.6f + 0.4f * along / radius()) + e->hitRadius * 0.5f; // Bredere ytterst
        }
        if (!inBeam) { j++; continue; }
        auto it = lastHitTime.find(e->id);
        if (it != lastHitTime.end() && time - it->second < cooldown()) { j++; continue; }
        lastHitTime[e->id] = time;
        if (!hitEnemy(enemies, j, dmg, light, pickups)) j++;
    }

    if (lastHitTime.size() > 256) {
        for (auto it = lastHitTime.begin(); it != lastHitTime.end(); ) {
            if (time - it->second >= cooldown()) it = lastHitTime.erase(it);
            else ++it;
        }
    }
}

void SpotlightWeapon::draw() const {
    // Lyskjegla på gulvet
    for (int b = 0; b < beamCount; b++) {
        Vector2 d = beamDirection(b), n = { -d.y, d.x };
        Vector2 near0 = Vector2Add(lastPlayerPos, Vector2Scale(n, area() * 0.4f));
        Vector2 near1 = Vector2Subtract(lastPlayerPos, Vector2Scale(n, area() * 0.4f));
        Vector2 end = Vector2Add(lastPlayerPos, Vector2Scale(d, radius()));
        Vector2 far0 = Vector2Add(end, Vector2Scale(n, area()));
        Vector2 far1 = Vector2Subtract(end, Vector2Scale(n, area()));
        Color c = Fade(hasScepter ? Color{ 255, 170, 240, 255 } : Color{ 255, 245, 190, 255 }, 0.35f);
        groundTri(near0, far0, far1, c);
        groundTri(near0, far1, near1, c);
        DrawCircleV(end, area(), c);
    }
}

void SpotlightWeapon::drawVfx() const {
    Color tint = hasScepter ? Color{ 200, 110, 190, 255 } : Color{ 190, 170, 110, 255 };
    for (int b = 0; b < beamCount; b++) {
        Vector2 d = beamDirection(b);
        Vector2 end = Vector2Add(lastPlayerPos, Vector2Scale(d, radius()));
        // Stråle fra en lampe over klovnen og ned mot gulvet
        Vector3 lamp = ToWorld3D(Vector2Add(lastPlayerPos, Vector2Scale(d, 10.0f)), 70.0f);
        VfxBeam(VfxTex::GLOW, lamp, ToWorld3D(end, 2.0f), area() * 2.2f, Color{ (unsigned char)(tint.r / 2), (unsigned char)(tint.g / 2), (unsigned char)(tint.b / 2), 255 });
        VfxBillboard(VfxTex::GLOW, lamp, 30.0f, WHITE);
        VfxDecal(VfxTex::GLOW, end, area() * 3.2f, tint);
        VfxBillboard(VfxTex::SPARK, ToWorld3D(end, 6.0f), area() * 1.6f, Fade(WHITE, 0.8f), time * 120.0f);
    }
}

// =====================================================================
// Sabelhugg (SABRE)
//  radius = rekkevidde
//  area   = hvor bred buen er (grader)
//  effect = liv tilbake per treff (Blodsabel)
//  Hugg nummer 2, 3 ... går bakover og til sidene, med litt forsinkelse
// =====================================================================
void SabreWeapon::tick(float deltaTime, Vector2 playerPos, std::vector<std::unique_ptr<Enemy>>& enemies, std::vector<Pickup>& pickups) {
    lastPlayerPos = playerPos;
    fireTimer += deltaTime;

    if (queued.empty() && fireTimer >= cooldown() && anyEnemyWithin(playerPos, radius() * 1.3f, enemies)) {
        Vector2 dir = Vector2Normalize(Vector2Subtract(nearestEnemies(playerPos, enemies, 1)[0]->position, playerPos));
        int count = stats.projectiles + mods.extraProjectiles;
        const float offsets[6] = { 0.0f, 180.0f, 90.0f, -90.0f, 45.0f, -135.0f };
        for (int i = 0; i < count; i++) queued.push_back({ rotateDegrees(dir, offsets[i % 6] + (i / 6) * 22.0f), i * 0.13f });
        fireTimer = 0.0f;
    }

    Color steel = hasScepter ? Color{ 255, 70, 90, 255 } : Color{ 255, 150, 150, 255 };
    for (size_t i = 0; i < queued.size(); ) {
        queued[i].delay -= deltaTime;
        if (queued[i].delay > 0.0f) { i++; continue; }
        Vector2 d = queued[i].direction;
        float cosLimit = cosf(area() * 0.5f * DEG2RAD);
        int hits = 0;
        for (size_t j = 0; j < enemies.size(); ) {
            Enemy* e = enemies[j].get();
            Vector2 to = Vector2Subtract(e->position, playerPos);
            float dist = Vector2Length(to);
            bool inside = dist <= radius() + e->hitRadius && (dist < 20.0f || Vector2DotProduct(Vector2Scale(to, 1.0f / dist), d) >= cosLimit);
            if (!inside) { j++; continue; }
            hits++;
            e->knockBack(to, 90.0f);
            if (!hitEnemy(enemies, j, scaledDamage(), steel, pickups)) j++;
        }
        if (stats.effect > 0.0f) Weapon::pendingHeal += std::min(8, hits) * stats.effect;

        // Hugget: stor bue foran klovnen som tones ut
        Vector2 at = Vector2Add(playerPos, Vector2Scale(d, radius() * 0.5f));
        VfxParticle(VfxTex::SLASH, ToWorld3D(at, 12.0f), { 0, 0, 0 }, 0.22f, radius() * 1.7f, radius() * 1.95f,
                    steel, 0.0f, 0.0f, headingDeg(d) + SLASH_ROTATION_OFFSET + 90.0f, 0.0f, true);
        VfxParticle(VfxTex::GLOW, ToWorld3D(at, 10.0f), { 0, 0, 0 }, 0.15f, radius() * 1.2f, radius() * 0.6f,
                    hasScepter ? Color{ 160, 20, 40, 255 } : Color{ 120, 90, 90, 255 }, 0.0f, 0.0f, 0.0f, 0.0f, true);
        PlaySfxPitch(Sfx::HIT, 0.7f);
        queued[i] = queued.back();
        queued.pop_back();
    }
}

void SabreWeapon::draw() const {}

// =====================================================================
// Virvelvind (TORNADO)
//  speed    = hvor fort virvlene vandrer
//  radius   = virvelens størrelse
//  duration = hvor lenge de lever
//  effect   = hvor hardt de suger fiender inn
// =====================================================================
namespace { constexpr float TWISTER_TICK = 0.25f; }

void TornadoWeapon::tick(float deltaTime, Vector2 playerPos, std::vector<std::unique_ptr<Enemy>>& enemies, std::vector<Pickup>& pickups) {
    fireTimer += deltaTime;
    if (fireTimer >= cooldown() && !enemies.empty()) {
        int count = stats.projectiles + mods.extraProjectiles;
        for (int i = 0; i < count; i++) {
            float a = frandf(0, 2.0f * PI);
            float life = stats.duration * mods.durationMult;
            twisters.push_back({ { playerPos.x + cosf(a) * 40.0f, playerPos.y + sinf(a) * 40.0f }, { cosf(a), sinf(a) }, life, life, 0.0f, 0.0f });
        }
        fireTimer = 0.0f;
    }

    Color wind = hasScepter ? Color{ 170, 120, 255, 255 } : Color{ 190, 240, 220, 255 };
    for (size_t i = 0; i < twisters.size(); ) {
        Twister& t = twisters[i];
        t.life -= deltaTime;
        t.spin += deltaTime * 720.0f;
        if (t.life <= 0.0f) { twisters[i] = twisters.back(); twisters.pop_back(); continue; }

        // Vandrer mot nærmeste fiende, men med litt tilfeldig slingring. Holder seg nær spilleren.
        Enemy* target = nearestUnhitEnemy(t.position, 320.0f, {}, enemies);
        Vector2 want = target ? Vector2Normalize(Vector2Subtract(target->position, t.position)) : t.velocity;
        if (Vector2Distance(t.position, playerPos) > 420.0f) want = Vector2Normalize(Vector2Subtract(playerPos, t.position));
        want = rotateDegrees(want, sinf(t.spin * 0.01f) * 50.0f);
        t.velocity = Vector2Normalize(Vector2Lerp(t.velocity, want, std::min(1.0f, 1.8f * deltaTime)));
        t.position = Vector2Add(t.position, Vector2Scale(t.velocity, projSpeed() * deltaTime));

        // Suger fiender inn mot midten
        float pullRange = radius() * 2.0f;
        for (auto& e : enemies) {
            Vector2 to = Vector2Subtract(t.position, e->position);
            float d = Vector2Length(to);
            if (d > pullRange || d < 4.0f) continue;
            float k = stats.effect * e->knockbackScale * (1.0f - d / pullRange) * deltaTime;
            e->position = Vector2Add(e->position, Vector2Scale(to, k / d));
        }

        t.tickTimer += deltaTime;
        if (t.tickTimer >= TWISTER_TICK) {
            t.tickTimer -= TWISTER_TICK;
            damageEnemiesInRadius(t.position, radius(), scaledDamage(), wind, false, enemies, pickups);
        }

        // Støv som virvles opp rundt foten
        float a = t.spin * DEG2RAD;
        VfxParticle(VfxTex::GLOW, ToWorld3D({ t.position.x + cosf(a) * radius() * 0.7f, t.position.y + sinf(a) * radius() * 0.7f }, 4.0f),
                    { -sinf(a) * 120.0f, 90.0f, cosf(a) * 120.0f }, 0.5f, 7.0f, 3.0f, wind, -30.0f, 1.0f);
        i++;
    }
}

void TornadoWeapon::draw() const {
    for (const Twister& t : twisters) {
        float fade = std::min(1.0f, t.life / 0.4f) * std::min(1.0f, (t.maxLife - t.life) / 0.3f);
        DrawCircleV(t.position, radius(), Fade(BLACK, 0.18f * fade));
        DrawRing(t.position, radius() * 0.9f, radius(), t.spin, t.spin + 240.0f, 24, Fade(hasScepter ? VIOLET : Color{ 200, 240, 230, 255 }, 0.35f * fade));
    }
}

void TornadoWeapon::drawVfx() const {
    for (const Twister& t : twisters) {
        float fade = std::min(1.0f, t.life / 0.4f) * std::min(1.0f, (t.maxLife - t.life) / 0.3f);
        // Traktform: ringer som blir større og slingrer oppover
        const int layers = 7;
        for (int k = 0; k < layers; k++) {
            float h = 4.0f + k * 13.0f;
            float size = radius() * (0.7f + k * 0.28f) * 2.0f;
            Vector2 wob = { t.position.x + sinf(t.spin * 0.02f + k * 0.7f) * k * 3.0f, t.position.y + cosf(t.spin * 0.017f + k) * k * 2.0f };
            // Lagene overlapper additivt, så hvert lag må være svakt (ellers blir midten helt hvit)
            unsigned char v = (unsigned char)(80 * fade * (1.0f - k * 0.1f));
            Color c = hasScepter ? Color{ (unsigned char)(v * 0.75f), (unsigned char)(v * 0.35f), v, 255 } : Color{ (unsigned char)(v * 0.95f), v, v, 255 };
            // Tåke-teksturen er grønn, så vi tar ned grønt for at vinden skal bli hvit/grå
            if (!hasScepter) c.g = (unsigned char)(c.g * 0.55f);
            VfxDecal(VfxTex::POISON_MIST, wob, size, c, t.spin * (1.0f + k * 0.1f), h);
            if (k == 2 || k == 5) VfxDecal(VfxTex::SLASH, wob, size * 0.8f, Color{ (unsigned char)(v * 0.6f), (unsigned char)(v * 0.6f), (unsigned char)(v * 0.6f), 255 }, -t.spin * 1.3f, h + 2.0f);
        }
        VfxBillboard(VfxTex::GLOW, ToWorld3D(t.position, 30.0f), radius() * 2.2f, Color{ (unsigned char)(30 * fade), (unsigned char)(40 * fade), (unsigned char)(40 * fade), 255 });
    }
}
