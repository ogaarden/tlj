#include "damage_numbers.hpp"
#include "render3d.hpp"
#include "settings.hpp"
#include <vector>
#include <algorithm>

namespace {
    struct DamageNumber {
        Vector2 position;   // Posisjon på gulvet
        float height;       // Høyde over gulvet (stiger oppover)
        float riseSpeed;
        float driftX;       // Litt sidelengs drift
        int amount;
        Color color;
        float lifetime;
        float maxLifetime;
        bool crit;
    };

    std::vector<DamageNumber> damageNumbers;
}

void SpawnDamageNumber(Vector2 worldPos, int amount, Color color, bool crit) {
    if (amount <= 0) return;

    // Litt tilfeldig spredning så tallene ikke legger seg oppå hverandre
    float offsetX = (float)GetRandomValue(-10, 10);
    float driftX = (float)GetRandomValue(-20, 20);

    damageNumbers.push_back({
        .position = { worldPos.x + offsetX, worldPos.y },
        .height = 40.0f,
        .riseSpeed = 70.0f,
        .driftX = driftX,
        .amount = amount,
        .color = color,
        .lifetime = crit ? 1.0f : 0.8f,
        .maxLifetime = crit ? 1.0f : 0.8f,
        .crit = crit
    });
}

void UpdateDamageNumbers(float deltaTime) {
    for (size_t i = 0; i < damageNumbers.size(); ) {
        auto& d = damageNumbers[i];
        d.position.x += d.driftX * deltaTime;
        d.height += d.riseSpeed * deltaTime;
        d.riseSpeed = std::max(0.0f, d.riseSpeed - 50.0f * deltaTime); // Bremser opp mot slutten
        d.lifetime -= deltaTime;

        if (d.lifetime <= 0.0f) {
            damageNumbers[i] = damageNumbers.back();
            damageNumbers.pop_back();
        } else {
            i++;
        }
    }
}

void DrawDamageNumbers(const Camera3D& camera) {
    // Verdenen blir større med vindushøyden (fast synsvinkel), så tallene skal også det
    float scale = std::max(0.7f, GetScreenHeight() / (float)Settings::SCREEN_HEIGHT);
    int shadow = std::max(1, (int)(scale + 0.5f));
    for (const auto& d : damageNumbers) {
        Vector2 screenPos = GroundToScreen(camera, d.position, d.height);

        float t = d.lifetime / d.maxLifetime; // 1 -> 0
        float alpha = (t < 0.4f) ? t / 0.4f : 1.0f; // Fade ut de siste 40%

        // Større tall = større font, med et lite "pop" når tallet dukker opp
        int fontSize = 16;
        if (d.amount >= 50) fontSize = 20;
        if (d.amount >= 150) fontSize = 26;
        if (t > 0.85f) fontSize += 4;
        if (d.crit) fontSize = (int)(fontSize * 1.5f) + (t > 0.8f ? 8 : 0); // Kritisk: større, med ekstra "pop"
        fontSize = (int)(fontSize * scale);

        const char* text = d.crit ? TextFormat("%d!", d.amount) : TextFormat("%d", d.amount);
        int width = MeasureText(text, fontSize);
        int x = (int)screenPos.x - width / 2;
        int y = (int)screenPos.y;

        DrawText(text, x + shadow, y + shadow, fontSize, Fade(BLACK, alpha)); // Skygge
        if (d.crit) {
            // Tykk mørk kontur og gul-oransje farge så kritiske treff synes i kaoset
            for (int dx = -1; dx <= 1; dx++)
                for (int dy = -1; dy <= 1; dy++)
                    if (dx || dy) DrawText(text, x + dx * shadow * 2, y + dy * shadow * 2, fontSize, Fade(Color{ 60, 10, 0, 255 }, alpha));
            DrawText(text, x, y, fontSize, Fade(Color{ 255, 200, 40, 255 }, alpha));
        } else {
            DrawText(text, x, y, fontSize, Fade(d.color, alpha));
        }
    }
}

void ClearDamageNumbers() {
    damageNumbers.clear();
}
