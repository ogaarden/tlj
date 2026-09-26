#include "enemy.hpp"
#include "vfx.hpp"
#include "explosions.hpp"
#include "castle.hpp"
#include "render3d.hpp"

// =====================================================================
// KONGENS TJENERE (fra 2-4 min): hunder, prest, trommeslager og kanonér.
// De har bare tegnede figurer (ingen egne 3D-modeller), og bruker den enkle
// standardmodellen hvis man har slått av figurene i innstillingene.
// Auraene (helbredelse og fart) brukes på fiendene rundt i spill-løkka.
// =====================================================================

namespace {
    Vector2 dirTo(Vector2 from, Vector2 to, float* dist = nullptr) {
        Vector2 d = Vector2Subtract(to, from);
        float len = Vector2Length(d);
        if (dist) *dist = len;
        return len > 0.01f ? Vector2Scale(d, 1.0f / len) : Vector2{ 0, 1 };
    }

    // Går mot `range` fra spilleren: nærmere hvis for langt unna, rygger hvis for nær
    void keepDistance(Enemy& e, Vector2 playerPos, float range, float dt) {
        float dist;
        Vector2 dir = dirTo(e.position, playerPos, &dist);
        e.facing = dir;
        if (dist > range + 40.0f) e.position = Vector2Add(e.position, Vector2Scale(dir, e.speed * dt));
        else if (dist < range - 60.0f) e.position = Vector2Subtract(e.position, Vector2Scale(dir, e.speed * 0.75f * dt));
    }

    constexpr float HOUND_CROUCH = 0.4f;   // Varsel før hoppet
    constexpr float HOUND_LUNGE = 0.45f;
    constexpr float CANNON_WINDUP = 0.6f;
    constexpr float CANNON_RANGE = 470.0f;
}

// --- Hund ---
Hound::Hound(Vector2 spawnPos, Texture2D tex) {
    position = spawnPos;
    speed = 235.0f;
    hp = maxHp = 45;
    damage = 9;
    xpValue = 9;
    orbColor = Color{ 90, 90, 100, 255 };
    orbRadius = 5.0f;
    goldChance = 0.012f;
    goldValue = 1;
    hitRadius = 15.0f;
    texture = tex;
    lungeCooldown = 0.8f + (id % 7) * 0.2f; // Flokken hopper ikke samtidig
}

void Hound::update(Vector2 playerPosition) {
    float dt = GetFrameTime();
    float dist;
    Vector2 dir = dirTo(position, playerPosition, &dist);
    timer -= dt;
    switch (state) {
        case State::RUN:
            facing = dir;
            position = Vector2Add(position, Vector2Scale(dir, speed * dt));
            lungeCooldown -= dt;
            if (lungeCooldown <= 0.0f && dist < 260.0f) { state = State::CROUCH; timer = HOUND_CROUCH; lungeDir = dir; }
            break;
        case State::CROUCH:
            facing = lungeDir;
            if (timer <= 0.0f) { state = State::LUNGE; timer = HOUND_LUNGE; }
            break;
        case State::LUNGE:
            position = Vector2Add(position, Vector2Scale(lungeDir, speed * 2.3f * dt));
            if (timer <= 0.0f) { state = State::RUN; lungeCooldown = 2.4f; }
            break;
    }
}

void Hound::draw() const {
    // Varsel-pil på gulvet mens den kryper sammen
    if (state == State::CROUCH) {
        float t = 1.0f - timer / HOUND_CROUCH;
        Vector2 end = Vector2Add(position, Vector2Scale(lungeDir, speed * 2.3f * HOUND_LUNGE));
        DrawLineEx(position, end, 2.0f + 5.0f * t, Fade(RED, 0.2f + 0.4f * t));
    }
    Enemy::draw();
}

Color Hound::spriteTint() const {
    return state == State::CROUCH ? Color{ 255, 170, 160, 255 } : WHITE;
}

// --- Hoffprest ---
Priest::Priest(Vector2 spawnPos, Texture2D tex) {
    position = spawnPos;
    speed = 95.0f;
    hp = maxHp = 150;
    damage = 8;
    xpValue = 32;
    orbColor = Color{ 235, 225, 200, 255 };
    orbRadius = 6.0f;
    goldChance = 0.05f;
    goldValue = 2;
    texture = tex;
    pulseTimer = 1.0f + (id % 5) * 0.2f;
}

void Priest::update(Vector2 playerPosition) {
    float dt = GetFrameTime();
    keepDistance(*this, playerPosition, 300.0f, dt);
    pulseTimer -= dt;
    if (pulseTimer <= 0.0f) { pulseTimer = 1.6f; auraPulse = true; }
}

void Priest::drawVfx() const {
    Enemy::drawVfx();
    // Gyllen ring på gulvet som viser hvor langt helbredelsen når, og glød i røkelseskaret
    float t = (float)GetTime();
    float beat = 1.0f - fmodf(t * 0.625f + id * 0.37f, 1.0f);
    VfxDecal(VfxTex::SHOCKWAVE, position, auraRadius() * 2.0f * (1.05f - 0.1f * beat), Color{ 70, 60, 28, 255 }, t * 20.0f, 1.0f);
    VfxDecal(VfxTex::GLOW, position, auraRadius() * 1.2f, Color{ 22, 20, 8, 255 }, 0.0f, 0.8f);
    VfxBillboard(VfxTex::GLOW, ToWorld3D(Vector2Add(position, Vector2Scale(facing, 22.0f)), 45.0f), 40.0f, Color{ 255, 210, 110, 255 });
}

// --- Trommeslager ---
Drummer::Drummer(Vector2 spawnPos, Texture2D tex) {
    position = spawnPos;
    speed = 115.0f;
    hp = maxHp = 130;
    damage = 10;
    xpValue = 28;
    orbColor = Color{ 60, 80, 170, 255 };
    orbRadius = 6.0f;
    goldChance = 0.05f;
    goldValue = 2;
    texture = tex;
}

void Drummer::update(Vector2 playerPosition) {
    float dt = GetFrameTime();
    keepDistance(*this, playerPosition, 150.0f, dt);
}

void Drummer::drawVfx() const {
    Enemy::drawVfx();
    // Lydringer i takt med trommen (to slag i sekundet)
    float t = (float)GetTime() * 2.0f + id * 0.21f;
    float k = t - floorf(t);
    VfxDecal(VfxTex::SHOCKWAVE, position, auraRadius() * 2.0f * k, Color{ (unsigned char)(40 * (1.0f - k)), (unsigned char)(55 * (1.0f - k)), (unsigned char)(130 * (1.0f - k)), 255 }, 0.0f, 1.0f);
}

// --- Kanonér ---
Cannoneer::Cannoneer(Vector2 spawnPos, Texture2D tex) {
    position = spawnPos;
    speed = 85.0f;
    hp = maxHp = 120;
    damage = 30;          // Granaten gjør full skade, berøring en tredjedel
    xpValue = 30;
    orbColor = Color{ 150, 100, 60, 255 };
    orbRadius = 6.0f;
    goldChance = 0.05f;
    goldValue = 2;
    texture = tex;
    shootTimer = 1.5f + (id % 6) * 0.35f;
}

void Cannoneer::update(Vector2 playerPosition) {
    float dt = GetFrameTime();
    if (windup > 0.0f) {
        // Står stille og sikter, så fyrer den
        facing = dirTo(position, playerPosition);
        windup -= dt;
        if (windup <= 0.0f) {
            Vector2 target = { playerPosition.x + (float)GetRandomValue(-25, 25), playerPosition.y + (float)GetRandomValue(-25, 25) };
            SpawnMortarShell(Vector2Add(position, Vector2Scale(facing, 10.0f)), target, 1.15f, 80.0f, (float)damage);
            VfxMuzzle(position, facing, Color{ 255, 170, 80, 255 });
            AddCameraShake(0.1f);
            shootTimer = 3.4f;
        }
        return;
    }
    keepDistance(*this, playerPosition, CANNON_RANGE, dt);
    shootTimer -= dt;
    if (shootTimer <= 0.0f && Vector2Distance(position, playerPosition) < CANNON_RANGE + 150.0f) windup = CANNON_WINDUP;
}

void Cannoneer::drawVfx() const {
    Enemy::drawVfx();
    // Lunta gløder, og blusser opp rett før skuddet
    float t = (float)GetTime();
    float k = windup > 0.0f ? 1.0f - windup / CANNON_WINDUP : 0.0f;
    Vector3 fuse = ToWorld3D(Vector2Add(position, Vector2Scale(facing, -14.0f)), 62.0f);
    VfxBillboard(VfxTex::SPARK, fuse, 14.0f + 30.0f * k, Color{ 255, 190, 90, 255 }, t * 400.0f);
    VfxBillboard(VfxTex::GLOW, fuse, 18.0f + 40.0f * k, Color{ 255, 120, 40, 255 });
}
