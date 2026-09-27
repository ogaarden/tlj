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
    hp = maxHp = 120;
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
    float dt = GameDt();
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
    hp = maxHp = 480;
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
    float dt = GameDt();
    keepDistance(*this, playerPosition, 300.0f, dt);
    pulseTimer -= dt;
    if (pulseTimer <= 0.0f) { pulseTimer = 1.6f; auraPulse = true; }
}

void Priest::drawVfx() const {
    Enemy::drawVfx();
    // Gyllen ring på gulvet som viser hvor langt helbredelsen når, og glød i røkelseskaret
    float t = (float)GetTime();
    float beat = 1.0f - fmodf(t * 0.625f + id * 0.37f, 1.0f);
    VfxDecal(VfxTex::SHOCKWAVE, position, auraRadius() * 2.0f * (1.05f - 0.1f * beat), Color{ 42, 36, 16, 255 }, t * 20.0f, 1.0f);
    VfxDecal(VfxTex::GLOW, position, auraRadius() * 1.2f, Color{ 22, 20, 8, 255 }, 0.0f, 0.8f);
    // Gløden i røkelseskaret (lavere på 3D-modellen, der karet henger i kjedet)
    float censerHeight = HasSprite(spriteId()) ? 45.0f : 13.0f;
    VfxBillboard(VfxTex::GLOW, ToWorld3D(Vector2Add(position, Vector2Scale(facing, HasSprite(spriteId()) ? 22.0f : 11.0f)), censerHeight), 40.0f, Color{ 255, 210, 110, 255 });
}

// --- Trommeslager ---
Drummer::Drummer(Vector2 spawnPos, Texture2D tex) {
    position = spawnPos;
    speed = 115.0f;
    hp = maxHp = 400;
    damage = 10;
    xpValue = 28;
    orbColor = Color{ 60, 80, 170, 255 };
    orbRadius = 6.0f;
    goldChance = 0.05f;
    goldValue = 2;
    texture = tex;
}

void Drummer::update(Vector2 playerPosition) {
    float dt = GameDt();
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
    hp = maxHp = 360;
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
    float dt = GameDt();
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
    Vector3 fuse = HasSprite(spriteId())
        ? ToWorld3D(Vector2Add(position, Vector2Scale(facing, -14.0f)), 62.0f)
        : ToWorld3D(Vector2Add(position, Vector2Add(Vector2Scale(facing, 13.0f), Vector2Scale({ facing.y, -facing.x }, 9.0f))), 24.0f);
    VfxBillboard(VfxTex::SPARK, fuse, 14.0f + 30.0f * k, Color{ 255, 190, 90, 255 }, t * 400.0f);
    VfxBillboard(VfxTex::GLOW, fuse, 18.0f + 40.0f * k, Color{ 255, 120, 40, 255 });
}

// --- Skattmester ---
Treasurer::Treasurer(Vector2 spawnPos, Texture2D tex) {
    position = spawnPos;
    speed = 135.0f;
    hp = maxHp = 2400;     // Tåler en del: man må jage ham en stund
    damage = 0;
    xpValue = 120;
    orbColor = GOLD;
    orbRadius = 8.0f;
    goldChance = 0.0f;    // Gullet kommer fra sekken (extraLoot)
    hitRadius = 18.0f;
    knockbackScale = 0.5f;
    ignoresAuras = true;  // Ellers blir han umulig å ta igjen under Kongens fest
    modelScale = 1.3f;    // Litt større, så han er lett å få øye på
    texture = tex;
    wander = (float)GetRandomValue(-100, 100) / 100.0f;
}

void Treasurer::update(Vector2 playerPosition) {
    float dt = GameDt();
    life -= dt;
    if (life <= 0.0f) { escaped = true; hp = 0; return; } // Rømte med gullet!
    if (stumble > 0.0f) { stumble -= dt; return; }

    // Løper bort fra spilleren, i en slak bue så han ikke bare forsvinner rett frem
    Vector2 away = dirTo(playerPosition, position);
    float t = (float)GetTime() * 0.7f + id;
    Vector2 side = { -away.y, away.x };
    Vector2 dir = Vector2Normalize(Vector2Add(away, Vector2Scale(side, 0.5f * sinf(t) + 0.4f * wander)));
    facing = dir;
    position = Vector2Add(position, Vector2Scale(dir, speed * dt));

    // Snubler i den tunge sekken og mister en mynt
    stumbleTimer -= dt;
    if (stumbleTimer <= 0.0f) {
        stumbleTimer = 1.6f + (float)GetRandomValue(0, 100) / 100.0f;
        stumble = 0.55f;
        droppedCoin = true;
    }
}

void Treasurer::drawVfx() const {
    // Gyllen glans rundt ham, og gnister fra sekken
    float t = (float)GetTime();
    float pulse = 0.75f + 0.25f * sinf(t * 6.0f);
    VfxDecal(VfxTex::GLOW, position, 110.0f, Color{ (unsigned char)(110 * pulse), (unsigned char)(85 * pulse), 20, 255 }, 0.0f, 1.0f);
    VfxBillboard(VfxTex::GLOW, ToWorld3D(position, 30.0f), 70.0f, Color{ 90, 70, 15, 255 });
    if (GetRandomValue(0, 5) == 0)
        VfxBillboard(VfxTex::SPARK, ToWorld3D({ position.x + GetRandomValue(-12, 12), position.y + GetRandomValue(-12, 12) }, 30.0f + GetRandomValue(0, 20)), 16.0f, Color{ 255, 220, 120, 255 }, t * 300.0f);
}

void Treasurer::onDeath() {
    if (escaped) {
        // Forsvinner i en sky av gullstøv
        VfxDeath(position, Color{ 255, 210, 90, 255 });
        VfxShockwave(position, 90.0f, Color{ 255, 210, 90, 255 });
    } else {
        VfxExplosion(position, 70.0f);
        VfxShockwave(position, 160.0f, Color{ 255, 215, 80, 255 });
        AddCameraShake(0.4f);
    }
}

void Treasurer::extraLoot(std::vector<Pickup>& pickups) const {
    if (escaped) return;
    // Sekken sprekker: en ring av mynter og en skattekiste
    for (int i = 0; i < 16; i++) {
        float a = i * PI / 8.0f;
        float d = 30.0f + (i % 2) * 22.0f;
        pickups.push_back({ { position.x + cosf(a) * d, position.y + sinf(a) * d }, 2, GOLD, 5.0f, 30.0f, PickupType::COIN });
    }
    pickups.push_back({ { position.x, position.y - 12.0f }, 3, GOLD, 14.0f, 0.0f, PickupType::CHEST }); // Gullkiste (den eneste i spillet)
}
