#ifndef HUD_HPP
#define HUD_HPP

#include <raylib.h>
#include <vector>
#include <memory>
#include <string>
#include "player.hpp"
#include "enemy.hpp"
#include "curses.hpp"

// =====================================================================
// HUD UNDER SPILLING
//
//  ┌───────────────────── XP-bar over hele toppen ─────────────────────┐
//  │ [portrett] HP-bar         [ 04:21 ]                    ┌────────┐ │
//  │  LV 5      aegis/gull/kills  boss om 05:39              │minimap │ │
//  │ curses                                                  └────────┘ │
//  │                                                                    │
//  │ kontroller           [ 5 ability-slots ]                           │
//  └────────────────────────────────────────────────────────────────────┘
//
// Alt skaleres etter vindusstørrelsen og festes til hjørnene, så det
// fungerer både i lite vindu og i fullskjerm.
// =====================================================================

struct HudState {
    const Player* player = nullptr;
    const std::vector<std::unique_ptr<Enemy>>* enemies = nullptr;
    const std::vector<Pickup>* pickups = nullptr;
    const std::vector<CurseId>* curses = nullptr;

    Texture2D portrait{};      // 3D-portrett av klovnen (render-tekstur, lagret opp-ned)
    const char* characterName = "";
    const char* echelonName = "";

    float gameTime = 0.0f;
    float bossTime = 0.0f;     // Når bossen kommer
    bool inBossArena = false;
    int bossId = -1;
    Vector2 arenaCenter = { 0, 0 };
    float arenaRadius = 0.0f;

    int runCoins = 0;
    int kills = 0;
    float cameraYaw = 0.0f;    // Minimapet roterer med kameraet (Q/E)
    bool showMinimap = true;
};

// Skalering for HUD-en: følger vinduet, men aldri så lite at teksten blir uleselig
float HudScale();

void DrawGameHud(const HudState& hud);

#endif // HUD_HPP
