#include "ui.hpp"
#include "settings.hpp"
#include <raymath.h>
#include <rlgl.h>
#include <cmath>
#include <algorithm>

namespace {

Color mix(Color a, Color b, float t) {
    t = Clamp(t, 0.0f, 1.0f);
    return {
        (unsigned char)(a.r + (b.r - a.r) * t),
        (unsigned char)(a.g + (b.g - a.g) * t),
        (unsigned char)(a.b + (b.b - a.b) * t),
        (unsigned char)(a.a + (b.a - a.a) * t)
    };
}

Color shade(Color c, float k) {
    auto ch = [k](unsigned char v) { return (unsigned char)Clamp(v * k, 0.0f, 255.0f); };
    return { ch(c.r), ch(c.g), ch(c.b), c.a };
}

// raylib vil ha trekanter mot klokka – denne sorterer hjørnene så rekkefølgen ikke spiller noen rolle
void triangle(Vector2 a, Vector2 b, Vector2 c, Color color) {
    float cross = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    if (cross < 0.0f) DrawTriangle(a, b, c, color);
    else DrawTriangle(a, c, b, color);
}

// Enkel hash for faste "tilfeldige" verdier (stjerner, murstein osv.)
float hash01(int n) {
    unsigned int h = (unsigned int)n * 374761393u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= h >> 16;
    return (h % 10000) / 10000.0f;
}

float textSpacing(float size) { return size / 10.0f; } // Samme som DrawText bruker

// ---------------------------------------------------------------------
// SLOTTET
// Inspirert av klassiske plattformspill-slott: murvegg med tinder, to runde
// tårn med spisse tak og et høyt midttårn med stor portåpning. Skumring i
// bakgrunnen, lys i vinduene og flagg som vaier.
// ---------------------------------------------------------------------

constexpr Color STONE_LIGHT = { 196, 164, 128, 255 };
constexpr Color STONE_MID   = { 160, 128,  98, 255 };
constexpr Color STONE_SHADE = { 112,  86,  72, 255 };
constexpr Color MORTAR      = {  92,  70,  60, 255 };
constexpr Color ROOF_RED    = { 176,  36,  44, 255 };
constexpr Color ROOF_DARK   = { 110,  18,  30, 255 };
constexpr Color WINDOW_GLOW = { 255, 196,  90, 255 };
constexpr Color DOOR_DARK   = {  30,  18,  22, 255 };

// Murvegg med forskjøvne steinrader og litt fargevariasjon per stein
void brickWall(Rectangle r, float u, Color base, int seed) {
    DrawRectangleRec(r, MORTAR);
    float bh = 14.0f * u;
    float bw = 34.0f * u;
    int row = 0;
    for (float y = r.y; y < r.y + r.height; y += bh, row++) {
        float offset = (row % 2) ? bw * 0.5f : 0.0f;
        int col = 0;
        for (float x = r.x - offset; x < r.x + r.width; x += bw, col++) {
            float x0 = std::max(x + 1.0f * u, r.x);
            float x1 = std::min(x + bw - 1.0f * u, r.x + r.width);
            float y1 = std::min(y + bh - 1.0f * u, r.y + r.height);
            if (x1 <= x0 || y1 <= y) continue;
            float v = 0.9f + 0.2f * hash01(seed + row * 131 + col * 17);
            DrawRectangleRec({ x0, y + 1.0f * u, x1 - x0, y1 - y - 1.0f * u }, shade(base, v));
            // Lys kant øverst på hver stein
            DrawRectangleRec({ x0, y + 1.0f * u, x1 - x0, 1.5f * u }, Fade(WHITE, 0.10f));
        }
    }
}

// Tinder (merloner) langs toppen av en vegg
void battlements(float x, float y, float width, float u, Color base) {
    float merlon = 22.0f * u;
    int count = std::max(2, (int)(width / (merlon * 1.6f)));
    float step = (width - merlon) / (count - 1);
    for (int i = 0; i < count; i++) {
        Rectangle m = { x + i * step, y - merlon * 0.9f, merlon, merlon * 0.9f };
        brickWall(m, u, base, 900 + i * 7 + (int)x);
        DrawRectangleRec({ m.x, m.y, m.width, 2.0f * u }, Fade(WHITE, 0.15f));
    }
    // Gesims under tindene
    DrawRectangleRec({ x - 4.0f * u, y - 2.0f * u, width + 8.0f * u, 7.0f * u }, shade(base, 0.8f));
}

// Buet vindu/åpning: rektangel med halvsirkel på toppen
void archShape(float cx, float bottom, float width, float height, Color color) {
    float r = width / 2.0f;
    DrawRectangleRec({ cx - r, bottom - height + r, width, height - r }, color);
    DrawCircleSector({ cx, bottom - height + r }, r, 180.0f, 360.0f, 16, color);
}

void litWindow(float cx, float bottom, float width, float height, float u, float time, int seed) {
    archShape(cx, bottom, width + 4.0f * u, height + 3.0f * u, shade(STONE_SHADE, 0.7f));
    float flicker = 0.85f + 0.15f * sinf(time * (3.0f + hash01(seed) * 3.0f) + seed);
    archShape(cx, bottom, width, height, mix(Color{ 120, 60, 30, 255 }, WINDOW_GLOW, flicker));
    // Vinduskarm og sprosse
    DrawRectangleRec({ cx - 1.0f * u, bottom - height + 2.0f * u, 2.0f * u, height - 2.0f * u }, Fade(DOOR_DARK, 0.6f));
    DrawRectangleRec({ cx - width / 2.0f - 3.0f * u, bottom, width + 6.0f * u, 3.0f * u }, STONE_LIGHT);
    // Varm glød rundt vinduet
    UI::DrawGlow({ cx, (bottom - height * 0.5f) }, width * 1.6f, Fade(WINDOW_GLOW, 0.18f * flicker), Fade(WINDOW_GLOW, 0.0f));
}

// Flagg som vaier i vinden (små segmenter som følger en sinusbølge)
void flag(float poleX, float poleTop, float u, float time, Color color, Color stripe) {
    DrawRectangleRec({ poleX - 1.5f * u, poleTop, 3.0f * u, 46.0f * u }, Color{ 70, 60, 55, 255 });
    DrawCircleV({ poleX, poleTop }, 3.5f * u, UI::GOLD_LIGHT);
    const int segments = 10;
    float len = 44.0f * u, h = 22.0f * u;
    for (int i = 0; i < segments; i++) {
        float t0 = (float)i / segments, t1 = (float)(i + 1) / segments;
        float w0 = sinf(time * 4.0f - t0 * 5.0f) * 4.0f * u * t0;
        float w1 = sinf(time * 4.0f - t1 * 5.0f) * 4.0f * u * t1;
        float taper0 = 1.0f - t0 * 0.45f, taper1 = 1.0f - t1 * 0.45f;
        Vector2 a = { poleX + t0 * len, poleTop + 4.0f * u + w0 };
        Vector2 b = { poleX + t1 * len, poleTop + 4.0f * u + w1 };
        Vector2 c = { poleX + t1 * len, poleTop + 4.0f * u + w1 + h * taper1 };
        Vector2 d = { poleX + t0 * len, poleTop + 4.0f * u + w0 + h * taper0 };
        Color col = (i % 3 == 1) ? stripe : color;
        triangle(a, b, c, col);
        triangle(a, c, d, col);
    }
}

// Rundt tårn: murvegg med skygge på høyre side, tinder og spisst tak
void tower(float cx, float top, float bottom, float width, float roofHeight, float u, float time, int seed, bool flagOnTop) {
    Rectangle body = { cx - width / 2.0f, top, width, bottom - top };
    brickWall(body, u, STONE_MID, seed);
    // Rundhet: lys stripe til venstre, skygge til høyre
    DrawRectangleGradientH((int)body.x, (int)body.y, (int)(width * 0.35f), (int)body.height, Fade(WHITE, 0.12f), Fade(WHITE, 0.0f));
    DrawRectangleGradientH((int)(body.x + width * 0.55f), (int)body.y, (int)(width * 0.45f) + 1, (int)body.height, Fade(BLACK, 0.0f), Fade(BLACK, 0.35f));

    // Overheng og tak
    float eave = 10.0f * u;
    DrawRectangleRec({ body.x - eave, top - 8.0f * u, width + eave * 2.0f, 12.0f * u }, STONE_SHADE);
    Vector2 left = { body.x - eave - 4.0f * u, top - 6.0f * u };
    Vector2 right = { body.x + width + eave + 4.0f * u, top - 6.0f * u };
    Vector2 peak = { cx, top - roofHeight };
    triangle(left, right, peak, ROOF_RED);
    triangle({ cx, top - 6.0f * u }, right, peak, ROOF_DARK); // Skyggesiden av taket
    // Takstein-linjer
    for (int i = 1; i < 5; i++) {
        float t = i / 5.0f;
        Vector2 l = Vector2Lerp(peak, left, t), r = Vector2Lerp(peak, right, t);
        DrawLineEx(l, r, 1.5f * u, Fade(BLACK, 0.25f));
    }
    if (flagOnTop) flag(cx, peak.y - 44.0f * u, u, time + seed, ROOF_RED, UI::GOLD_LIGHT);
    else DrawCircleV(peak, 5.0f * u, UI::GOLD_LIGHT);

    // Smale vinduer nedover tårnet
    litWindow(cx, top + 70.0f * u, 14.0f * u, 30.0f * u, u, time, seed + 1);
    litWindow(cx, top + 150.0f * u, 14.0f * u, 30.0f * u, u, time, seed + 2);
}

// Tegnes ugjennomsiktig (to lag: skyggeside og lys topp) så sirklene ikke synes gjennom hverandre
void cloud(float x, float y, float u, Color color) {
    Color light = mix(color, Color{ 255, 200, 190, 255 }, 0.25f);
    for (int layer = 0; layer < 2; layer++) {
        Color c = layer == 0 ? color : light;
        float lift = layer == 0 ? 0.0f : -5.0f * u;
        float shrink = layer == 0 ? 1.0f : 0.8f;
        DrawCircleV({ x, y + lift }, 26.0f * u * shrink, c);
        DrawCircleV({ x + 30.0f * u, y - 12.0f * u + lift }, 32.0f * u * shrink, c);
        DrawCircleV({ x + 64.0f * u, y + lift }, 26.0f * u * shrink, c);
        if (layer == 0) DrawRectangleRec({ x, y, 64.0f * u, 26.0f * u }, c);
    }
}

// Narrelue med tre tupper og bjeller, tegnes med tuppene oppover fra `base`
void jesterHat(Vector2 base, float size, float tilt, float time) {
    auto rot = [&](Vector2 p) { return Vector2Add(base, Vector2Rotate(p, tilt)); };
    const Color colors[3] = { UI::ROYAL_RED, Color{ 110, 40, 150, 255 }, Color{ 30, 130, 70, 255 } };
    const Vector2 tips[3] = {
        { -size * 1.05f, -size * 0.55f + sinf(time * 3.0f) * size * 0.05f },
        {  0.0f,         -size * 1.15f + sinf(time * 3.0f + 1.0f) * size * 0.05f },
        {  size * 1.05f, -size * 0.55f + sinf(time * 3.0f + 2.0f) * size * 0.05f }
    };
    const float baseX[3] = { -0.55f, 0.0f, 0.55f };
    for (int i = 0; i < 3; i++) {
        Vector2 a = rot({ size * (baseX[i] - 0.42f), 0.0f });
        Vector2 b = rot({ size * (baseX[i] + 0.42f), 0.0f });
        Vector2 mid = rot({ tips[i].x * 0.5f + size * baseX[i] * 0.5f, tips[i].y * 0.62f });
        Vector2 tip = rot(tips[i]);
        // Kontur først, så fyll
        float o = size * 0.06f;
        triangle({ a.x - o, a.y + o }, { b.x + o, b.y + o }, mid, UI::INK);
        triangle(mid, tip, { mid.x + o, mid.y }, UI::INK);
        triangle(a, b, mid, colors[i]);
        triangle(a, mid, tip, colors[i]);
        triangle(b, mid, tip, shade(colors[i], 0.75f));
        DrawCircleV(tip, size * 0.17f, UI::INK);
        DrawCircleV(tip, size * 0.13f, UI::GOLD_LIGHT);
        DrawCircleV({ tip.x - size * 0.04f, tip.y - size * 0.04f }, size * 0.04f, WHITE);
    }
    // Lua-kanten med rutemønster
    Vector2 l = rot({ -size * 0.95f, 0.0f }), r = rot({ size * 0.95f, 0.0f });
    DrawLineEx(l, r, size * 0.28f, UI::INK);
    DrawLineEx(l, r, size * 0.18f, UI::GOLD_LIGHT);
    for (int i = 0; i < 5; i++) {
        Vector2 p = Vector2Lerp(l, r, (i + 0.5f) / 5.0f);
        DrawCircleV(p, size * 0.05f, (i % 2) ? UI::ROYAL_RED : Color{ 110, 40, 150, 255 });
    }
}

} // namespace

namespace UI {

float Scale() {
    float sx = GetScreenWidth() / (float)Settings::SCREEN_WIDTH;
    float sy = GetScreenHeight() / (float)Settings::SCREEN_HEIGHT;
    return std::max(0.1f, std::min(sx, sy));
}

void BeginCanvas() {
    float s = Scale();
    Camera2D cam{};
    cam.zoom = s;
    cam.offset = {
        std::floor((GetScreenWidth() - Settings::SCREEN_WIDTH * s) / 2.0f),
        std::floor((GetScreenHeight() - Settings::SCREEN_HEIGHT * s) / 2.0f)
    };
    BeginMode2D(cam);
}

void EndCanvas() {
    EndMode2D();
}

void DrawGlow(Vector2 center, float radius, Color inner, Color outer) {
    const int segments = 36;
    rlBegin(RL_TRIANGLES);
    for (int i = 0; i < segments; i++) {
        float a0 = 2.0f * PI * i / segments;
        float a1 = 2.0f * PI * (i + 1) / segments;
        // Samme rekkefølge som raylibs egne sirkler (mot klokka på skjermen)
        rlColor4ub(inner.r, inner.g, inner.b, inner.a);
        rlVertex2f(center.x, center.y);
        rlColor4ub(outer.r, outer.g, outer.b, outer.a);
        rlVertex2f(center.x + cosf(a1) * radius, center.y + sinf(a1) * radius);
        rlVertex2f(center.x + cosf(a0) * radius, center.y + sinf(a0) * radius);
    }
    rlEnd();
}

void DrawPanel(Rectangle r, float scale, Color edge, Color fill) {
    float e = 2.0f * scale;
    DrawRectangleRec({ r.x + 4.0f * scale, r.y + 5.0f * scale, r.width, r.height }, Fade(BLACK, 0.35f)); // Skygge
    DrawRectangleRec(r, fill);
    DrawRectangleGradientV((int)r.x, (int)r.y, (int)r.width, (int)(r.height * 0.4f), Fade(WHITE, 0.05f), Fade(WHITE, 0.0f));
    DrawRectangleLinesEx(r, e, edge);
    DrawRectangleLinesEx({ r.x + e * 2.0f, r.y + e * 2.0f, r.width - e * 4.0f, r.height - e * 4.0f }, 1.0f * scale, Fade(edge, 0.35f));
    // Nagler i hjørnene
    float n = 3.0f * scale;
    float in = e + n + 1.0f * scale;
    Vector2 corners[4] = { { r.x + in, r.y + in }, { r.x + r.width - in, r.y + in },
                           { r.x + in, r.y + r.height - in }, { r.x + r.width - in, r.y + r.height - in } };
    for (Vector2 c : corners) {
        DrawCircleV(c, n, GOLD_DARK);
        DrawCircleV({ c.x - n * 0.3f, c.y - n * 0.3f }, n * 0.45f, GOLD_LIGHT);
    }
}

void DrawOutlinedText(const char* text, float x, float y, float size, Color color, float outline) {
    Font font = GetFontDefault();
    float sp = textSpacing(size);
    Color ink = Fade(INK, color.a / 255.0f);
    DrawTextEx(font, text, { x + outline, y + outline * 1.5f }, size, sp, Fade(BLACK, 0.5f * color.a / 255.0f));
    for (int dx = -1; dx <= 1; dx++) {
        for (int dy = -1; dy <= 1; dy++) {
            if (dx == 0 && dy == 0) continue;
            DrawTextEx(font, text, { x + dx * outline, y + dy * outline }, size, sp, ink);
        }
    }
    DrawTextEx(font, text, { x, y }, size, sp, color);
}

void DrawCenteredText(const char* text, float centerX, float y, float size, Color color, float outline) {
    float w = MeasureTextEx(GetFontDefault(), text, size, textSpacing(size)).x;
    DrawOutlinedText(text, centerX - w / 2.0f, y, size, color, outline);
}

void DrawBar(Rectangle r, float pct, Color fill, Color back, float scale) {
    pct = Clamp(pct, 0.0f, 1.0f);
    DrawRectangleRec({ r.x - 2.0f * scale, r.y - 2.0f * scale, r.width + 4.0f * scale, r.height + 4.0f * scale }, INK);
    DrawRectangleRec(r, back);
    if (pct > 0.0f) {
        Rectangle f = { r.x, r.y, r.width * pct, r.height };
        DrawRectangleRec(f, fill);
        DrawRectangleGradientV((int)f.x, (int)f.y, (int)std::ceil(f.width), (int)(f.height * 0.5f), Fade(WHITE, 0.35f), Fade(WHITE, 0.05f));
        DrawRectangleRec({ f.x, f.y + f.height - 2.0f * scale, f.width, 2.0f * scale }, Fade(BLACK, 0.25f));
    }
    DrawRectangleLinesEx({ r.x - 1.0f * scale, r.y - 1.0f * scale, r.width + 2.0f * scale, r.height + 2.0f * scale }, 1.0f * scale, Fade(GOLD_LIGHT, 0.35f));
}

void DrawTitleLogo(float centerX, float top, float width, float time) {
    Font font = GetFontDefault();

    // --- JESTER: så stor som bredden tillater ---
    const char* word = "JESTER";
    const int letters = 6;
    float probe = 100.0f;
    float probeWidth = MeasureTextEx(font, word, probe, textSpacing(probe)).x;
    float size = probe * width / probeWidth;
    float sp = textSpacing(size);

    // --- THE LAST: over, med sverd-streker på hver side ---
    float smallSize = size * 0.26f;
    const char* the = "THE  LAST";
    float theW = MeasureTextEx(font, the, smallSize, smallSize * 0.35f).x;
    float theY = top;
    float lineY = theY + smallSize * 0.45f;
    float gap = smallSize * 0.8f;
    for (int side = -1; side <= 1; side += 2) {
        float x0 = centerX + side * (theW / 2.0f + gap);
        float x1 = centerX + side * (width / 2.0f);
        DrawLineEx({ x0, lineY + 3 }, { x1, lineY + 3 }, smallSize * 0.12f, Fade(BLACK, 0.5f));
        DrawLineEx({ x0, lineY }, { x1, lineY }, smallSize * 0.12f, GOLD_DARK);
        DrawLineEx({ x0, lineY - smallSize * 0.03f }, { x1, lineY - smallSize * 0.03f }, smallSize * 0.04f, GOLD_LIGHT);
        // Diamant-ende
        Vector2 d = { x1, lineY };
        float ds = smallSize * 0.22f;
        triangle({ d.x - ds, d.y }, { d.x, d.y - ds }, { d.x + ds, d.y }, GOLD_LIGHT);
        triangle({ d.x - ds, d.y }, { d.x + ds, d.y }, { d.x, d.y + ds }, GOLD_DARK);
    }
    {
        float x = centerX - theW / 2.0f;
        float o = smallSize * 0.08f;
        for (int dx = -1; dx <= 1; dx++)
            for (int dy = -1; dy <= 1; dy++)
                DrawTextEx(font, the, { x + dx * o, theY + dy * o }, smallSize, smallSize * 0.35f, INK);
        DrawTextEx(font, the, { x, theY }, smallSize, smallSize * 0.35f, Color{ 240, 232, 214, 255 });
    }

    // --- JESTER: bokstav for bokstav, med bølge og vekslende narrefarger ---
    float jesterY = theY + smallSize * 1.25f;
    float totalW = MeasureTextEx(font, word, size, sp).x;
    float x = centerX - totalW / 2.0f;
    const Color fills[2] = { Color{ 214, 40, 52, 255 }, GOLD_LIGHT };
    const Color darks[2] = { Color{ 120, 14, 28, 255 }, Color{ 200, 128, 20, 255 } };
    float outline = size * 0.055f;
    Vector2 hatBase{};
    for (int i = 0; i < letters; i++) {
        char glyph[2] = { word[i], '\0' };
        float gw = MeasureTextEx(font, glyph, size, sp).x;
        float bob = sinf(time * 2.2f - i * 0.7f) * size * 0.035f;
        Vector2 p = { x, jesterY + bob };
        if (i == 0) hatBase = { p.x + gw * 0.55f, p.y + size * 0.06f };

        // Tykk skygge og kontur
        for (int k = 1; k <= 3; k++) {
            DrawTextEx(font, glyph, { p.x + outline * 0.6f * k, p.y + outline * 0.9f * k }, size, sp, Color{ 40, 10, 20, 255 });
        }
        for (int dx = -1; dx <= 1; dx++)
            for (int dy = -1; dy <= 1; dy++)
                DrawTextEx(font, glyph, { p.x + dx * outline, p.y + dy * outline }, size, sp, INK);

        // Fyll: mørk nederst, lys øverst (tegnes som to lag der det lyse er forskjøvet opp)
        DrawTextEx(font, glyph, p, size, sp, darks[i % 2]);
        DrawTextEx(font, glyph, { p.x, p.y - outline * 0.45f }, size, sp, fills[i % 2]);
        x += gw + sp;
    }

    // Narrelue på J-en
    jesterHat(hatBase, size * 0.40f, -0.22f + sinf(time * 1.5f) * 0.05f, time);

    // Små gnister som blinker rundt logoen
    for (int i = 0; i < 7; i++) {
        float phase = fmodf(time * 0.7f + hash01(i * 11) * 5.0f, 5.0f);
        if (phase > 1.0f) continue;
        float a = sinf(phase * PI);
        Vector2 c = { centerX + (hash01(i * 7 + 1) - 0.5f) * width * 1.05f, jesterY + hash01(i * 3 + 2) * size * 0.9f };
        float r = size * 0.07f * a;
        DrawLineEx({ c.x - r, c.y }, { c.x + r, c.y }, 2.0f, Fade(WHITE, a));
        DrawLineEx({ c.x, c.y - r }, { c.x, c.y + r }, 2.0f, Fade(WHITE, a));
    }
}

void DrawCastleBackdrop(float time, float dim) {
    float w = (float)GetScreenWidth();
    float h = (float)GetScreenHeight();
    float u = Scale();
    float cx = w / 2.0f;
    float ground = h * 0.83f;

    // --- HIMMEL: skumring fra dyp lilla til varm oransje ved horisonten ---
    DrawRectangleGradientV(0, 0, (int)w, (int)(ground * 0.55f), Color{ 18, 16, 48, 255 }, Color{ 70, 40, 100, 255 });
    DrawRectangleGradientV(0, (int)(ground * 0.55f), (int)w, (int)(ground * 0.45f) + 1, Color{ 70, 40, 100, 255 }, Color{ 238, 120, 90, 255 });

    // Stjerner som blinker
    for (int i = 0; i < 90; i++) {
        float sx = hash01(i * 3 + 1) * w;
        float sy = hash01(i * 5 + 2) * ground * 0.5f;
        float tw = 0.5f + 0.5f * sinf(time * (1.0f + hash01(i) * 2.0f) + i);
        float r = (0.6f + hash01(i * 7) * 1.2f) * u;
        DrawCircleV({ sx, sy }, r, Fade(WHITE, 0.25f + 0.6f * tw * (1.0f - sy / (ground * 0.5f))));
    }

    // Stor, blek måne bak slottet
    Vector2 moon = { cx + 360.0f * u, ground - 470.0f * u };
    UI::DrawGlow({ moon.x, moon.y }, 150.0f * u, Fade(Color{ 255, 230, 200, 255 }, 0.25f), Fade(Color{ 255, 230, 200, 255 }, 0.0f));
    DrawCircleV(moon, 62.0f * u, Color{ 255, 240, 214, 255 });
    DrawCircleV({ moon.x - 18.0f * u, moon.y - 10.0f * u }, 12.0f * u, Color{ 236, 216, 190, 255 });
    DrawCircleV({ moon.x + 20.0f * u, moon.y + 18.0f * u }, 8.0f * u, Color{ 236, 216, 190, 255 });

    // Skyer som driver sakte forbi
    for (int i = 0; i < 6; i++) {
        float speed = (8.0f + hash01(i * 13) * 10.0f) * u;
        float span = w + 300.0f * u;
        float x = fmodf(hash01(i * 17) * span + time * speed, span) - 150.0f * u;
        float y = (80.0f + hash01(i * 19) * 260.0f) * u;
        float sc = u * (0.7f + hash01(i * 23) * 0.6f);
        cloud(x, y, sc, mix(Color{ 60, 40, 96, 255 }, Color{ 150, 80, 110, 255 }, y / (ground * 0.8f)));
    }

    // --- ÅSER i to lag ---
    for (int i = -1; i < (int)(w / (260.0f * u)) + 2; i++) {
        float hx = i * 260.0f * u + hash01(i + 50) * 80.0f * u;
        DrawCircleV({ hx, ground + 60.0f * u }, (150.0f + hash01(i + 60) * 70.0f) * u, Color{ 64, 44, 92, 255 });
    }
    for (int i = -1; i < (int)(w / (340.0f * u)) + 2; i++) {
        float hx = i * 340.0f * u + 120.0f * u;
        DrawCircleV({ hx, ground + 120.0f * u }, (180.0f + hash01(i + 80) * 60.0f) * u, Color{ 44, 56, 70, 255 });
    }

    // --- SLOTTET ---
    float wallTop = ground - 210.0f * u;
    float wallHalf = 290.0f * u;

    // Bakre tårn (lengst ut, litt mørkere)
    for (int side = -1; side <= 1; side += 2) {
        tower(cx + side * 350.0f * u, ground - 300.0f * u, ground, 100.0f * u, 120.0f * u, u, time, 40 + side, true);
    }

    // Ringmur
    Rectangle wall = { cx - wallHalf, wallTop, wallHalf * 2.0f, ground - wallTop };
    brickWall(wall, u, STONE_LIGHT, 7);
    battlements(wall.x, wallTop, wall.width, u, STONE_LIGHT);
    DrawRectangleGradientV((int)wall.x, (int)(ground - 60.0f * u), (int)wall.width, (int)(60.0f * u), Fade(BLACK, 0.0f), Fade(BLACK, 0.35f));
    for (int side = -1; side <= 1; side += 2) {
        litWindow(cx + side * 205.0f * u, wallTop + 80.0f * u, 22.0f * u, 38.0f * u, u, time, 10 + side);
        litWindow(cx + side * 140.0f * u, wallTop + 80.0f * u, 22.0f * u, 38.0f * u, u, time, 20 + side);
    }

    // Midttårnet (keep)
    float keepHalf = 95.0f * u;
    float keepTop = ground - 335.0f * u;
    Rectangle keep = { cx - keepHalf, keepTop, keepHalf * 2.0f, ground - keepTop };
    brickWall(keep, u, STONE_MID, 3);
    DrawRectangleGradientH((int)(cx + keepHalf * 0.3f), (int)keepTop, (int)(keepHalf * 0.7f) + 1, (int)keep.height, Fade(BLACK, 0.0f), Fade(BLACK, 0.3f));
    battlements(keep.x, keepTop, keep.width, u, STONE_MID);

    // Flagg på toppen av keepen
    flag(cx - 20.0f * u, keepTop - 64.0f * u, u * 1.3f, time + 3.0f, UI::ROYAL_RED, UI::GOLD_LIGHT);

    // Rosevindu over porten
    Vector2 rose = { cx, keepTop + 90.0f * u };
    float roseFlicker = 0.9f + 0.1f * sinf(time * 2.3f);
    UI::DrawGlow({ rose.x, rose.y }, 70.0f * u, Fade(WINDOW_GLOW, 0.25f), Fade(WINDOW_GLOW, 0.0f));
    DrawCircleV(rose, 31.0f * u, STONE_SHADE);
    DrawCircleV(rose, 26.0f * u, mix(Color{ 150, 60, 40, 255 }, WINDOW_GLOW, roseFlicker));
    for (int i = 0; i < 8; i++) {
        float a = i * PI / 4.0f + time * 0.1f;
        DrawLineEx(rose, { rose.x + cosf(a) * 26.0f * u, rose.y + sinf(a) * 26.0f * u }, 2.5f * u, Fade(DOOR_DARK, 0.7f));
    }
    DrawCircleV(rose, 7.0f * u, UI::ROYAL_RED);

    // Stor portåpning med portcullis
    float doorW = 96.0f * u, doorH = 140.0f * u;
    archShape(cx, ground, doorW + 18.0f * u, doorH + 12.0f * u, STONE_SHADE);
    archShape(cx, ground, doorW, doorH, DOOR_DARK);
    UI::DrawGlow({ cx, (ground - 30.0f * u) }, 60.0f * u, Fade(Color{ 255, 150, 60, 255 }, 0.25f), Fade(Color{ 255, 150, 60, 255 }, 0.0f));
    for (int i = -2; i <= 2; i++) {
        float gx = cx + i * doorW / 5.5f;
        DrawRectangleRec({ gx - 2.0f * u, ground - doorH * 0.93f, 4.0f * u, doorH * 0.6f }, Color{ 60, 50, 50, 255 });
        triangle({ gx - 3.0f * u, ground - doorH * 0.33f }, { gx + 3.0f * u, ground - doorH * 0.33f }, { gx, ground - doorH * 0.26f }, Color{ 60, 50, 50, 255 });
    }
    for (int i = 0; i < 3; i++) {
        float gy = ground - doorH * (0.85f - i * 0.2f);
        DrawRectangleRec({ cx - doorW / 2.0f + 4.0f * u, gy, doorW - 8.0f * u, 3.5f * u }, Color{ 60, 50, 50, 255 });
    }

    // Bannere på hver side av porten
    for (int side = -1; side <= 1; side += 2) {
        float bx = cx + side * 150.0f * u;
        float by = wallTop + 115.0f * u;
        float bw = 36.0f * u, bh = 70.0f * u;
        float sway = sinf(time * 1.7f + side) * 2.0f * u;
        DrawRectangleRec({ bx - bw / 2.0f - 4.0f * u, by - 4.0f * u, bw + 8.0f * u, 5.0f * u }, UI::GOLD_DARK);
        DrawRectangleRec({ bx - bw / 2.0f, by, bw, bh }, UI::ROYAL_RED);
        triangle({ bx - bw / 2.0f, by + bh }, { bx + bw / 2.0f, by + bh }, { bx + sway, by + bh + 18.0f * u }, UI::ROYAL_RED);
        DrawRectangleRec({ bx - bw / 2.0f + 4.0f * u, by, 3.0f * u, bh }, UI::GOLD_LIGHT);
        DrawRectangleRec({ bx + bw / 2.0f - 7.0f * u, by, 3.0f * u, bh }, UI::GOLD_LIGHT);
        // Rute-emblem (narremønster)
        Vector2 e = { bx, by + bh * 0.45f };
        float es = 10.0f * u;
        triangle({ e.x - es, e.y }, { e.x, e.y - es * 1.3f }, { e.x + es, e.y }, UI::GOLD_LIGHT);
        triangle({ e.x - es, e.y }, { e.x + es, e.y }, { e.x, e.y + es * 1.3f }, UI::GOLD_DARK);
    }

    // --- BAKKE: gress, steinvei inn mot porten og busker ---
    DrawRectangleRec({ 0, ground, w, h - ground }, Color{ 40, 70, 48, 255 });
    DrawRectangleGradientV(0, (int)ground, (int)w, (int)(h - ground) + 1, Color{ 58, 104, 62, 255 }, Color{ 18, 34, 26, 255 });
    triangle({ cx - doorW / 2.0f, ground }, { cx + doorW / 2.0f, ground }, { cx + doorW * 1.6f, h }, Color{ 150, 128, 110, 255 });
    triangle({ cx - doorW / 2.0f, ground }, { cx + doorW * 1.6f, h }, { cx - doorW * 1.6f, h }, Color{ 150, 128, 110, 255 });
    for (int i = 1; i < 6; i++) {
        float t = i / 6.0f;
        float y = ground + (h - ground) * t * t;
        float half = doorW / 2.0f + (doorW * 1.1f) * t * t;
        DrawLineEx({ cx - half, y }, { cx + half, y }, 1.5f * u, Fade(BLACK, 0.25f));
    }
    DrawRectangleGradientV(0, (int)ground, (int)w, (int)(h - ground) + 1, Fade(BLACK, 0.0f), Fade(BLACK, 0.45f));
    for (int i = 0; i < 16; i++) {
        float bx = hash01(i * 29) * w;
        if (fabsf(bx - cx) < doorW * 1.8f) continue;
        float r = (18.0f + hash01(i * 31) * 22.0f) * u;
        DrawCircleV({ bx, ground + 4.0f * u }, r, Color{ 30, 60, 40, 255 });
        DrawCircleV({ bx - r * 0.3f, ground - r * 0.2f }, r * 0.6f, Color{ 44, 86, 54, 255 });
    }

    // Ildfluer / gnister som stiger opp
    for (int i = 0; i < 26; i++) {
        float life = fmodf(time * (0.08f + hash01(i * 41) * 0.08f) + hash01(i * 43), 1.0f);
        float fx = hash01(i * 47) * w + sinf(time + i) * 14.0f * u;
        float fy = h - life * h * 0.7f;
        float a = sinf(life * PI);
        DrawCircleV({ fx, fy }, 2.0f * u, Fade(Color{ 255, 220, 140, 255 }, 0.6f * a));
    }

    // Vignett og evt. mørklegging for undermenyer
    DrawRectangleGradientV(0, 0, (int)w, (int)(h * 0.25f), Fade(BLACK, 0.35f), Fade(BLACK, 0.0f));
    DrawRectangleGradientH(0, 0, (int)(w * 0.2f), (int)h, Fade(BLACK, 0.35f), Fade(BLACK, 0.0f));
    DrawRectangleGradientH((int)(w * 0.8f), 0, (int)(w * 0.2f) + 1, (int)h, Fade(BLACK, 0.0f), Fade(BLACK, 0.35f));
    if (dim > 0.0f) DrawRectangle(0, 0, (int)w, (int)h, Fade(Color{ 8, 6, 14, 255 }, dim));
}

void HandleWindowShortcuts() {
    if (IsKeyPressed(KEY_F11)) ToggleBorderlessWindowed();
}

} // namespace UI
