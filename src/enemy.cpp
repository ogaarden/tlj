#include "enemy.hpp"
#include "vfx.hpp"
#include "damage_numbers.hpp"
#include "explosions.hpp"
#include "castle.hpp"
#include "render3d.hpp"
#include "audio.hpp"

// --- Farger og hjelpere for 3D-modellene ---
namespace {
    const Color SKIN = { 236, 196, 160, 255 };
    const Color STEEL = { 175, 182, 195, 255 };
    const Color STEEL_DARK = { 95, 100, 115, 255 };
    const Color LEATHER = { 90, 60, 40, 255 };
    const Color WOOD = { 120, 80, 45, 255 };
    const Color EYE_BLACK = { 20, 18, 24, 255 };

    Vector2 sideOf(Vector2 facing) { return { -facing.y, facing.x }; }

    // Plasserer deler relativt til fienden: fwd = fremover, side = sidelengs, up = høyde
    struct Rig {
        Vector2 pos, f, s;
        Rig(Vector2 p, Vector2 facing) : pos(p), f(facing), s(sideOf(facing)) {
            if (Vector2Length(f) < 0.001f) { f = { 0, 1 }; s = sideOf(f); }
        }
        Vector3 at(float fwd, float side, float up) const {
            return { pos.x + f.x * fwd + s.x * side, up, pos.y + f.y * fwd + s.y * side };
        }
        void ball(float fwd, float side, float up, float r, Color c, int rings = 4, int slices = 6) const {
            ShadedSphere(at(fwd, side, up), r, c, rings, slices);
        }
        void blob(float fwd, float side, float up, Vector3 radii, Color c, int rings = 5, int slices = 7) const {
            ShadedEllipsoid(at(fwd, side, up), f, radii, c, rings, slices);
        }
        void limb(Vector3 a, Vector3 b, float r0, float r1, Color c, int slices = 5) const {
            ShadedCylinder(a, b, r0, r1, c, slices);
        }
    };
}

// --- Baseklasse ---
void Enemy::draw() const {
    // På gulvet: bare skyggen. Selve figuren tegnes i draw3D().
    DrawShadow({ position.x + 4.0f, position.y + 4.0f }, 15.0f * modelScale, 8.0f * modelScale);
}

void Enemy::draw3D() const {
    // Enkel figur: kropp + hode i fiendens farge
    ShadedCylinder(ToWorld3D(position, 0.0f), ToWorld3D(position, 24.0f), 11.0f, 9.0f, orbColor);
    ShadedSphere(ToWorld3D(position, 31.0f), 7.0f, SKIN);
}

void Enemy::drawVfx() const {
    if (!elite) return;
    // Elite: gyllen aura på gulvet og glød rundt kroppen
    float pulse = 0.75f + 0.25f * sinf((float)GetTime() * 5.0f + id);
    VfxDecal(VfxTex::GLOW, position, 70.0f * modelScale, Color{ (unsigned char)(200 * pulse), (unsigned char)(150 * pulse), 40, 255 }, 0.0f, 1.0f);
    VfxDecal(VfxTex::SHOCKWAVE, position, 55.0f * modelScale, Color{ 160, 120, 40, 255 }, (float)GetTime() * 90.0f, 1.2f);
    VfxBillboard(VfxTex::GLOW, ToWorld3D(position, 22.0f * modelScale), 60.0f * modelScale, Color{ 90, 70, 20, 255 });
}

void Enemy::makeElite() {
    elite = true;
    modelScale = 1.45f;
    hp *= 4;
    maxHp = hp;
    damage = (int)(damage * 1.5f);
    xpValue *= 4;
    goldChance = fminf(1.0f, goldChance * 3.0f + 0.1f);
    goldValue += 2;
    hitRadius *= 1.4f;
    orbRadius *= 1.4f;
}

float Enemy::walkCycle() const {
    return (float)GetTime() * (4.0f + speed * 0.04f) + id * 1.7f;
}

// --- 3D-modeller for vanlige fiender ---

// Footman: blå soldat med hjelm, fjærbusk, skjold og spyd
void Footman::draw3D() const {
    Rig r(position, facing);
    float w = walkCycle();
    float step = sinf(w);
    float bob = fabsf(cosf(w)) * 2.0f;

    // Bein og støvler
    for (int s = -1; s <= 1; s += 2) {
        float st = step * 4.0f * s;
        r.limb(r.at(st * 0.5f, s * 4.0f, 2.0f), r.at(0.0f, s * 4.0f, 13.0f + bob), 2.8f, 3.2f, STEEL_DARK);
        r.blob(2.0f + st, s * 4.0f, 2.0f, { 4.5f, 2.2f, 2.8f }, LEATHER, 4, 6);
    }
    // Våpenkjole og brystplate
    r.limb(r.at(0, 0, 11.0f + bob), r.at(0, 0, 28.0f + bob), 10.0f, 8.0f, orbColor, 8);
    r.blob(3.5f, 0.0f, 23.0f + bob, { 6.0f, 6.5f, 7.5f }, STEEL);
    r.blob(8.2f, 0.0f, 17.0f + bob, { 1.2f, 4.5f, 1.6f }, Color{ 240, 220, 120, 255 }, 4, 5); // Gullstripe
    r.limb(r.at(0, 0, 11.5f + bob), r.at(0, 0, 14.0f + bob), 10.4f, 10.2f, LEATHER, 8);          // Belte

    // Hode, hjelm med visir og rød fjærbusk
    r.ball(0.5f, 0, 33.0f + bob, 6.2f, SKIN);
    r.ball(0.0f, 0, 34.5f + bob, 7.0f, STEEL, 5, 8);
    r.blob(6.0f, 0.0f, 33.5f + bob, { 1.0f, 1.2f, 4.5f }, EYE_BLACK, 3, 5);                      // Visir-spalte
    r.blob(-2.0f, 0.0f, 42.0f + bob, { 6.0f, 3.5f, 1.8f }, Color{ 210, 40, 45, 255 }, 4, 6);     // Fjærbusk

    // Skjold på venstre arm (blått med gullkant og bule)
    r.blob(5.0f, -11.0f, 21.0f + bob, { 1.6f, 9.5f, 7.5f }, Color{ 220, 180, 60, 255 });
    r.blob(5.8f, -11.0f, 21.0f + bob, { 1.4f, 8.2f, 6.3f }, orbColor);
    r.ball(7.2f, -11.0f, 21.0f + bob, 2.2f, Color{ 230, 190, 70, 255 }, 3, 5);

    // Spyd i høyre hånd, svinger litt i takt med gangen
    Vector3 hand = r.at(2.0f + step * 2.0f, 11.0f, 19.0f + bob);
    Vector3 butt = r.at(-6.0f, 12.0f, 3.0f + bob);
    Vector3 tip = r.at(14.0f + step * 2.0f, 11.0f, 50.0f + bob);
    r.limb(butt, tip, 1.3f, 1.3f, WOOD, 5);
    r.limb(tip, r.at(15.5f + step * 2.0f, 11.0f, 58.0f + bob), 2.8f, 0.0f, STEEL, 5);
    ShadedSphere(hand, 3.0f, SKIN, 4, 5);
}

// Goon: stor ogre med mage, støttenner, horn, lysende øyne og klubbe
void Goon::draw3D() const {
    const Color OGRE = { 110, 165, 95, 255 };
    const Color OGRE_DARK = { 80, 125, 70, 255 };
    const Color TUSK = { 240, 235, 210, 255 };
    Rig r(position, facing);
    float w = walkCycle() * 0.7f;
    float step = sinf(w);
    float sway = sinf(w) * 1.5f;
    float bob = fabsf(cosf(w)) * 2.5f;

    // Korte, tykke bein
    for (int s = -1; s <= 1; s += 2) {
        float st = step * 4.0f * s;
        r.limb(r.at(st * 0.5f, s * 7.0f, 2.0f), r.at(0.0f, s * 7.0f, 13.0f + bob), 5.0f, 5.5f, OGRE_DARK);
        r.blob(2.5f + st, s * 7.0f, 2.5f, { 6.0f, 3.0f, 4.5f }, OGRE_DARK, 4, 6);
    }
    // Lendeklede og stor mage
    r.limb(r.at(0, sway, 10.0f + bob), r.at(0, sway, 16.0f + bob), 13.5f, 14.0f, LEATHER, 8);
    r.blob(2.0f, sway, 26.0f + bob, { 15.0f, 14.0f, 16.0f }, OGRE);
    r.blob(8.0f, sway, 23.0f + bob, { 7.0f, 9.0f, 10.0f }, Color{ 150, 195, 125, 255 });           // Lysere mage

    // Hode med underbitt, støttenner, horn og røde øyne
    float head = 43.0f + bob;
    r.ball(3.0f, sway, head, 9.5f, OGRE, 6, 8);
    r.blob(8.0f, sway, head - 5.0f, { 5.0f, 3.5f, 7.5f }, OGRE_DARK, 4, 6);                       // Kjeve
    for (int s = -1; s <= 1; s += 2) {
        r.limb(r.at(11.5f, sway + s * 4.0f, head - 6.0f), r.at(13.0f, sway + s * 4.5f, head + 1.0f), 1.6f, 0.0f, TUSK, 5);
        r.ball(10.5f, sway + s * 3.5f, head + 2.5f, 1.8f, Color{ 255, 60, 40, 255 }, 3, 4);           // Øye
        r.limb(r.at(2.0f, sway + s * 6.5f, head + 6.0f), r.at(0.0f, sway + s * 11.0f, head + 15.0f), 2.8f, 0.0f, TUSK, 5);
    }

    // Armer: venstre henger, høyre holder en pigget klubbe
    float swing = -step * 3.0f;
    r.limb(r.at(0, sway - 15.0f, 33.0f + bob), r.at(3.0f - swing, sway - 18.0f, 16.0f + bob), 4.5f, 4.0f, OGRE);
    r.ball(3.0f - swing, sway - 18.0f, 15.0f + bob, 5.0f, OGRE_DARK, 4, 6);
    Vector3 hand = r.at(6.0f + swing, sway + 18.0f, 18.0f + bob);
    r.limb(r.at(0, sway + 15.0f, 33.0f + bob), hand, 4.5f, 4.0f, OGRE);
    Vector3 clubTop = r.at(14.0f + swing, sway + 20.0f, 42.0f + bob);
    r.limb(hand, clubTop, 2.2f, 5.0f, WOOD, 6);
    ShadedSphere(clubTop, 5.5f, WOOD, 4, 6);
    ShadedSphere(hand, 5.0f, OGRE_DARK, 4, 6);
}

// Lackey: liten, ond hoffnarr i to farger med to-tuppet lue, bjeller og kniv
void Lackey::draw3D() const {
    const Color PURPLE_C = { 120, 60, 170, 255 };
    Rig r(position, facing);
    float w = walkCycle() * 1.4f;
    float hop = fabsf(sinf(w)) * 5.0f;                 // Hopper av gårde
    float step = sinf(w);

    // Tynne bein med spisse sko
    for (int s = -1; s <= 1; s += 2) {
        r.limb(r.at(step * 2.0f * s, s * 3.0f, 1.5f + hop), r.at(0.0f, s * 3.0f, 9.0f + hop), 1.6f, 2.0f, s < 0 ? PURPLE_C : orbColor, 5);
        r.limb(r.at(1.0f + step * 2.0f * s, s * 3.0f, 1.5f + hop), r.at(6.0f + step * 2.0f * s, s * 3.0f, 3.5f + hop), 1.8f, 0.0f, s < 0 ? orbColor : PURPLE_C, 5);
    }
    // Kropp i to farger (narredrakt)
    r.blob(0.0f, -2.3f, 14.0f + hop, { 5.5f, 6.5f, 3.6f }, PURPLE_C, 5, 7);
    r.blob(0.0f, 2.3f, 14.0f + hop, { 5.5f, 6.5f, 3.6f }, orbColor, 5, 7);
    // Hode med ondt glis
    float head = 24.0f + hop;
    r.ball(0.5f, 0, head, 5.8f, SKIN, 5, 7);
    for (int s = -1; s <= 1; s += 2) r.ball(5.0f, s * 2.2f, head + 1.2f, 1.1f, EYE_BLACK, 3, 4);
    r.blob(5.3f, 0.0f, head - 2.3f, { 0.8f, 0.9f, 3.2f }, Color{ 200, 30, 40, 255 }, 3, 5);
    // To-tuppet lue med bjeller
    for (int s = -1; s <= 1; s += 2) {
        Vector3 base = r.at(0.0f, s * 2.5f, head + 3.5f);
        Vector3 tip = r.at(-4.0f, s * 9.0f, head + 11.0f + sinf(w + s) * 1.5f);
        r.limb(base, tip, 3.6f, 0.6f, s < 0 ? orbColor : PURPLE_C, 6);
        ShadedSphere(tip, 1.8f, GOLD, 3, 5);
    }
    // Liten kniv
    Vector3 hand = r.at(4.0f, 7.0f, 13.0f + hop);
    r.limb(r.at(0, 5.0f, 17.0f + hop), hand, 1.4f, 1.2f, orbColor, 5);
    r.limb(hand, r.at(10.0f, 7.5f, 16.0f + hop), 1.2f, 0.0f, STEEL, 4);
}

void Enemy::takeDamage(int amount, Color numberColor, bool isDamageOverTime) {
    if (amount <= 0) return;
    hp -= amount;
    hitFlash = isDamageOverTime ? fmaxf(hitFlash, 0.25f) : 1.0f;

    if (!isDamageOverTime) {
        SpawnDamageNumber(position, amount, numberColor);
        VfxHit(position, numberColor);
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
    if (elite) pickups.push_back({ { position.x + 12.0f, position.y }, 1, GOLD, 14.0f, 0.0f, PickupType::CHEST });

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
    hp = 110;       // Middels: vanlig fotsoldat
    maxHp = 110;
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
    hp = 320;       // Tank: treg, men tåler mye og slår hardt
    maxHp = 320;
    damage = 25;
    xpValue = 40;
    hitRadius = 22.0f;
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

    // Fase 2: rasende under halv HP
    if (!enraged && hp <= maxHp / 2) {
        enraged = true;
        enragedAt = (float)GetTime();
        speed *= 1.35f;
        summonsRequested += 8;
        summonTimer = 0.0f;
        PlaySfx(Sfx::BOSS_GONG);
        AddCameraShake(0.8f);
        VfxShockwave(position, 220.0f, RED);
    }
    if (enraged) {
        summonTimer += dt;
        if (summonTimer >= 10.0f) { summonTimer = 0.0f; summonsRequested += 5; }
    }
    float chaseTime = enraged ? BOSS_CHASE_TIME * 0.55f : BOSS_CHASE_TIME;
    float dashSpeed = enraged ? BOSS_DASH_SPEED * 1.3f : BOSS_DASH_SPEED;

    switch (phase) {
        case Phase::CHASE: {
            Vector2 dir = Vector2Normalize(Vector2Subtract(playerPosition, position));
            position = Vector2Add(position, Vector2Scale(dir, speed * dt));
            if (phaseTimer >= chaseTime) {
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
            position = Vector2Add(position, Vector2Scale(dashDirection, dashSpeed * dt));
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
        Vector2 end = Vector2Add(position, Vector2Scale(dashDirection, BOSS_DASH_SPEED * (enraged ? 1.3f : 1.0f) * BOSS_DASH_TIME));
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
    ShadedCylinder(ToWorld3D(position, 0.0f), ToWorld3D(top, 60.0f), 40.0f, 21.0f, (windingUp || enraged) ? ROBE_RAGE : ROBE, 20);
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
}

void Boss::drawVfx() const {
    // Mørk-rød aura rundt kongen, og septeret gløder når han lader opp
    float t = (float)GetTime();
    VfxDecal(VfxTex::GLOW, position, 150.0f, Color{ 90, 10, 20, 255 }, 0.0f, 1.0f);
    if (enraged) {
        // Rasende: brennende rød aura og glødende øyne
        float pulse = 0.75f + 0.25f * sinf(t * 8.0f);
        VfxDecal(VfxTex::SHOCKWAVE, position, 150.0f * pulse, Color{ 255, 60, 40, 255 }, -t * 120.0f, 1.3f);
        VfxBillboard(VfxTex::GLOW, ToWorld3D(position, 60.0f), 180.0f, Color{ (unsigned char)(120 * pulse), 20, 10, 255 });
        if (GetRandomValue(0, 3) == 0) VfxBubble({ position.x + GetRandomValue(-35, 35), position.y + GetRandomValue(-35, 35) }, 10.0f, Color{ 255, 90, 40, 255 });
    }
    if (phase == Phase::WINDUP) {
        Vector2 look = dashDirection;
        Vector2 side = { -look.y, look.x };
        Vector2 tip = Vector2Add(Vector2Add(position, Vector2Scale(side, 30.0f)), Vector2Scale(look, 4.0f));
        float pulse = 0.5f + 0.5f * sinf(t * 20.0f);
        float k = phaseTimer / BOSS_WINDUP_TIME;
        VfxBillboard(VfxTex::GLOW, ToWorld3D(tip, 115.0f), 60.0f + 30.0f * pulse, Color{ 255, 200, 80, 255 });
        VfxBillboard(VfxTex::SPARK, ToWorld3D(tip, 115.0f), 70.0f + 40.0f * k, WHITE, t * 300.0f);
        VfxDecal(VfxTex::SHOCKWAVE, position, 120.0f + 60.0f * k, Color{ 255, 80, 60, 255 }, t * 200.0f, 1.5f);
    }
    if (phase == Phase::DASH) {
        VfxTrail(ToWorld3D(position, 40.0f), Color{ 255, 90, 60, 255 }, 90.0f, 0.3f);
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
    facing = dir;
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
    // Bombe med sinte øyne og små føtter. Blinker hvitt/rødt mens lunta brenner.
    Rig r(position, facing);
    bool flash = fuseLit && ((int)(fuseTimer * 20.0f) % 2 == 0);
    Color body = flash ? WHITE : (fuseLit ? Color{ 170, 40, 30, 255 } : Color{ 45, 45, 58, 255 });
    float wobble = fuseLit ? 1.0f + 0.1f * sinf(fuseTimer * 60.0f) : 1.0f;
    float w = walkCycle() * 1.5f;
    float run = fuseLit ? 0.0f : sinf(w);
    float bob = fuseLit ? 0.0f : fabsf(cosf(w)) * 2.5f;

    // Små løpeføtter
    for (int s = -1; s <= 1; s += 2) r.blob(3.0f + run * 4.0f * s, s * 5.0f, 2.0f, { 4.0f, 2.0f, 2.8f }, Color{ 230, 140, 40, 255 }, 3, 5);

    float c = 15.0f + bob;
    r.ball(0, 0, c, 12.5f * wobble, body, 7, 10);
    r.ball(-4.0f, 4.0f, c + 5.0f, 3.0f, Color{ 120, 120, 140, 255 }, 3, 4);           // Glans
    // Sinte øyne: hvite med svarte pupiller og skrå øyenbryn
    for (int s = -1; s <= 1; s += 2) {
        r.ball(10.5f, s * 4.0f, c + 2.0f, 3.0f, WHITE, 4, 6);
        r.ball(12.8f, s * 3.6f, c + 1.5f, 1.4f, EYE_BLACK, 3, 4);
        r.limb(r.at(11.0f, s * 1.5f, c + 4.5f), r.at(10.0f, s * 7.0f, c + 7.5f), 1.0f, 1.0f, EYE_BLACK, 4);
    }
    r.limb(r.at(0, 0, c + 11.0f), r.at(0, 0, c + 16.0f), 4.5f, 4.5f, STEEL, 8);            // Tut
    r.limb(r.at(0, 0, c + 16.0f), r.at(-2.0f, 2.0f, c + 23.0f), 1.2f, 1.2f, LEATHER, 5);   // Lunte
}

void Exploder::drawVfx() const {
    // Gnisten på lunta, og en rød advarselsglød når den er tent
    float t = (float)GetTime();
    float flicker = 0.7f + 0.3f * sinf(t * 40.0f + position.x);
    Rig r(position, facing);
    Vector3 spark = r.at(-2.0f, 2.0f, 40.0f);
    VfxBillboard(VfxTex::SPARK, spark, 22.0f * flicker, Color{ 255, 210, 120, 255 }, t * 500.0f);
    VfxBillboard(VfxTex::GLOW, spark, 16.0f, Color{ 255, 150, 50, 255 });
    if (fuseLit) {
        float k = 1.0f - fuseTimer / EXPLODER_FUSE_TIME;
        VfxBillboard(VfxTex::GLOW, ToWorld3D(position, 15.0f), 50.0f + 50.0f * k, Color{ 255, 60, 30, 255 });
    }
}

void Exploder::onDeath() {
    SpawnExplosion(position, explosionRadius, explosionDamage);
}

void Exploder::applyEchelonModifiers(float hpMult, float damageMult, float speedMult) {
    Enemy::applyEchelonModifiers(hpMult, damageMult, speedMult);
    explosionDamage *= damageMult;
}
