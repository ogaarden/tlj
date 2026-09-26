#include "castle.hpp"
#include "render3d.hpp"
#include "vfx.hpp"
#include <rlgl.h>
#include <raymath.h>
#include <cmath>

namespace {

constexpr int TILE = 64;                   // Størrelse på én flis
constexpr int CARPET_SPACING = TILE * 24;  // Avstand mellom løperne i storsalen
constexpr int CARPET_WIDTH = TILE * 3;

// Slottsgulv: lys marmor og mørk stein
constexpr Color MARBLE_LIGHT = { 206, 196, 176, 255 };
constexpr Color MARBLE_DARK  = {  74,  68,  80, 255 };

// Tronsal: mørkere og mer høytidelig
constexpr Color THRONE_LIGHT = { 170, 160, 140, 255 };
constexpr Color THRONE_DARK  = {  28,  26,  36, 255 };

constexpr Color CARPET_RED  = { 130,  20,  30, 255 };
constexpr Color CARPET_GOLD = { 212, 175,  55, 255 };
constexpr Color STONE       = {  90,  88,  95, 255 };
constexpr Color STONE_DARK  = {  50,  48,  55, 255 };

// Enkel hash så hver flis får sin egen (faste) fargevariasjon – ser mer ut som ekte stein
int tileHash(int x, int y) {
    unsigned int h = (unsigned int)(x * 73856093) ^ (unsigned int)(y * 19349663);
    h ^= h >> 13;
    h *= 0x5bd1e995;
    h ^= h >> 15;
    return (int)(h % 17) - 8; // -8 .. 8
}

Color shade(Color c, int amount) {
    auto clamp = [](int v) { return (unsigned char)(v < 0 ? 0 : (v > 255 ? 255 : v)); };
    return { clamp(c.r + amount), clamp(c.g + amount), clamp(c.b + amount), c.a };
}

// Én flis med fuge og en liten "bevel" (lys kant oppe/venstre, mørk nede/høyre) for dybde
void drawTile(int tx, int ty, Color light, Color dark) {
    bool isLight = ((tx + ty) & 1) == 0;
    Color base = shade(isLight ? light : dark, tileHash(tx, ty));

    int x = tx * TILE;
    int y = ty * TILE;
    DrawRectangle(x, y, TILE, TILE, shade(base, -35));                 // Fuge
    DrawRectangle(x + 2, y + 2, TILE - 4, TILE - 4, base);             // Selve flisen
    DrawRectangle(x + 2, y + 2, TILE - 4, 2, shade(base, 22));         // Lys kant oppe
    DrawRectangle(x + 2, y + 2, 2, TILE - 4, shade(base, 14));         // Lys kant venstre
    DrawRectangle(x + 2, y + TILE - 4, TILE - 4, 2, shade(base, -22)); // Mørk kant nede
    DrawRectangle(x + TILE - 4, y + 2, 2, TILE - 4, shade(base, -14)); // Mørk kant høyre
}

void drawCarpet(Rectangle r, bool vertical) {
    DrawRectangleRec(r, CARPET_RED);
    // Gullkanter langs sidene
    if (vertical) {
        DrawRectangle((int)r.x + 8, (int)r.y, 4, (int)r.height, CARPET_GOLD);
        DrawRectangle((int)(r.x + r.width) - 12, (int)r.y, 4, (int)r.height, CARPET_GOLD);
    } else {
        DrawRectangle((int)r.x, (int)r.y + 8, (int)r.width, 4, CARPET_GOLD);
        DrawRectangle((int)r.x, (int)(r.y + r.height) - 12, (int)r.width, 4, CARPET_GOLD);
    }
}

// Gyllent mosaikk-emblem (kompassrose) der løperne krysser hverandre
void drawEmblem(Vector2 c, float r) {
    DrawCircleV(c, r + 10.0f, CARPET_GOLD);
    DrawCircleV(c, r + 4.0f, shade(CARPET_RED, -20));
    DrawCircleV(c, r * 0.8f, CARPET_RED);
    DrawRing(c, r * 0.55f, r * 0.6f, 0.0f, 360.0f, 48, CARPET_GOLD);
    for (int i = 0; i < 8; i++) {
        float a = i * PI / 4.0f;
        float len = (i % 2 == 0) ? r * 0.95f : r * 0.6f;
        Vector2 tip = { c.x + cosf(a) * len, c.y + sinf(a) * len };
        Vector2 l = { c.x + cosf(a + 0.35f) * r * 0.2f, c.y + sinf(a + 0.35f) * r * 0.2f };
        Vector2 rr = { c.x + cosf(a - 0.35f) * r * 0.2f, c.y + sinf(a - 0.35f) * r * 0.2f };
        DrawTriangle(tip, l, rr, (i % 2 == 0) ? CARPET_GOLD : shade(CARPET_GOLD, -40));
        DrawTriangle(tip, rr, l, (i % 2 == 0) ? CARPET_GOLD : shade(CARPET_GOLD, -40));
    }
    DrawCircleV(c, r * 0.18f, shade(CARPET_GOLD, 20));
    DrawCircleV(c, r * 0.1f, CARPET_RED);
}

// Fyrfat: steinsokkel, jernskål og glør (3D)
void drawBrazier(Vector2 p) {
    ShadedCylinder(ToWorld3D(p, 0.0f), ToWorld3D(p, 5.0f), 13.0f, 12.0f, STONE_DARK, 10);
    ShadedCylinder(ToWorld3D(p, 5.0f), ToWorld3D(p, 26.0f), 6.0f, 5.0f, STONE, 8);
    ShadedCylinder(ToWorld3D(p, 26.0f), ToWorld3D(p, 34.0f), 6.0f, 15.0f, Color{ 60, 55, 60, 255 }, 10);
    ShadedCylinder(ToWorld3D(p, 33.0f), ToWorld3D(p, 35.0f), 15.5f, 15.5f, CARPET_GOLD, 10);
    ShadedSphere(ToWorld3D(p, 33.0f), 10.0f, Color{ 255, 120, 40, 255 }, 4, 8);
}

// Flamme, lys og glør fra et fyrfat (VFX)
void brazierFire(Vector2 p, float seed) {
    float t = (float)GetTime();
    float flicker = 0.85f + 0.15f * sinf(t * 13.0f + seed) * sinf(t * 7.3f + seed * 2.0f);
    VfxDecal(VfxTex::GLOW, p, 330.0f * flicker, Color{ 80, 48, 18, 255 }, 0.0f, 0.8f);           // Varm lyspøl på gulvet
    VfxBillboard(VfxTex::EXPLOSION, ToWorld3D(p, 46.0f), 42.0f * flicker, Color{ 255, 190, 140, 255 }, t * 60.0f + seed * 50.0f);
    VfxBillboard(VfxTex::EXPLOSION, ToWorld3D(p, 54.0f), 28.0f * flicker, Color{ 255, 230, 180, 255 }, -t * 90.0f + seed * 30.0f);
    VfxBillboard(VfxTex::GLOW, ToWorld3D(p, 44.0f), 80.0f, Color{ 160, 80, 20, 255 });
    if (GetRandomValue(0, 14) == 0) VfxBubble({ p.x + GetRandomValue(-6, 6), p.y + GetRandomValue(-6, 6) }, 44.0f, Color{ 255, 150, 50, 255 });
}

// Alle fyrfat-posisjoner innenfor en radius: fire rundt hvert løperkryss
template <typename F>
void forEachBrazier(Vector2 center, float radius, F&& fn) {
    const float offset = CARPET_WIDTH / 2.0f + 70.0f;
    int x0 = (int)floorf((center.x - radius) / CARPET_SPACING), x1 = (int)ceilf((center.x + radius) / CARPET_SPACING);
    int y0 = (int)floorf((center.y - radius) / CARPET_SPACING), y1 = (int)ceilf((center.y + radius) / CARPET_SPACING);
    for (int ix = x0; ix <= x1; ix++) {
        for (int iy = y0; iy <= y1; iy++) {
            Vector2 cross = { (float)(ix * CARPET_SPACING), (float)(iy * CARPET_SPACING) };
            for (int k = 0; k < 4; k++) {
                Vector2 p = { cross.x + ((k & 1) ? offset : -offset), cross.y + ((k & 2) ? offset : -offset) };
                if (fabsf(p.x - center.x) < radius && fabsf(p.y - center.y) < radius) fn(p, (float)(ix * 7 + iy * 13 + k));
            }
        }
    }
}

} // namespace

void DrawShadow(Vector2 feet, float width, float height) {
    DrawEllipse((int)feet.x, (int)feet.y, width, height, Fade(BLACK, 0.28f));
    DrawEllipse((int)feet.x, (int)feet.y, width * 0.65f, height * 0.65f, Fade(BLACK, 0.18f));
}

void DrawCastleFloor(Vector2 center, float viewRadius) {
    float minX = center.x - viewRadius - TILE;
    float maxX = center.x + viewRadius + TILE;
    float minY = center.y - viewRadius - TILE;
    float maxY = center.y + viewRadius + TILE;

    int startX = (int)floorf(minX / TILE);
    int endX = (int)floorf(maxX / TILE);
    int startY = (int)floorf(minY / TILE);
    int endY = (int)floorf(maxY / TILE);

    for (int ty = startY; ty <= endY; ty++) {
        for (int tx = startX; tx <= endX; tx++) {
            drawTile(tx, ty, MARBLE_LIGHT, MARBLE_DARK);
        }
    }

    // Røde løpere i et rutenett gjennom storsalen
    int firstCarpetX = (int)floorf(minX / CARPET_SPACING);
    int lastCarpetX = (int)floorf(maxX / CARPET_SPACING);
    for (int i = firstCarpetX; i <= lastCarpetX; i++) {
        float cx = (float)(i * CARPET_SPACING);
        drawCarpet({ cx - CARPET_WIDTH / 2.0f, minY, (float)CARPET_WIDTH, maxY - minY }, true);
    }
    int firstCarpetY = (int)floorf(minY / CARPET_SPACING);
    int lastCarpetY = (int)floorf(maxY / CARPET_SPACING);
    for (int i = firstCarpetY; i <= lastCarpetY; i++) {
        float cy = (float)(i * CARPET_SPACING);
        drawCarpet({ minX, cy - CARPET_WIDTH / 2.0f, maxX - minX, (float)CARPET_WIDTH }, false);
    }

    // Emblem der løperne krysser, og skygger under fyrfatene
    for (int ix = firstCarpetX; ix <= lastCarpetX; ix++) {
        for (int iy = firstCarpetY; iy <= lastCarpetY; iy++) {
            drawEmblem({ (float)(ix * CARPET_SPACING), (float)(iy * CARPET_SPACING) }, CARPET_WIDTH * 0.62f);
        }
    }
    forEachBrazier(center, viewRadius, [](Vector2 p, float) { DrawShadow({ p.x + 5.0f, p.y + 5.0f }, 16.0f, 11.0f); });
}

void DrawCastleProps3D(Vector2 center, float viewRadius) {
    forEachBrazier(center, viewRadius, [](Vector2 p, float) { drawBrazier(p); });
}

void DrawCastlePropsVfx(Vector2 center, float viewRadius) {
    forEachBrazier(center, viewRadius, [](Vector2 p, float seed) { brazierFire(p, seed); });
}

namespace {
    // Fyrfatene i tronsalen står i en ring innenfor muren
    constexpr int THRONE_BRAZIERS = 8;
    Vector2 throneBrazier(Vector2 center, float radius, int i) {
        float a = (360.0f / THRONE_BRAZIERS * i + 22.5f) * DEG2RAD;
        return { center.x + cosf(a) * (radius - 45.0f), center.y + sinf(a) * (radius - 45.0f) };
    }
}

void DrawThroneRoomVfx(Vector2 center, float radius) {
    for (int i = 0; i < THRONE_BRAZIERS; i++) brazierFire(throneBrazier(center, radius, i), (float)i * 3.0f);
}

namespace {
    constexpr float WALL_THICKNESS = 50.0f;
    constexpr float WALL_HEIGHT = 55.0f;
    constexpr float PILLAR_HEIGHT = 120.0f;
    constexpr int PILLAR_COUNT = 16;
}

void DrawThroneRoomFloor(Vector2 center, float radius) {
    // --- Sjakkbrett-gulv (bare fliser innenfor sirkelen) ---
    int start = (int)floorf(-radius / TILE) - 1;
    int end = (int)ceilf(radius / TILE) + 1;
    int baseX = (int)floorf(center.x / TILE);
    int baseY = (int)floorf(center.y / TILE);
    for (int ty = start; ty <= end; ty++) {
        for (int tx = start; tx <= end; tx++) {
            int worldTx = baseX + tx;
            int worldTy = baseY + ty;
            Vector2 tileCenter = { worldTx * TILE + TILE / 2.0f, worldTy * TILE + TILE / 2.0f };
            if (Vector2Distance(tileCenter, center) < radius + TILE) {
                drawTile(worldTx, worldTy, THRONE_LIGHT, THRONE_DARK);
            }
        }
    }

    // --- Rød løper fra inngangen og opp til tronen ---
    float carpetWidth = 140.0f;
    float carpetTop = center.y - radius + 90.0f;
    float carpetBottom = center.y + radius;
    drawCarpet({ center.x - carpetWidth / 2.0f, carpetTop, carpetWidth, carpetBottom - carpetTop }, true);

    // --- Stort emblem midt i salen ---
    drawEmblem(center, 120.0f);
    for (int i = 0; i < THRONE_BRAZIERS; i++) {
        Vector2 p = throneBrazier(center, radius, i);
        DrawShadow({ p.x + 5.0f, p.y + 5.0f }, 16.0f, 11.0f);
    }

    // --- Podium under tronen ---
    Vector2 throne = { center.x, center.y - radius + 70.0f };
    DrawRectangle((int)throne.x - 80, (int)throne.y - 40, 160, 90, STONE_DARK);
    DrawRectangleLines((int)throne.x - 80, (int)throne.y - 40, 160, 90, STONE);

    // --- Mur-fundament (dekker de hakkete flis-kantene) og mørket utenfor ---
    DrawRing(center, radius, radius + WALL_THICKNESS, 0.0f, 360.0f, 96, STONE_DARK);
    DrawRing(center, radius - 10.0f, radius, 0.0f, 360.0f, 96, Fade(BLACK, 0.35f)); // Skygge langs muren
}

void DrawThroneRoom3D(Vector2 center, float radius) {
    // --- Murvegg rundt salen, bygget av skrå blokker ---
    const int segments = 48;
    float wallRadius = radius + WALL_THICKNESS / 2.0f;
    float segmentLength = 2.0f * PI * wallRadius / segments + 2.0f;
    for (int i = 0; i < segments; i++) {
        float angle = (360.0f / segments) * (i + 0.5f);
        float a = angle * DEG2RAD;
        Vector2 pos = { center.x + cosf(a) * wallRadius, center.y + sinf(a) * wallRadius };

        // Åpning i muren der løperen går inn (sør)
        if (fabsf(angle - 90.0f) < 6.0f) continue;

        Color blockColor = (i % 2 == 0) ? STONE : shade(STONE, -10);
        // Blokken ligger langs sirkelen (lokal x-akse = tangenten): yaw = -vinkel - 90
        ShadedCube(ToWorld3D(pos, WALL_HEIGHT / 2.0f), { segmentLength, WALL_HEIGHT, WALL_THICKNESS }, -angle - 90.0f, blockColor);
        // Tinder på toppen av muren (annenhver blokk)
        if (i % 2 == 0) {
            ShadedCube(ToWorld3D(pos, WALL_HEIGHT + 8.0f), { segmentLength * 0.5f, 16.0f, WALL_THICKNESS * 0.8f }, -angle - 90.0f, shade(STONE, 12));
        }
    }

    // --- Søyler med bannere ---
    for (int i = 0; i < PILLAR_COUNT; i++) {
        float angle = (360.0f / PILLAR_COUNT) * i * DEG2RAD;
        Vector2 dir = { cosf(angle), sinf(angle) };
        Vector2 pillarPos = Vector2Add(center, Vector2Scale(dir, radius + 6.0f));

        ShadedCylinder(ToWorld3D(pillarPos, 0.0f), ToWorld3D(pillarPos, 10.0f), 28.0f, 28.0f, STONE_DARK, 16);          // Sokkel
        ShadedCylinder(ToWorld3D(pillarPos, 10.0f), ToWorld3D(pillarPos, PILLAR_HEIGHT), 20.0f, 18.0f, shade(STONE, 20), 16);
        ShadedCylinder(ToWorld3D(pillarPos, PILLAR_HEIGHT), ToWorld3D(pillarPos, PILLAR_HEIGHT + 12.0f), 20.0f, 25.0f, shade(STONE, 30), 16); // Kapitél

        // Rødt banner med gullkant på innsiden av annenhver søyle (ikke ved inngangen)
        if (i % 2 == 1 && dir.y < 0.9f) {
            Vector2 bannerPos = Vector2Subtract(pillarPos, Vector2Scale(dir, 21.0f));
            float yaw = -angle * RAD2DEG - 90.0f;
            ShadedCube(ToWorld3D(bannerPos, PILLAR_HEIGHT - 40.0f), { 26.0f, 70.0f, 3.0f }, yaw, CARPET_RED);
            ShadedCube(ToWorld3D(bannerPos, PILLAR_HEIGHT - 6.0f), { 30.0f, 4.0f, 4.0f }, yaw, CARPET_GOLD);
            ShadedCube(ToWorld3D(Vector2Subtract(bannerPos, Vector2Scale(dir, 2.0f)), PILLAR_HEIGHT - 45.0f), { 10.0f, 10.0f, 2.0f }, yaw, CARPET_GOLD);
        }
    }

    // --- Fyrfat langs muren ---
    for (int i = 0; i < THRONE_BRAZIERS; i++) drawBrazier(throneBrazier(center, radius, i));

    // --- Tronen ---
    Vector2 throne = { center.x, center.y - radius + 70.0f };
    ShadedCube(ToWorld3D(throne, 6.0f), { 150.0f, 12.0f, 80.0f }, 0.0f, STONE);                                 // Podium
    ShadedCube(ToWorld3D({ throne.x, throne.y + 5.0f }, 28.0f), { 70.0f, 30.0f, 50.0f }, 0.0f, CARPET_GOLD);     // Sete
    ShadedCube(ToWorld3D({ throne.x, throne.y + 5.0f }, 45.0f), { 56.0f, 6.0f, 40.0f }, 0.0f, CARPET_RED);       // Pute
    ShadedCube(ToWorld3D({ throne.x, throne.y - 20.0f }, 80.0f), { 70.0f, 100.0f, 12.0f }, 0.0f, CARPET_GOLD);   // Ryggstø
    ShadedCube(ToWorld3D({ throne.x, throne.y - 13.0f }, 75.0f), { 50.0f, 70.0f, 3.0f }, 0.0f, CARPET_RED);
    ShadedCube(ToWorld3D({ throne.x - 38.0f, throne.y + 5.0f }, 50.0f), { 10.0f, 16.0f, 50.0f }, 0.0f, shade(CARPET_GOLD, -25)); // Armlener
    ShadedCube(ToWorld3D({ throne.x + 38.0f, throne.y + 5.0f }, 50.0f), { 10.0f, 16.0f, 50.0f }, 0.0f, shade(CARPET_GOLD, -25));
    ShadedCylinder(ToWorld3D({ throne.x, throne.y - 20.0f }, 130.0f), ToWorld3D({ throne.x, throne.y - 20.0f }, 150.0f), 12.0f, 0.0f, CARPET_GOLD, 10);
    ShadedSphere(ToWorld3D({ throne.x, throne.y - 13.0f }, 110.0f), 7.0f, RED, 6, 8);                             // Juvel
}
