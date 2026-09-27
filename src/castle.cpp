#include "castle.hpp"
#include "render3d.hpp"
#include "vfx.hpp"
#include <rlgl.h>
#include <raymath.h>
#include <cmath>
#include <vector>

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

// ---------------------------------------------------------------------
// PORSELENSGULV (Higgsfield-teksturer i assets/floor/)
// Hvite delft-fliser speiles i 2x2-grupper så de danner store rosetter,
// og koboltblå stjernefliser går i bånd mellom gruppene.
// Begge flisene ligger i ÉN tekstur (atlas), så hele gulvet tegnes i én batch.
// ---------------------------------------------------------------------
Texture2D floorAtlas{};
bool floorLoaded = false;
constexpr int ATLAS_TILE = 256;

// Porselensflis: speilet delft (kind 0) eller koboltstjerne (kind 1), med glasur-glans og fuge
void drawPorcelainTile(int tx, int ty, float brightness) {
    // Mønster med periode 5: 4x4 hvite fliser (to og to speilet) omkranset av blå bånd
    int mx = ((tx % 5) + 5) % 5, my = ((ty % 5) + 5) % 5;
    bool blue = (mx == 4 || my == 4);
    Rectangle src = { blue ? (float)ATLAS_TILE : 0.0f, 0.0f, (float)ATLAS_TILE, (float)ATLAS_TILE };
    if (!blue) {
        // Speilvend så fire fliser danner én rosett (hjørnet med rosetten møtes i midten)
        if (mx % 2 == 1) src.width = -src.width;
        if (my % 2 == 1) src.height = -src.height;
    }
    int v = tileHash(tx, ty) / 2;                      // Litt variasjon i glasuren
    unsigned char b = (unsigned char)Clamp((235 + v) * brightness, 0.0f, 255.0f);
    float x = (float)(tx * TILE), y = (float)(ty * TILE);
    DrawRectangle((int)x, (int)y, TILE, TILE, Color{ 150, 135, 100, 255 });            // Fuge
    DrawTexturePro(floorAtlas, src, { x + 1.5f, y + 1.5f, TILE - 3.0f, TILE - 3.0f }, { 0, 0 }, 0.0f, Color{ b, b, b, 255 });
    // Glans: lys stripe langs øvre venstre kant, skygge nede til høyre (glassert kant)
    DrawRectangleGradientH((int)x + 2, (int)y + 2, TILE / 2, 3, Fade(WHITE, 0.45f), Fade(WHITE, 0.0f));
    DrawRectangleGradientV((int)x + 2, (int)y + 2, 3, TILE / 2, Fade(WHITE, 0.35f), Fade(WHITE, 0.0f));
    DrawRectangle((int)x + 2, (int)y + TILE - 4, TILE - 4, 2, Fade(BLACK, 0.18f));
    DrawRectangle((int)x + TILE - 4, (int)y + 2, 2, TILE - 4, Fade(BLACK, 0.14f));
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
            if (fabsf(cross.x) >= CARPET_SPACING * 2.0f || fabsf(cross.y) >= CARPET_SPACING * 2.0f) continue; // Kryss i veggen
            for (int k = 0; k < 4; k++) {
                Vector2 p = { cross.x + ((k & 1) ? offset : -offset), cross.y + ((k & 2) ? offset : -offset) };
                if (fabsf(p.x - center.x) < radius && fabsf(p.y - center.y) < radius) fn(p, (float)(ix * 7 + iy * 13 + k));
            }
        }
    }
}

// ---------------------------------------------------------------------
// SLOTTET ER AVGRENSET: 4 x 4 saler (hver CARPET_SPACING stor) med murvegg rundt.
// Hver sal har et tema med faste plasser for pynten (se ROOMS og addRoom):
//   Storsalen (midten, der man starter), Statuegalleriet, Rustkammeret,
//   Fanehallen, Hagegården (fontene) og Porselenssalongen.
// Pynten står rett opp fra gulvet og stenger veien for spilleren og fiendene
// (prosjektiler går forbi). Står noe foran spilleren, blir det gjennomsiktig.
// (Funksjonene heter fortsatt "Pillar" for kollisjon og minimap.)
// ---------------------------------------------------------------------
constexpr float MAP_HALF = CARPET_SPACING * 2.0f;   // Veggen står her (x og y fra -MAP_HALF til MAP_HALF)
constexpr float WALL_H = 110.0f;
constexpr float WALL_T = 70.0f;

enum class Room { GRAND, STATUES, ARMORY, BANNERS, GARDEN, PORCELAIN };
const char* ROOM_NAMES[] = { "Storsalen", "Statuegalleriet", "Rustkammeret", "Fanehallen", "Hagegaarden", "Porselenssalongen" };
// Kartet, rad for rad fra nord (øverst på skjermen) til sør. Midten (Storsalen) er der man starter.
const Room ROOMS[4][4] = {
    { Room::ARMORY,    Room::STATUES, Room::STATUES, Room::BANNERS   },
    { Room::GARDEN,    Room::GRAND,   Room::GRAND,   Room::PORCELAIN },
    { Room::PORCELAIN, Room::GRAND,   Room::GRAND,   Room::GARDEN    },
    { Room::BANNERS,   Room::ARMORY,  Room::STATUES, Room::ARMORY    },
};

enum class Decor { VASE, ARMOR, TOPIARY, STATUE, BANNER, RACK, FOUNTAIN };
struct DecorItem {
    Vector2 pos;
    Decor kind;
    float radius;   // Kollisjon
    float height;   // For gjennomsiktighet når den står foran spilleren
    int variant;    // Litt variasjon (f.eks. hvilken statue)
};
constexpr float DECOR_SCALE = 1.35f;          // Alle modellene tegnes litt større enn de er bygget
constexpr float MAX_DECOR_RADIUS = 110.0f * DECOR_SCALE;

std::vector<DecorItem> decorItems;
struct RoomInfo { Room room; Vector2 center; bool turn; };
std::vector<RoomInfo> roomInfos;

void addDecor(Vector2 c, float lx, float ly, Decor kind, int variant = 0) {
    float r = 28.0f, h = 95.0f;
    switch (kind) {
        case Decor::VASE:     r = 28.0f;  h = 92.0f;  break;
        case Decor::ARMOR:    r = 26.0f;  h = 100.0f; break;
        case Decor::TOPIARY:  r = 28.0f;  h = 95.0f;  break;
        case Decor::STATUE:   r = 36.0f;  h = 135.0f; break;
        case Decor::BANNER:   r = 22.0f;  h = 155.0f; break;
        case Decor::RACK:     r = 34.0f;  h = 90.0f;  break;
        case Decor::FOUNTAIN: r = 105.0f; h = 70.0f;  break;
    }
    decorItems.push_back({ { c.x + lx, c.y + ly }, kind, r * DECOR_SCALE, h * DECOR_SCALE, variant });
}

// Fast oppsett for hver type sal (koordinater fra midten av salen, innenfor løperne)
void addRoom(Room room, Vector2 c, int ix, int iy) {
    bool turn = ((ix + iy) & 1) != 0; // Annenhver sal er dreid 90 grader, så de ikke er like
    auto add = [&](float x, float y, Decor k, int v = 0) { if (turn) addDecor(c, y, x, k, v); else addDecor(c, x, y, k, v); };
    switch (room) {
        case Room::GRAND:
            // Åpen sal: en statue i midten og fire fanestativer rundt
            add(0, 0, Decor::STATUE, 2);
            for (int k = 0; k < 4; k++) add((k & 1) ? 300.0f : -300.0f, (k & 2) ? 300.0f : -300.0f, Decor::BANNER);
            break;
        case Room::STATUES:
            // To rekker statuer langs en midtgang
            for (int i = 0; i < 4; i++) {
                float y = -450.0f + i * 300.0f;
                add(-240.0f, y, Decor::STATUE, i % 2);
                add(240.0f, y, Decor::STATUE, (i + 1) % 2);
            }
            break;
        case Room::ARMORY:
            // Rustninger i to rekker og våpenstativer mellom
            for (int i = -1; i <= 1; i++) {
                add(i * 330.0f, -360.0f, Decor::ARMOR);
                add(i * 330.0f, 360.0f, Decor::ARMOR);
            }
            add(-330.0f, 0.0f, Decor::RACK);
            add(330.0f, 0.0f, Decor::RACK);
            break;
        case Room::BANNERS:
            // Lange rekker med faner som danner en gang
            for (int i = 0; i < 5; i++) {
                float y = -480.0f + i * 240.0f;
                add(-190.0f, y, Decor::BANNER, i % 2);
                add(190.0f, y, Decor::BANNER, (i + 1) % 2);
            }
            break;
        case Room::GARDEN:
            // Fontene i midten og en ring av klippede hekker
            add(0, 0, Decor::FOUNTAIN);
            for (int k = 0; k < 8; k++) {
                float a = (22.5f + k * 45.0f) * DEG2RAD;
                add(cosf(a) * 420.0f, sinf(a) * 420.0f, Decor::TOPIARY);
            }
            break;
        case Room::PORCELAIN:
            // Delftvaser i en rombe
            add(0, -440.0f, Decor::VASE); add(0, 440.0f, Decor::VASE);
            add(-440.0f, 0, Decor::VASE); add(440.0f, 0, Decor::VASE);
            for (int k = 0; k < 4; k++) add((k & 1) ? 220.0f : -220.0f, (k & 2) ? 220.0f : -220.0f, Decor::VASE);
            add(0, 0, Decor::ARMOR);
            break;
    }
}

void buildMap() {
    if (!decorItems.empty()) return;
    for (int iy = -2; iy <= 1; iy++)
        for (int ix = -2; ix <= 1; ix++) {
            Vector2 c = { (ix + 0.5f) * CARPET_SPACING, (iy + 0.5f) * CARPET_SPACING };
            addRoom(ROOMS[iy + 2][ix + 2], c, ix, iy);
            roomInfos.push_back({ ROOMS[iy + 2][ix + 2], c, ((ix + iy) & 1) != 0 });
        }
}

// Alle ting innenfor en firkant rundt center
template <typename F>
void forEachDecor(Vector2 center, float radius, F&& fn) {
    buildMap();
    for (const DecorItem& d : decorItems)
        if (fabsf(d.pos.x - center.x) <= radius && fabsf(d.pos.y - center.y) <= radius) fn(d);
}

// Delftvase (hvit med koboltblå bånd) på en marmorsokkel med gullkant
void drawVaseDecor(Vector2 p, unsigned char alpha) {
    const Color MARBLE = { 224, 214, 198, alpha };
    const Color MARBLE_SHADE = { 168, 156, 140, alpha };
    const Color GOLDC = { 225, 180, 60, alpha };
    const Color PORCELAIN = { 240, 240, 245, alpha };
    const Color COBALT = { 40, 70, 170, alpha };
    ShadedCube(ToWorld3D(p, 5.0f), { 58.0f, 10.0f, 58.0f }, 45.0f, MARBLE_SHADE);                 // Fot
    ShadedCube(ToWorld3D(p, 43.5f), { 44.0f, 3.0f, 44.0f }, 45.0f, GOLDC);                        // Gullkant (stikker litt ut)
    ShadedCube(ToWorld3D(p, 27.0f), { 38.0f, 38.0f, 38.0f }, 45.0f, MARBLE);                      // Sokkel
    ShadedCylinder(ToWorld3D(p, 46.0f), ToWorld3D(p, 53.0f), 9.0f, 7.0f, PORCELAIN, 16);          // Vasefot
    ShadedSphere(ToWorld3D(p, 66.0f), 15.0f, PORCELAIN, 8, 16);                                   // Vasekropp
    ShadedCylinder(ToWorld3D(p, 60.0f), ToWorld3D(p, 68.0f), 15.3f, 15.3f, COBALT, 16);           // Blått bånd
    ShadedCylinder(ToWorld3D(p, 76.0f), ToWorld3D(p, 90.0f), 6.0f, 9.5f, PORCELAIN, 16);          // Hals
    ShadedCylinder(ToWorld3D(p, 86.0f), ToWorld3D(p, 90.5f), 9.8f, 10.0f, COBALT, 16);
    ShadedCylinder(ToWorld3D(p, 90.5f), ToWorld3D(p, 91.5f), 10.0f, 10.5f, GOLDC, 16);            // Gullkant
}

// Rustning på stativ: rund sokkel, bein, karmosin våpenkjole, hjelm med rød fjærbusk og hellebard
void drawArmorDecor(Vector2 p, unsigned char alpha) {
    const Color STEEL = { 150, 158, 175, alpha };
    const Color STEEL_DARK = { 90, 95, 110, alpha };
    const Color MARBLE = { 205, 196, 180, alpha };
    const Color RED_C = { 165, 30, 45, alpha };
    const Color GOLDC = { 225, 180, 60, alpha };
    const Color WOODC = { 120, 80, 45, alpha };
    ShadedCylinder(ToWorld3D(p, 5.0f), ToWorld3D(p, 7.0f), 26.5f, 26.5f, GOLDC, 20);              // Gullbånd
    ShadedCylinder(ToWorld3D(p, 0.0f), ToWorld3D(p, 10.0f), 25.0f, 25.0f, MARBLE, 20);            // Rund sokkel
    ShadedCylinder(ToWorld3D(p, 10.0f), ToWorld3D(p, 10.5f), 23.0f, 23.0f, MARBLE, 20);           // Toppflate
    for (int s = -1; s <= 1; s += 2) {                                                             // Bein
        Vector2 leg = { p.x + s * 6.0f, p.y };
        ShadedCylinder(ToWorld3D(leg, 10.0f), ToWorld3D(leg, 40.0f), 4.5f, 5.0f, STEEL_DARK, 8);
    }
    ShadedCylinder(ToWorld3D(p, 34.0f), ToWorld3D(p, 64.0f), 13.0f, 12.0f, RED_C, 12);            // Våpenkjole
    ShadedCylinder(ToWorld3D(p, 50.0f), ToWorld3D(p, 52.0f), 13.3f, 13.2f, GOLDC, 12);            // Belte
    ShadedCylinder(ToWorld3D(p, 62.0f), ToWorld3D(p, 67.0f), 13.5f, 9.0f, STEEL, 12);             // Brynje over skuldrene
    for (int s = -1; s <= 1; s += 2) {                                                             // Skulderplater og armer
        Vector2 sh = { p.x + s * 14.0f, p.y };
        ShadedEllipsoid(ToWorld3D(sh, 63.0f), { 0.0f, 1.0f }, { 6.0f, 3.5f, 5.0f }, STEEL, 4, 8);
        ShadedCylinder(ToWorld3D(sh, 61.0f), ToWorld3D({ sh.x + s * 2.0f, sh.y }, 42.0f), 3.8f, 3.2f, STEEL_DARK, 8);
    }
    ShadedCylinder(ToWorld3D(p, 67.0f), ToWorld3D(p, 82.0f), 7.5f, 7.0f, STEEL, 10);              // Hjelm (bøtte)
    ShadedCylinder(ToWorld3D(p, 82.0f), ToWorld3D(p, 84.0f), 7.0f, 4.0f, STEEL_DARK, 10);
    ShadedCube(ToWorld3D({ p.x, p.y + 7.0f }, 75.0f), { 10.0f, 1.6f, 2.0f }, 0.0f, Color{ 20, 18, 24, alpha }); // Visir
    ShadedEllipsoid(ToWorld3D({ p.x, p.y - 2.0f }, 88.0f), { 0.0f, 1.0f }, { 7.0f, 4.0f, 2.5f }, RED_C, 4, 6); // Fjærbusk
    // Hellebard ved siden av
    Vector2 h = { p.x + 21.0f, p.y + 3.0f };
    ShadedCylinder(ToWorld3D(h, 10.0f), ToWorld3D(h, 100.0f), 1.6f, 1.6f, WOODC, 6);
    ShadedCube(ToWorld3D({ h.x + 4.0f, h.y }, 92.0f), { 10.0f, 12.0f, 2.0f }, 0.0f, STEEL);
}

// Klippet hekk (tre kuler) i en steinkrukke med gullkant
void drawTopiaryDecor(Vector2 p, unsigned char alpha) {
    const Color STONE_C = { 150, 140, 130, alpha };
    const Color STONE_D = { 110, 102, 95, alpha };
    const Color GOLDC = { 225, 180, 60, alpha };
    const Color LEAF = { 70, 130, 70, alpha };
    const Color LEAF_L = { 95, 160, 90, alpha };
    ShadedCube(ToWorld3D(p, 5.0f), { 52.0f, 10.0f, 52.0f }, 45.0f, STONE_D);                      // Fot
    ShadedCylinder(ToWorld3D(p, 10.0f), ToWorld3D(p, 34.0f), 16.0f, 22.0f, STONE_C, 16);          // Krukke
    ShadedCylinder(ToWorld3D(p, 34.0f), ToWorld3D(p, 38.0f), 23.0f, 23.0f, GOLDC, 16);            // Gullkant
    ShadedCylinder(ToWorld3D(p, 38.0f), ToWorld3D(p, 38.6f), 20.0f, 20.0f, Color{ 90, 60, 40, alpha }, 16); // Jord
    ShadedCylinder(ToWorld3D(p, 38.0f), ToWorld3D(p, 44.0f), 3.0f, 3.0f, Color{ 100, 70, 40, alpha }, 6); // Stamme
    ShadedSphere(ToWorld3D(p, 52.0f), 17.0f, LEAF, 7, 12);
    ShadedSphere(ToWorld3D(p, 73.0f), 12.0f, LEAF_L, 6, 10);
    ShadedSphere(ToWorld3D(p, 88.0f), 7.0f, LEAF, 5, 8);
}

// Marmorstatue på en trinnet sokkel: 0 = konge med septer, 1 = ridder med sverd, 2 = stor dronning (Storsalen)
void drawStatueDecor(Vector2 p, int variant, unsigned char alpha) {
    const Color STONE_C = { 150, 142, 132, alpha };
    const Color MARBLE = { 232, 228, 222, alpha };
    const Color MARBLE_S = { 196, 192, 188, alpha };
    const Color GOLDC = { 225, 180, 60, alpha };
    float big = variant == 2 ? 1.2f : 1.0f;
    ShadedCube(ToWorld3D(p, 6.0f), { 66.0f * big, 12.0f, 66.0f * big }, 0.0f, STONE_C);                  // Nederste trinn
    ShadedCube(ToWorld3D(p, 19.0f), { 54.0f * big, 14.0f, 54.0f * big }, 0.0f, MARBLE_S);
    ShadedCube(ToWorld3D(p, 26.5f), { 57.0f * big, 3.0f, 57.0f * big }, 0.0f, GOLDC);                    // Gullkant
    ShadedCube(ToWorld3D(p, 31.0f), { 50.0f * big, 6.0f, 50.0f * big }, 0.0f, MARBLE_S);
    // Figuren (litt forenklet, ser mot kameraet): bein/kappe, kropp, hode
    float b = 34.0f;
    auto at = [&](float x, float y, float h) { return ToWorld3D({ p.x + x * big, p.y + y * big }, b + h * big); };
    ShadedCylinder(at(0, 0, 0), at(0, 0, 36), 17.0f * big, 12.0f * big, MARBLE, 14);                     // Kappe
    ShadedCylinder(at(0, 0, 36), at(0, 0, 54), 12.0f * big, 13.0f * big, MARBLE, 12);                    // Bryst
    ShadedSphere(at(0, 0, 63), 8.0f * big, MARBLE, 6, 10);                                                // Hode
    for (int s = -1; s <= 1; s += 2) ShadedSphere(at(s * 13.0f, 0, 52), 5.5f * big, MARBLE_S, 4, 8);     // Skuldre
    if (variant == 1) {
        // Ridder: hjelm med kam og et sverd holdt foran seg med spissen ned
        ShadedCylinder(at(0, 0, 60), at(0, 0, 72), 8.5f * big, 8.0f * big, MARBLE_S, 10);
        ShadedCube(at(0, 0, 76), { 2.0f, 8.0f, 12.0f }, 0.0f, MARBLE);
        ShadedCylinder(at(0, 8, 40), at(0, 9, 4), 1.8f, 0.3f, MARBLE_S, 6);                               // Klinge
        ShadedCube(at(0, 8, 42), { 14.0f, 2.5f, 3.0f }, 0.0f, MARBLE_S);                                   // Parerstang
        for (int s = -1; s <= 1; s += 2) ShadedCylinder(at(s * 13.0f, 0, 50), at(s * 3.0f, 8, 44), 3.5f, 3.0f, MARBLE, 6);
    } else {
        // Konge/dronning: krone og septer løftet i høyre hånd
        ShadedCylinder(at(0, 0, 69), at(0, 0, 74), 7.0f * big, 8.0f * big, MARBLE_S, 10);
        for (int k = 0; k < 5; k++) {
            float a = k * 2.0f * PI / 5.0f;
            ShadedSphere(at(cosf(a) * 7.0f, sinf(a) * 7.0f, 76), 1.6f * big, MARBLE, 3, 4);
        }
        ShadedCylinder(at(13.0f, 0, 50), at(17.0f, 4, 62), 3.5f, 3.0f, MARBLE, 6);                       // Løftet arm
        ShadedCylinder(at(17.0f, 4, 44), at(18.0f, 5, 86), 1.5f, 1.5f, MARBLE_S, 6);                      // Septer
        ShadedSphere(at(18.0f, 5, 88), 3.5f * big, MARBLE, 4, 6);
        ShadedCylinder(at(-13.0f, 0, 50), at(-14.0f, 3, 36), 3.5f, 3.0f, MARBLE, 6);
    }
}

// Fanestativ: jernstang med tverrstang og en karmosin fane (med gullkrone og frynser) som vender mot kameraet
void drawBannerDecor(Vector2 p, int variant, unsigned char alpha) {
    const Color IRON = { 60, 58, 66, alpha };
    const Color GOLDC = { 225, 180, 60, alpha };
    const Color CLOTH = variant == 0 ? Color{ 160, 25, 40, alpha } : Color{ 40, 60, 150, alpha };
    const Color TRIM = variant == 0 ? GOLDC : Color{ 235, 235, 240, alpha };
    for (int k = 0; k < 3; k++) {                                                                  // Trefot
        float a = (90.0f + k * 120.0f) * DEG2RAD;
        ShadedCylinder(ToWorld3D(p, 12.0f), ToWorld3D({ p.x + cosf(a) * 18.0f, p.y + sinf(a) * 18.0f }, 0.0f), 2.0f, 1.6f, IRON, 5);
    }
    ShadedCylinder(ToWorld3D(p, 0.0f), ToWorld3D(p, 150.0f), 2.6f, 2.3f, IRON, 6);                // Stang
    ShadedSphere(ToWorld3D(p, 154.0f), 5.0f, GOLDC, 4, 6);                                         // Gullknapp
    ShadedCube(ToWorld3D(p, 141.0f), { 56.0f, 3.5f, 3.5f }, 0.0f, IRON);                           // Tverrstang
    Vector2 f = { p.x, p.y + 3.5f };                                                                // Fanen litt foran stanga
    ShadedCube(ToWorld3D(f, 100.0f), { 48.0f, 78.0f, 2.0f }, 0.0f, CLOTH);
    ShadedCube(ToWorld3D({ f.x, f.y + 1.2f }, 136.0f), { 50.0f, 4.0f, 1.0f }, 0.0f, TRIM);         // Kant oppe
    for (int s = -1; s <= 1; s += 2)                                                               // Kanter på sidene
        ShadedCube(ToWorld3D({ f.x + s * 23.0f, f.y + 1.2f }, 100.0f), { 3.0f, 74.0f, 1.0f }, 0.0f, TRIM);
    for (int k = -3; k <= 3; k++)                                                                  // Frynser
        ShadedCube(ToWorld3D({ f.x + k * 7.0f, f.y }, 58.0f), { 4.5f, 8.0f, 2.2f }, 0.0f, TRIM);
    // Kronemerke midt på
    ShadedCube(ToWorld3D({ f.x, f.y + 1.2f }, 96.0f), { 20.0f, 8.0f, 1.0f }, 0.0f, TRIM);
    for (int k = -1; k <= 1; k++) ShadedCube(ToWorld3D({ f.x + k * 7.0f, f.y + 1.2f }, 103.0f), { 4.0f, 7.0f, 1.0f }, 0.0f, TRIM);
}

// Våpenstativ: treramme med spyd som lener seg mot den og et rundt skjold foran
void drawRackDecor(Vector2 p, unsigned char alpha) {
    const Color WOODC = { 120, 80, 45, alpha };
    const Color WOOD_D = { 90, 60, 35, alpha };
    const Color STEEL = { 170, 176, 190, alpha };
    const Color RED_C = { 160, 25, 40, alpha };
    const Color GOLDC = { 225, 180, 60, alpha };
    ShadedCube(ToWorld3D(p, 4.0f), { 70.0f, 8.0f, 24.0f }, 0.0f, WOOD_D);                          // Bunnplanke
    for (int s = -1; s <= 1; s += 2) ShadedCube(ToWorld3D({ p.x + s * 32.0f, p.y }, 30.0f), { 5.0f, 52.0f, 5.0f }, 0.0f, WOODC);
    ShadedCube(ToWorld3D(p, 54.0f), { 70.0f, 5.0f, 6.0f }, 0.0f, WOODC);                           // Tverrligger
    for (int k = 0; k < 5; k++) {                                                                  // Spyd
        float x = -24.0f + k * 12.0f;
        ShadedCylinder(ToWorld3D({ p.x + x, p.y + 6.0f }, 6.0f), ToWorld3D({ p.x + x * 0.9f, p.y - 3.0f }, 84.0f), 1.3f, 1.3f, WOODC, 5);
        ShadedCylinder(ToWorld3D({ p.x + x * 0.9f, p.y - 3.0f }, 84.0f), ToWorld3D({ p.x + x * 0.88f, p.y - 4.0f }, 96.0f), 2.6f, 0.0f, STEEL, 5);
    }
    // Rundt skjold som står lent mot foten, vendt mot kameraet
    Vector3 sc = ToWorld3D({ p.x, p.y + 14.0f }, 22.0f);
    ShadedCylinder(Vector3Add(sc, { 0, 0, -1.5f }), Vector3Add(sc, { 0, 0, 1.5f }), 17.0f, 17.0f, GOLDC, 16);
    ShadedCylinder(Vector3Add(sc, { 0, 0, 1.4f }), Vector3Add(sc, { 0, 0, 2.2f }), 14.5f, 14.5f, RED_C, 16);
    ShadedSphere(Vector3Add(sc, { 0, 0, 2.5f }), 4.0f, GOLDC, 4, 6);
}

// Fontene: rund steinkant med vann, og en søyle med to skåler i midten
void drawFountainDecor(Vector2 p, unsigned char alpha) {
    const Color STONE_C = { 170, 164, 156, alpha };
    const Color STONE_D = { 125, 120, 115, alpha };
    const Color WATER = { 70, 130, 200, alpha };
    const Color GOLDC = { 225, 180, 60, alpha };
    ShadedCylinder(ToWorld3D(p, 0.0f), ToWorld3D(p, 22.0f), 100.0f, 98.0f, STONE_C, 36);          // Ytterkant
    ShadedCylinder(ToWorld3D(p, 22.0f), ToWorld3D(p, 25.0f), 101.0f, 101.0f, STONE_D, 36);        // Kant oppe
    ShadedCylinder(ToWorld3D(p, 25.0f), ToWorld3D(p, 25.5f), 88.0f, 88.0f, WATER, 36);            // Vann
    ShadedCylinder(ToWorld3D(p, 25.0f), ToWorld3D(p, 55.0f), 10.0f, 7.0f, STONE_C, 14);           // Midtsøyle
    ShadedCylinder(ToWorld3D(p, 55.0f), ToWorld3D(p, 62.0f), 8.0f, 36.0f, STONE_C, 24);           // Nedre skål
    ShadedCylinder(ToWorld3D(p, 62.0f), ToWorld3D(p, 62.5f), 32.0f, 32.0f, WATER, 24);
    ShadedCylinder(ToWorld3D(p, 62.0f), ToWorld3D(p, 80.0f), 5.0f, 4.0f, STONE_C, 10);
    ShadedCylinder(ToWorld3D(p, 80.0f), ToWorld3D(p, 85.0f), 4.0f, 18.0f, STONE_C, 16);           // Øvre skål
    ShadedCylinder(ToWorld3D(p, 85.0f), ToWorld3D(p, 85.5f), 15.0f, 15.0f, WATER, 16);
    ShadedSphere(ToWorld3D(p, 89.0f), 4.0f, GOLDC, 4, 6);
}

void drawDecor(const DecorItem& d, unsigned char alpha = 255) {
    rlPushMatrix();
    rlTranslatef(d.pos.x, 0.0f, d.pos.y);
    rlScalef(DECOR_SCALE, DECOR_SCALE, DECOR_SCALE);
    rlTranslatef(-d.pos.x, 0.0f, -d.pos.y);
    switch (d.kind) {
        case Decor::VASE:     drawVaseDecor(d.pos, alpha); break;
        case Decor::ARMOR:    drawArmorDecor(d.pos, alpha); break;
        case Decor::TOPIARY:  drawTopiaryDecor(d.pos, alpha); break;
        case Decor::STATUE:   drawStatueDecor(d.pos, d.variant, alpha); break;
        case Decor::BANNER:   drawBannerDecor(d.pos, d.variant, alpha); break;
        case Decor::RACK:     drawRackDecor(d.pos, alpha); break;
        case Decor::FOUNTAIN: drawFountainDecor(d.pos, alpha); break;
    }
    rlPopMatrix();
}

// Står tingen mellom kameraet og `focus` (spilleren) på skjermen? 0 = nei, 1 = helt over.
float decorCover(const DecorItem& d, const Camera3D& cam, Vector2 focus) {
    if (d.kind == Decor::FOUNTAIN) return 0.0f; // Lav og bred – skjuler ingenting
    Vector3 fwd = Vector3Subtract(cam.target, cam.position);
    Vector2 fwd2 = Vector2Normalize({ fwd.x, fwd.z });
    if (Vector2DotProduct(Vector2Subtract(d.pos, focus), fwd2) > 0.0f) return 0.0f;
    Vector2 b = GetWorldToScreen(ToWorld3D(d.pos, 0.0f), cam);
    Vector2 t = GetWorldToScreen(ToWorld3D(d.pos, d.height), cam);
    Vector2 f = GetWorldToScreen(ToWorld3D(focus, 35.0f), cam);
    Vector2 bt = Vector2Subtract(t, b);
    float len2 = Vector2DotProduct(bt, bt);
    float u = len2 > 0.0f ? Clamp(Vector2DotProduct(Vector2Subtract(f, b), bt) / len2, 0.0f, 1.0f) : 0.0f;
    Vector2 c = Vector2Add(b, Vector2Scale(bt, u));
    Vector3 right = Vector3Normalize(Vector3CrossProduct(fwd, { 0, 1, 0 }));
    Vector2 edge = GetWorldToScreen(Vector3Add(ToWorld3D(d.pos, d.height * u), Vector3Scale(right, d.radius + 6.0f)), cam);
    float dist = Vector2Distance(f, c) - Vector2Distance(edge, c);
    const float MARGIN = 30.0f;
    return Clamp(1.0f - dist / MARGIN, 0.0f, 1.0f);
}

// --- TEKSTURER til salenes gulv (laget av tools/gen_room_textures.py) ---
struct FloorTex { Texture2D t{}; bool ok = false; };
FloorTex texGrass, texGravel, texSlate, texFlowers, texRunnerRed, texRunnerBlue, texDelftRug, texMedallion;

void loadFloorTex(FloorTex& f, const char* path, bool tiled) {
    if (!FileExists(path)) return;
    f.t = LoadTexture(path);
    GenTextureMipmaps(&f.t);
    SetTextureFilter(f.t, TEXTURE_FILTER_TRILINEAR);
    SetTextureWrap(f.t, tiled ? TEXTURE_WRAP_REPEAT : TEXTURE_WRAP_CLAMP);
    f.ok = f.t.id != 0;
}

// Teksturerte former på gulvet. UV følger verdenskoordinatene (scale = verdensenheter per tekstur),
// så teksturen ligger fast på gulvet og går sømløst over flere former.
void texBegin(const FloorTex& f) {
    rlDrawRenderBatchActive();
    rlDisableBackfaceCulling();
    rlSetTexture(f.t.id);
    rlBegin(RL_TRIANGLES);
}
void texEnd() {
    rlEnd();
    rlSetTexture(0);
    rlDrawRenderBatchActive();
    rlEnableBackfaceCulling();
}
void texVertex(Vector2 p, float u, float v, Color c) {
    rlColor4ub(c.r, c.g, c.b, c.a);
    rlTexCoord2f(u, v);
    rlVertex2f(p.x, p.y);
}
void texQuadWorld(Vector2 a, Vector2 b, Vector2 c, Vector2 d, float scale, Color tint) {
    auto V = [&](Vector2 p) { texVertex(p, p.x / scale, p.y / scale, tint); };
    V(a); V(b); V(c); V(a); V(c); V(d);
}

// Firkant med verdens-UV
void texRect(const FloorTex& f, Rectangle r, float scale, Color tint = WHITE) {
    if (!f.ok) { DrawRectangleRec(r, tint); return; }
    texBegin(f);
    texQuadWorld({ r.x, r.y }, { r.x + r.width, r.y }, { r.x + r.width, r.y + r.height }, { r.x, r.y + r.height }, scale, tint);
    texEnd();
}

// Ring (eller skive når inner = 0) med verdens-UV
void texRing(const FloorTex& f, Vector2 c, float inner, float outer, float scale, Color tint = WHITE) {
    if (!f.ok) { DrawRing(c, inner, outer, 0.0f, 360.0f, 64, tint); return; }
    texBegin(f);
    const int SEG = 72;
    for (int i = 0; i < SEG; i++) {
        float a0 = i * 2.0f * PI / SEG, a1 = (i + 1) * 2.0f * PI / SEG;
        Vector2 o0 = { c.x + cosf(a0) * outer, c.y + sinf(a0) * outer }, o1 = { c.x + cosf(a1) * outer, c.y + sinf(a1) * outer };
        Vector2 i0 = { c.x + cosf(a0) * inner, c.y + sinf(a0) * inner }, i1 = { c.x + cosf(a1) * inner, c.y + sinf(a1) * inner };
        texQuadWorld(i0, o0, o1, i1, scale, tint);
    }
    texEnd();
}

// Stripe fra a til b med en viss bredde (grusganger)
void texStrip(const FloorTex& f, Vector2 a, Vector2 b, float width, float scale, Color tint = WHITE) {
    Vector2 d = Vector2Normalize(Vector2Subtract(b, a));
    Vector2 n = { -d.y * width / 2.0f, d.x * width / 2.0f };
    if (!f.ok) { DrawLineEx(a, b, width, tint); return; }
    texBegin(f);
    texQuadWorld(Vector2Add(a, n), Vector2Add(b, n), Vector2Subtract(b, n), Vector2Subtract(a, n), scale, tint);
    texEnd();
}

// Hele teksturen lagt på en rund skive (tepper og medaljonger)
void texDiscMapped(const FloorTex& f, Vector2 c, float r) {
    if (!f.ok) { DrawCircleV(c, r, WHITE); return; }
    texBegin(f);
    const int SEG = 96;
    for (int i = 0; i < SEG; i++) {
        float a0 = i * 2.0f * PI / SEG, a1 = (i + 1) * 2.0f * PI / SEG;
        texVertex(c, 0.5f, 0.5f, WHITE);
        texVertex({ c.x + cosf(a0) * r, c.y + sinf(a0) * r }, 0.5f + cosf(a0) * 0.5f, 0.5f + sinf(a0) * 0.5f, WHITE);
        texVertex({ c.x + cosf(a1) * r, c.y + sinf(a1) * r }, 0.5f + cosf(a1) * 0.5f, 0.5f + sinf(a1) * 0.5f, WHITE);
    }
    texEnd();
}

// Løper: teksturen går på tvers (u) og gjentas langs (v)
void texRunner(const FloorTex& f, Vector2 c, float width, float length, bool alongY) {
    if (!f.ok) { DrawRectangleRec(alongY ? Rectangle{ c.x - width / 2, c.y - length / 2, width, length } : Rectangle{ c.x - length / 2, c.y - width / 2, length, width }, CARPET_RED); return; }
    float vlen = length / width;
    texBegin(f);
    Vector2 p[4]; float uv[4][2];
    if (alongY) {
        p[0] = { c.x - width / 2, c.y - length / 2 }; p[1] = { c.x + width / 2, c.y - length / 2 };
        p[2] = { c.x + width / 2, c.y + length / 2 }; p[3] = { c.x - width / 2, c.y + length / 2 };
    } else {
        p[0] = { c.x - length / 2, c.y + width / 2 }; p[1] = { c.x - length / 2, c.y - width / 2 };
        p[2] = { c.x + length / 2, c.y - width / 2 }; p[3] = { c.x + length / 2, c.y + width / 2 };
    }
    float u[4] = { 0, 1, 1, 0 }, v[4] = { 0, 0, vlen, vlen };
    (void)uv;
    int idx[6] = { 0, 1, 2, 0, 2, 3 };
    for (int k : idx) texVertex(p[k], u[k], v[k], WHITE);
    texEnd();
}

// --- GULVET I HVER SAL: løpere, gress, tepper og steingulv som gir salene sitt eget preg ---
void drawRoomFloor(const RoomInfo& r) {
    Vector2 c = r.center;
    const Color STONE_LIGHT = { 205, 198, 188, 255 };
    switch (r.room) {
        case Room::STATUES:
            // Vevd rød løper med kronemedaljonger mellom statuerekkene, med steinkant
            DrawRectangleRec(r.turn ? Rectangle{ c.x - 616, c.y - 146, 1232, 292 } : Rectangle{ c.x - 146, c.y - 616, 292, 1232 }, Color{ 60, 50, 50, 90 });
            texRunner(texRunnerRed, c, 270.0f, 1200.0f, !r.turn);
            break;
        case Room::BANNERS:
            // Vevd koboltblå løper med liljer
            DrawRectangleRec(r.turn ? Rectangle{ c.x - 636, c.y - 141, 1272, 282 } : Rectangle{ c.x - 141, c.y - 636, 282, 1272 }, Color{ 40, 40, 60, 90 });
            texRunner(texRunnerBlue, c, 260.0f, 1240.0f, !r.turn);
            break;
        case Room::ARMORY: {
            // Skiferheller med jernramme og nagler
            Rectangle area = { c.x - 520, c.y - 520, 1040, 1040 };
            DrawRectangleRec({ area.x - 16, area.y - 16, area.width + 32, area.height + 32 }, Color{ 48, 46, 54, 255 });
            texRect(texSlate, area, 256.0f);
            DrawRectangleLinesEx({ area.x - 10, area.y - 10, area.width + 20, area.height + 20 }, 8.0f, Color{ 92, 90, 98, 255 });
            for (int k = 0; k <= 16; k++) {
                float t = area.x - 10 + k * (area.width + 20) / 16.0f;
                for (float yy : { area.y - 6, area.y + area.height + 6 }) DrawCircleV({ t, yy }, 3.5f, Color{ 150, 148, 158, 255 });
                float t2 = area.y - 10 + k * (area.height + 20) / 16.0f;
                for (float xx : { area.x - 6, area.x + area.width + 6 }) DrawCircleV({ xx, t2 }, 3.5f, Color{ 150, 148, 158, 255 });
            }
            // Rundt skjold-emblem i midten
            DrawCircleV(c, 120.0f, Color{ 222, 178, 64, 255 });
            DrawCircleV(c, 108.0f, Color{ 140, 28, 40, 255 });
            DrawRing(c, 60.0f, 68.0f, 0.0f, 360.0f, 48, Color{ 222, 178, 64, 255 });
            for (int k = 0; k < 2; k++) {                                 // Kryssede sverd
                float a = (45.0f + k * 90.0f) * DEG2RAD;
                Vector2 d = { cosf(a) * 95.0f, sinf(a) * 95.0f };
                DrawLineEx(Vector2Subtract(c, d), Vector2Add(c, d), 9.0f, Color{ 210, 214, 225, 255 });
            }
            break;
        }
        case Room::GARDEN: {
            // Gressplen med steinkant, grusganger, grus rundt fontenen og blomsterbed rundt hekkene
            texRing(texSlate, c, 580.0f, 612.0f, 128.0f, STONE_LIGHT);
            texRing(texGrass, c, 0.0f, 582.0f, 170.0f);
            for (int k = 0; k < 4; k++) {
                Vector2 dir = { cosf(k * PI / 2.0f), sinf(k * PI / 2.0f) };
                texStrip(texGravel, Vector2Add(c, Vector2Scale(dir, 150.0f)), Vector2Add(c, Vector2Scale(dir, 584.0f)), 80.0f, 128.0f);
            }
            texRing(texGravel, c, 245.0f, 330.0f, 128.0f);
            texRing(texGravel, c, 0.0f, 175.0f, 128.0f);
            texRing(texSlate, c, 168.0f, 182.0f, 128.0f, STONE_LIGHT);
            for (int k = 0; k < 8; k++) {
                float a = (22.5f + k * 45.0f) * DEG2RAD;
                Vector2 bed = { c.x + cosf(a) * 420.0f, c.y + sinf(a) * 420.0f };
                texRing(texSlate, bed, 78.0f, 90.0f, 128.0f, STONE_LIGHT);
                texRing(texFlowers, bed, 0.0f, 80.0f, 110.0f);
            }
            break;
        }
        case Room::PORCELAIN:
            // Stort rundt delftteppe
            DrawCircleV({ c.x + 6, c.y + 8 }, 566.0f, Color{ 0, 0, 0, 50 });
            texDiscMapped(texDelftRug, c, 560.0f);
            break;
        case Room::GRAND:
            // Innlagt marmormedaljong med kompassrose under statuen
            texDiscMapped(texMedallion, c, 230.0f);
            break;
    }
}

// --- MURVEGGEN rundt slottet ---
// Steinblokker i to nyanser, en lys kant oppå og pilastre med vegglykter
constexpr float WALL_SEGMENT = 128.0f;
constexpr float SCONCE_EVERY = 512.0f;

template <typename F>
void forEachWallSegment(Vector2 center, float radius, F&& fn) {
    // fn(midtpunkt, lengderetning (0 = langs x), segmentnummer, side)
    for (int side = 0; side < 4; side++) {
        bool alongX = side < 2;
        float fixed = (side % 2 == 0) ? -MAP_HALF - WALL_T / 2.0f : MAP_HALF + WALL_T / 2.0f;
        float fixedCam = alongX ? center.y : center.x;
        if (fabsf(fixed - fixedCam) > radius + WALL_T) continue;
        float from = fmaxf(-MAP_HALF - WALL_T, (alongX ? center.x : center.y) - radius);
        float to = fminf(MAP_HALF + WALL_T, (alongX ? center.x : center.y) + radius);
        int i0 = (int)floorf((from + MAP_HALF + WALL_T) / WALL_SEGMENT), i1 = (int)ceilf((to + MAP_HALF + WALL_T) / WALL_SEGMENT);
        for (int i = i0; i <= i1; i++) {
            float along = -MAP_HALF - WALL_T + (i + 0.5f) * WALL_SEGMENT;
            if (along > MAP_HALF + WALL_T) continue;
            Vector2 m = alongX ? Vector2{ along, fixed } : Vector2{ fixed, along };
            fn(m, alongX, i, side);
        }
    }
}

// Vegglykter på innsiden av muren
template <typename F>
void forEachSconce(Vector2 center, float radius, F&& fn) {
    for (int side = 0; side < 4; side++) {
        for (float a = -MAP_HALF + SCONCE_EVERY / 2.0f; a < MAP_HALF; a += SCONCE_EVERY) {
            float inner = (side % 2 == 0) ? -MAP_HALF + 12.0f : MAP_HALF - 12.0f;
            Vector2 p = side < 2 ? Vector2{ a, inner } : Vector2{ inner, a };
            if (fabsf(p.x - center.x) < radius && fabsf(p.y - center.y) < radius) fn(p, side);
        }
    }
}


} // namespace

int PillarsNear(Vector2 center, float radius, Vector2* out, int maxCount) {
    int n = 0;
    forEachDecor(center, radius, [&](const DecorItem& d) { if (n < maxCount) out[n++] = d.pos; });
    return n;
}

bool ResolvePillarCollision(Vector2& pos, float radius, Vector2 goal, float slide) {
    bool hit = false;
    forEachDecor(pos, radius + MAX_DECOR_RADIUS + 4.0f, [&](const DecorItem& d) {
        Vector2 dv = Vector2Subtract(pos, d.pos);
        float dist = Vector2Length(dv);
        float minDist = radius + d.radius;
        if (dist >= minDist) return;
        hit = true;
        Vector2 nrm = dist > 0.01f ? Vector2Scale(dv, 1.0f / dist) : Vector2{ 1, 0 };
        pos = Vector2Add(d.pos, Vector2Scale(nrm, minDist));
        // Gli rundt tingen på den siden som er nærmest målet (så fiender ikke setter seg fast)
        if (slide > 0.0f) {
            Vector2 tangent = { -nrm.y, nrm.x };
            Vector2 toGoal = Vector2Subtract(goal, pos);
            if (Vector2DotProduct(tangent, toGoal) < 0.0f) tangent = Vector2Scale(tangent, -1.0f);
            pos = Vector2Add(pos, Vector2Scale(tangent, slide));
        }
    });
    // Murveggen
    pos = ClampToCastle(pos, radius);
    return hit;
}

Vector2 ClampToCastle(Vector2 pos, float margin) {
    float m = MAP_HALF - margin;
    return { Clamp(pos.x, -m, m), Clamp(pos.y, -m, m) };
}

bool InsideCastle(Vector2 pos, float margin) {
    float m = MAP_HALF - margin;
    return fabsf(pos.x) <= m && fabsf(pos.y) <= m;
}

float CastleHalfSize() { return MAP_HALF; }

const char* CastleRoomName(Vector2 pos) {
    int ix = (int)floorf(pos.x / CARPET_SPACING) + 2, iy = (int)floorf(pos.y / CARPET_SPACING) + 2;
    if (ix < 0 || ix > 3 || iy < 0 || iy > 3) return "";
    return ROOM_NAMES[(int)ROOMS[iy][ix]];
}

void InitCastleTextures() {
    const char* white = "assets/floor/porcelain_white.png";
    const char* blue = "assets/floor/porcelain_blue.png";
    if (!FileExists(white) || !FileExists(blue)) return;
    Image atlas = GenImageColor(ATLAS_TILE * 2, ATLAS_TILE, BLACK); // RGBA8
    const char* files[2] = { white, blue };
    for (int i = 0; i < 2; i++) {
        Image img = LoadImage(files[i]);
        ImageResize(&img, ATLAS_TILE, ATLAS_TILE);
        ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
        // Dempes litt mot hvitt/lyst, så figurene og effektene synes godt oppå gulvet
        float soften = (i == 0) ? 0.35f : 0.12f;
        Color* px = (Color*)img.data;
        for (int k = 0; k < img.width * img.height; k++) {
            px[k].r = (unsigned char)(px[k].r + (255 - px[k].r) * soften);
            px[k].g = (unsigned char)(px[k].g + (255 - px[k].g) * soften);
            px[k].b = (unsigned char)(px[k].b + (255 - px[k].b) * soften);
        }
        // Kopier inn i atlaset piksel for piksel (ImageDraw finnes ikke i alle raylib-versjoner)
        Color* dst = (Color*)atlas.data;
        for (int y = 0; y < ATLAS_TILE; y++)
            for (int x = 0; x < ATLAS_TILE; x++)
                dst[y * ATLAS_TILE * 2 + i * ATLAS_TILE + x] = px[y * ATLAS_TILE + x];
        UnloadImage(img);
    }
    loadFloorTex(texGrass, "assets/floor/grass.png", true);
    loadFloorTex(texGravel, "assets/floor/gravel.png", true);
    loadFloorTex(texSlate, "assets/floor/slate.png", true);
    loadFloorTex(texFlowers, "assets/floor/flowerbed.png", true);
    loadFloorTex(texRunnerRed, "assets/floor/runner_red.png", true);
    loadFloorTex(texRunnerBlue, "assets/floor/runner_blue.png", true);
    loadFloorTex(texDelftRug, "assets/floor/delft_rug.png", false);
    loadFloorTex(texMedallion, "assets/floor/medallion.png", false);
    floorAtlas = LoadTextureFromImage(atlas);
    UnloadImage(atlas);
    GenTextureMipmaps(&floorAtlas);
    SetTextureFilter(floorAtlas, TEXTURE_FILTER_TRILINEAR);
    floorLoaded = floorAtlas.id != 0;
}

void UnloadCastleTextures() {
    if (floorLoaded) UnloadTexture(floorAtlas);
    floorLoaded = false;
}

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

    const int edge = (int)(MAP_HALF / TILE);
    for (int ty = startY; ty <= endY; ty++) {
        for (int tx = startX; tx <= endX; tx++) {
            bool inside = tx >= -edge && tx < edge && ty >= -edge && ty < edge;
            if (!inside) drawTile(tx, ty, Color{ 52, 48, 58, 255 }, Color{ 40, 37, 46, 255 }); // Mørk stein utenfor muren
            else if (floorLoaded) drawPorcelainTile(tx, ty, 1.0f);
            else drawTile(tx, ty, MARBLE_LIGHT, MARBLE_DARK);
        }
    }

    // Hver sal sitt gulv (løpere, gress, tepper ...)
    buildMap();
    for (const RoomInfo& r : roomInfos)
        if (fabsf(r.center.x - center.x) < viewRadius + 800.0f && fabsf(r.center.y - center.y) < viewRadius + 800.0f) drawRoomFloor(r);

    // Røde løpere mellom salene (bare inne i slottet)
    float inMinX = fmaxf(minX, -MAP_HALF), inMaxX = fminf(maxX, MAP_HALF);
    float inMinY = fmaxf(minY, -MAP_HALF), inMaxY = fminf(maxY, MAP_HALF);
    for (int i = -1; i <= 1; i++) {
        float c = (float)(i * CARPET_SPACING);
        if (c > minX - CARPET_WIDTH && c < maxX + CARPET_WIDTH && inMaxY > inMinY)
            drawCarpet({ c - CARPET_WIDTH / 2.0f, inMinY, (float)CARPET_WIDTH, inMaxY - inMinY }, true);
        if (c > minY - CARPET_WIDTH && c < maxY + CARPET_WIDTH && inMaxX > inMinX)
            drawCarpet({ inMinX, c - CARPET_WIDTH / 2.0f, inMaxX - inMinX, (float)CARPET_WIDTH }, false);
    }
    // Emblem der løperne krysser, og skygger under fyrfatene og pynten
    for (int ix = -1; ix <= 1; ix++)
        for (int iy = -1; iy <= 1; iy++)
            drawEmblem({ (float)(ix * CARPET_SPACING), (float)(iy * CARPET_SPACING) }, CARPET_WIDTH * 0.62f);
    forEachBrazier(center, viewRadius, [](Vector2 p, float) { DrawShadow({ p.x + 5.0f, p.y + 5.0f }, 16.0f, 11.0f); });
    forEachDecor(center, viewRadius, [](const DecorItem& d) {
        DrawShadow({ d.pos.x + 8.0f, d.pos.y + 9.0f }, d.radius * 1.3f, d.radius * 1.0f);
    });
    // Skygge langs foten av muren
    float m = MAP_HALF;
    DrawRectangleGradientV((int)-m, (int)-m, (int)(2 * m), 40, Fade(BLACK, 0.35f), Fade(BLACK, 0.0f));
    DrawRectangleGradientH((int)-m, (int)-m, 40, (int)(2 * m), Fade(BLACK, 0.3f), Fade(BLACK, 0.0f));
    DrawRectangleGradientH((int)m - 40, (int)-m, 40, (int)(2 * m), Fade(BLACK, 0.0f), Fade(BLACK, 0.3f));
}

void DrawCastleProps3D(Vector2 center, float viewRadius, const Camera3D& camera) {
    forEachBrazier(center, viewRadius, [](Vector2 p, float) { drawBrazier(p); });
    // Pynt som skjuler spilleren tegnes gjennomsiktig til slutt (DrawCastlePillarsFaded)
    forEachDecor(center, viewRadius, [&](const DecorItem& d) { if (decorCover(d, camera, center) <= 0.0f) drawDecor(d); });

    // Murveggen: steinblokker, lys kant oppå og pilastre der vegglyktene henger
    forEachWallSegment(center, viewRadius, [](Vector2 m, bool alongX, int i, int side) {
        Color block = shade(STONE, (i % 2 == 0) ? 0 : -10);
        Vector3 size = alongX ? Vector3{ WALL_SEGMENT, WALL_H, WALL_T } : Vector3{ WALL_T, WALL_H, WALL_SEGMENT };
        ShadedCube(ToWorld3D(m, WALL_H / 2.0f), size, 0.0f, block);
        Vector3 cap = alongX ? Vector3{ WALL_SEGMENT, 10.0f, WALL_T + 10.0f } : Vector3{ WALL_T + 10.0f, 10.0f, WALL_SEGMENT };
        ShadedCube(ToWorld3D(m, WALL_H + 5.0f), cap, 0.0f, shade(STONE, 25));
        // Murtinder på annenhver blokk
        if (i % 2 == 0) {
            Vector3 crenel = alongX ? Vector3{ WALL_SEGMENT * 0.5f, 18.0f, WALL_T } : Vector3{ WALL_T, 18.0f, WALL_SEGMENT * 0.5f };
            ShadedCube(ToWorld3D(m, WALL_H + 19.0f), crenel, 0.0f, shade(STONE, 12));
        }
        (void)side;
    });
    forEachSconce(center, viewRadius, [](Vector2 p, int side) {
        // Pilaster med en jernlykt
        Vector2 in = side == 0 ? Vector2{ 0, 1 } : side == 1 ? Vector2{ 0, -1 } : side == 2 ? Vector2{ 1, 0 } : Vector2{ -1, 0 };
        ShadedCube(ToWorld3D(p, WALL_H / 2.0f + 6.0f), { 36.0f, WALL_H + 12.0f, 36.0f }, 0.0f, shade(STONE, 18));
        Vector2 lamp = Vector2Add(p, Vector2Scale(in, 24.0f));
        ShadedCylinder(ToWorld3D(lamp, 58.0f), ToWorld3D(lamp, 70.0f), 5.0f, 9.0f, Color{ 50, 45, 50, 255 }, 8);
        ShadedSphere(ToWorld3D(lamp, 70.0f), 6.0f, Color{ 255, 150, 60, 255 }, 4, 6);
    });
}

void DrawCastlePillarsFaded(Vector2 center, float viewRadius, const Camera3D& camera) {
    // Tegnes etter figurene, så spilleren synes gjennom
    rlDrawRenderBatchActive();
    forEachDecor(center, viewRadius, [&](const DecorItem& d) {
        float cover = decorCover(d, camera, center);
        if (cover > 0.0f) drawDecor(d, (unsigned char)(255.0f * (1.0f - 0.65f * cover)));
    });
    rlDrawRenderBatchActive();
}

void DrawCastlePropsVfx(Vector2 center, float viewRadius) {
    forEachBrazier(center, viewRadius, [](Vector2 p, float seed) { brazierFire(p, seed); });
    // Vegglyktene: liten flamme og en varm lyspøl på gulvet
    forEachSconce(center, viewRadius, [](Vector2 p, int side) {
        Vector2 in = side == 0 ? Vector2{ 0, 1 } : side == 1 ? Vector2{ 0, -1 } : side == 2 ? Vector2{ 1, 0 } : Vector2{ -1, 0 };
        Vector2 lamp = Vector2Add(p, Vector2Scale(in, 24.0f));
        float t = (float)GetTime();
        float flicker = 0.85f + 0.15f * sinf(t * 11.0f + p.x * 0.01f + p.y * 0.013f);
        VfxDecal(VfxTex::GLOW, Vector2Add(p, Vector2Scale(in, 70.0f)), 200.0f * flicker, Color{ 70, 42, 16, 255 }, 0.0f, 0.8f);
        VfxBillboard(VfxTex::EXPLOSION, ToWorld3D(lamp, 80.0f), 22.0f * flicker, Color{ 255, 200, 140, 255 }, t * 70.0f + p.x);
        VfxBillboard(VfxTex::GLOW, ToWorld3D(lamp, 76.0f), 55.0f, Color{ 150, 75, 20, 255 });
    });
    // Fontenene: vann som glitrer og spruter
    forEachDecor(center, viewRadius, [](const DecorItem& d) {
        if (d.kind != Decor::FOUNTAIN) return;
        float t = (float)GetTime();
        VfxBillboard(VfxTex::GLOW, ToWorld3D(d.pos, 92.0f), 40.0f, Color{ 60, 110, 160, 255 });
        VfxDecal(VfxTex::SHOCKWAVE, d.pos, 120.0f + 20.0f * sinf(t * 2.0f), Color{ 40, 70, 110, 255 }, t * 15.0f, 26.0f);
        if (GetRandomValue(0, 3) == 0) {
            float a = GetRandomValue(0, 360) * DEG2RAD;
            VfxBubble({ d.pos.x + cosf(a) * 30.0f, d.pos.y + sinf(a) * 30.0f }, 60.0f, Color{ 160, 210, 255, 255 });
        }
    });
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
                if (floorLoaded) drawPorcelainTile(worldTx, worldTy, 0.72f); // Litt mørkere i tronsalen
                else drawTile(worldTx, worldTy, THRONE_LIGHT, THRONE_DARK);
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
