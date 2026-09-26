#include "hud.hpp"
#include "ui.hpp"
#include "abilities.hpp"
#include "items.hpp"
#include <raymath.h>
#include <rlgl.h>
#include <cmath>
#include <algorithm>

namespace {

constexpr Color HP_RED     = { 214,  44,  56, 255 };
constexpr Color HP_BACK    = {  60,  14,  22, 255 };
constexpr Color XP_BLUE    = {  70, 150, 255, 255 };
constexpr Color XP_BACK    = {  16,  22,  48, 255 };
constexpr Color CURSE_VIOLET = { 176, 110, 240, 255 };
constexpr Color MAP_FLOOR  = {  34,  30,  40, 235 };

std::string formatTime(float seconds) {
    int t = std::max(0, (int)seconds);
    return TextFormat("%02d:%02d", t / 60, t % 60);
}

// --- Små ikoner (tegnet med former, så de skalerer skarpt) ---

void shieldIcon(Vector2 c, float r, Color color) {
    DrawRectangleRec({ c.x - r, c.y - r, r * 2.0f, r }, UI::INK);
    DrawTriangle({ c.x - r, c.y - 1 }, { c.x, c.y + r * 1.25f }, { c.x + r, c.y - 1 }, UI::INK);
    float i = r * 0.72f;
    DrawRectangleRec({ c.x - i, c.y - i, i * 2.0f, i }, color);
    DrawTriangle({ c.x - i, c.y - 1 }, { c.x, c.y + i * 1.25f }, { c.x + i, c.y - 1 }, color);
    DrawRectangleRec({ c.x - i * 0.15f, c.y - i, i * 0.3f, i * 2.0f }, Fade(WHITE, 0.5f));
}

void coinIcon(Vector2 c, float r) {
    DrawCircleV(c, r + 1.5f, UI::INK);
    DrawCircleV(c, r, UI::GOLD_DARK);
    DrawCircleV({ c.x - r * 0.1f, c.y - r * 0.1f }, r * 0.78f, GOLD);
    DrawRectangleRec({ c.x - r * 0.15f, c.y - r * 0.45f, r * 0.3f, r * 0.9f }, UI::GOLD_DARK);
}

void skullIcon(Vector2 c, float r) {
    DrawCircleV({ c.x, c.y - r * 0.15f }, r + 1.5f, UI::INK);
    DrawRectangleRec({ c.x - r * 0.6f - 1.5f, c.y + r * 0.2f, r * 1.2f + 3.0f, r * 0.8f + 1.5f }, UI::INK);
    DrawCircleV({ c.x, c.y - r * 0.15f }, r, Color{ 230, 226, 214, 255 });
    DrawRectangleRec({ c.x - r * 0.6f, c.y + r * 0.2f, r * 1.2f, r * 0.8f }, Color{ 230, 226, 214, 255 });
    DrawCircleV({ c.x - r * 0.38f, c.y - r * 0.1f }, r * 0.26f, UI::INK);
    DrawCircleV({ c.x + r * 0.38f, c.y - r * 0.1f }, r * 0.26f, UI::INK);
}

void crownIcon(Vector2 c, float r, Color color) {
    DrawRectangleRec({ c.x - r, c.y, r * 2.0f, r * 0.6f }, color);
    DrawTriangle({ c.x - r, c.y }, { c.x - r * 0.33f, c.y }, { c.x - r, c.y - r * 0.8f }, color);
    DrawTriangle({ c.x - r * 0.45f, c.y }, { c.x + r * 0.45f, c.y }, { c.x, c.y - r }, color);
    DrawTriangle({ c.x + r * 0.33f, c.y }, { c.x + r, c.y }, { c.x + r, c.y - r * 0.8f }, color);
    DrawCircleV({ c.x, c.y + r * 0.3f }, r * 0.18f, UI::ROYAL_RED);
}

// Tegner en render-tekstur (lagret opp-ned) som en rund bit, f.eks. portrettet
void texturedCircle(Texture2D tex, Vector2 c, float r) {
    const int segments = 48;
    // RL_QUADS (med to like hjørner) fordi rlgl nullstiller teksturen når man bytter til RL_TRIANGLES
    rlSetTexture(tex.id);
    rlBegin(RL_QUADS);
    rlColor4ub(255, 255, 255, 255);
    rlNormal3f(0.0f, 0.0f, 1.0f);
    for (int i = 0; i < segments; i++) {
        float a0 = 2.0f * PI * i / segments;
        float a1 = 2.0f * PI * (i + 1) / segments;
        auto vert = [&](float cx, float cy) {
            rlTexCoord2f(0.5f + (cx - c.x) / (2.0f * r), 0.5f - (cy - c.y) / (2.0f * r));
            rlVertex2f(cx, cy);
        };
        vert(c.x, c.y);
        vert(c.x + cosf(a1) * r, c.y + sinf(a1) * r);
        vert(c.x + cosf(a0) * r, c.y + sinf(a0) * r);
        vert(c.x + cosf(a0) * r, c.y + sinf(a0) * r);
    }
    rlEnd();
    rlSetTexture(0);
}

// Tekst med kontur i HUD-en (int-størrelser gir skarpere standardfont)
void hudText(const char* text, float x, float y, float size, Color color) {
    UI::DrawOutlinedText(text, std::round(x), std::round(y), std::round(size), color, std::max(1.0f, size / 12.0f));
}

float hudTextWidth(const char* text, float size) {
    size = std::round(size);
    return MeasureTextEx(GetFontDefault(), text, size, size / 10.0f).x;
}

// ---------------------------------------------------------------------
// SPILLERPANEL (oppe til venstre): portrett, level, HP og ressurser
// ---------------------------------------------------------------------
void drawPlayerPanel(const HudState& hud, float s, float top) {
    const Player& p = *hud.player;
    float x = 12.0f * s;
    Rectangle panel = { x, top, 330.0f * s, 104.0f * s };
    UI::DrawPanel(panel, s);

    // Portrett i en rund gullramme
    Vector2 pc = { panel.x + 52.0f * s, panel.y + 50.0f * s };
    float pr = 38.0f * s;
    DrawCircleV(pc, pr + 4.0f * s, UI::INK);
    DrawCircleV(pc, pr + 2.5f * s, UI::GOLD_DARK);
    DrawCircleV(pc, pr, Color{ 50, 34, 60, 255 });
    if (hud.portrait.id != 0) {
        texturedCircle(hud.portrait, pc, pr);
        DrawRing(pc, pr - 1.0f * s, pr + 0.5f * s, 200.0f, 330.0f, 24, Fade(UI::GOLD_LIGHT, 0.7f));
    }

    // Level-merke nederst på portrettet
    const char* lv = TextFormat("LV %d", p.level);
    float lvSize = 16.0f * s;
    float lvW = hudTextWidth(lv, lvSize) + 14.0f * s;
    Rectangle badge = { pc.x - lvW / 2.0f, pc.y + pr - 8.0f * s, lvW, 21.0f * s };
    DrawRectangleRounded({ badge.x - 2.0f * s, badge.y - 2.0f * s, badge.width + 4.0f * s, badge.height + 4.0f * s }, 0.5f, 6, UI::INK);
    DrawRectangleRounded(badge, 0.5f, 6, UI::ROYAL_RED);
    DrawRectangleRoundedLinesEx(badge, 0.5f, 6, 1.5f * s, UI::GOLD_LIGHT);
    hudText(lv, badge.x + 7.0f * s, badge.y + 3.0f * s, lvSize, UI::GOLD_LIGHT);

    // Navn og HP
    float bx = panel.x + 104.0f * s;
    float bw = panel.x + panel.width - bx - 16.0f * s;
    hudText(hud.characterName, bx, panel.y + 12.0f * s, 16.0f * s, Color{ 240, 232, 214, 255 });

    Rectangle hpBar = { bx, panel.y + 34.0f * s, bw, 20.0f * s };
    float hpPct = p.maxHp > 0.0f ? p.hp / p.maxHp : 0.0f;
    UI::DrawBar(hpBar, hpPct, HP_RED, HP_BACK, s);
    const char* hpText = TextFormat("%.0f / %.0f", std::max(0.0f, p.hp), p.maxHp);
    hudText(hpText, hpBar.x + hpBar.width / 2.0f - hudTextWidth(hpText, 14.0f * s) / 2.0f, hpBar.y + 3.0f * s, 14.0f * s, WHITE);
    if (p.slowTimer > 0.0f) hudText("SLOWED", hpBar.x + hpBar.width - hudTextWidth("SLOWED", 10.0f * s), hpBar.y - 12.0f * s, 10.0f * s, SKYBLUE);

    // Ressurser: aegis, gull og kills på én rad
    float ry = panel.y + 78.0f * s;
    float rx = bx + 8.0f * s;
    float ir = 7.0f * s;
    shieldIcon({ rx, ry }, ir, p.aegis > 0 ? Color{ 90, 200, 120, 255 } : GRAY);
    hudText(TextFormat("%d", p.aegis), rx + 13.0f * s, ry - 8.0f * s, 16.0f * s, WHITE);
    rx += 58.0f * s;
    coinIcon({ rx, ry }, ir);
    hudText(TextFormat("%d", hud.runCoins), rx + 13.0f * s, ry - 8.0f * s, 16.0f * s, GOLD);
    rx += 72.0f * s;
    skullIcon({ rx, ry }, ir);
    hudText(TextFormat("%d", hud.kills), rx + 13.0f * s, ry - 8.0f * s, 16.0f * s, WHITE);

    // Curses som små lilla lapper under panelet
    if (hud.curses && !hud.curses->empty()) {
        float cx = panel.x;
        float cy = panel.y + panel.height + 10.0f * s;
        for (CurseId id : *hud.curses) {
            const char* name = GetCurse(id).name.c_str();
            float tw = hudTextWidth(name, 14.0f * s) + 16.0f * s;
            Rectangle tag = { cx, cy, tw, 22.0f * s };
            DrawRectangleRounded(tag, 0.4f, 6, Color{ 40, 18, 60, 220 });
            DrawRectangleRoundedLinesEx(tag, 0.4f, 6, 1.5f * s, CURSE_VIOLET);
            hudText(name, tag.x + 8.0f * s, tag.y + 4.0f * s, 14.0f * s, CURSE_VIOLET);
            cx += tw + 6.0f * s;
        }
    }
}

// ---------------------------------------------------------------------
// KLOKKE (oppe i midten) og boss-HP i tronsalen
// ---------------------------------------------------------------------
void drawTimer(const HudState& hud, float s, float top, const Enemy* boss) {
    float w = (float)GetScreenWidth();
    float cx = w / 2.0f;
    std::string time = formatTime(hud.gameTime);
    Rectangle plaque = { cx - 90.0f * s, top, 180.0f * s, 64.0f * s };
    UI::DrawPanel(plaque, s, hud.inBossArena ? UI::ROYAL_RED : UI::PANEL_EDGE);

    float ts = 30.0f * s;
    hudText(time.c_str(), cx - hudTextWidth(time.c_str(), ts) / 2.0f, plaque.y + 8.0f * s, ts, hud.inBossArena ? Color{ 255, 90, 90, 255 } : WHITE);

    if (!hud.inBossArena) {
        // Nedtelling og liten bar mot bossen
        float remaining = hud.bossTime - hud.gameTime;
        const char* label = TextFormat("KONGEN OM %s", formatTime(remaining).c_str());
        float ls = 10.0f * s;
        hudText(label, cx - hudTextWidth(label, ls) / 2.0f, plaque.y + 40.0f * s, ls, remaining < 30.0f ? Color{ 255, 120, 90, 255 } : Color{ 220, 200, 160, 255 });
        Rectangle prog = { plaque.x + 18.0f * s, plaque.y + 53.0f * s, plaque.width - 36.0f * s, 4.0f * s };
        DrawRectangleRec(prog, Fade(BLACK, 0.6f));
        DrawRectangleRec({ prog.x, prog.y, prog.width * Clamp(hud.gameTime / hud.bossTime, 0.0f, 1.0f), prog.height }, UI::ROYAL_RED);
    } else {
        const char* label = "KONGENS TRONSAL";
        float ls = 10.0f * s;
        hudText(label, cx - hudTextWidth(label, ls) / 2.0f, plaque.y + 44.0f * s, ls, UI::GOLD_LIGHT);
    }

    // --- Boss-bar under klokka ---
    if (boss) {
        float bw = std::min(620.0f * s, w - 2.0f * (12.0f * s + 330.0f * s + 20.0f * s));
        bw = std::max(bw, 260.0f * s);
        Rectangle bar = { cx - bw / 2.0f, plaque.y + plaque.height + 18.0f * s, bw, 18.0f * s };
        float pct = std::max(0.0f, (float)boss->hp / (float)boss->maxHp);
        UI::DrawBar(bar, pct, Color{ 200, 30, 40, 255 }, Color{ 40, 8, 14, 230 }, s);
        // Små delstreker for hver 10 %
        for (int i = 1; i < 10; i++) {
            float lx = bar.x + bar.width * i / 10.0f;
            DrawRectangleRec({ lx, bar.y, 1.0f * s, bar.height }, Fade(BLACK, 0.35f));
        }
        crownIcon({ bar.x - 2.0f * s, bar.y + 6.0f * s }, 13.0f * s, UI::INK);
        crownIcon({ bar.x - 2.0f * s, bar.y + 6.0f * s }, 11.0f * s, UI::GOLD_LIGHT);
        const char* name = TextFormat("KONGEN  -  %s", hud.echelonName);
        hudText(name, cx - hudTextWidth(name, 14.0f * s) / 2.0f, bar.y + bar.height + 6.0f * s, 14.0f * s, WHITE);
    }
}

// ---------------------------------------------------------------------
// MINIMAP (oppe til høyre)
// Spilleren er alltid i midten, og kartet roterer med kameraet så "opp"
// på kartet er "opp" på skjermen.
// ---------------------------------------------------------------------
void drawMinimap(const HudState& hud, float s, float top, const Enemy* boss) {
    const float size = 196.0f * s;
    const float worldRange = hud.inBossArena ? hud.arenaRadius * 1.12f : 1300.0f; // Verdensenheter fra midten til kanten
    float w = (float)GetScreenWidth();
    Rectangle frame = { w - 12.0f * s - size - 16.0f * s, top, size + 16.0f * s, size + 40.0f * s };
    UI::DrawPanel(frame, s);

    Rectangle map = { frame.x + 8.0f * s, frame.y + 8.0f * s, size, size };
    Vector2 mc = { map.x + size / 2.0f, map.y + size / 2.0f };
    float k = (size / 2.0f) / worldRange;

    // Kamerarotasjon: skjerm-opp i verden er (-sin, -cos), skjerm-høyre er (cos, -sin)
    float yaw = hud.cameraYaw * DEG2RAD;
    Vector2 up = { -sinf(yaw), -cosf(yaw) };
    Vector2 right = { cosf(yaw), -sinf(yaw) };
    // Vanligvis er spilleren i midten; i tronsalen er hele arenaen i midten
    Vector2 origin = hud.inBossArena ? hud.arenaCenter : hud.player->position;
    auto toMap = [&](Vector2 world) {
        Vector2 d = Vector2Subtract(world, origin);
        return Vector2{ mc.x + Vector2DotProduct(d, right) * k, mc.y - Vector2DotProduct(d, up) * k };
    };
    auto inside = [&](Vector2 m, float margin) {
        return m.x > map.x - margin && m.x < map.x + size + margin && m.y > map.y - margin && m.y < map.y + size + margin;
    };

    BeginScissorMode((int)map.x, (int)map.y, (int)size, (int)size);
    DrawRectangleRec(map, hud.inBossArena ? Color{ 24, 20, 30, 240 } : MAP_FLOOR);

    // Rutenett som på slottsgulvet (roterer og flytter seg med spilleren)
    if (!hud.inBossArena) {
        const float cell = 256.0f;
        float reach = worldRange * 1.5f;
        float gx0 = std::floor((origin.x - reach) / cell) * cell;
        float gy0 = std::floor((origin.y - reach) / cell) * cell;
        for (float gx = gx0; gx <= origin.x + reach; gx += cell) {
            DrawLineEx(toMap({ gx, origin.y - reach }), toMap({ gx, origin.y + reach }), 1.0f * s, Fade(WHITE, 0.06f));
        }
        for (float gy = gy0; gy <= origin.y + reach; gy += cell) {
            DrawLineEx(toMap({ origin.x - reach, gy }), toMap({ origin.x + reach, gy }), 1.0f * s, Fade(WHITE, 0.06f));
        }
        // De røde løperne i storsalen (samme mønster som castle.cpp: hver 24. flis, 3 fliser brede)
        const float carpetSpacing = 64.0f * 24.0f;
        const float carpetWidth = 64.0f * 3.0f;
        Color carpet = Fade(UI::ROYAL_RED, 0.55f);
        for (float cx = std::floor((origin.x - reach) / carpetSpacing) * carpetSpacing; cx <= origin.x + reach; cx += carpetSpacing) {
            DrawLineEx(toMap({ cx, origin.y - reach }), toMap({ cx, origin.y + reach }), carpetWidth * k, carpet);
        }
        for (float cy = std::floor((origin.y - reach) / carpetSpacing) * carpetSpacing; cy <= origin.y + reach; cy += carpetSpacing) {
            DrawLineEx(toMap({ origin.x - reach, cy }), toMap({ origin.x + reach, cy }), carpetWidth * k, carpet);
        }
        // Startpunktet (midt i storsalen)
        Vector2 home = toMap({ 0.0f, 0.0f });
        if (inside(home, 10.0f)) {
            DrawCircleLinesV(home, 6.0f * s, Fade(UI::GOLD_LIGHT, 0.5f));
            DrawCircleV(home, 2.0f * s, Fade(UI::GOLD_LIGHT, 0.5f));
        }
    } else {
        // Tronsalen: rund arena med trone i nord
        Vector2 c = toMap(hud.arenaCenter);
        DrawCircleV(c, hud.arenaRadius * k, Color{ 60, 50, 60, 255 });
        DrawRing(c, hud.arenaRadius * k - 2.0f * s, hud.arenaRadius * k, 0.0f, 360.0f, 64, UI::GOLD_DARK);
        Vector2 carpetA = toMap({ hud.arenaCenter.x, hud.arenaCenter.y - hud.arenaRadius });
        Vector2 carpetB = toMap({ hud.arenaCenter.x, hud.arenaCenter.y + hud.arenaRadius });
        DrawLineEx(carpetA, carpetB, 14.0f * s, Fade(UI::ROYAL_RED, 0.7f));
    }

    // Loot-radius rundt spilleren
    Vector2 pm = toMap(hud.player->position);
    DrawCircleLinesV(pm, hud.player->lootRadius * k, Fade(XP_BLUE, 0.25f));

    // Pickups: XP som små blå prikker, gull som gule
    if (hud.pickups) {
        for (const Pickup& pk : *hud.pickups) {
            Vector2 m = toMap(pk.position);
            if (!inside(m, 4.0f)) continue;
            if (pk.type == PickupType::CHEST) {
                DrawRectangleRec({ m.x - 4.0f * s, m.y - 3.5f * s, 8.0f * s, 7.0f * s }, UI::INK);
                DrawRectangleRec({ m.x - 3.0f * s, m.y - 2.5f * s, 6.0f * s, 5.0f * s }, UI::GOLD_LIGHT);
            } else if (pk.type == PickupType::COIN) DrawCircleV(m, 2.4f * s, GOLD);
            else DrawRectangleRec({ m.x - 0.8f * s, m.y - 0.8f * s, 1.6f * s, 1.6f * s }, Fade(XP_BLUE, 0.8f));
        }
    }

    // Fiender: røde prikker, kamikaze oransje
    if (hud.enemies) {
        for (const auto& e : *hud.enemies) {
            if (e->id == hud.bossId) continue;
            Vector2 m = toMap(e->position);
            if (!inside(m, 4.0f)) continue;
            bool exploder = dynamic_cast<const Exploder*>(e.get()) != nullptr;
            float r = (1.6f + e->hitRadius * 0.06f) * s;
            DrawCircleV(m, r + 0.8f * s, Fade(BLACK, 0.6f));
            DrawCircleV(m, r, exploder ? Color{ 255, 150, 40, 255 } : Color{ 240, 60, 60, 255 });
        }
    }

    // Spilleren: pil som peker dit klovnen ser
    {
        Vector2 f = hud.player->facingDir;
        Vector2 dir = { Vector2DotProduct(f, right), -Vector2DotProduct(f, up) };
        if (Vector2Length(dir) < 0.01f) dir = { 0.0f, -1.0f };
        dir = Vector2Normalize(dir);
        Vector2 side = { -dir.y, dir.x };
        float a = 8.0f * s;
        Vector2 tip = Vector2Add(pm, Vector2Scale(dir, a));
        Vector2 l = Vector2Add(Vector2Subtract(pm, Vector2Scale(dir, a * 0.6f)), Vector2Scale(side, a * 0.7f));
        Vector2 r = Vector2Subtract(Vector2Subtract(pm, Vector2Scale(dir, a * 0.6f)), Vector2Scale(side, a * 0.7f));
        Vector2 back = Vector2Subtract(pm, Vector2Scale(dir, a * 0.2f));
        auto tri = [](Vector2 p1, Vector2 p2, Vector2 p3, Color c) {
            float cross = (p2.x - p1.x) * (p3.y - p1.y) - (p2.y - p1.y) * (p3.x - p1.x);
            if (cross < 0.0f) DrawTriangle(p1, p2, p3, c); else DrawTriangle(p1, p3, p2, c);
        };
        DrawCircleV(pm, a * 0.9f, Fade(BLACK, 0.4f));
        tri(tip, l, back, WHITE);
        tri(tip, back, r, Color{ 220, 220, 230, 255 });
    }
    EndScissorMode();

    // Kompass: "N" langs kanten der verdens-nord er
    {
        Vector2 north = { Vector2DotProduct({ 0.0f, -1.0f }, right), -Vector2DotProduct({ 0.0f, -1.0f }, up) };
        float edge = size / 2.0f - 9.0f * s;
        float scaleToEdge = edge / std::max(fabsf(north.x), fabsf(north.y));
        Vector2 np = Vector2Add(mc, Vector2Scale(north, scaleToEdge));
        DrawCircleV(np, 8.0f * s, UI::INK);
        DrawCircleLinesV(np, 8.0f * s, UI::GOLD_DARK);
        hudText("N", np.x - hudTextWidth("N", 10.0f * s) / 2.0f, np.y - 5.0f * s, 10.0f * s, UI::GOLD_LIGHT);
    }

    DrawRectangleLinesEx(map, 1.5f * s, UI::INK);

    // Bossen: krone, festet til kanten hvis den er utenfor kartet
    if (boss) {
        Vector2 m = toMap(boss->position);
        Vector2 clamped = { Clamp(m.x, map.x + 10.0f * s, map.x + size - 10.0f * s), Clamp(m.y, map.y + 10.0f * s, map.y + size - 10.0f * s) };
        float pulse = 1.0f + 0.15f * sinf((float)GetTime() * 6.0f);
        DrawCircleV(clamped, 11.0f * s * pulse, Fade(RED, 0.35f));
        crownIcon(clamped, 8.0f * s, UI::INK);
        crownIcon(clamped, 6.5f * s, UI::GOLD_LIGHT);
    }

    // Echelon-navnet under kartet
    float es = 14.0f * s;
    hudText(hud.echelonName, frame.x + frame.width / 2.0f - hudTextWidth(hud.echelonName, es) / 2.0f, map.y + size + 9.0f * s, es, Color{ 255, 170, 70, 255 });
}

// Rød, pulserende kant når HP er lav
void drawLowHpVignette(const Player& p) {
    float pct = p.maxHp > 0.0f ? p.hp / p.maxHp : 1.0f;
    if (pct > 0.3f || p.hp <= 0.0f) return;
    float w = (float)GetScreenWidth(), h = (float)GetScreenHeight();
    float strength = (0.3f - pct) / 0.3f;
    float pulse = 0.55f + 0.45f * sinf((float)GetTime() * 5.0f);
    float a = (0.2f + 0.4f * strength) * pulse;
    int band = (int)(std::min(w, h) * 0.18f);
    Color red = Color{ 200, 0, 20, 255 };
    DrawRectangleGradientV(0, 0, (int)w, band, Fade(red, a), Fade(red, 0.0f));
    DrawRectangleGradientV(0, (int)h - band, (int)w, band, Fade(red, 0.0f), Fade(red, a));
    DrawRectangleGradientH(0, 0, band, (int)h, Fade(red, a), Fade(red, 0.0f));
    DrawRectangleGradientH((int)w - band, 0, band, (int)h, Fade(red, 0.0f), Fade(red, a));
}

} // namespace

float HudScale() {
    return Clamp(UI::Scale(), 0.7f, 2.5f);
}

void DrawGameHud(const HudState& hud) {
    if (!hud.player) return;
    const Player& p = *hud.player;
    float s = HudScale();
    float w = (float)GetScreenWidth();
    float h = (float)GetScreenHeight();

    drawLowHpVignette(p);

    // --- XP-bar over hele toppen ---
    float xpH = 12.0f * s;
    float xpPct = p.xpToNextLevel > 0 ? (float)p.currentXp / (float)p.xpToNextLevel : 0.0f;
    DrawRectangleRec({ 0, 0, w, xpH + 3.0f * s }, UI::INK);
    DrawRectangleRec({ 0, 0, w, xpH }, XP_BACK);
    DrawRectangleRec({ 0, 0, w * Clamp(xpPct, 0.0f, 1.0f), xpH }, XP_BLUE);
    DrawRectangleGradientV(0, 0, (int)(w * Clamp(xpPct, 0.0f, 1.0f)), (int)(xpH * 0.5f), Fade(WHITE, 0.4f), Fade(WHITE, 0.0f));
    DrawRectangleRec({ 0, xpH, w, 1.5f * s }, Fade(UI::GOLD_LIGHT, 0.4f));

    const Enemy* boss = nullptr;
    if (hud.inBossArena && hud.enemies) {
        for (const auto& e : *hud.enemies) if (e->id == hud.bossId) boss = e.get();
    }

    float top = xpH + 10.0f * s;
    drawPlayerPanel(hud, s, top);
    drawTimer(hud, s, top, boss);
    if (hud.showMinimap) drawMinimap(hud, s, top, boss);

    // --- Ability-slots nederst i midten ---
    DrawAbilityHud(p, w / 2.0f, h - 12.0f * s, s);

    // --- Items: en rad med små runde plasser over ability-panelet ---
    {
        const float r = 15.0f * s;
        const float gap = 8.0f * s;
        float totalW = MAX_ITEM_SLOTS * r * 2.0f + (MAX_ITEM_SLOTS - 1) * gap;
        float y = h - 12.0f * s - 110.0f * s - r - 22.0f * s; // Plass til "KLAR!" over ability-slotsene
        for (int i = 0; i < MAX_ITEM_SLOTS; i++) {
            Vector2 c = { w / 2.0f - totalW / 2.0f + r + i * (r * 2.0f + gap), y };
            bool has = i < (int)p.items.size();
            DrawCircleV(c, r + 2.0f * s, UI::INK);
            DrawCircleV(c, r, has ? Color{ 56, 42, 60, 235 } : Color{ 24, 20, 30, 200 });
            if (!has) continue;
            ItemId id = p.items[i];
            DrawItemIcon(id, c, r * 0.72f);
            // Nivå som en liten gullbue rundt
            float pct = (float)p.itemLevels[(int)id] / MAX_ITEM_LEVEL;
            DrawRing(c, r - 1.5f * s, r + 1.0f * s, -90.0f, -90.0f + 360.0f * pct, 24, UI::GOLD_LIGHT);
        }
    }

    // --- Kontroller nederst til venstre ---
    const char* hint = "[Q/E] Roter   [M] Kart   [ESC] Avslutt";
    hudText(hint, 14.0f * s, h - 24.0f * s, 12.0f * s, Fade(Color{ 220, 210, 190, 255 }, 0.7f));
}
