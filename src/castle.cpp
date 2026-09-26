#include "castle.hpp"
#include "settings.hpp"
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

} // namespace

void DrawShadow(Vector2 feet, float width, float height) {
    DrawEllipse((int)feet.x, (int)feet.y, width, height, Fade(BLACK, 0.28f));
    DrawEllipse((int)feet.x, (int)feet.y, width * 0.65f, height * 0.65f, Fade(BLACK, 0.18f));
}

void DrawCastleFloor(const Camera2D& camera) {
    // Kameraet kan rotere, så vi tegner alt innenfor en sirkel som dekker hele skjermen
    float halfDiagonal = sqrtf((float)(Settings::SCREEN_WIDTH * Settings::SCREEN_WIDTH +
                                       Settings::SCREEN_HEIGHT * Settings::SCREEN_HEIGHT)) * 0.5f / camera.zoom;
    float minX = camera.target.x - halfDiagonal - TILE;
    float maxX = camera.target.x + halfDiagonal + TILE;
    float minY = camera.target.y - halfDiagonal - TILE;
    float maxY = camera.target.y + halfDiagonal + TILE;

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
}

void DrawThroneRoom(Vector2 center, float radius) {
    const float wallThickness = 50.0f;

    // Mørk bakgrunn utenfor salen
    DrawCircleV(center, radius + wallThickness + 200.0f, Color{ 12, 10, 16, 255 });

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

    // --- Tronen øverst i salen ---
    Vector2 throne = { center.x, center.y - radius + 70.0f };
    DrawRectangle((int)throne.x - 70, (int)throne.y - 10, 140, 60, STONE_DARK);                 // Podium
    DrawRectangle((int)throne.x - 70, (int)throne.y - 10, 140, 4, STONE);
    DrawRectangle((int)throne.x - 40, (int)throne.y - 55, 80, 70, CARPET_GOLD);                 // Ryggstø
    DrawTriangle({ throne.x - 40, throne.y - 55 }, { throne.x + 40, throne.y - 55 }, { throne.x, throne.y - 85 }, CARPET_GOLD);
    DrawRectangle((int)throne.x - 30, (int)throne.y - 45, 60, 50, CARPET_RED);                  // Pute
    DrawCircleV({ throne.x, throne.y - 62 }, 6.0f, RED);                                        // Juvel
    DrawRectangle((int)throne.x - 48, (int)throne.y - 5, 12, 40, shade(CARPET_GOLD, -30));      // Armlener
    DrawRectangle((int)throne.x + 36, (int)throne.y - 5, 12, 40, shade(CARPET_GOLD, -30));

    // --- Murvegg rundt salen (dekker de hakkete flis-kantene) ---
    DrawRing(center, radius, radius + wallThickness, 0.0f, 360.0f, 96, STONE);
    DrawRing(center, radius, radius + 6.0f, 0.0f, 360.0f, 96, STONE_DARK);                   // Skygge innerst
    DrawRing(center, radius + wallThickness - 6.0f, radius + wallThickness, 0.0f, 360.0f, 96, shade(STONE, 25));

    // --- Søyler og bannere langs veggen ---
    const int pillarCount = 16;
    for (int i = 0; i < pillarCount; i++) {
        float angle = (360.0f / pillarCount) * i * DEG2RAD;
        Vector2 dir = { cosf(angle), sinf(angle) };
        Vector2 pillarPos = Vector2Add(center, Vector2Scale(dir, radius + wallThickness * 0.5f));

        // Banner mellom søylene (bare annenhver, og ikke der løperen går ut)
        if (i % 2 == 1 && dir.y < 0.9f) {
            float bannerAngle = angle + (180.0f / pillarCount) * DEG2RAD;
            Vector2 bdir = { cosf(bannerAngle), sinf(bannerAngle) };
            Vector2 bannerPos = Vector2Add(center, Vector2Scale(bdir, radius - 6.0f));
            DrawCircleV(bannerPos, 12.0f, CARPET_RED);
            DrawCircleLines((int)bannerPos.x, (int)bannerPos.y, 12.0f, CARPET_GOLD);
            DrawCircleV(bannerPos, 4.0f, CARPET_GOLD);
        }

        // Søyle: skygge, sokkel og topp med lys kant
        DrawCircleV(Vector2Add(pillarPos, { 6.0f, 8.0f }), 26.0f, Fade(BLACK, 0.35f));
        DrawCircleV(pillarPos, 26.0f, STONE_DARK);
        DrawCircleV(pillarPos, 21.0f, shade(STONE, 15));
        DrawCircleV(Vector2Add(pillarPos, { -5.0f, -5.0f }), 9.0f, shade(STONE, 45));
    }
}
