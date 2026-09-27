#include "enemy.hpp"
#include "vfx.hpp"
#include "explosions.hpp"
#include "render3d.hpp"
#include "audio.hpp"

// =====================================================================
// TUNGE FIENDER: Kyrasser, Fanebærer og Beleiringskjempe
// Få og seige i stedet for flere småfiender sent i runden. Hver av dem har én tydelig
// ting du må reagere på: stormingen, vernet eller steinblokken.
// =====================================================================

namespace {
    const Color SKIN = { 236, 196, 160, 255 };
    const Color PLATE = { 150, 158, 175, 255 };
    const Color PLATE_DARK = { 70, 75, 90, 255 };
    const Color LIVERY = { 165, 30, 45, 255 };
    const Color LIVERY_GOLD = { 225, 180, 60, 255 };
    const Color WOOD = { 120, 80, 45, 255 };
    const Color LEATHER = { 90, 60, 40, 255 };
    const Color EYE_BLACK = { 20, 18, 24, 255 };

    // Samme hjelper som i enemy.cpp: deler plasseres relativt til fienden
    struct Rig {
        Vector2 pos, f, s;
        Rig(Vector2 p, Vector2 facing) : pos(p), f(facing) {
            if (Vector2Length(f) < 0.001f) f = { 0, 1 };
            s = { -f.y, f.x };
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
        void limb(Vector3 a, Vector3 b, float r0, float r1, Color c, int slices = 6) const {
            ShadedCylinder(a, b, r0, r1, c, slices);
        }
    };

    Vector2 dirTo(Vector2 from, Vector2 to, float* dist = nullptr) {
        Vector2 d = Vector2Subtract(to, from);
        float len = Vector2Length(d);
        if (dist) *dist = len;
        return len > 0.01f ? Vector2Scale(d, 1.0f / len) : Vector2{ 0, 1 };
    }

    constexpr float KNIGHT_AIM = 0.9f;
    constexpr float KNIGHT_CHARGE = 0.55f;
    constexpr float KNIGHT_CHARGE_SPEED = 640.0f;
    constexpr float GIANT_WINDUP = 0.9f;
}

// =====================================================================
// Kyrasser
// =====================================================================
Knight::Knight(Vector2 spawnPos, Texture2D tex) {
    position = spawnPos;
    speed = 95.0f;
    hp = maxHp = 1200;
    damage = 30;
    xpValue = 120;
    orbColor = PLATE;
    orbRadius = 9.0f;
    goldChance = 0.12f;
    goldValue = 3;
    hitRadius = 22.0f;
    modelScale = 1.2f;
    knockbackScale = 0.2f;
    texture = tex;
    timer = 1.5f + (id % 5) * 0.4f;
}

void Knight::update(Vector2 playerPosition) {
    float dt = GetFrameTime();
    float dist;
    Vector2 dir = dirTo(position, playerPosition, &dist);
    timer -= dt;
    switch (phase) {
        case Phase::WALK:
            facing = dir;
            position = Vector2Add(position, Vector2Scale(dir, speed * dt));
            if (timer <= 0.0f && dist < 430.0f) { phase = Phase::AIM; timer = KNIGHT_AIM; chargeDir = dir; }
            break;
        case Phase::AIM:
            // Følger deg med lansa til like før stormingen – så låser den retningen
            if (timer > 0.3f) chargeDir = Vector2Normalize(Vector2Lerp(chargeDir, dir, std::min(1.0f, 5.0f * dt)));
            facing = chargeDir;
            if (timer <= 0.0f) { phase = Phase::CHARGE; timer = KNIGHT_CHARGE; PlaySfxPitch(Sfx::BOSS_CHARGE, 1.4f); }
            break;
        case Phase::CHARGE:
            position = Vector2Add(position, Vector2Scale(chargeDir, KNIGHT_CHARGE_SPEED * dt));
            if (timer <= 0.0f) { phase = Phase::RECOVER; timer = 0.9f; }
            break;
        case Phase::RECOVER:
            // Puster ut etter stormingen – et vindu for å slå tilbake
            if (timer <= 0.0f) { phase = Phase::WALK; timer = 3.0f; }
            break;
    }
}

void Knight::draw() const {
    if (phase == Phase::AIM) {
        // Rødt felt der den kommer til å storme
        float t = 1.0f - timer / KNIGHT_AIM;
        Vector2 end = Vector2Add(position, Vector2Scale(chargeDir, KNIGHT_CHARGE_SPEED * KNIGHT_CHARGE));
        DrawLineEx(position, end, 26.0f + 14.0f * t, Fade(Color{ 200, 20, 30, 255 }, 0.18f + 0.35f * t));
        DrawLineEx(position, end, 4.0f, Fade(Color{ 255, 70, 60, 255 }, 0.5f + 0.5f * t));
    }
    Enemy::draw();
}

void Knight::drawVfx() const {
    Enemy::drawVfx();
    if (phase == Phase::CHARGE) {
        VfxTrail(ToWorld3D(position, 30.0f), Color{ 255, 120, 90, 255 }, 60.0f, 0.3f);
    } else if (phase == Phase::AIM) {
        float t = 1.0f - timer / KNIGHT_AIM;
        // Lansespissen gløder opp
        Vector2 tip = Vector2Add(position, Vector2Scale(facing, 34.0f * modelScale * ENEMY_VISUAL_SCALE));
        VfxBillboard(VfxTex::GLOW, ToWorld3D(tip, 30.0f * modelScale), 20.0f + 50.0f * t, Color{ 255, 60, 40, 255 });
    }
}

// Tung ridder i full rustning: stor hjelm med visir og karmosin fjærbusk, våpenkjole,
// store skulderplater, lanse i høyre hånd og et skjold med kongens farger
void Knight::draw3D() const {
    Rig r(position, facing);
    bool charging = phase == Phase::CHARGE;
    bool aiming = phase == Phase::AIM;
    float w = walkCycle() * (charging ? 1.8f : 0.8f);
    float step = (aiming || phase == Phase::RECOVER) ? 0.0f : sinf(w);
    float bob = fabsf(cosf(w)) * 2.0f;
    float lean = charging ? 4.0f : (aiming ? -1.5f : 0.0f); // Lener seg frem når den stormer

    // Bein i plate
    for (int s = -1; s <= 1; s += 2) {
        float st = step * 5.0f * s;
        r.limb(r.at(st * 0.5f, s * 5.0f, 2.0f), r.at(0.0f, s * 5.0f, 15.0f + bob), 3.8f, 4.4f, PLATE);
        r.ball(1.0f + st * 0.3f, s * 5.0f, 9.0f + bob, 3.6f, PLATE_DARK, 4, 6); // Kneplate
        r.blob(2.5f + st, s * 5.0f, 2.2f, { 5.5f, 2.4f, 3.4f }, PLATE_DARK, 4, 6);
    }
    // Brynje og våpenkjole
    r.limb(r.at(0, 0, 13.0f + bob), r.at(lean, 0, 34.0f + bob), 11.5f, 11.0f, PLATE, 10);
    r.limb(r.at(0.5f, 0, 12.5f + bob), r.at(lean + 0.5f, 0, 28.0f + bob), 12.0f, 11.3f, LIVERY, 10);
    r.blob(lean + 10.5f, 0.0f, 22.0f + bob, { 1.0f, 3.2f, 7.0f }, LIVERY_GOLD, 4, 5); // Gullkors
    r.blob(lean + 10.7f, 0.0f, 23.5f + bob, { 1.0f, 7.0f, 1.8f }, LIVERY_GOLD, 4, 5);
    r.limb(r.at(0, 0, 13.0f + bob), r.at(0, 0, 15.5f + bob), 12.3f, 12.2f, LEATHER, 10); // Belte
    // Store skulderplater
    for (int s = -1; s <= 1; s += 2) r.blob(lean, s * 11.5f, 34.0f + bob, { 6.5f, 5.0f, 5.5f }, PLATE, 5, 7);

    // Topphjelm med visir-spalte og fjærbusk
    float head = 43.0f + bob;
    r.limb(r.at(lean, 0, head - 6.0f), r.at(lean, 0, head + 7.0f), 7.8f, 7.2f, PLATE, 10);
    r.ball(lean, 0, head + 7.0f, 7.2f, PLATE, 5, 8);
    r.blob(lean + 7.2f, 0.0f, head + 1.5f, { 1.0f, 5.5f, 1.1f }, EYE_BLACK, 3, 5);
    r.limb(r.at(lean + 6.9f, 0.0f, head - 5.0f), r.at(lean + 7.4f, 0.0f, head + 9.0f), 1.0f, 1.0f, LIVERY_GOLD, 4); // Nesebeskytter
    r.blob(lean - 3.0f, 0.0f, head + 15.0f, { 9.0f, 4.0f, 2.4f }, Color{ 210, 30, 45, 255 }, 4, 6);

    // Skjold på venstre arm
    r.limb(r.at(lean, -11.0f, 33.0f + bob), r.at(lean + 5.0f, -14.0f, 22.0f + bob), 3.2f, 3.0f, PLATE);
    r.blob(lean + 8.0f, -15.0f, 22.0f + bob, { 1.8f, 10.0f, 12.0f }, LIVERY_GOLD, 5, 7);
    r.blob(lean + 8.8f, -15.0f, 22.0f + bob, { 1.6f, 8.6f, 10.4f }, LIVERY, 5, 7);

    // Lanse: senkes rett frem når den sikter og stormer, ellers holdt skrått opp
    Vector3 hand = r.at(lean + 6.0f, 12.0f, 24.0f + bob);
    r.limb(r.at(lean, 11.5f, 33.0f + bob), hand, 3.2f, 3.0f, PLATE);
    ShadedSphere(hand, 3.4f, PLATE_DARK, 4, 6);
    bool lowered = charging || aiming;
    Vector3 butt = lowered ? r.at(lean - 10.0f, 12.0f, 24.0f + bob) : r.at(-6.0f, 12.0f, 10.0f + bob);
    Vector3 tip = lowered ? r.at(lean + 42.0f, 10.0f, 26.0f + bob) : r.at(14.0f, 12.0f, 66.0f + bob);
    r.limb(butt, tip, 1.8f, 1.2f, WOOD, 6);
    Vector3 dir = Vector3Normalize(Vector3Subtract(tip, butt));
    r.limb(Vector3Add(hand, Vector3Scale(dir, 3.0f)), Vector3Add(hand, Vector3Scale(dir, -5.0f)), 5.0f, 2.0f, PLATE, 8); // Vamplate
    r.limb(tip, Vector3Add(tip, Vector3Scale(dir, 10.0f)), 2.8f, 0.0f, PLATE, 6);
}

// =====================================================================
// Fanebærer
// =====================================================================
Bannerman::Bannerman(Vector2 spawnPos, Texture2D tex) {
    position = spawnPos;
    speed = 105.0f;
    hp = maxHp = 600;
    damage = 14;
    xpValue = 70;
    orbColor = LIVERY;
    orbRadius = 7.0f;
    goldChance = 0.1f;
    goldValue = 3;
    hitRadius = 18.0f;
    modelScale = 1.1f;
    knockbackScale = 0.5f;
    texture = tex;
}

void Bannerman::update(Vector2 playerPosition) {
    // Holder seg bak de andre: går mot ca. 330 fra spilleren og rygger hvis du kommer nær
    float dt = GetFrameTime();
    float dist;
    Vector2 dir = dirTo(position, playerPosition, &dist);
    facing = dir;
    if (dist > 370.0f) position = Vector2Add(position, Vector2Scale(dir, speed * dt));
    else if (dist < 270.0f) position = Vector2Subtract(position, Vector2Scale(dir, speed * 0.8f * dt));
}

void Bannerman::drawVfx() const {
    Enemy::drawVfx();
    // Blå vern-ring på gulvet som viser hvor langt fanen rekker, og en glød rundt fanen
    float t = (float)GetTime();
    float pulse = 0.7f + 0.3f * sinf(t * 2.5f + id);
    VfxDecal(VfxTex::SHOCKWAVE, position, auraRadius() * 2.0f, Color{ (unsigned char)(40 * pulse), (unsigned char)(80 * pulse), (unsigned char)(170 * pulse), 255 }, t * 20.0f, 0.8f);
    VfxBillboard(VfxTex::GLOW, ToWorld3D(position, 70.0f * modelScale * ENEMY_VISUAL_SCALE), 70.0f, Color{ 60, 100, 200, 255 });
}

// Fanebærer i livré med stor kongelig fane på en høy stang. Fanen blafrer i vinden.
void Bannerman::draw3D() const {
    Rig r(position, facing);
    float w = walkCycle() * 0.9f;
    float step = sinf(w);
    float bob = fabsf(cosf(w)) * 1.8f;

    for (int s = -1; s <= 1; s += 2) {
        float st = step * 4.0f * s;
        r.limb(r.at(st * 0.5f, s * 3.8f, 2.0f), r.at(0.0f, s * 3.8f, 13.0f + bob), 2.6f, 3.0f, Color{ 40, 50, 95, 255 });
        r.blob(2.0f + st, s * 3.8f, 2.0f, { 4.2f, 2.0f, 2.6f }, EYE_BLACK, 4, 6);
    }
    r.limb(r.at(0, 0, 12.0f + bob), r.at(0, 0, 29.0f + bob), 8.0f, 7.5f, LIVERY, 8);
    r.limb(r.at(0, 0, 12.0f + bob), r.at(0, 0, 14.0f + bob), 8.4f, 8.2f, LIVERY_GOLD, 8);
    r.blob(3.0f, 0.0f, 24.0f + bob, { 6.0f, 6.0f, 6.5f }, PLATE); // Brystplate
    float head = 34.0f + bob;
    r.ball(0.5f, 0, head, 5.8f, SKIN, 5, 7);
    for (int s = -1; s <= 1; s += 2) r.ball(5.2f, s * 2.0f, head + 0.8f, 0.9f, EYE_BLACK, 3, 4);
    r.ball(0.0f, 0, head + 2.5f, 6.2f, PLATE, 4, 7); // Hjelm

    // Fanestang i begge hender, rett opp
    Vector3 base = r.at(7.0f, 3.0f, 4.0f);
    Vector3 top = r.at(7.0f, 3.0f, 92.0f);
    for (int s = -1; s <= 1; s += 2) {
        Vector3 hand = r.at(7.0f, 3.0f, 22.0f + bob + s * 5.0f);
        r.limb(r.at(0, s * 6.5f, 27.0f + bob), hand, 2.2f, 2.0f, LIVERY, 5);
        ShadedSphere(hand, 2.3f, SKIN, 3, 5);
    }
    r.limb(base, top, 1.5f, 1.3f, WOOD, 6);
    ShadedSphere(top, 3.0f, LIVERY_GOLD, 4, 6);
    ShadedCylinder(r.at(7.0f, -12.0f, 88.0f), r.at(7.0f, 18.0f, 88.0f), 1.0f, 1.0f, LIVERY_GOLD, 5); // Tverrstang

    // Fanen: en rekke smale biter som bølger bakover
    float t = (float)GetTime() * 4.0f + id;
    const int PIECES = 6;
    for (int i = 0; i < PIECES; i++) {
        float side = -10.0f + i * 5.0f;
        float wave = sinf(t - i * 0.8f) * (1.0f + i * 0.6f);
        Color c = (i % 2 == 0) ? LIVERY : Color{ 185, 40, 55, 255 };
        float yaw = atan2f(r.f.x, r.f.y) * RAD2DEG;
        ShadedCube(r.at(7.0f - wave * 0.6f + 2.0f, side + 2.5f, 72.0f), { 1.4f, 30.0f, 5.2f }, yaw, c);
    }
    // Gullkrone midt på fanen
    r.blob(9.8f, 3.0f, 74.0f, { 1.0f, 5.0f, 3.5f }, LIVERY_GOLD, 3, 5);
}

// =====================================================================
// Beleiringskjempe
// =====================================================================
Giant::Giant(Vector2 spawnPos, Texture2D tex) {
    position = spawnPos;
    speed = 62.0f;
    hp = maxHp = 2500;
    damage = 45;
    xpValue = 400;
    orbColor = Color{ 150, 125, 105, 255 };
    orbRadius = 14.0f;
    goldChance = 0.6f;
    goldValue = 6;
    hitRadius = 34.0f;
    modelScale = 1.9f;
    knockbackScale = 0.05f;
    texture = tex;
    throwTimer = 2.5f + (id % 4) * 0.5f;
}

void Giant::update(Vector2 playerPosition) {
    float dt = GetFrameTime();
    float dist;
    Vector2 dir = dirTo(position, playerPosition, &dist);
    facing = dir;
    if (windup > 0.0f) {
        // Står stille med steinen over hodet – så kaster den dit du står
        windup -= dt;
        if (windup <= 0.0f) {
            Vector2 target = { playerPosition.x + (float)GetRandomValue(-20, 20), playerPosition.y + (float)GetRandomValue(-20, 20) };
            SpawnMortarShell(Vector2Add(position, Vector2Scale(dir, 20.0f)), target, 1.3f, 115.0f, (float)damage * 1.5f);
            PlaySfxPitch(Sfx::EXPLOSION, 0.6f);
            throwTimer = 4.8f;
        }
        return;
    }
    position = Vector2Add(position, Vector2Scale(dir, speed * dt));
    throwTimer -= dt;
    if (throwTimer <= 0.0f && dist < 620.0f) windup = GIANT_WINDUP;
}

void Giant::onDeath() {
    // Kjempen brister og tre troll velter ut
    for (int i = 0; i < 3; i++) {
        float a = i * 2.0f * PI / 3.0f + facing.x;
        pendingSpawns.push_back({ EnemyType::GOON, { position.x + cosf(a) * 40.0f, position.y + sinf(a) * 40.0f } });
    }
    VfxExplosion(position, 90.0f);
    VfxShockwave(position, 160.0f, Color{ 180, 150, 110, 255 });
}

void Giant::drawVfx() const {
    Enemy::drawVfx();
    if (windup > 0.0f) {
        float t = 1.0f - windup / GIANT_WINDUP;
        VfxBillboard(VfxTex::GLOW, ToWorld3D(position, 80.0f * modelScale * ENEMY_VISUAL_SCALE), 40.0f + 60.0f * t, Color{ 200, 120, 60, 255 });
    }
}

// Enorm, skallet kjempe med skjegg, lendeklede, lenker på håndleddene og arr.
// Løfter en steinblokk over hodet når den skal kaste.
void Giant::draw3D() const {
    const Color HIDE = { 170, 135, 110, 255 };
    const Color HIDE_DARK = { 125, 95, 75, 255 };
    const Color BEARD = { 90, 60, 40, 255 };
    const Color IRON = { 60, 62, 70, 255 };
    const Color ROCK = { 135, 128, 120, 255 };
    Rig r(position, facing);
    bool lifting = windup > 0.0f;
    float lift = lifting ? 1.0f - windup / GIANT_WINDUP : 0.0f;
    float w = walkCycle() * 0.6f;
    float step = lifting ? 0.0f : sinf(w);
    float sway = lifting ? 0.0f : sinf(w) * 1.8f;
    float bob = lifting ? 0.0f : fabsf(cosf(w)) * 2.0f;

    // Tykke bein med fotklær av lær
    for (int s = -1; s <= 1; s += 2) {
        float st = step * 5.0f * s;
        r.limb(r.at(st * 0.5f, s * 7.5f, 2.0f), r.at(0.0f, s * 7.5f, 16.0f + bob), 5.5f, 6.5f, HIDE_DARK, 7);
        r.blob(3.0f + st, s * 7.5f, 2.5f, { 7.0f, 3.2f, 5.0f }, LEATHER, 4, 6);
    }
    // Lendeklede og overkropp
    r.limb(r.at(0, sway, 13.0f + bob), r.at(0, sway, 20.0f + bob), 13.5f, 13.0f, LEATHER, 9);
    r.blob(1.0f, sway, 32.0f + bob, { 13.0f, 15.0f, 16.0f }, HIDE, 6, 8);
    r.blob(7.5f, sway, 34.0f + bob, { 6.0f, 7.0f, 11.0f }, Color{ 185, 150, 125, 255 }, 5, 7); // Bryst
    r.limb(r.at(8.0f, sway - 6.0f, 38.0f + bob), r.at(9.5f, sway + 5.0f, 28.0f + bob), 0.8f, 0.8f, Color{ 200, 90, 80, 255 }, 3); // Arr

    // Hode: lite i forhold til kroppen, skallet, med stort skjegg
    float head = 51.0f + bob;
    r.ball(3.0f, sway, head, 7.5f, HIDE, 6, 8);
    r.blob(7.0f, sway, head - 5.5f, { 4.5f, 6.0f, 6.0f }, BEARD, 4, 6);
    for (int s = -1; s <= 1; s += 2) {
        r.ball(9.2f, sway + s * 2.8f, head + 1.5f, 1.4f, Color{ 255, 190, 60, 255 }, 3, 4); // Gule øyne
        r.blob(8.8f, sway + s * 2.8f, head + 3.6f, { 1.0f, 2.2f, 0.8f }, BEARD, 3, 4);       // Bryn
    }

    // Armer: henger og svinger – eller løfter steinblokka over hodet
    for (int s = -1; s <= 1; s += 2) {
        Vector3 shoulder = r.at(0.0f, sway + s * 14.0f, 42.0f + bob);
        Vector3 hand = lifting ? r.at(2.0f - 4.0f * lift, sway + s * 9.0f, 50.0f + 22.0f * lift)
                               : r.at(4.0f - step * 4.0f * s, sway + s * 17.0f, 18.0f + bob);
        r.limb(shoulder, hand, 5.0f, 4.2f, HIDE);
        ShadedSphere(hand, 5.0f, HIDE_DARK, 4, 6);
        // Avbrutt lenke rundt håndleddet
        Vector3 cuff = Vector3Lerp(shoulder, hand, 0.8f);
        ShadedSphere(cuff, 5.2f, IRON, 4, 6);
    }
    if (lifting) {
        ShadedSphere(r.at(-2.0f * lift, sway, 62.0f + 26.0f * lift), 12.0f, ROCK, 5, 7);
        ShadedSphere(r.at(-2.0f * lift + 4.0f, sway + 5.0f, 66.0f + 26.0f * lift), 7.0f, Color{ 115, 110, 104, 255 }, 4, 6);
    }
}
