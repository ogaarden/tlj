#include "enemy.hpp"
#include "damage_numbers.hpp"
#include "explosions.hpp"
#include "castle.hpp"

// --- Baseklasse ---
void Enemy::draw() const {

    float enemyRadius = 15.0f; // Fast radius for kroppen (eller bruk orbRadius)
    DrawShadow({ position.x + 3.0f, position.y + 10.0f }, 14.0f, 6.0f);
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
    // Kongen, sett ovenfra: kappe, hermelinkrage, hode med krone og skjegg, og septer
    const float r = hitRadius;
    const Color ROBE       = { 110, 20, 40, 255 };
    const Color ROBE_DARK  = { 70, 10, 25, 255 };
    const Color ERMINE     = { 240, 236, 225, 255 };
    const Color SKIN       = { 236, 196, 160, 255 };
    const Color CROWN_GOLD = { 230, 185, 40, 255 };
    const Color BEARD      = { 225, 225, 230, 255 };

    bool windingUp = (phase == Phase::WINDUP);
    bool dashing = (phase == Phase::DASH);

    // Retningen kongen ser (mot spilleren, eller dash-retningen)
    Vector2 facing = (windingUp || dashing) ? dashDirection
                                            : Vector2Normalize(Vector2Subtract(lastPlayerPos, position));
    Vector2 side = { -facing.y, facing.x };

    // Varsel-linje mens kongen lader opp "Royal Charge"
    if (windingUp) {
        float t = phaseTimer / BOSS_WINDUP_TIME;
        Vector2 end = Vector2Add(position, Vector2Scale(dashDirection, BOSS_DASH_SPEED * BOSS_DASH_TIME));
        DrawLineEx(position, end, r * 2.0f, Fade(RED, 0.15f + 0.25f * t));
    }

    DrawShadow({ position.x + 6.0f, position.y + r * 0.55f }, r * 1.1f, r * 0.55f);

    // Kappen (flagrer litt bakover når han dasher)
    Vector2 cape = dashing ? Vector2Subtract(position, Vector2Scale(facing, 10.0f)) : position;
    DrawCircleV(cape, r, ROBE_DARK);
    DrawCircleV(position, r - 4.0f, windingUp ? Color{ 170, 30, 40, 255 } : ROBE);

    // Septer i hånden: gullstav med kule, holdes høyt mens han lader opp
    Vector2 hand = Vector2Add(position, Vector2Scale(side, r * 0.8f));
    float scepterLength = windingUp ? r * 1.4f : r * 1.0f;
    Vector2 scepterTip = Vector2Add(hand, Vector2Scale(facing, scepterLength));
    DrawLineEx(hand, scepterTip, 5.0f, CROWN_GOLD);
    DrawCircleV(scepterTip, windingUp ? 9.0f : 7.0f, CROWN_GOLD);
    DrawCircleV(scepterTip, 3.5f, RED);
    if (windingUp) DrawCircleV(scepterTip, 16.0f, Fade(YELLOW, 0.3f));
    DrawCircleV(hand, 6.0f, SKIN);

    // Hermelinkrage (hvit med svarte prikker)
    DrawRing(position, r * 0.45f, r * 0.72f, 0.0f, 360.0f, 32, ERMINE);
    for (int i = 0; i < 8; i++) {
        float a = (45.0f * i + 20.0f) * DEG2RAD;
        DrawCircleV(Vector2Add(position, { cosf(a) * r * 0.6f, sinf(a) * r * 0.6f }), 2.0f, BLACK);
    }

    // Hode og skjegg (skjegget peker i retningen han ser)
    DrawCircleV(Vector2Add(position, Vector2Scale(facing, r * 0.3f)), r * 0.28f, BEARD);
    DrawCircleV(position, r * 0.44f, SKIN);

    // Krone: gullring med tagger og juveler
    float crownRadius = r * 0.26f;
    DrawRing(position, crownRadius - 3.0f, crownRadius + 2.0f, 0.0f, 360.0f, 32, CROWN_GOLD);
    for (int i = 0; i < 5; i++) {
        float a = (72.0f * i - 90.0f) * DEG2RAD;
        Vector2 dir = { cosf(a), sinf(a) };
        Vector2 base = Vector2Add(position, Vector2Scale(dir, crownRadius));
        Vector2 tip = Vector2Add(position, Vector2Scale(dir, crownRadius + 7.0f));
        Vector2 perp = { -dir.y * 3.5f, dir.x * 3.5f };
        DrawTriangle(Vector2Add(base, perp), Vector2Subtract(base, perp), tip, CROWN_GOLD);
        DrawTriangle(Vector2Subtract(base, perp), Vector2Add(base, perp), tip, CROWN_GOLD); // Uansett vinding
        DrawCircleV(tip, 2.5f, (i % 2 == 0) ? RED : BLUE);
    }
    DrawCircleV(position, crownRadius - 3.0f, Color{ 150, 20, 40, 255 }); // Fløyel inni kronen
    DrawCircleV(Vector2Add(position, { -2.0f, -2.0f }), 2.5f, CROWN_GOLD);   // Liten kule på toppen

    DrawCircleLines((int)position.x, (int)position.y, r, BLACK);
    // HP-baren til kongen tegnes i HUD-en øverst på skjermen
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
    DrawShadow({ position.x + 3.0f, position.y + 9.0f }, 12.0f, 5.0f);
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
