#include "echelon.hpp"
#include <algorithm>

namespace {
    // Hver echelon sin unike effekt. De stacker oppå hverandre.
    const std::vector<EchelonData> ECHELONS = {
        { 1,  "Echelon I",    "Basic",
            [](EchelonModifiers&) {} },
        { 2,  "Echelon II",   "Kamikaze-fiender som eksploderer",
            [](EchelonModifiers& m) { m.exploders = true; } },
        { 3,  "Echelon III",  "Fiender slower deg ved treff",
            [](EchelonModifiers& m) { m.slowOnHit = 0.4f; m.slowDuration = 1.5f; } },
        { 4,  "Echelon IV",   "+25% fiendeskade",
            [](EchelonModifiers& m) { m.enemyDamageMult *= 1.25f; } },
        { 5,  "Echelon V",    "Velg en curse foer runden",
            [](EchelonModifiers& m) { m.curses += 1; } },
        { 6,  "Echelon VI",   "Fiender er 15% raskere",
            [](EchelonModifiers& m) { m.enemySpeedMult *= 1.15f; } },
        { 7,  "Echelon VII",  "-25% XP",
            [](EchelonModifiers& m) { m.xpMult *= 0.75f; } },
        { 8,  "Echelon VIII", "HP-regen virker ikke",
            [](EchelonModifiers& m) { m.noRegen = true; } },
        { 9,  "Echelon IX",   "+30% fiende-HP",
            [](EchelonModifiers& m) { m.enemyHpMult *= 1.3f; } },
        { 10, "Echelon X",    "Velg en curse til",
            [](EchelonModifiers& m) { m.curses += 1; } },
    };
}

const EchelonData& GetEchelon(int echelon) {
    int index = std::clamp(echelon, 1, MAX_ECHELON) - 1;
    return ECHELONS[index];
}

EchelonModifiers GetEchelonModifiers(int echelon) {
    EchelonModifiers mods;
    int last = std::clamp(echelon, 1, MAX_ECHELON);
    for (int e = 1; e <= last; e++) {
        GetEchelon(e).apply(mods);
    }
    return mods;
}

float GetBossTimer(int echelon) {
    return BASE_BOSS_TIMER + BOSS_TIMER_PER_ECHELON * (std::clamp(echelon, 1, MAX_ECHELON) - 1);
}
