#ifndef GAMETIME_HPP
#define GAMETIME_HPP

#include <raylib.h>

// =====================================================================
// SPILLTID: hitstop og slow-motion
// All spill-logikk (fiender, spilleren, våpen, effekter) bruker GameDt() i stedet for
// GetFrameTime(), så tiden kan fryses et lite øyeblikk når noe stort skjer
// (en kjempe faller, en miniboss dør, kongen bytter fase). Menyer og tekst går i vanlig tid.
// =====================================================================
inline float g_timeScale = 1.0f;
inline float g_slowTimer = 0.0f;   // Sanntid som er igjen av slow-motion
inline float g_slowScale = 1.0f;   // Hvor sakte (0.05 = nesten stopp)

inline float GameDt() { return GetFrameTime() * g_timeScale; }

// Frys/senk tiden i `realSeconds` sekunder (sanntid). Den sterkeste effekten vinner.
inline void HitStop(float realSeconds, float scale) {
    if (g_slowTimer <= 0.0f || scale <= g_slowScale) g_slowScale = scale;
    if (realSeconds > g_slowTimer) g_slowTimer = realSeconds;
}

// Kalles én gang per frame med sanntid. Tiden glir mykt tilbake til normal på slutten.
inline void UpdateTimeScale(float realDt) {
    if (g_slowTimer > 0.0f) {
        g_slowTimer -= realDt;
        float ease = g_slowTimer < 0.12f ? g_slowTimer / 0.12f : 1.0f; // Glir ut de siste 0.12 sek
        g_timeScale = 1.0f + (g_slowScale - 1.0f) * (ease > 0.0f ? ease : 0.0f);
    } else {
        g_timeScale = 1.0f;
        g_slowScale = 1.0f;
    }
}

inline void ResetTimeScale() { g_timeScale = 1.0f; g_slowTimer = 0.0f; g_slowScale = 1.0f; }

#endif // GAMETIME_HPP
