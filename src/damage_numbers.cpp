#include "damage_numbers.hpp"
#include <vector>

namespace {
    struct DamageNumber {
        Vector2 position;   // Verdensposisjon
        Vector2 velocity;   // Driver oppover og litt til siden
        int amount;
        Color color;
        float lifetime;
        float maxLifetime;
    };

    std::vector<DamageNumber> damageNumbers;
}

void SpawnDamageNumber(Vector2 worldPos, int amount, Color color) {
    if (amount <= 0) return;

    // Litt tilfeldig spredning så tallene ikke legger seg oppå hverandre
    float offsetX = (float)GetRandomValue(-10, 10);
    float driftX = (float)GetRandomValue(-20, 20);

    damageNumbers.push_back({
        .position = { worldPos.x + offsetX, worldPos.y - 20.0f },
        .velocity = { driftX, -60.0f },
        .amount = amount,
        .color = color,
        .lifetime = 0.8f,
        .maxLifetime = 0.8f
    });
}

void UpdateDamageNumbers(float deltaTime) {
    for (size_t i = 0; i < damageNumbers.size(); ) {
        auto& d = damageNumbers[i];
        d.position.x += d.velocity.x * deltaTime;
        d.position.y += d.velocity.y * deltaTime;
        d.velocity.y += 40.0f * deltaTime; // Bremser opp mot slutten
        d.lifetime -= deltaTime;

        if (d.lifetime <= 0.0f) {
            damageNumbers[i] = damageNumbers.back();
            damageNumbers.pop_back();
        } else {
            i++;
        }
    }
}

void DrawDamageNumbers(const Camera2D& camera) {
    for (const auto& d : damageNumbers) {
        Vector2 screenPos = GetWorldToScreen2D(d.position, camera);

        float t = d.lifetime / d.maxLifetime; // 1 -> 0
        float alpha = (t < 0.4f) ? t / 0.4f : 1.0f; // Fade ut de siste 40%

        // Større tall = større font, med et lite "pop" når tallet dukker opp
        int fontSize = 16;
        if (d.amount >= 50) fontSize = 20;
        if (d.amount >= 150) fontSize = 26;
        if (t > 0.85f) fontSize += 4;

        const char* text = TextFormat("%d", d.amount);
        int width = MeasureText(text, fontSize);
        int x = (int)screenPos.x - width / 2;
        int y = (int)screenPos.y;

        DrawText(text, x + 1, y + 1, fontSize, Fade(BLACK, alpha)); // Skygge
        DrawText(text, x, y, fontSize, Fade(d.color, alpha));
    }
}

void ClearDamageNumbers() {
    damageNumbers.clear();
}
