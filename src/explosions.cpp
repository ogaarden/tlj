#include "explosions.hpp"
#include "audio.hpp"
#include "vfx.hpp"
#include <raymath.h>
#include <vector>

namespace {
    constexpr float EXPLOSION_ANIM_TIME = 0.35f;

    struct Explosion {
        Vector2 position;
        float radius;
        float damage;
        float timer;
        bool resolved; // Skaden sjekkes bare én gang, første frame
    };

    std::vector<Explosion> explosions;
}

void SpawnExplosion(Vector2 position, float radius, float damage) {
    explosions.push_back({ position, radius, damage, EXPLOSION_ANIM_TIME, false });
    VfxExplosion(position, radius);
    PlaySfx(Sfx::EXPLOSION);
}

float UpdateExplosions(float deltaTime, Vector2 playerPos, float playerRadius) {
    float damageToPlayer = 0.0f;

    for (size_t i = 0; i < explosions.size(); ) {
        auto& e = explosions[i];

        if (!e.resolved) {
            e.resolved = true;
            if (Vector2Distance(e.position, playerPos) <= e.radius + playerRadius) {
                damageToPlayer += e.damage;
            }
        }

        e.timer -= deltaTime;
        if (e.timer <= 0.0f) {
            explosions[i] = explosions.back();
            explosions.pop_back();
        } else {
            i++;
        }
    }
    return damageToPlayer;
}

void DrawExplosions() {
    for (const auto& e : explosions) {
        float t = 1.0f - e.timer / EXPLOSION_ANIM_TIME; // 0 -> 1
        // Svidd merke på gulvet (selve ildkula er VFX, se VfxExplosion)
        DrawCircleV(e.position, e.radius * 0.8f, Fade(BLACK, 0.35f * (1.0f - t)));
        DrawCircleLines((int)e.position.x, (int)e.position.y, e.radius, Fade(RED, 0.8f * (1.0f - t)));
    }
}

void DrawExplosions3D() {
    // Ildkula og glørne tegnes av VFX-systemet (vfx.cpp)
}

void ClearExplosions() {
    explosions.clear();
}
