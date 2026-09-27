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
    // Mørk, rødlig skygge (tydeligere enn spillerens), så fiendene synes på det lyse gulvet
    float k = ENEMY_VISUAL_SCALE * modelScale;
    DrawEllipse((int)position.x + 3, (int)position.y + 3, 17.0f * k, 9.5f * k, Fade(Color{ 40, 0, 8, 255 }, 0.55f));
    DrawEllipse((int)position.x + 3, (int)position.y + 3, 11.0f * k, 6.0f * k, Fade(Color{ 20, 0, 4, 255 }, 0.35f));
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
    modelScale = 1.3f; // Sammen med ENEMY_VISUAL_SCALE: godt over 2x vanlig størrelse
    hp *= 4;
    maxHp = hp;
    damage = (int)(damage * 1.5f);
    xpValue *= 4;
    goldChance = fminf(1.0f, goldChance * 3.0f + 0.03f);
    goldValue += 1;
    hitRadius *= 1.4f;
    orbRadius *= 1.4f;
    knockbackScale *= 0.4f;
}

void Enemy::applySlow(float amount, float duration) {
    slowAmount = fmaxf(slowAmount * (slowTimer > 0.0f ? 1.0f : 0.0f), fminf(0.9f, amount));
    slowTimer = fmaxf(slowTimer, duration);
}

void Enemy::knockBack(Vector2 direction, float strength) {
    Vector2 d = Vector2Normalize(direction);
    knockVelocity = Vector2Add(knockVelocity, Vector2Scale(d, strength * knockbackScale));
}

void Enemy::applyStatusMovement(Vector2 before, float dt) {
    if (wardTimer > 0.0f) wardTimer -= dt;
    if (hasteTimer > 0.0f) {
        hasteTimer -= dt;
        position = Vector2Add(before, Vector2Scale(Vector2Subtract(position, before), 1.4f));
    }
    if (slowTimer > 0.0f) {
        slowTimer -= dt;
        position = Vector2Add(before, Vector2Scale(Vector2Subtract(position, before), 1.0f - slowAmount));
        if (slowTimer <= 0.0f) slowAmount = 0.0f;
    }
    if (knockVelocity.x != 0.0f || knockVelocity.y != 0.0f) {
        position = Vector2Add(position, Vector2Scale(knockVelocity, dt));
        knockVelocity = Vector2Scale(knockVelocity, fmaxf(0.0f, 1.0f - 9.0f * dt));
        if (Vector2LengthSqr(knockVelocity) < 4.0f) knockVelocity = { 0, 0 };
    }
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
    if (damageTakenMult < 1.0f) amount = std::max(1, (int)(amount * damageTakenMult));
    if (wardTimer > 0.0f) amount = std::max(1, amount / 2); // Fanebærerens vern
    bool crit = !isDamageOverTime && GetRandomValue(1, 1000) <= (int)(critChance * 1000.0f);
    if (crit) amount = (int)(amount * critMultiplier);
    hp -= amount;
    hitFlash = isDamageOverTime ? fmaxf(hitFlash, 0.25f) : 1.0f;

    if (!isDamageOverTime) {
        SpawnDamageNumber(position, amount, numberColor, crit);
        VfxHit(position, crit ? Color{ 255, 220, 80, 255 } : numberColor);
        if (crit) VfxHit(position, WHITE);
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

// XP-tiers: blå (små), grønn, rød og lilla (elite og store)
Color XpTierColor(int value) {
    if (value < 15) return Color{ 80, 170, 255, 255 };
    if (value < 40) return Color{ 90, 230, 120, 255 };
    if (value < 120) return Color{ 255, 80, 90, 255 };
    return Color{ 200, 110, 255, 255 };
}

float XpTierRadius(int value) {
    if (value < 15) return 6.0f;
    if (value < 40) return 7.5f;
    if (value < 120) return 9.5f;
    return 12.0f;
}

void Enemy::applyEchelonModifiers(float hpMult, float damageMult, float speedMult) {
    hp = (int)(hp * hpMult);
    maxHp = hp;
    damage = (int)(damage * damageMult);
    speed *= speedMult;
}

void Enemy::dropLoot(std::vector<Pickup>& pickups) const {
    // XP-krystall med farge etter verdi, som spretter litt ut fra fienden
    Vector2 scatter = { position.x + (float)GetRandomValue(-10, 10), position.y + (float)GetRandomValue(-10, 10) };
    // Med hundrevis av fiender slås krystaller som ligger tett sammen til én større
    // (samme XP, men færre ting på bakken og en fin "oppgradering" av krystallfargen)
    bool merged = false;
    for (auto& p : pickups) {
        if (p.type != PickupType::XP || p.pull > 0.0f) continue;
        if (Vector2DistanceSqr(p.position, scatter) < 28.0f * 28.0f) {
            p.value += xpValue;
            p.color = XpTierColor(p.value);
            p.radius = XpTierRadius(p.value);
            merged = true;
            break;
        }
    }
    if (!merged) pickups.push_back({ scatter, xpValue, XpTierColor(xpValue), XpTierRadius(xpValue), 15.0f, PickupType::XP });
    extraLoot(pickups);
    if (miniboss) {
        // Miniboss: Kongens septer, en sølvkiste og en haug med XP
        pickups.push_back({ { position.x, position.y - 20.0f }, 1, SKYBLUE, 16.0f, 0.0f, PickupType::SCEPTER });
        pickups.push_back({ { position.x + 30.0f, position.y + 10.0f }, 2, GOLD, 14.0f, 0.0f, PickupType::CHEST }); // Sølvkiste
        for (int i = 0; i < 12; i++) {
            float a = i * PI / 6.0f;
            int v = xpValue / 12;
            pickups.push_back({ { position.x + cosf(a) * 45.0f, position.y + sinf(a) * 45.0f }, v, XpTierColor(v), XpTierRadius(v), 30.0f, PickupType::XP });
        }
        return;
    }
    if (elite || chestCarrier) {
        // Items skal være sjeldne: en vanlig elite har bare 8 % sjanse for en trekiste,
        // og elites deler en lang nedkjøling (Firkløver gjør begge deler bedre).
        // Uten kiste slipper eliten en ekstra stor krystall i stedet.
        bool eliteChest = chestCooldown <= 0.0f && GetRandomValue(1, 1000) <= (int)(80 * luck);
        if (chestCarrier || eliteChest) {
            pickups.push_back({ { position.x + 12.0f, position.y }, 1, GOLD, 14.0f, 0.0f, PickupType::CHEST }); // Trekiste
            if (!chestCarrier) chestCooldown = 90.0f / luck;
        } else {
            int bonus = xpValue * 2;
            pickups.push_back({ { position.x + 12.0f, position.y }, bonus, XpTierColor(bonus), XpTierRadius(bonus), 15.0f, PickupType::XP });
        }
        if (GetRandomValue(1, 100) <= (int)(30 * luck)) pickups.push_back({ { position.x - 14.0f, position.y + 6.0f }, 1, WHITE, 10.0f, 0.0f, PickupType::FOOD });
    } else {
        // Sjeldne godbiter
        int roll = GetRandomValue(1, 10000);
        if (roll <= (int)(25 * luck)) pickups.push_back({ { position.x, position.y - 10.0f }, 1, SKYBLUE, 12.0f, 0.0f, PickupType::VACUUM });
        else if (roll <= (int)(85 * luck)) pickups.push_back({ { position.x, position.y - 10.0f }, 1, WHITE, 10.0f, 0.0f, PickupType::FOOD });
    }

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
    hp = 260;       // Middels: vanlig fotsoldat
    maxHp = 260;
    damage = 10;
    xpValue = 15;
    orbColor = BLUE;
    goldChance = 0.018f;
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
    hp = 1100;      // Tank: treg, men tåler MYE og slår hardt
    maxHp = 1100;
    damage = 25;
    xpValue = 40;
    hitRadius = 22.0f;
    orbColor = GREEN;
    goldChance = 0.06f;
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
    hp = 40;
    maxHp = 40;
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

Boss::Boss(Vector2 spawnPos, Texture2D tex) {
    position = spawnPos;
    speed = 150.0f;
    // Sluttbossen: skal ta 1-2 minutter med et godt build. Echelon-effekter legges på i tillegg.
    hp = 250000;
    maxHp = hp;
    damage = 45;
    xpValue = 0;
    orbColor = MAROON;
    orbRadius = 0.0f;
    hitRadius = 40.0f;
    knockbackScale = 0.0f;
    texture = tex;
}

void Boss::announce(const char* text) {
    announcement = text;
    enragedAt = (float)GetTime();
    PlaySfx(Sfx::BOSS_GONG);
    AddCameraShake(0.8f);
}

void Boss::shootRing(int count, float speed, float offset, float dmg) {
    for (int i = 0; i < count; i++) {
        float a = offset + (float)i / count * 2.0f * PI;
        SpawnEnemyShot(position, { cosf(a), sinf(a) }, speed, dmg);
    }
}

void Boss::shootFan(Vector2 dir, int count, float spreadDeg, float speed, float dmg) {
    for (int i = 0; i < count; i++) {
        float off = (count > 1 ? (i / (float)(count - 1) - 0.5f) : 0.0f) * spreadDeg * DEG2RAD;
        SpawnEnemyShot(position, Vector2Rotate(dir, off), speed, dmg);
    }
}

void Boss::update(Vector2 playerPosition) {
    float dt = GetFrameTime();
    lastPlayerPos = playerPosition;
    phaseTimer += dt;
    Vector2 toPlayer = Vector2Normalize(Vector2Subtract(playerPosition, position));
    float shotDmg = damage * 0.5f;

    // --- Dekreter: røde sirkler som eksploderer etter en stund ---
    for (size_t i = 0; i < decrees.size(); ) {
        decrees[i].t += dt / 1.3f;
        if (decrees[i].t >= 1.0f) {
            SpawnExplosion(decrees[i].pos, 95.0f, damage * 1.2f);
            VfxExplosion(decrees[i].pos, 80.0f);
            decrees[i] = decrees.back();
            decrees.pop_back();
            continue;
        }
        i++;
    }

    // --- Fasebytter ---
    if (mode == Mode::THRONE && hp <= maxHp * 55 / 100) {
        mode = Mode::LEAP;
        phaseTimer = 0.0f;
        leapFrom = position;
        leapTo = Vector2Add(position, Vector2Scale(Vector2Subtract(playerPosition, position), 0.6f));
        announce("KONGEN REISER SEG!");
    }
    // I fase 2 står kongen midt i alle AOE-våpnene dine, så han har rustning der
    damageTakenMult = mode == Mode::HUNT ? (enraged ? 0.45f : 0.55f) : 1.0f;
    if (mode == Mode::HUNT && !enraged && hp <= maxHp / 4) {
        enraged = true;
        speed *= 1.35f;
        summonsRequested += 8;
        announce("KONGEN ER RASENDE!");
        shootRing(32, 260.0f, 0.0f, shotDmg);
    }

    switch (mode) {
    // ================= FASE 1: PÅ TRONEN =================
    case Mode::THRONE: {
        position = Vector2Lerp(position, thronePos, std::min(1.0f, 3.0f * dt)); // Går opp og setter seg
        facing = toPlayer;
        if (attackTimer > 0.0f) {
            attackTimer -= dt;
            if (attackTimer <= 0.0f) { patternTime = 0.0f; shotTimer = 0.0f; volleysLeft = 3; PlaySfx(Sfx::BOSS_CHARGE); }
            break;
        }
        patternTime += dt;
        shotTimer -= dt;
        bool done = false;
        switch (throneAttack % 4) {
            case 0: // Dobbel spiral av septerkuler
                if (shotTimer <= 0.0f) {
                    shotTimer = 0.09f;
                    spiralAngle += 0.23f;
                    for (int k = 0; k < 3; k++) {
                        float a = spiralAngle + k * 2.0f * PI / 3.0f;
                        SpawnEnemyShot(position, { cosf(a), sinf(a) }, 210.0f, shotDmg);
                    }
                }
                done = patternTime > 4.0f;
                break;
            case 1: // Vifter rett mot spilleren
                if (shotTimer <= 0.0f && volleysLeft > 0) {
                    shotTimer = 0.45f;
                    volleysLeft--;
                    shootFan(toPlayer, 11, 70.0f, 330.0f, shotDmg);
                    PlaySfxPitch(Sfx::ZAP, 0.8f);
                }
                done = volleysLeft == 0 && shotTimer <= 0.0f;
                break;
            case 2: // Kongelige dekreter: røde sirkler der spilleren står og er på vei
                if (shotTimer <= 0.0f && volleysLeft > 0) {
                    shotTimer = 0.5f;
                    volleysLeft--;
                    decrees.push_back({ playerPosition, 0.0f });
                    for (int k = 0; k < 3; k++) {
                        float a = GetRandomValue(0, 628) / 100.0f;
                        float d = (float)GetRandomValue(110, 220);
                        decrees.push_back({ { playerPosition.x + cosf(a) * d, playerPosition.y + sinf(a) * d }, 0.0f });
                    }
                }
                done = volleysLeft == 0 && shotTimer <= 0.0f;
                break;
            case 3: // Vaktene kommer + en ring av kuler
                summonsRequested += 6;
                shootRing(24, 180.0f, spiralAngle, shotDmg);
                done = true;
                break;
        }
        if (done) { throneAttack++; attackTimer = 1.1f; }
        break;
    }
    // ================= OVERGANG: HOPPER NED =================
    case Mode::LEAP: {
        float t = std::min(1.0f, phaseTimer / 0.9f);
        position = Vector2Lerp(leapFrom, leapTo, t);
        if (t >= 1.0f) {
            SpawnExplosion(position, 220.0f, damage * 1.3f);
            VfxShockwave(position, 260.0f, Color{ 255, 80, 40, 255 });
            VfxExplosion(position, 160.0f);
            shootRing(28, 240.0f, 0.0f, shotDmg);
            summonsRequested += 6;
            mode = Mode::HUNT;
            phase = Phase::CHASE;
            phaseTimer = 0.0f;
            dashesLeft = 0;
        }
        break;
    }
    // ================= FASE 2: JAKTEN =================
    case Mode::HUNT: {
        summonTimer += dt;
        if (summonTimer >= (enraged ? 9.0f : 14.0f)) { summonTimer = 0.0f; summonsRequested += enraged ? 6 : 4; }
        float windup = enraged ? 0.5f : 0.7f;
        float dashSpeed = enraged ? 1000.0f : 820.0f;
        switch (phase) {
            case Phase::CHASE:
                facing = toPlayer;
                position = Vector2Add(position, Vector2Scale(toPlayer, speed * dt));
                if (enraged) { // Spiral mens han jager
                    chaseShotTimer -= dt;
                    if (chaseShotTimer <= 0.0f) {
                        chaseShotTimer = 0.2f;
                        spiralAngle += 0.4f;
                        shootRing(4, 200.0f, spiralAngle, shotDmg);
                    }
                }
                if (phaseTimer >= (enraged ? 1.4f : 2.2f)) {
                    phaseTimer = 0.0f;
                    // Annenhver gang: tre storminger på rad, eller et tramp
                    if (dashesLeft <= 0 && GetRandomValue(0, 2) == 0) {
                        phase = Phase::STOMP;
                        PlaySfxPitch(Sfx::BOSS_CHARGE, 0.8f);
                    } else {
                        if (dashesLeft <= 0) dashesLeft = 3;
                        phase = Phase::WINDUP;
                        PlaySfx(Sfx::BOSS_CHARGE);
                    }
                }
                break;
            case Phase::WINDUP:
                dashDirection = toPlayer;
                facing = toPlayer;
                if (phaseTimer >= windup) { phase = Phase::DASH; phaseTimer = 0.0f; }
                break;
            case Phase::DASH:
                position = Vector2Add(position, Vector2Scale(dashDirection, dashSpeed * dt));
                if (phaseTimer >= 0.45f) {
                    phaseTimer = 0.0f;
                    dashesLeft--;
                    shootFan(Vector2Scale(dashDirection, -1.0f), 7, 120.0f, 220.0f, shotDmg); // Kuler bak seg
                    phase = dashesLeft > 0 ? Phase::WINDUP : Phase::CHASE;
                    if (phase == Phase::WINDUP) phaseTimer = windup * 0.4f; // Kortere sikting mellom stormene
                }
                break;
            case Phase::STOMP:
                if (phaseTimer >= 1.0f) {
                    SpawnExplosion(position, 230.0f, damage * 1.4f);
                    VfxShockwave(position, 260.0f, Color{ 255, 120, 40, 255 });
                    AddCameraShake(0.7f);
                    PlaySfx(Sfx::EXPLOSION);
                    shootRing(enraged ? 36 : 24, 230.0f, spiralAngle, shotDmg);
                    phase = Phase::CHASE;
                    phaseTimer = 0.0f;
                }
                break;
        }
        break;
    }
    }
}

// Retningen kongen ser: mot spilleren, eller låst i dash-retningen
static Vector2 kingFacing(bool locked, Vector2 dashDirection, Vector2 from, Vector2 to) {
    return locked ? dashDirection : Vector2Normalize(Vector2Subtract(to, from));
}

void Boss::draw() const {
    // Dekreter: rød sirkel som fylles opp før den eksploderer
    for (const auto& d : decrees) {
        DrawCircleV(d.pos, 95.0f, Fade(RED, 0.12f + 0.12f * d.t));
        DrawCircleV(d.pos, 95.0f * d.t, Fade(RED, 0.25f));
        DrawRing(d.pos, 91.0f, 95.0f, 0, 360, 40, Fade(RED, 0.8f));
    }
    // Tramp: stor ring rundt kongen
    if (mode == Mode::HUNT && phase == Phase::STOMP) {
        float t = phaseTimer;
        DrawCircleV(position, 230.0f, Fade(Color{ 255, 90, 30, 255 }, 0.1f + 0.15f * t));
        DrawCircleV(position, 230.0f * t, Fade(Color{ 255, 90, 30, 255 }, 0.2f));
        DrawRing(position, 225.0f, 230.0f, 0, 360, 64, Fade(RED, 0.8f));
    }
    // Landingssted når han hopper ned fra tronen
    if (mode == Mode::LEAP) DrawRing(leapTo, 200.0f, 220.0f, 0, 360, 64, Fade(RED, 0.7f));
    // På gulvet: varsel-felt for Royal Charge + skygge
    if (phase == Phase::WINDUP) {
        float t = std::min(1.0f, phaseTimer / 0.7f);
        Vector2 end = Vector2Add(position, Vector2Scale(dashDirection, (enraged ? 1000.0f : 820.0f) * 0.45f));
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
        float k = std::min(1.0f, phaseTimer / 0.7f);
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
    hp = 70;
    maxHp = 70;
    damage = 25;
    xpValue = 12;
    orbColor = ORANGE;
    orbRadius = 5.0f;
    goldChance = 0.024f;
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

Color Exploder::spriteTint() const {
    // Blinker rødt når lunta er tent
    if (!fuseLit) return WHITE;
    return ((int)(fuseTimer * 20.0f) % 2 == 0) ? Color{ 255, 120, 110, 255 } : Color{ 255, 60, 50, 255 };
}

void Exploder::applyEchelonModifiers(float hpMult, float damageMult, float speedMult) {
    Enemy::applyEchelonModifiers(hpMult, damageMult, speedMult);
    explosionDamage *= damageMult;
}

// =====================================================================
// ARMBRØSTSKYTTER og fiende-prosjektiler
// =====================================================================
namespace {
    constexpr float ARCHER_RANGE = 340.0f;      // Avstanden den prøver å holde
    constexpr float ARCHER_AIM_TIME = 0.55f;    // Tid spilleren har til å flytte seg
    constexpr float ARCHER_COOLDOWN = 2.4f;
    constexpr float SHOT_SPEED = 330.0f;

    struct EnemyShot {
        Vector2 position;
        Vector2 direction;
        float speed;
        float damage;
        float life;
    };
    std::vector<EnemyShot> enemyShots;
}

void SpawnEnemyShot(Vector2 from, Vector2 dir, float speed, float damage) {
    enemyShots.push_back({ from, dir, speed, damage, 3.0f });
}

float UpdateEnemyShots(float deltaTime, Vector2 playerPos, float playerRadius) {
    float damageToPlayer = 0.0f;
    for (size_t i = 0; i < enemyShots.size(); ) {
        EnemyShot& s = enemyShots[i];
        s.position = Vector2Add(s.position, Vector2Scale(s.direction, s.speed * deltaTime));
        s.life -= deltaTime;
        bool hit = Vector2Distance(s.position, playerPos) < playerRadius + 5.0f;
        if (hit) {
            damageToPlayer += s.damage;
            VfxHit(s.position, Color{ 255, 90, 60, 255 });
        }
        if (hit || s.life <= 0.0f) { enemyShots[i] = enemyShots.back(); enemyShots.pop_back(); }
        else i++;
    }
    return damageToPlayer;
}

void DrawEnemyShots3D() {
    for (const EnemyShot& s : enemyShots) {
        Vector2 tail = Vector2Subtract(s.position, Vector2Scale(s.direction, 16.0f));
        Vector2 tip = Vector2Add(s.position, Vector2Scale(s.direction, 6.0f));
        ShadedCylinder(ToWorld3D(tail, 22.0f), ToWorld3D(s.position, 22.0f), 1.0f, 1.0f, WOOD, 4);
        ShadedCylinder(ToWorld3D(s.position, 22.0f), ToWorld3D(tip, 22.0f), 2.2f, 0.0f, STEEL, 4);
    }
}

void DrawEnemyShotsVfx() {
    for (const EnemyShot& s : enemyShots) {
        VfxBillboard(VfxTex::GLOW, ToWorld3D(s.position, 22.0f), 22.0f, Color{ 255, 70, 40, 255 });
        VfxTrail(ToWorld3D(s.position, 22.0f), Color{ 200, 50, 30, 255 }, 9.0f, 0.12f);
    }
}

void ClearEnemyShots() { enemyShots.clear(); }

Archer::Archer(Vector2 spawnPos, Texture2D tex) {
    position = spawnPos;
    speed = 120.0f;
    hp = 170;
    maxHp = 170;
    damage = 14;         // Pilene gjør full skade, berøring halv
    xpValue = 22;
    orbColor = Color{ 60, 150, 70, 255 };
    goldChance = 0.036f;
    goldValue = 1;
    orbRadius = 6.0f;
    texture = tex;
    shootTimer = 1.0f + (id % 10) * 0.12f; // Så ikke alle skyter samtidig
}

void Archer::update(Vector2 playerPosition) {
    float dt = GetFrameTime();
    Vector2 toPlayer = Vector2Subtract(playerPosition, position);
    float dist = Vector2Length(toPlayer);
    Vector2 dir = dist > 0.01f ? Vector2Scale(toPlayer, 1.0f / dist) : Vector2{ 0, 1 };
    facing = aimTimer > 0.0f ? aimDir : dir;

    if (aimTimer > 0.0f) {
        // Står stille og sikter – retningen er låst, så spilleren kan unngå pila
        aimTimer -= dt;
        if (aimTimer <= 0.0f) {
            SpawnEnemyShot(Vector2Add(position, Vector2Scale(aimDir, 14.0f)), aimDir, SHOT_SPEED, (float)damage);
            shootTimer = ARCHER_COOLDOWN;
        }
        return;
    }

    // Hold passe avstand: gå nærmere hvis for langt unna, rygg hvis for nær
    if (dist > ARCHER_RANGE + 40.0f) position = Vector2Add(position, Vector2Scale(dir, speed * dt));
    else if (dist < ARCHER_RANGE - 80.0f) position = Vector2Subtract(position, Vector2Scale(dir, speed * 0.7f * dt));

    shootTimer -= dt;
    if (shootTimer <= 0.0f && dist < ARCHER_RANGE + 120.0f) {
        aimTimer = ARCHER_AIM_TIME;
        aimDir = dir;
    }
}

void Archer::draw() const {
    // Varsel-linje på gulvet mens den sikter
    if (aimTimer > 0.0f) {
        float t = 1.0f - aimTimer / ARCHER_AIM_TIME;
        Vector2 end = Vector2Add(position, Vector2Scale(aimDir, SHOT_SPEED * 1.6f));
        DrawLineEx(position, end, 3.0f + 5.0f * t, Fade(RED, 0.25f + 0.45f * t));
    }
    Enemy::draw();
}

void Archer::draw3D() const {
    const Color HOOD = { 45, 85, 50, 255 };
    const Color CLOAK = { 60, 110, 65, 255 };
    Rig r(position, facing);
    bool aiming = aimTimer > 0.0f;
    float w = walkCycle();
    float step = aiming ? 0.0f : sinf(w);
    float bob = aiming ? 0.0f : fabsf(cosf(w)) * 2.0f;

    for (int s = -1; s <= 1; s += 2) {
        r.limb(r.at(step * 3.0f * s, s * 3.5f, 2.0f), r.at(0.0f, s * 3.5f, 13.0f + bob), 2.4f, 2.8f, LEATHER);
    }
    // Kappe (kjegle) og hette
    r.limb(r.at(-1.0f, 0, 3.0f + bob), r.at(0, 0, 30.0f + bob), 11.0f, 6.5f, CLOAK, 7);
    r.ball(0.5f, 0, 33.0f + bob, 5.8f, SKIN);
    r.limb(r.at(-1.5f, 0, 31.0f + bob), r.at(-4.0f, 0, 45.0f + bob), 7.5f, 0.0f, HOOD, 7);      // Spiss hette
    r.blob(4.5f, 0.0f, 33.5f + bob, { 1.5f, 2.5f, 4.0f }, EYE_BLACK, 3, 4);                     // Skygge i hetta
    r.blob(-6.5f, 5.0f, 26.0f + bob, { 2.5f, 7.0f, 2.5f }, LEATHER, 3, 5);                      // Kogger
    // Armbrøst foran seg (løftes når den sikter)
    float up = aiming ? 24.0f : 18.0f;
    Vector3 stockBack = r.at(2.0f, 1.0f, up + bob);
    Vector3 stockFront = r.at(15.0f, 1.0f, up + bob);
    r.limb(stockBack, stockFront, 1.6f, 1.4f, WOOD, 5);
    r.limb(r.at(13.0f, -7.0f, up + bob), r.at(13.0f, 9.0f, up + bob), 1.1f, 1.1f, Color{ 80, 55, 35, 255 }, 4);
    r.ball(4.0f, 5.0f, up - 1.0f + bob, 2.5f, SKIN, 3, 4);
}

void Archer::drawVfx() const {
    Enemy::drawVfx();
    if (aimTimer > 0.0f) {
        // Rødt glimt i armbrøsten rett før den skyter
        float t = 1.0f - aimTimer / ARCHER_AIM_TIME;
        Rig r(position, facing);
        VfxBillboard(VfxTex::SPARK, r.at(15.0f, 1.0f, 24.0f), 10.0f + 26.0f * t, Color{ 255, 80, 60, 255 }, t * 180.0f);
    }
}

// =====================================================================
// 3D-MODELLER FOR KONGENS TJENERE (hund, prest, trommeslager, kanonér)
// Oppførselen ligger i servants.cpp. Samme livré som vaktene: karmosin og gull.
// =====================================================================
namespace {
    const Color LIVERY = { 165, 30, 45, 255 };
    const Color LIVERY_GOLD = { 225, 180, 60, 255 };
    const Color NAVY = { 40, 50, 95, 255 };
}

// Hund: lang, slank kropp på fire bein, spiss snute, livré-dekken på ryggen
void Hound::draw3D() const {
    const Color FUR = { 78, 78, 90, 255 };
    const Color FUR_DARK = { 55, 55, 66, 255 };
    Rig r(position, facing);
    float w = walkCycle() * 1.3f;
    bool crouch = state == State::CROUCH;
    bool lunge = state == State::LUNGE;
    float gait = crouch ? 0.0f : sinf(w) * (lunge ? 1.6f : 1.0f);
    float low = crouch ? -5.0f : 0.0f;                        // Kryper sammen før hoppet
    float bob = crouch ? 0.0f : fabsf(cosf(w)) * 1.5f;
    float body = 15.0f + bob + low + (lunge ? 4.0f : 0.0f);

    // Fire bein: venstre foran går sammen med høyre bak (trav)
    for (int k = 0; k < 4; k++) {
        float fwd = (k < 2) ? 8.0f : -8.0f;
        float side = (k % 2 == 0) ? -3.5f : 3.5f;
        float phase = ((k == 0 || k == 3) ? 1.0f : -1.0f) * gait;
        Vector3 hip = r.at(fwd, side, body - 2.0f);
        Vector3 paw = r.at(fwd + phase * 5.0f, side, 1.5f + fmaxf(0.0f, phase) * 2.0f);
        r.limb(hip, paw, 2.0f, 1.4f, k < 2 ? FUR : FUR_DARK, 5);
        ShadedSphere(paw, 1.8f, FUR_DARK, 3, 4);
    }
    // Kropp, bryst og dekken
    r.blob(0.0f, 0.0f, body, { 13.0f, 5.5f, 5.0f }, FUR);
    r.blob(8.0f, 0.0f, body - 1.0f, { 5.5f, 6.0f, 5.0f }, FUR);
    r.blob(-1.0f, 0.0f, body + 3.0f, { 8.0f, 2.5f, 6.0f }, LIVERY, 4, 6);
    r.blob(-1.0f, 0.0f, body + 5.2f, { 3.0f, 0.8f, 3.0f }, LIVERY_GOLD, 3, 5);
    // Hode med snute, ører og halsbånd
    float head = body + 5.0f + (crouch ? -3.0f : 0.0f);
    r.limb(r.at(10.0f, 0, body + 1.0f), r.at(15.0f, 0, head), 3.2f, 2.8f, FUR, 6);
    r.limb(r.at(12.0f, 0, body + 3.0f), r.at(13.0f, 0, body + 4.0f), 3.6f, 3.6f, LIVERY, 6);         // Halsbånd
    r.ball(16.0f, 0, head, 4.2f, FUR, 5, 7);
    r.blob(21.0f, 0, head - 1.2f, { 4.5f, 2.2f, 2.2f }, FUR_DARK, 4, 6);                          // Snute
    r.ball(25.0f, 0, head - 0.8f, 1.2f, EYE_BLACK, 3, 4);                                          // Nese
    for (int s = -1; s <= 1; s += 2) {
        r.ball(18.5f, s * 2.4f, head + 1.4f, 1.0f, Color{ 255, 70, 50, 255 }, 3, 4);              // Sinte øyne
        r.limb(r.at(15.0f, s * 2.5f, head + 3.0f), r.at(13.0f, s * 3.5f, head + 7.5f), 1.6f, 0.2f, FUR_DARK, 4);
    }
    // Hale
    r.limb(r.at(-12.0f, 0, body + 1.0f), r.at(-19.0f, sinf(w * 2.0f) * 2.0f, body + 6.0f), 1.4f, 0.5f, FUR, 4);
}

// Hoffprest: lang kremhvit kjortel med karmosin stola, skallet hode og gyllent røkelseskar
void Priest::draw3D() const {
    const Color ROBE = { 236, 228, 205, 255 };
    Rig r(position, facing);
    float w = walkCycle() * 0.8f;
    float bob = fabsf(cosf(w)) * 1.2f;
    float sway = sinf(w) * 1.0f;

    r.limb(r.at(0, sway * 0.3f, 0.5f), r.at(0, sway, 30.0f + bob), 12.0f, 7.5f, ROBE, 10);            // Kjortel
    for (int s = -1; s <= 1; s += 2)                                                                    // Stola
        r.limb(r.at(7.0f, sway + s * 3.0f, 4.0f), r.at(5.5f, sway + s * 3.2f, 30.0f + bob), 1.4f, 1.4f, LIVERY, 4);
    r.limb(r.at(0, sway, 29.0f + bob), r.at(0, sway, 31.0f + bob), 8.0f, 8.0f, LIVERY_GOLD, 8);        // Gullkant
    // Rundt, skallet hode med grå krans
    float head = 37.0f + bob;
    r.ball(0.5f, sway, head, 6.8f, SKIN, 6, 8);
    r.blob(-2.0f, sway, head - 0.5f, { 4.0f, 3.0f, 7.2f }, Color{ 170, 170, 175, 255 }, 4, 6);
    for (int s = -1; s <= 1; s += 2) r.ball(6.0f, sway + s * 2.3f, head + 0.5f, 1.0f, EYE_BLACK, 3, 4);
    // Arm og røkelseskar som svinger i kjedet
    Vector3 hand = r.at(8.0f, sway + 9.0f, 24.0f + bob);
    r.limb(r.at(1.0f, sway + 7.0f, 28.0f + bob), hand, 2.8f, 2.4f, ROBE, 5);
    ShadedSphere(hand, 2.2f, SKIN, 3, 5);
    float swing = sinf((float)GetTime() * 3.0f + id) * 4.0f;
    Vector3 censer = r.at(10.0f + swing, sway + 10.0f, 13.0f + bob);
    r.limb(hand, censer, 0.4f, 0.4f, LIVERY_GOLD, 3);
    ShadedSphere(censer, 3.4f, LIVERY_GOLD, 4, 6);
}

// Trommeslager: høy karmosin shako, snorer på jakka, stor tromme på magen
void Drummer::draw3D() const {
    Rig r(position, facing);
    float w = walkCycle();
    float step = sinf(w);
    float bob = fabsf(cosf(w)) * 2.0f;

    for (int s = -1; s <= 1; s += 2) {
        float st = step * 4.0f * s;
        r.limb(r.at(st * 0.5f, s * 3.8f, 2.0f), r.at(0.0f, s * 3.8f, 13.0f + bob), 2.6f, 3.0f, NAVY);
        r.blob(2.0f + st, s * 3.8f, 2.0f, { 4.2f, 2.0f, 2.6f }, EYE_BLACK, 4, 6);
    }
    r.limb(r.at(0, 0, 12.0f + bob), r.at(0, 0, 29.0f + bob), 8.0f, 7.5f, LIVERY, 8);                  // Jakke
    for (int i = 0; i < 3; i++) r.blob(7.2f, 0.0f, 19.0f + i * 3.5f + bob, { 0.8f, 0.8f, 4.5f }, LIVERY_GOLD, 3, 4); // Snorer
    // Hode og shako
    float head = 34.0f + bob;
    r.ball(0.5f, 0, head, 5.8f, SKIN, 5, 7);
    for (int s = -1; s <= 1; s += 2) r.ball(5.2f, s * 2.0f, head + 0.8f, 0.9f, EYE_BLACK, 3, 4);
    r.limb(r.at(0, 0, head + 3.0f), r.at(-0.5f, 0, head + 16.0f), 5.8f, 6.8f, LIVERY, 8);
    r.limb(r.at(0, 0, head + 3.0f), r.at(0, 0, head + 5.0f), 6.3f, 6.3f, NAVY, 8);                    // Skygge
    r.limb(r.at(-0.4f, 0, head + 12.0f), r.at(-0.5f, 0, head + 13.5f), 6.7f, 6.8f, LIVERY_GOLD, 8);
    // Trommen foran magen (liggende sylinder på tvers)
    Vector3 drumL = r.at(10.0f, -6.5f, 17.0f + bob), drumR = r.at(10.0f, 6.5f, 17.0f + bob);
    ShadedCylinder(drumL, drumR, 8.0f, 8.0f, Color{ 45, 75, 170, 255 }, 12);
    ShadedCylinder(r.at(10.0f, -7.2f, 17.0f + bob), r.at(10.0f, -6.0f, 17.0f + bob), 8.6f, 8.6f, LIVERY_GOLD, 12);
    ShadedCylinder(r.at(10.0f, 6.0f, 17.0f + bob), r.at(10.0f, 7.2f, 17.0f + bob), 8.6f, 8.6f, LIVERY_GOLD, 12);
    // Stikkene slår i takt (to slag i sekundet)
    float beat = fabsf(sinf((float)GetTime() * PI * 2.0f + id));
    for (int s = -1; s <= 1; s += 2) {
        float lift = (s < 0 ? beat : 1.0f - beat) * 9.0f;
        Vector3 hand = r.at(8.0f, s * 8.0f, 26.0f + bob + lift * 0.4f);
        r.limb(r.at(0, s * 7.5f, 27.0f + bob), hand, 2.2f, 2.0f, LIVERY, 5);
        r.limb(hand, r.at(15.0f, s * 3.0f, 24.0f + bob + lift), 0.8f, 0.8f, WOOD, 4);
    }
}

// Kanonér: kraftig kar med skjegg, alpelue, lærforkle og en bronsemorter på skulderen
void Cannoneer::draw3D() const {
    const Color BRONZE = { 190, 140, 70, 255 };
    Rig r(position, facing);
    bool aiming = windup > 0.0f;
    float w = walkCycle() * 0.8f;
    float step = aiming ? 0.0f : sinf(w);
    float bob = aiming ? 0.0f : fabsf(cosf(w)) * 2.0f;

    for (int s = -1; s <= 1; s += 2) {
        float st = step * 4.0f * s;
        r.limb(r.at(st * 0.5f, s * 5.0f, 2.0f), r.at(0.0f, s * 5.0f, 12.0f + bob), 3.2f, 3.6f, NAVY);
        r.blob(2.0f + st, s * 5.0f, 2.5f, { 5.0f, 2.8f, 3.4f }, LEATHER, 4, 6);
    }
    r.blob(0.0f, 0.0f, 21.0f + bob, { 10.0f, 11.0f, 10.5f }, LIVERY);                                // Kraftig kropp
    r.blob(5.0f, 0.0f, 18.0f + bob, { 6.0f, 9.0f, 8.0f }, LEATHER);                                  // Forkle
    float head = 35.0f + bob;
    r.ball(0.5f, 0, head, 6.2f, SKIN, 5, 7);
    r.blob(4.5f, 0, head - 4.0f, { 4.0f, 5.0f, 5.0f }, Color{ 110, 70, 40, 255 }, 4, 6);             // Skjegg
    r.ball(6.0f, 0, head + 0.5f, 1.8f, Color{ 230, 120, 110, 255 }, 3, 4);                            // Rød nese
    r.blob(-1.0f, 0, head + 5.0f, { 7.5f, 2.5f, 7.5f }, LIVERY, 4, 7);                               // Alpelue
    // Morteren på høyre skulder: løftes og siktes når han fyrer
    float angle = aiming ? 38.0f : 12.0f;
    float a = angle * DEG2RAD;
    Vector3 back = r.at(-6.0f, 9.0f, 30.0f + bob);
    Vector3 muzzle = r.at(-6.0f + cosf(a) * 20.0f, 9.0f, 30.0f + bob + sinf(a) * 20.0f);
    ShadedCylinder(back, muzzle, 4.5f, 5.5f, BRONZE, 10);
    ShadedSphere(back, 4.8f, BRONZE, 4, 6);
    r.limb(r.at(0, 10.0f, 28.0f + bob), r.at(5.0f, 11.0f, 32.0f + bob), 3.2f, 2.8f, LIVERY, 5);    // Arm som holder
    // Lunte i venstre hånd
    Vector3 hand = r.at(8.0f, -9.0f, 18.0f + bob);
    r.limb(r.at(0, -9.5f, 27.0f + bob), hand, 3.2f, 2.8f, LIVERY, 5);
    r.limb(hand, r.at(13.0f, -9.0f, 24.0f + bob), 0.7f, 0.7f, WOOD, 4);
}

// Skattmester: rund kar i karmosin frakk med gullkjede, flosshatt og en diger pengesekk på ryggen
void Treasurer::draw3D() const {
    const Color SACK = { 150, 110, 70, 255 };
    Rig r(position, facing);
    float w = walkCycle() * 1.2f;
    float step = sinf(w);
    float bob = fabsf(cosf(w)) * 2.5f;

    for (int s = -1; s <= 1; s += 2) {
        float st = step * 4.5f * s;
        r.limb(r.at(st * 0.5f, s * 4.5f, 2.0f), r.at(0.0f, s * 4.5f, 11.0f + bob), 2.8f, 3.2f, NAVY);
        r.blob(2.0f + st, s * 4.5f, 2.0f, { 4.5f, 2.2f, 2.8f }, EYE_BLACK, 4, 6);
    }
    r.blob(1.0f, 0.0f, 20.0f + bob, { 11.0f, 11.0f, 10.0f }, LIVERY);                                // Rund mage
    r.blob(9.0f, 0.0f, 22.0f + bob, { 2.0f, 5.0f, 5.0f }, Color{ 240, 230, 210, 255 }, 4, 5);        // Vest
    for (int i = -2; i <= 2; i++) r.ball(10.5f, i * 2.2f, 25.0f - i * i * 0.6f + bob, 1.0f, LIVERY_GOLD, 3, 4); // Gullkjede
    float head = 35.0f + bob;
    r.ball(1.0f, 0, head, 6.5f, SKIN, 5, 7);
    r.ball(6.5f, 0, head - 0.5f, 2.0f, Color{ 235, 140, 120, 255 }, 3, 4);                             // Nese
    r.ball(5.8f, 2.4f, head + 1.5f, 1.6f, LIVERY_GOLD, 3, 5);                                          // Monokkel
    r.blob(5.5f, 0, head - 3.0f, { 2.0f, 1.2f, 5.0f }, Color{ 90, 60, 40, 255 }, 3, 5);              // Bart
    r.limb(r.at(0, 0, head + 4.0f), r.at(0, 0, head + 5.0f), 8.5f, 8.5f, EYE_BLACK, 10);            // Hattebrem
    r.limb(r.at(0, 0, head + 5.0f), r.at(-0.5f, 0, head + 15.0f), 5.5f, 6.0f, EYE_BLACK, 8);        // Flosshatt
    r.limb(r.at(0, 0, head + 6.0f), r.at(0, 0, head + 7.5f), 5.7f, 5.8f, LIVERY_GOLD, 8);
    // Pengesekken på ryggen, med mynter som titter opp
    r.blob(-11.0f, 0.0f, 28.0f + bob, { 9.0f, 11.0f, 9.0f }, SACK);
    r.limb(r.at(-11.0f, 0, 37.0f + bob), r.at(-11.0f, 0, 41.0f + bob), 3.0f, 4.5f, SACK, 6);
    for (int i = 0; i < 3; i++) r.ball(-11.0f + (i - 1) * 2.5f, (i - 1) * 1.5f, 42.0f + bob, 2.2f, LIVERY_GOLD, 3, 5);
    r.ball(-9.0f, -6.0f, 30.0f + bob, 2.0f, LIVERY_GOLD, 3, 5);
    // Armene holder sekken over skulderen
    for (int s = -1; s <= 1; s += 2)
        r.limb(r.at(2.0f, s * 9.0f, 27.0f + bob), r.at(-6.0f, s * 7.0f, 35.0f + bob), 2.6f, 2.2f, LIVERY, 5);
}
