#include "icons.hpp"
#include "ui.hpp"
#include <raymath.h>
#include <cmath>

namespace {

constexpr Color INK    = { 12, 10, 16, 255 };
constexpr Color STEEL  = { 205, 212, 225, 255 };
constexpr Color STEEL_D= { 120, 128, 145, 255 };
constexpr Color WOOD   = { 130, 85, 45, 255 };
constexpr Color GOLD_L = { 255, 214, 90, 255 };
constexpr Color GOLD_D = { 170, 115, 25, 255 };
constexpr Color HEART  = { 225, 45, 60, 255 };
constexpr Color GREEN_L= { 110, 220, 90, 255 };
constexpr Color BLUE_L = { 90, 170, 255, 255 };
constexpr Color VIOLET_L = { 170, 90, 240, 255 };

// Trekant uansett hjørnerekkefølge
void tri(Vector2 a, Vector2 b, Vector2 c, Color col) {
    float cross = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    if (cross < 0.0f) DrawTriangle(a, b, c, col);
    else DrawTriangle(a, c, b, col);
}

void quad(Vector2 a, Vector2 b, Vector2 c, Vector2 d, Color col) {
    tri(a, b, c, col);
    tri(a, c, d, col);
}

// Linje med mørk kontur (tykkelse w)
void inkLine(Vector2 a, Vector2 b, float w, Color col) {
    DrawLineEx(a, b, w + 3.0f, INK);
    DrawCircleV(a, (w + 3.0f) / 2.0f, INK);
    DrawCircleV(b, (w + 3.0f) / 2.0f, INK);
    DrawLineEx(a, b, w, col);
    DrawCircleV(a, w / 2.0f, col);
    DrawCircleV(b, w / 2.0f, col);
}

void inkCircle(Vector2 c, float r, Color col) {
    DrawCircleV(c, r + 1.8f, INK);
    DrawCircleV(c, r, col);
    DrawCircleV({ c.x - r * 0.3f, c.y - r * 0.3f }, r * 0.35f, Fade(WHITE, 0.45f));
}

Vector2 P(Vector2 c, float s, float x, float y) { return { c.x + x * s, c.y + y * s }; }

// Hjerte: to sirkler og en trekant
void heart(Vector2 c, float s, Color col) {
    for (int pass = 0; pass < 2; pass++) {
        float g = pass == 0 ? 0.12f : 0.0f; // Kontur først
        Color k = pass == 0 ? INK : col;
        DrawCircleV(P(c, s, -0.42f, -0.25f), s * (0.46f + g), k);
        DrawCircleV(P(c, s, 0.42f, -0.25f), s * (0.46f + g), k);
        tri(P(c, s, -0.9f - g, -0.1f), P(c, s, 0.9f + g, -0.1f), P(c, s, 0.0f, 0.85f + g * 1.5f), k);
    }
    DrawCircleV(P(c, s, -0.5f, -0.38f), s * 0.16f, Fade(WHITE, 0.6f));
}

void plus(Vector2 c, float s, Color col) {
    DrawRectangleRec({ c.x - s * 0.5f - 1.5f, c.y - s * 0.17f - 1.5f, s + 3.0f, s * 0.34f + 3.0f }, INK);
    DrawRectangleRec({ c.x - s * 0.17f - 1.5f, c.y - s * 0.5f - 1.5f, s * 0.34f + 3.0f, s + 3.0f }, INK);
    DrawRectangleRec({ c.x - s * 0.5f, c.y - s * 0.17f, s, s * 0.34f }, col);
    DrawRectangleRec({ c.x - s * 0.17f, c.y - s * 0.5f, s * 0.34f, s }, col);
}

void shield(Vector2 c, float s, Color face, Color rim) {
    for (int pass = 0; pass < 3; pass++) {
        float g = pass == 0 ? 0.14f : (pass == 1 ? 0.07f : 0.0f);
        Color k = pass == 0 ? INK : (pass == 1 ? rim : face);
        float w = 0.72f + g, top = -0.8f - g;
        DrawRectangleRec({ c.x - w * s, c.y + top * s, w * 2.0f * s, (0.15f - top) * s }, k);
        tri(P(c, s, -w, 0.1f), P(c, s, w, 0.1f), P(c, s, 0.0f, 0.95f + g), k);
    }
}

void sword(Vector2 c, float s, float angle, Color blade) {
    Vector2 dir = { cosf(angle), sinf(angle) };
    Vector2 side = { -dir.y, dir.x };
    auto at = [&](float along, float across) {
        return Vector2{ c.x + (dir.x * along + side.x * across) * s, c.y + (dir.y * along + side.y * across) * s };
    };
    // Blad med kontur
    quad(at(-0.35f, -0.2f), at(0.75f, -0.2f), at(0.75f, 0.2f), at(-0.35f, 0.2f), INK);
    tri(at(0.75f, -0.2f), at(0.75f, 0.2f), at(1.05f, 0.0f), INK);
    quad(at(-0.3f, -0.12f), at(0.75f, -0.12f), at(0.75f, 0.12f), at(-0.3f, 0.12f), blade);
    tri(at(0.75f, -0.12f), at(0.75f, 0.12f), at(0.95f, 0.0f), blade);
    DrawLineEx(at(-0.3f, 0.0f), at(0.85f, 0.0f), 1.5f, Fade(WHITE, 0.6f));
    // Parerstang og skaft
    inkLine(at(-0.38f, -0.42f), at(-0.38f, 0.42f), s * 0.16f, GOLD_L);
    inkLine(at(-0.45f, 0.0f), at(-0.85f, 0.0f), s * 0.14f, WOOD);
    inkCircle(at(-0.92f, 0.0f), s * 0.1f, GOLD_L);
}

} // namespace

void DrawAbilityIcon(AbilityId id, Vector2 c, float s) {
    float t = (float)GetTime();
    switch (id) {
        case AbilityId::TREFORK: { // Trefork: skaft og tre tinder
            inkLine(P(c, s, 0.0f, 0.95f), P(c, s, 0.0f, -0.35f), s * 0.16f, WOOD);
            inkLine(P(c, s, -0.5f, -0.3f), P(c, s, 0.5f, -0.3f), s * 0.16f, STEEL);
            for (int i = -1; i <= 1; i++) {
                Vector2 base = P(c, s, i * 0.5f, -0.3f);
                Vector2 tip = P(c, s, i * 0.5f, i == 0 ? -1.0f : -0.8f);
                inkLine(base, tip, s * 0.13f, STEEL);
                tri({ tip.x - s * 0.16f, tip.y + s * 0.05f }, { tip.x + s * 0.16f, tip.y + s * 0.05f }, { tip.x, tip.y - s * 0.22f }, STEEL);
            }
        } break;
        case AbilityId::GROUND_SLAM: { // Hammer som slår i bakken med sprekker
            for (int i = -2; i <= 2; i++) {
                if (i == 0) continue;
                inkLine(P(c, s, i * 0.12f, 0.75f), P(c, s, i * 0.42f, 0.95f - fabsf(i) * 0.12f), s * 0.07f, GOLD_L);
            }
            inkLine(P(c, s, -0.8f, 0.8f), P(c, s, 0.8f, 0.8f), s * 0.1f, Color{ 140, 110, 80, 255 });
            inkLine(P(c, s, 0.55f, -0.75f), P(c, s, -0.05f, 0.35f), s * 0.16f, WOOD);
            Vector2 h0 = P(c, s, -0.55f, 0.1f), h1 = P(c, s, 0.25f, 0.55f);
            inkLine(h0, h1, s * 0.5f, STEEL_D);
            DrawLineEx(h0, h1, s * 0.3f, STEEL);
        } break;
        case AbilityId::RICOCHET: { // Kule som spretter i sikksakk
            Vector2 pts[4] = { P(c, s, -0.9f, -0.6f), P(c, s, -0.35f, 0.6f), P(c, s, 0.2f, -0.45f), P(c, s, 0.65f, 0.35f) };
            for (int i = 0; i < 3; i++) {
                DrawLineEx(pts[i], pts[i + 1], s * 0.12f, INK);
                DrawLineEx(pts[i], pts[i + 1], s * 0.06f, Fade(BLUE_L, 0.6f + i * 0.13f));
            }
            for (int i = 1; i < 3; i++) DrawCircleV(pts[i], s * 0.1f, Fade(WHITE, 0.7f));
            inkCircle(pts[3], s * 0.3f, BLUE_L);
        } break;
        case AbilityId::MAGIC_MISSILE: { // Komet med hale
            for (int i = 5; i >= 1; i--) {
                Vector2 p = P(c, s, -0.15f * i - 0.1f, 0.15f * i + 0.1f);
                DrawCircleV(p, s * (0.34f - i * 0.05f), Fade(VIOLET_L, 0.25f + 0.1f * (5 - i)));
            }
            Vector2 head = P(c, s, 0.25f, -0.25f);
            DrawCircleV(head, s * 0.55f, Fade(VIOLET_L, 0.3f));
            inkCircle(head, s * 0.34f, Color{ 220, 170, 255, 255 });
            for (int k = 0; k < 4; k++) {
                float a = k * PI / 2.0f + t * 2.0f;
                DrawLineEx(head, { head.x + cosf(a) * s * 0.6f, head.y + sinf(a) * s * 0.6f }, 2.0f, Fade(WHITE, 0.7f));
            }
        } break;
        case AbilityId::ROT: { // Giftsky med bobler
            for (int i = 0; i < 3; i++) {
                float a = i * 2.1f;
                inkCircle(P(c, s, cosf(a) * 0.35f, 0.1f + sinf(a) * 0.3f), s * 0.42f, Color{ 70, 170, 60, 255 });
            }
            for (int i = 0; i < 4; i++) {
                float life = fmodf(t * 0.8f + i * 0.25f, 1.0f);
                DrawCircleV(P(c, s, -0.5f + i * 0.33f, 0.2f - life * 1.0f), s * 0.1f * (1.0f - life * 0.5f), Fade(GREEN_L, 1.0f - life));
            }
            // Liten hodeskalle i skya
            Vector2 sk = P(c, s, 0.0f, 0.12f);
            DrawCircleV(sk, s * 0.24f, Color{ 230, 240, 210, 255 });
            DrawRectangleRec({ sk.x - s * 0.14f, sk.y + s * 0.1f, s * 0.28f, s * 0.16f }, Color{ 230, 240, 210, 255 });
            DrawCircleV(P(sk, s, -0.09f, -0.02f), s * 0.07f, INK);
            DrawCircleV(P(sk, s, 0.09f, -0.02f), s * 0.07f, INK);
        } break;
        case AbilityId::DAGGER: {
            sword(P(c, s, 0.05f, 0.05f), s * 0.95f, -PI / 4.0f, STEEL);
        } break;
        case AbilityId::ORBIT_BLADES: { // Blader i sirkel rundt et senter
            DrawRing(c, s * 0.62f, s * 0.7f, 0.0f, 360.0f, 32, Fade(Color{ 255, 120, 200, 255 }, 0.5f));
            inkCircle(c, s * 0.2f, Color{ 255, 150, 210, 255 });
            for (int i = 0; i < 3; i++) {
                float a = t * 2.5f + i * 2.0f * PI / 3.0f;
                Vector2 p = { c.x + cosf(a) * s * 0.66f, c.y + sinf(a) * s * 0.66f };
                Vector2 d = { -sinf(a), cosf(a) };
                Vector2 n = { cosf(a), sinf(a) };
                Vector2 tip = { p.x + d.x * s * 0.4f, p.y + d.y * s * 0.4f };
                Vector2 b0 = { p.x - d.x * s * 0.25f + n.x * s * 0.14f, p.y - d.y * s * 0.25f + n.y * s * 0.14f };
                Vector2 b1 = { p.x - d.x * s * 0.25f - n.x * s * 0.14f, p.y - d.y * s * 0.25f - n.y * s * 0.14f };
                tri({ tip.x + d.x * 2, tip.y + d.y * 2 }, { b0.x + n.x * 2, b0.y + n.y * 2 }, { b1.x - n.x * 2, b1.y - n.y * 2 }, INK);
                tri(tip, b0, b1, STEEL);
            }
        } break;
        case AbilityId::LIGHTNING: { // Lynbolt
            Vector2 pts[6] = { P(c, s, 0.25f, -1.0f), P(c, s, -0.45f, 0.1f), P(c, s, 0.0f, 0.1f),
                               P(c, s, -0.3f, 1.0f), P(c, s, 0.5f, -0.2f), P(c, s, 0.05f, -0.2f) };
            // Kontur: litt større kopi
            Vector2 mid = c;
            Vector2 big[6];
            for (int i = 0; i < 6; i++) big[i] = Vector2Add(mid, Vector2Scale(Vector2Subtract(pts[i], mid), 1.18f));
            tri(big[0], big[1], big[2], INK); tri(big[0], big[2], big[5], INK);
            tri(big[5], big[4], big[3], INK); tri(big[5], big[3], big[2], INK);
            Color bolt = { 255, 235, 90, 255 };
            tri(pts[0], pts[1], pts[2], bolt); tri(pts[0], pts[2], pts[5], bolt);
            tri(pts[5], pts[4], pts[3], bolt); tri(pts[5], pts[3], pts[2], bolt);
            DrawLineEx(pts[0], pts[1], 2.0f, Fade(WHITE, 0.7f));
        } break;
        default: { // Restituer: hjerte med pluss
            heart(c, s * 0.9f, HEART);
            plus(P(c, s, 0.45f, 0.45f), s * 0.5f, GREEN_L);
        } break;
    }
}

void DrawUpgradeIcon(ShopUpgrade up, Vector2 c, float s) {
    float t = (float)GetTime();
    switch (up) {
        case ShopUpgrade::VITALITY:
            heart(c, s * 0.95f, HEART);
            break;
        case ShopUpgrade::ARMOR:
            shield(c, s, STEEL, STEEL_D);
            DrawRectangleRec({ c.x - s * 0.08f, c.y - s * 0.65f, s * 0.16f, s * 1.3f }, STEEL_D);
            DrawRectangleRec({ c.x - s * 0.55f, c.y - s * 0.2f, s * 1.1f, s * 0.16f }, STEEL_D);
            break;
        case ShopUpgrade::REGEN:
            heart(c, s * 0.8f, Color{ 90, 190, 90, 255 });
            plus(P(c, s, 0.0f, -0.05f), s * 0.6f, WHITE);
            break;
        case ShopUpgrade::EVASION: { // Sommerfugl
            float flap = 0.75f + 0.25f * sinf(t * 6.0f);
            for (int side = -1; side <= 1; side += 2) {
                inkCircle(P(c, s, side * 0.45f * flap, -0.3f), s * 0.42f, Color{ 90, 190, 255, 255 });
                inkCircle(P(c, s, side * 0.35f * flap, 0.35f), s * 0.3f, Color{ 190, 120, 255, 255 });
            }
            inkLine(P(c, s, 0.0f, -0.55f), P(c, s, 0.0f, 0.6f), s * 0.14f, INK);
            DrawLineEx(P(c, s, 0.0f, -0.55f), P(c, s, -0.25f, -0.95f), 1.5f, INK);
            DrawLineEx(P(c, s, 0.0f, -0.55f), P(c, s, 0.25f, -0.95f), 1.5f, INK);
        } break;
        case ShopUpgrade::SPEED: { // Vinget støvel
            for (int i = 0; i < 3; i++) inkLine(P(c, s, -1.0f, -0.35f + i * 0.3f), P(c, s, -0.55f, -0.35f + i * 0.3f), s * 0.08f, Fade(WHITE, 0.8f));
            quad(P(c, s, -0.4f, -0.75f), P(c, s, 0.15f, -0.75f), P(c, s, 0.15f, 0.55f), P(c, s, -0.4f, 0.55f), INK);
            quad(P(c, s, -0.4f, 0.2f), P(c, s, 0.85f, 0.35f), P(c, s, 0.85f, 0.75f), P(c, s, -0.4f, 0.75f), INK);
            quad(P(c, s, -0.32f, -0.67f), P(c, s, 0.07f, -0.67f), P(c, s, 0.07f, 0.5f), P(c, s, -0.32f, 0.5f), Color{ 150, 90, 50, 255 });
            quad(P(c, s, -0.32f, 0.27f), P(c, s, 0.77f, 0.42f), P(c, s, 0.77f, 0.67f), P(c, s, -0.32f, 0.67f), Color{ 150, 90, 50, 255 });
            for (int i = 0; i < 3; i++) {
                tri(P(c, s, 0.07f, -0.6f + i * 0.18f), P(c, s, 0.7f - i * 0.1f, -0.95f + i * 0.15f), P(c, s, 0.07f, -0.45f + i * 0.18f), WHITE);
            }
        } break;
        case ShopUpgrade::MIGHT:
            sword(c, s, -PI / 2.0f + 0.35f, STEEL);
            break;
        case ShopUpgrade::HASTE: { // Timeglass
            inkLine(P(c, s, -0.6f, -0.85f), P(c, s, 0.6f, -0.85f), s * 0.16f, WOOD);
            inkLine(P(c, s, -0.6f, 0.85f), P(c, s, 0.6f, 0.85f), s * 0.16f, WOOD);
            tri(P(c, s, -0.55f, -0.78f), P(c, s, 0.55f, -0.78f), P(c, s, 0.0f, 0.02f), Fade(Color{ 200, 230, 255, 255 }, 0.9f));
            tri(P(c, s, -0.55f, 0.78f), P(c, s, 0.55f, 0.78f), P(c, s, 0.0f, -0.02f), Fade(Color{ 200, 230, 255, 255 }, 0.9f));
            float sand = fmodf(t * 0.4f, 1.0f);
            tri(P(c, s, -0.4f * (1.0f - sand), -0.2f - 0.55f * (1.0f - sand)), P(c, s, 0.4f * (1.0f - sand), -0.2f - 0.55f * (1.0f - sand)), P(c, s, 0.0f, -0.05f), GOLD_L);
            tri(P(c, s, -0.45f * sand, 0.75f), P(c, s, 0.45f * sand, 0.75f), P(c, s, 0.0f, 0.75f - 0.45f * sand), GOLD_L);
            DrawLineEx(P(c, s, 0.0f, 0.0f), P(c, s, 0.0f, 0.7f), 2.0f, GOLD_L);
        } break;
        case ShopUpgrade::AREA: { // Ringer som sprer seg
            for (int i = 0; i < 3; i++) {
                float r = fmodf(t * 0.5f + i / 3.0f, 1.0f);
                DrawRing(c, s * r * 0.95f, s * r * 0.95f + 3.0f, 0.0f, 360.0f, 32, Fade(Color{ 255, 170, 70, 255 }, 1.0f - r));
            }
            inkCircle(c, s * 0.25f, Color{ 255, 170, 70, 255 });
        } break;
        case ShopUpgrade::PROJECTILE: { // Tre piler i vifte
            for (int i = -1; i <= 1; i++) {
                float a = -PI / 2.0f + i * 0.45f;
                Vector2 d = { cosf(a), sinf(a) };
                Vector2 base = P(c, s, 0.0f, 0.85f);
                Vector2 tip = { base.x + d.x * s * 1.35f, base.y + d.y * s * 1.35f };
                inkLine(base, tip, s * 0.1f, WOOD);
                Vector2 n = { -d.y, d.x };
                tri({ tip.x + d.x * s * 0.3f, tip.y + d.y * s * 0.3f }, { tip.x + n.x * s * 0.18f, tip.y + n.y * s * 0.18f }, { tip.x - n.x * s * 0.18f, tip.y - n.y * s * 0.18f }, STEEL);
            }
        } break;
        case ShopUpgrade::MAGNET: { // Hesteskomagnet
            DrawRing(P(c, s, 0.0f, 0.05f), s * 0.3f - 1.5f, s * 0.8f + 1.5f, 0.0f, 180.0f, 24, INK);
            DrawRing(P(c, s, 0.0f, 0.05f), s * 0.3f, s * 0.8f, 0.0f, 180.0f, 24, HEART);
            for (int side = -1; side <= 1; side += 2) {
                Rectangle leg = { c.x + (side < 0 ? -0.8f : 0.3f) * s, c.y - 0.75f * s, 0.5f * s, 0.8f * s };
                DrawRectangleRec({ leg.x - 1.5f, leg.y - 1.5f, leg.width + 3.0f, leg.height + 3.0f }, INK);
                DrawRectangleRec(leg, HEART);
                DrawRectangleRec({ leg.x, leg.y, leg.width, leg.height * 0.35f }, STEEL);
            }
        } break;
        case ShopUpgrade::GROWTH: { // XP-krystall med pil opp
            Vector2 top = P(c, s, 0.0f, -0.9f), bot = P(c, s, 0.0f, 0.9f), l = P(c, s, -0.6f, 0.0f), r = P(c, s, 0.6f, 0.0f);
            tri(P(c, s, 0.0f, -1.05f), P(c, s, -0.72f, 0.0f), P(c, s, 0.72f, 0.0f), INK);
            tri(P(c, s, 0.0f, 1.05f), P(c, s, -0.72f, 0.0f), P(c, s, 0.72f, 0.0f), INK);
            tri(top, l, c, Color{ 130, 200, 255, 255 });
            tri(top, c, r, BLUE_L);
            tri(bot, l, c, Color{ 50, 110, 220, 255 });
            tri(bot, c, r, Color{ 30, 80, 180, 255 });
            tri(P(c, s, 0.55f, -0.2f), P(c, s, 0.95f, -0.2f), P(c, s, 0.75f, -0.6f), GREEN_L);
            DrawRectangleRec({ c.x + 0.68f * s, c.y - 0.22f * s, 0.14f * s, 0.45f * s }, GREEN_L);
        } break;
        case ShopUpgrade::GREED: { // Stabel med mynter
            for (int i = 0; i < 4; i++) {
                Vector2 p = P(c, s, -0.15f + (i % 2) * 0.1f, 0.6f - i * 0.3f);
                DrawEllipse((int)p.x, (int)p.y, s * 0.62f + 1.5f, s * 0.24f + 1.5f, INK);
                DrawEllipse((int)p.x, (int)p.y, s * 0.62f, s * 0.24f, GOLD_D);
                DrawEllipse((int)p.x, (int)(p.y - s * 0.06f), s * 0.6f, s * 0.2f, GOLD_L);
            }
            Vector2 coin = P(c, s, 0.5f, 0.35f);
            inkCircle(coin, s * 0.35f, GOLD_L);
            DrawRectangleRec({ coin.x - s * 0.05f, coin.y - s * 0.18f, s * 0.1f, s * 0.36f }, GOLD_D);
        } break;
        case ShopUpgrade::EXTRA_CHOICE: { // Tre kort i vifte
            for (int i = -1; i <= 1; i++) {
                float a = i * 0.35f;
                Vector2 d = { sinf(a), -cosf(a) }, n = { cosf(a), sinf(a) };
                Vector2 base = P(c, s, i * 0.15f, 0.55f);
                auto at = [&](float x, float y) { return Vector2{ base.x + (n.x * x + d.x * y) * s, base.y + (n.y * x + d.y * y) * s }; };
                quad(at(-0.36f, -0.05f), at(0.36f, -0.05f), at(0.36f, 1.25f), at(-0.36f, 1.25f), INK);
                Color face = i == 0 ? GOLD_L : Color{ 240, 232, 214, 255 };
                quad(at(-0.3f, 0.0f), at(0.3f, 0.0f), at(0.3f, 1.2f), at(-0.3f, 1.2f), face);
                tri(at(0.0f, 0.4f), at(-0.14f, 0.6f), at(0.14f, 0.6f), i == 0 ? HEART : VIOLET_L);
                tri(at(0.0f, 0.8f), at(-0.14f, 0.6f), at(0.14f, 0.6f), i == 0 ? HEART : VIOLET_L);
            }
        } break;
        case ShopUpgrade::AEGIS: { // Skjold med glorie og vinger
            for (int side = -1; side <= 1; side += 2) {
                for (int f = 0; f < 3; f++) {
                    Vector2 root = P(c, s, side * 0.5f, -0.1f + f * 0.15f);
                    Vector2 tip = P(c, s, side * (1.05f - f * 0.12f), -0.55f + f * 0.3f);
                    inkLine(root, tip, s * 0.2f, WHITE);
                }
            }
            shield(P(c, s, 0.0f, 0.1f), s * 0.7f, GOLD_L, GOLD_D);
            DrawRing(P(c, s, 0.0f, -0.85f), s * 0.22f, s * 0.32f, 0.0f, 360.0f, 24, GOLD_L);
        } break;
        default: break;
    }
}
