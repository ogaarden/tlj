#include "enemy.hpp"
#include "damage_numbers.hpp"
#include "explosions.hpp"
#include "castle.hpp"
#include "render3d.hpp"
#include "audio.hpp"

// --- Farger brukt av 3D-modellene ---
namespace {
    const Color SKIN = { 236, 196, 160, 255 };
    const Color STEEL = { 170, 175, 185, 255 };
    const Color LEATHER = { 90, 60, 40, 255 };

    Vector2 sideOf(Vector2 facing) { return { -facing.y, facing.x }; }
}

// --- Baseklasse ---
void Enemy::draw() const {
    // På gulvet: bare skyggen. Selve figuren tegnes i draw3D().
    DrawShadow({ position.x + 4.0f, position.y + 4.0f }, 15.0f, 8.0f);
}

void Enemy::draw3D() const {
    // Enkel figur: kropp + hode i fiendens farge
    ShadedCylinder(ToWorld3D(position, 0.0f), ToWorld3D(position, 24.0f), 11.0f, 9.0f, orbColor);
    ShadedSphere(ToWorld3D(position, 31.0f), 7.0f, SKIN);
}

// --- 3D-modeller for vanlige fiender ---

// Footman: blå soldat med hjelm og spyd
void Footman::draw3D() const {
    Vector2 side = sideOf(facing);
    ShadedCylinder(ToWorld3D(position, 0.0f), ToWorld3D(position, 26.0f), 11.0f, 9.0f, orbColor);
    ShadedCylinder(ToWorld3D(position, 11.0f), ToWorld3D(position, 14.0f), 11.5f, 11.0f, LEATHER);  // Belte
    ShadedSphere(ToWorld3D(position, 32.0f), 7.0f, SKIN);
    ShadedSphere(ToWorld3D(position, 34.0f), 7.6f, STEEL, 6, 10);                                   // Hjelm
    ShadedCylinder(ToWorld3D(position, 30.5f), ToWorld3D(position, 31.5f), 10.0f, 10.0f, STEEL);    // Hjelmkant

    Vector2 hand = Vector2Add(position, Vector2Scale(side, 11.0f));
    Vector2 tip = Vector2Add(hand, Vector2Scale(facing, 12.0f));
    ShadedCylinder(ToWorld3D(hand, 4.0f), ToWorld3D(tip, 48.0f), 1.5f, 1.5f, LEATHER, 6);         // Spydskaft
    ShadedCylinder(ToWorld3D(tip, 48.0f), ToWorld3D(Vector2Add(tip, Vector2Scale(facing, 1.5f)), 56.0f), 3.0f, 0.0f, STEEL, 6);
    ShadedSphere(ToWorld3D(hand, 18.0f), 3.5f, SKIN, 4, 6);
}

// Goon: stor grønn brute med horn
void Goon::draw3D() const {
    const Color HEAD = { 120, 190, 110, 255 };
    const Color HORN = { 230, 225, 200, 255 };
    Vector2 side = sideOf(facing);
    ShadedCylinder(ToWorld3D(position, 0.0f), ToWorld3D(position, 28.0f), 16.0f, 14.0f, orbColor);
    ShadedSphere(ToWorld3D(Vector2Add(position, Vector2Scale(side, 15.0f)), 20.0f), 6.0f, HEAD, 5, 8);   // Armer
    ShadedSphere(ToWorld3D(Vector2Subtract(position, Vector2Scale(side, 15.0f)), 20.0f), 6.0f, HEAD, 5, 8);
    ShadedSphere(ToWorld3D(position, 37.0f), 10.0f, HEAD);
    for (int s = -1; s <= 1; s += 2) {
        Vector2 hornBase = Vector2Add(position, Vector2Scale(side, 7.0f * s));
        Vector2 hornTip = Vector2Add(position, Vector2Scale(side, 11.0f * s));
        ShadedCylinder(ToWorld3D(hornBase, 43.0f), ToWorld3D(hornTip, 54.0f), 3.0f, 0.0f, HORN, 6);
    }
}

// Lackey: liten gul hoffnarr-lakei med spiss lue og bjelle
void Lackey::draw3D() const {
    const Color HAT = { 120, 60, 170, 255 };
    ShadedCylinder(ToWorld3D(position, 0.0f), ToWorld3D(position, 18.0f), 9.0f, 6.0f, orbColor);
    ShadedSphere(ToWorld3D(position, 23.0f), 6.0f, SKIN, 6, 10);
    ShadedCylinder(ToWorld3D(position, 26.0f), ToWorld3D(Vector2Subtract(position, Vector2Scale(facing, 5.0f)), 40.0f), 6.5f, 0.0f, HAT, 8);
    ShadedSphere(ToWorld3D(Vector2Subtract(position, Vector2Scale(facing, 5.0f)), 40.0f), 2.5f, GOLD, 4, 6);
}

void Enemy::takeDamage(int amount, Color numberColor, bool isDamageOverTime) {
    if (amount <= 0) return;
    hp -= amount;

    if (!isDamageOverTime) {
        SpawnDamageNumber(position, amount, numberColor);
        PlaySfx(Sfx::HIT);
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
    facing = dir;
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
    facing = dir;
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
    facing = dir;
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
                PlaySfx(Sfx::BOSS_CHARGE);
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

// Retningen kongen ser: mot spilleren, eller låst i dash-retningen
static Vector2 kingFacing(bool locked, Vector2 dashDirection, Vector2 from, Vector2 to) {
    return locked ? dashDirection : Vector2Normalize(Vector2Subtract(to, from));
}

void Boss::draw() const {
    // På gulvet: varsel-felt for Royal Charge + skygge
    if (phase == Phase::WINDUP) {
        float t = phaseTimer / BOSS_WINDUP_TIME;
        Vector2 end = Vector2Add(position, Vector2Scale(dashDirection, BOSS_DASH_SPEED * BOSS_DASH_TIME));
        DrawLineEx(position, end, hitRadius * 2.0f, Fade(RED, 0.2f + 0.3f * t));
        DrawCircleV(end, hitRadius, Fade(RED, 0.25f + 0.3f * t));
    }
    DrawShadow({ position.x + 10.0f, position.y + 10.0f }, hitRadius * 1.15f, hitRadius * 0.8f);
}

void Boss::draw3D() const {
    // Kongen i 3D: kappe, hermelinkant og -krage, hode med skjegg, krone med juveler og septer
    const Color ROBE       = { 130, 22, 45, 255 };
    const Color ROBE_RAGE  = { 200, 35, 45, 255 };
    const Color ERMINE     = { 240, 236, 225, 255 };
    const Color CROWN_GOLD = { 235, 190, 45, 255 };
    const Color BEARD      = { 230, 230, 235, 255 };
    const Color VELVET     = { 120, 15, 35, 255 };

    bool windingUp = (phase == Phase::WINDUP);
    bool dashing = (phase == Phase::DASH);
    Vector2 look = kingFacing(windingUp || dashing, dashDirection, position, lastPlayerPos);
    Vector2 side = { -look.y, look.x };

    // Kongen lener seg fremover når han dasher
    Vector2 lean = dashing ? Vector2Scale(look, 10.0f) : Vector2{ 0, 0 };
    Vector2 top = Vector2Add(position, lean);

    // Kappe (kjegle) med hermelinkant nederst
    ShadedCylinder(ToWorld3D(position, 0.0f), ToWorld3D(top, 60.0f), 40.0f, 21.0f, windingUp ? ROBE_RAGE : ROBE, 20);
    ShadedCylinder(ToWorld3D(position, 0.0f), ToWorld3D(position, 6.0f), 41.5f, 40.5f, ERMINE, 20);

    // Hermelinkrage med svarte prikker
    ShadedCylinder(ToWorld3D(top, 54.0f), ToWorld3D(top, 64.0f), 26.0f, 23.0f, ERMINE, 16);
    for (int i = 0; i < 8; i++) {
        float a = (45.0f * i + 20.0f) * DEG2RAD;
        ShadedSphere(ToWorld3D({ top.x + cosf(a) * 24.5f, top.y + sinf(a) * 24.5f }, 59.0f), 1.8f, BLACK, 3, 4);
    }

    // Hode og skjegg
    ShadedSphere(ToWorld3D(top, 78.0f), 15.0f, SKIN, 10, 14);
    ShadedSphere(ToWorld3D(Vector2Add(top, Vector2Scale(look, 9.0f)), 70.0f), 10.0f, BEARD, 6, 10);

    // Krone: gullring med fem tagger og juveler, fløyel inni
    ShadedCylinder(ToWorld3D(top, 88.0f), ToWorld3D(top, 98.0f), 12.5f, 13.5f, CROWN_GOLD, 16);
    ShadedSphere(ToWorld3D(top, 97.0f), 10.0f, VELVET, 6, 10);
    for (int i = 0; i < 5; i++) {
        float a = (72.0f * i) * DEG2RAD;
        Vector2 point = { top.x + cosf(a) * 12.0f, top.y + sinf(a) * 12.0f };
        ShadedCylinder(ToWorld3D(point, 97.0f), ToWorld3D(point, 108.0f), 3.2f, 0.0f, CROWN_GOLD, 6);
        ShadedSphere(ToWorld3D(Vector2Add(top, { cosf(a) * 13.6f, sinf(a) * 13.6f }), 93.0f), 2.2f, (i % 2 == 0) ? RED : BLUE, 3, 5);
    }
    ShadedSphere(ToWorld3D(top, 104.0f), 3.0f, CROWN_GOLD, 4, 6);

    // Septer: løftes høyt og gløder mens han lader opp
    Vector2 hand = Vector2Add(top, Vector2Scale(side, 30.0f));
    float tipHeight = windingUp ? 110.0f : 82.0f;
    Vector2 tip = Vector2Add(hand, Vector2Scale(look, windingUp ? 4.0f : 14.0f));
    ShadedCylinder(ToWorld3D(hand, 36.0f), ToWorld3D(tip, tipHeight), 2.5f, 2.5f, CROWN_GOLD, 8);
    ShadedSphere(ToWorld3D(tip, tipHeight + 5.0f), 7.0f, CROWN_GOLD, 6, 10);
    ShadedSphere(ToWorld3D(Vector2Add(tip, Vector2Scale(look, 5.0f)), tipHeight + 6.0f), 3.2f, RED, 4, 6);
    ShadedSphere(ToWorld3D(hand, 44.0f), 6.0f, SKIN, 5, 8);
    if (windingUp) {
        float pulse = 0.5f + 0.5f * sinf((float)GetTime() * 20.0f);
        DrawSphere(ToWorld3D(tip, tipHeight + 5.0f), 12.0f + 4.0f * pulse, Fade(YELLOW, 0.35f));
    }
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
    // På gulvet: skygge, og hvor stor eksplosjonen blir når lunta er tent
    if (fuseLit) {
        float t = 1.0f - fuseTimer / EXPLODER_FUSE_TIME;
        DrawCircleV(position, explosionRadius, Fade(RED, 0.12f + 0.2f * t));
        DrawCircleLines((int)position.x, (int)position.y, explosionRadius, Fade(RED, 0.8f));
    }
    DrawShadow({ position.x + 4.0f, position.y + 4.0f }, 13.0f, 7.0f);
}

void Exploder::draw3D() const {
    // Bombe med lunte og gnist. Blinker hvitt/rødt mens lunta brenner.
    bool flash = fuseLit && ((int)(fuseTimer * 20.0f) % 2 == 0);
    Color body = flash ? WHITE : (fuseLit ? Color{ 170, 40, 30, 255 } : Color{ 45, 45, 55, 255 });
    float wobble = fuseLit ? 1.0f + 0.08f * sinf(fuseTimer * 60.0f) : 1.0f;

    ShadedSphere(ToWorld3D(position, 14.0f), 13.0f * wobble, body, 8, 12);
    ShadedCylinder(ToWorld3D(position, 25.0f), ToWorld3D(position, 30.0f), 4.5f, 4.5f, STEEL, 8);           // Tut
    ShadedCylinder(ToWorld3D(position, 30.0f), ToWorld3D({ position.x + 3.0f, position.y }, 37.0f), 1.2f, 1.2f, LEATHER, 5);  // Lunte

    // Gnisten flimrer
    float flicker = 0.7f + 0.3f * sinf((float)GetTime() * 40.0f + position.x);
    ShadedSphere(ToWorld3D({ position.x + 3.0f, position.y }, 38.0f), 3.0f * flicker, fuseLit ? YELLOW : ORANGE, 4, 6);
}

void Exploder::onDeath() {
    SpawnExplosion(position, explosionRadius, explosionDamage);
}

void Exploder::applyEchelonModifiers(float hpMult, float damageMult, float speedMult) {
    Enemy::applyEchelonModifiers(hpMult, damageMult, speedMult);
    explosionDamage *= damageMult;
}
