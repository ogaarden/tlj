#include "miniboss.hpp"
#include "explosions.hpp"
#include "render3d.hpp"
#include "castle.hpp"
#include "audio.hpp"
#include "vfx.hpp"
#include <cmath>
#include <algorithm>

namespace {

constexpr float SLAM_WINDUP = 1.1f;
constexpr float SLAM_RADIUS = 170.0f;
constexpr float KNIGHT_WINDUP = 0.8f;
constexpr float KNIGHT_DASH = 0.55f;
constexpr float KNIGHT_DASH_SPEED = 720.0f;

// Felles oppsett: stor, tåler mye, dyttes nesten ikke og gir mye XP
void setupMiniboss(Enemy& e, const char* title, int index, float hpMult, float damageMult, int baseDamage) {
    const float hpByIndex[3] = { 26000.0f, 60000.0f, 110000.0f }; // Ca. 40-60 sek med et normalt build
    e.miniboss = true;
    e.title = title;
    e.hp = e.maxHp = (int)(hpByIndex[index < 3 ? index : 2] * hpMult);
    e.damage = (int)(baseDamage * (1.0f + 0.3f * index) * damageMult);
    e.xpValue = 400 + 300 * index;
    e.modelScale = 2.9f; // Tydelig større enn elites
    e.hitRadius *= 2.1f;
    e.knockbackScale = 0.08f;
    e.goldChance = 0.0f;
}

// Lilla aura under alle minibosser
void minibossAura(const Enemy& e) {
    float t = (float)GetTime();
    float pulse = 0.75f + 0.25f * sinf(t * 4.0f);
    VfxDecal(VfxTex::GLOW, e.position, 150.0f, Color{ (unsigned char)(150 * pulse), (unsigned char)(40 * pulse), (unsigned char)(200 * pulse), 255 }, 0.0f, 1.0f);
    VfxDecal(VfxTex::SHOCKWAVE, e.position, 120.0f, Color{ 150, 60, 200, 255 }, t * 70.0f, 1.2f);
    VfxDecal(VfxTex::SHOCKWAVE, e.position, 90.0f, Color{ 90, 30, 140, 255 }, -t * 110.0f, 1.3f);
}

} // namespace

std::unique_ptr<Enemy> CreateMiniboss(MinibossKind kind, int index, Vector2 pos, Texture2D tex, float hpMult, float damageMult) {
    std::unique_ptr<Enemy> e;
    switch (kind) {
        case MinibossKind::EXECUTIONER:
            e = std::make_unique<Executioner>(pos, tex);
            setupMiniboss(*e, "BOEDELEN", index, hpMult, damageMult, 40);
            e->speed = 95.0f;
            break;
        case MinibossKind::MAGUS:
            e = std::make_unique<Magus>(pos, tex);
            setupMiniboss(*e, "HOFFMAGIKEREN", index, hpMult * 0.8f, damageMult, 22);
            e->speed = 120.0f;
            break;
        default:
            e = std::make_unique<IronKnight>(pos, tex);
            setupMiniboss(*e, "JERNRIDDEREN", index, hpMult, damageMult, 35);
            e->speed = 130.0f;
            break;
    }
    return e;
}

// ---------------------------------------------------------------------
// Bøddelen: går mot deg og løfter øksa. Rød sirkel viser hvor den treffer.
// ---------------------------------------------------------------------
void Executioner::update(Vector2 playerPos) {
    float dt = GameDt();
    Vector2 toPlayer = Vector2Subtract(playerPos, position);
    float dist = Vector2Length(toPlayer);
    timer -= dt;

    switch (phase) {
        case Phase::CHASE:
            if (dist > 1.0f) {
                facing = Vector2Scale(toPlayer, 1.0f / dist);
                position = Vector2Add(position, Vector2Scale(facing, speed * dt));
            }
            if (timer <= 0.0f && dist < SLAM_RADIUS * 1.6f) { phase = Phase::WINDUP; timer = SLAM_WINDUP; PlaySfxPitch(Sfx::BOSS_CHARGE, 1.3f); }
            break;
        case Phase::WINDUP:
            if (timer <= 0.0f) {
                SpawnExplosion(position, SLAM_RADIUS, damage * 1.6f);
                VfxShockwave(position, SLAM_RADIUS, Color{ 255, 60, 40, 255 });
                AddCameraShake(0.6f);
                PlaySfx(Sfx::EXPLOSION);
                phase = Phase::RECOVER;
                timer = 0.7f;
            }
            break;
        case Phase::RECOVER:
            if (timer <= 0.0f) { phase = Phase::CHASE; timer = 3.2f; }
            break;
    }
}

void Executioner::draw() const {
    if (phase == Phase::WINDUP) {
        float t = 1.0f - timer / SLAM_WINDUP;
        DrawCircleV(position, SLAM_RADIUS, Fade(RED, 0.10f + 0.15f * t));
        DrawCircleV(position, SLAM_RADIUS * t, Fade(RED, 0.25f));
        DrawRing(position, SLAM_RADIUS - 4.0f, SLAM_RADIUS, 0, 360, 48, Fade(RED, 0.8f));
    }
    Enemy::draw();
}

void Executioner::drawVfx() const {
    minibossAura(*this);
    if (phase == Phase::WINDUP) {
        float t = 1.0f - timer / SLAM_WINDUP;
        VfxBillboard(VfxTex::GLOW, ToWorld3D(position, 90.0f + 30.0f * t), 60.0f + 80.0f * t, Color{ 255, 60, 40, 255 });
    }
}

// ---------------------------------------------------------------------
// Hoffmagikeren: holder avstand, skyter ringer av magiske kuler og teleporterer
// ---------------------------------------------------------------------
void Magus::update(Vector2 playerPos) {
    float dt = GameDt();
    Vector2 toPlayer = Vector2Subtract(playerPos, position);
    float dist = Vector2Length(toPlayer);
    Vector2 dir = dist > 0.01f ? Vector2Scale(toPlayer, 1.0f / dist) : Vector2{ 0, 1 };
    facing = dir;

    if (dist > 380.0f) position = Vector2Add(position, Vector2Scale(dir, speed * dt));
    else if (dist < 260.0f) position = Vector2Subtract(position, Vector2Scale(dir, speed * 0.8f * dt));

    volleyTimer -= dt;
    if (volleyTimer <= 0.0f && dist < 650.0f) {
        const int shots = 16;
        for (int i = 0; i < shots; i++) {
            float a = volleyOffset + (float)i / shots * 2.0f * PI;
            SpawnEnemyShot(position, { cosf(a), sinf(a) }, 220.0f, (float)damage);
        }
        volleyOffset += PI / shots;
        VfxShockwave(position, 90.0f, Color{ 180, 80, 255, 255 });
        PlaySfxPitch(Sfx::ZAP, 0.7f);
        volleyTimer = 2.4f;
    }

    teleportTimer -= dt;
    if (teleportTimer <= 0.0f) {
        VfxDeath(position, Color{ 180, 80, 255, 255 });
        float a = GetRandomValue(0, 628) / 100.0f;
        position = { playerPos.x + cosf(a) * 330.0f, playerPos.y + sinf(a) * 330.0f };
        VfxDeath(position, Color{ 180, 80, 255, 255 });
        VfxShockwave(position, 120.0f, Color{ 180, 80, 255, 255 });
        teleportTimer = 7.0f;
    }
}

void Magus::draw() const {
    // Liten advarsel på gulvet rett før neste salve
    if (volleyTimer < 0.6f) DrawRing(position, 40.0f, 46.0f + (0.6f - volleyTimer) * 80.0f, 0, 360, 32, Fade(VIOLET, 0.5f));
    Enemy::draw();
}

void Magus::drawVfx() const {
    minibossAura(*this);
    float t = (float)GetTime();
    for (int i = 0; i < 3; i++) {
        float a = t * 2.0f + i * 2.1f;
        Vector2 p = { position.x + cosf(a) * 40.0f, position.y + sinf(a) * 40.0f };
        VfxBillboard(VfxTex::MAGIC_ORB, ToWorld3D(p, 70.0f), 30.0f, WHITE, t * 200.0f);
    }
}

// ---------------------------------------------------------------------
// Jernridderen: sikter (rød linje), og stormer så i full fart gjennom deg
// ---------------------------------------------------------------------
void IronKnight::update(Vector2 playerPos) {
    float dt = GameDt();
    Vector2 toPlayer = Vector2Subtract(playerPos, position);
    float dist = Vector2Length(toPlayer);
    Vector2 dir = dist > 0.01f ? Vector2Scale(toPlayer, 1.0f / dist) : Vector2{ 0, 1 };
    timer -= dt;

    switch (phase) {
        case Phase::CHASE:
            facing = dir;
            position = Vector2Add(position, Vector2Scale(dir, speed * dt));
            if (timer <= 0.0f && dist < 520.0f) { phase = Phase::WINDUP; timer = KNIGHT_WINDUP; dashDir = dir; PlaySfxPitch(Sfx::BOSS_CHARGE, 1.1f); }
            break;
        case Phase::WINDUP:
            // Sikter litt etter spilleren helt til rett før dashet
            if (timer > 0.25f) dashDir = Vector2Normalize(Vector2Lerp(dashDir, dir, std::min(1.0f, 4.0f * dt)));
            facing = dashDir;
            if (timer <= 0.0f) { phase = Phase::DASH; timer = KNIGHT_DASH; }
            break;
        case Phase::DASH:
            position = Vector2Add(position, Vector2Scale(dashDir, KNIGHT_DASH_SPEED * dt));
            VfxTrail(ToWorld3D(position, 30.0f), Color{ 200, 200, 255, 255 }, 50.0f, 0.3f);
            if (timer <= 0.0f) { phase = Phase::REST; timer = 0.8f; }
            break;
        case Phase::REST:
            if (timer <= 0.0f) { phase = Phase::CHASE; timer = 2.6f; }
            break;
    }
}

void IronKnight::draw() const {
    if (phase == Phase::WINDUP) {
        float t = 1.0f - timer / KNIGHT_WINDUP;
        Vector2 end = Vector2Add(position, Vector2Scale(dashDir, KNIGHT_DASH_SPEED * KNIGHT_DASH));
        DrawLineEx(position, end, 20.0f + 30.0f * t, Fade(RED, 0.2f + 0.4f * t));
    }
    Enemy::draw();
}

void IronKnight::drawVfx() const {
    minibossAura(*this);
    if (phase == Phase::DASH) VfxBillboard(VfxTex::GLOW, ToWorld3D(position, 40.0f), 120.0f, Color{ 160, 160, 255, 255 });
}
