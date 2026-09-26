#include "explosions.hpp"
#include "audio.hpp"
#include "vfx.hpp"
#include "render3d.hpp"
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

    struct Shell {
        Vector2 from, to;
        float t;          // 0 -> 1 i løpet av flyturen
        float flightTime;
        float radius, damage;
    };
    std::vector<Shell> shells;
    constexpr float SHELL_ARC = 220.0f; // Hvor høyt granaten går
}

void SpawnMortarShell(Vector2 from, Vector2 to, float flightTime, float radius, float damage) {
    shells.push_back({ from, to, 0.0f, flightTime, radius, damage });
}

void SpawnExplosion(Vector2 position, float radius, float damage) {
    explosions.push_back({ position, radius, damage, EXPLOSION_ANIM_TIME, false });
    VfxExplosion(position, radius);
    AddCameraShake(0.45f);
    PlaySfx(Sfx::EXPLOSION);
}

float UpdateExplosions(float deltaTime, Vector2 playerPos, float playerRadius) {
    float damageToPlayer = 0.0f;

    // Granater som lander blir til vanlige eksplosjoner
    for (size_t i = 0; i < shells.size(); ) {
        Shell& s = shells[i];
        s.t += deltaTime / s.flightTime;
        if (s.t >= 1.0f) {
            SpawnExplosion(s.to, s.radius, s.damage);
            shells[i] = shells.back();
            shells.pop_back();
        } else {
            i++;
        }
    }

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
    // Varselsirkel der granaten lander: fylles opp mens den flyr
    for (const Shell& s : shells) {
        DrawCircleV(s.to, s.radius, Fade(RED, 0.10f + 0.15f * s.t));
        DrawCircleV(s.to, s.radius * s.t, Fade(RED, 0.25f));
        DrawRing(s.to, s.radius - 3.0f, s.radius, 0.0f, 360.0f, 40, Fade(RED, 0.85f));
    }
    for (const auto& e : explosions) {
        float t = 1.0f - e.timer / EXPLOSION_ANIM_TIME; // 0 -> 1
        // Svidd merke på gulvet (selve ildkula er VFX, se VfxExplosion)
        DrawCircleV(e.position, e.radius * 0.8f, Fade(BLACK, 0.35f * (1.0f - t)));
        DrawCircleLines((int)e.position.x, (int)e.position.y, e.radius, Fade(RED, 0.8f * (1.0f - t)));
    }
}

void DrawExplosions3D() {
    // Ildkula og glørne tegnes av VFX-systemet (vfx.cpp). Her: granatene i lufta.
    for (const Shell& s : shells) {
        Vector2 p = Vector2Lerp(s.from, s.to, s.t);
        float h = 30.0f + 4.0f * SHELL_ARC * s.t * (1.0f - s.t);
        ShadedSphere(ToWorld3D(p, h), 8.0f, Color{ 45, 42, 50, 255 }, 5, 8);
        ShadedSphere(ToWorld3D(p, h + 6.0f), 2.5f, Color{ 255, 170, 60, 255 }, 3, 4); // Lunta
    }
}

void ClearExplosions() {
    explosions.clear();
    shells.clear();
}
