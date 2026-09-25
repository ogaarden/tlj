#include "enemy.hpp"
#include "damage_numbers.hpp"
#include "explosions.hpp"

// --- Baseklasse ---
void Enemy::draw() const {

    float enemyRadius = 15.0f; // Fast radius for kroppen (eller bruk orbRadius)
    DrawCircleV(position, enemyRadius, orbColor);
    DrawCircleLines(position.x, position.y, enemyRadius, BLACK); // Svart omriss for bedre synlighe

    if (hp < maxHp) {
        float barWidth = 30.0f;
        float barHeight = 4.0f;
        Vector2 barPos = { 
            position.x + (texture.width / 2.0f) - (barWidth / 2.0f), 
            position.y - 8.0f 
        };

        float hpPercent = (float)hp / (float)maxHp;
        DrawRectangleV(barPos, { barWidth, barHeight }, RED);
        DrawRectangleV(barPos, { barWidth * hpPercent, barHeight }, GREEN);
    }
}

void Enemy::takeDamage(int amount, Color numberColor, bool isDamageOverTime) {
    if (amount <= 0) return;
    hp -= amount;

    if (!isDamageOverTime) {
        SpawnDamageNumber(position, amount, numberColor);
        return;
    }

    // DoT: samle opp skaden og vis den som ett tall ca. 4 ganger i sekundet
    pendingDotDamage += amount;
    dotColor = numberColor;
    double now = GetTime();
    if (now - lastDotPopupTime >= 0.25 || isDead()) {
        SpawnDamageNumber(position, pendingDotDamage, dotColor);
        pendingDotDamage = 0;
        lastDotPopupTime = now;
    }
}
bool Enemy::isDead() const { return hp <= 0; }

void Enemy::applyEchelonModifiers(float hpMult, float damageMult, float speedMult) {
    hp = (int)(hp * hpMult);
    maxHp = hp;
    damage = (int)(damage * damageMult);
    speed *= speedMult;
}

void Enemy::dropLoot(std::vector<Pickup>& pickups) const {
    pickups.push_back({ position, xpValue, orbColor, orbRadius, 15.0f, PickupType::XP });

    // Gull er metaprogresjon, så sjansen er lav med vilje
    if (goldChance > 0.0f && GetRandomValue(1, 10000) <= (int)(goldChance * 10000.0f)) {
        Vector2 coinPos = { position.x + (float)GetRandomValue(-8, 8), position.y + (float)GetRandomValue(-8, 8) };
        pickups.push_back({ coinPos, goldValue, GOLD, 5.0f, 30.0f, PickupType::COIN });
    }
}

// --- Footman ---
Footman::Footman(Vector2 spawnPos, Texture2D tex) {
    position = spawnPos;
    speed = 140.0f;
    hp = 200;
    maxHp = 200;
    damage = 10;
    xpValue = 15;
    orbColor = BLUE;
    goldChance = 0.05f;
    goldValue = 1;
    orbRadius = 5.0f;
    texture = tex;
}

void Footman::update(Vector2 playerPosition) {
    Vector2 dir = Vector2Normalize(Vector2Subtract(playerPosition, position));
    position.x += dir.x * speed * GetFrameTime();
    position.y += dir.y * speed * GetFrameTime();
}

// --- Goon ---
Goon::Goon(Vector2 spawnPos, Texture2D tex) {
    position = spawnPos;
    speed = 70.0f;
    hp = 80;
    maxHp = 80;
    damage = 25;
    xpValue = 40;
    orbColor = GREEN;
    goldChance = 0.10f;
    goldValue = 2;
    orbRadius = 10.0f;
    texture = tex;
}

void Goon::update(Vector2 playerPosition) {
    Vector2 dir = Vector2Normalize(Vector2Subtract(playerPosition, position));
    position.x += dir.x * speed * GetFrameTime();
    position.y += dir.y * speed * GetFrameTime();
}

// --- Lackey ---
Lackey::Lackey(Vector2 spawnPos, Texture2D tex) {
    position = spawnPos;
    speed = 180.0f;
    hp = 10;
    maxHp = 10;
    damage = 5;
    xpValue = 8;
    orbColor = YELLOW;
    goldChance = 0.03f;
    goldValue = 1;
    orbRadius = 4.0f;
    texture = tex;
}

void Lackey::update(Vector2 playerPosition) {
    waveTimer += GetFrameTime() * 6.0f;
    Vector2 dir = Vector2Normalize(Vector2Subtract(playerPosition, position));
    Vector2 perp = { -dir.y, dir.x };
    float offset = std::sin(waveTimer) * 40.0f;

    position.x += (dir.x * speed + perp.x * offset) * GetFrameTime();
    position.y += (dir.y * speed + perp.y * offset) * GetFrameTime();
}
// --- Boss ---
namespace {
    constexpr float BOSS_CHASE_TIME = 3.0f;
    constexpr float BOSS_WINDUP_TIME = 0.8f; // Tid spilleren har til å se dashen komme
    constexpr float BOSS_DASH_TIME = 0.5f;
    constexpr float BOSS_DASH_SPEED = 700.0f;
}

Boss::Boss(Vector2 spawnPos, Texture2D tex) {
    position = spawnPos;
    speed = 90.0f;
    // ~20-30 sek for en typisk build ved 10 min. Echelon-effekter (f.eks. +HP på E9) legges på i tillegg.
    hp = 30000;
    maxHp = hp;
    damage = 30;
    xpValue = 0;
    orbColor = MAROON;
    orbRadius = 0.0f;
    hitRadius = 40.0f;
    texture = tex;
}

void Boss::update(Vector2 playerPosition) {
    float dt = GetFrameTime();
    lastPlayerPos = playerPosition;
    phaseTimer += dt;

    switch (phase) {
        case Phase::CHASE: {
            Vector2 dir = Vector2Normalize(Vector2Subtract(playerPosition, position));
            position = Vector2Add(position, Vector2Scale(dir, speed * dt));
            if (phaseTimer >= BOSS_CHASE_TIME) {
                phase = Phase::WINDUP;
                phaseTimer = 0.0f;
            }
            break;
        }
        case Phase::WINDUP:
            // Står stille og sikter – retningen låses når dashen starter
            dashDirection = Vector2Normalize(Vector2Subtract(playerPosition, position));
            if (phaseTimer >= BOSS_WINDUP_TIME) {
                phase = Phase::DASH;
                phaseTimer = 0.0f;
            }
            break;
        case Phase::DASH:
            position = Vector2Add(position, Vector2Scale(dashDirection, BOSS_DASH_SPEED * dt));
            if (phaseTimer >= BOSS_DASH_TIME) {
                phase = Phase::CHASE;
                phaseTimer = 0.0f;
            }
            break;
    }
}

void Boss::draw() const {
    // Varsel-linje mens bossen lader opp dashen
    if (phase == Phase::WINDUP) {
        float t = phaseTimer / BOSS_WINDUP_TIME;
        Vector2 end = Vector2Add(position, Vector2Scale(dashDirection, BOSS_DASH_SPEED * BOSS_DASH_TIME));
        DrawLineEx(position, end, hitRadius * 2.0f, Fade(RED, 0.15f + 0.25f * t));
    }

    Color body = (phase == Phase::WINDUP) ? RED : orbColor;
    DrawCircleV(position, hitRadius, body);
    DrawCircleLines((int)position.x, (int)position.y, hitRadius, BLACK);
    DrawCircleLines((int)position.x, (int)position.y, hitRadius + 4.0f, Fade(RED, 0.6f));
    // HP-baren til bossen tegnes i HUD-en øverst på skjermen
}

// --- Exploder (kamikaze) ---
namespace {
    constexpr float EXPLODER_FUSE_TIME = 0.6f;    // Tid fra den stopper til den smeller
    constexpr float EXPLODER_TRIGGER_RANGE = 55.0f;
}

Exploder::Exploder(Vector2 spawnPos, Texture2D tex) {
    position = spawnPos;
    speed = 200.0f;
    hp = 30;
    maxHp = 30;
    damage = 25;
    xpValue = 12;
    orbColor = ORANGE;
    orbRadius = 5.0f;
    goldChance = 0.04f;
    goldValue = 1;
    texture = tex;
}

void Exploder::update(Vector2 playerPosition) {
    float dt = GetFrameTime();

    if (fuseLit) {
        fuseTimer -= dt;
        if (fuseTimer <= 0.0f) hp = 0; // Sprenger seg selv – onDeath() lager eksplosjonen
        return;
    }

    if (Vector2Distance(position, playerPosition) <= EXPLODER_TRIGGER_RANGE) {
        fuseLit = true;
        fuseTimer = EXPLODER_FUSE_TIME;
        return;
    }

    Vector2 dir = Vector2Normalize(Vector2Subtract(playerPosition, position));
    position = Vector2Add(position, Vector2Scale(dir, speed * dt));
}

void Exploder::draw() const {
    // Blinker mens lunta brenner
    bool flash = fuseLit && ((int)(fuseTimer * 20.0f) % 2 == 0);
    Color body = flash ? WHITE : orbColor;

    if (fuseLit) {
        // Viser hvor stor eksplosjonen blir
        DrawCircleLines((int)position.x, (int)position.y, explosionRadius, Fade(RED, 0.6f));
    }
    DrawCircleV(position, 13.0f, body);
    DrawCircleLines((int)position.x, (int)position.y, 13.0f, RED);
    DrawCircleV(position, 4.0f, RED);
}

void Exploder::onDeath() {
    SpawnExplosion(position, explosionRadius, explosionDamage);
}

void Exploder::applyEchelonModifiers(float hpMult, float damageMult, float speedMult) {
    Enemy::applyEchelonModifiers(hpMult, damageMult, speedMult);
    explosionDamage *= damageMult;
}
