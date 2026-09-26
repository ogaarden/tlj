#ifndef VFX_HPP
#define VFX_HPP

#include <raylib.h>

// =====================================================================
// VFX: glød, lyn, eksplosjoner og partikler
//
// Teksturene ligger i assets/vfx/ (generert med Higgsfield på svart bakgrunn).
// De tegnes med ADDITIV blending: svart blir usynlig, bare det som lyser vises.
// Mangler en fil, lages en enkel erstatning i kode, så spillet alltid virker.
//
// Tegnerekkefølge per frame (inne i BeginMode3D, etter alle solide figurer):
//   VfxBegin(camera);  ...weapon->drawVfx()...  DrawVfxParticles();  VfxEnd();
// =====================================================================

enum class VfxTex {
    LIGHTNING_BOLT,   // Loddrett lynstrek (himmel -> bakke)
    LIGHTNING_IMPACT, // Elektrisk nedslag sett ovenfra
    MAGIC_ORB,        // Lilla magisk kule
    EXPLOSION,        // Ildkule sett ovenfra
    SHOCKWAVE,        // Gyllen sjokkbølge-ring
    POISON_MIST,      // Grønn gifttåke
    SLASH,            // Rosa sverdhugg-bue
    SPARK,            // Hvit stjernegnist (farges etter behov)
    GLOW,             // Myk rund glød (farges etter behov) – alltid laget i kode
    COUNT
};

void InitVfx();
void UnloadVfx();
Texture2D VfxTexture(VfxTex tex);

// --- Additivt tegnepass ---
void VfxBegin(const Camera3D& camera);
void VfxEnd();

// Tegnefunksjoner (bare mellom VfxBegin og VfxEnd)
void VfxBillboard(VfxTex tex, Vector3 pos, float size, Color tint, float rotationDeg = 0.0f);
void VfxDecal(VfxTex tex, Vector2 ground, float size, Color tint, float rotationDeg = 0.0f, float height = 0.6f);
void VfxBeam(VfxTex tex, Vector3 from, Vector3 to, float width, Color tint);

// --- Partikler (oppdateres og tegnes globalt) ---
void UpdateVfx(float deltaTime);
void DrawVfxParticles();
void ClearVfx();

// Ferdige effekter
void VfxHit(Vector2 ground, Color color);                          // Gnister når noe blir truffet
void VfxDeath(Vector2 ground, Color color);                        // Fiende dør: lysglimt og gnister
void VfxTrail(Vector3 pos, Color color, float size, float life);   // Spor bak prosjektiler
void VfxShockwave(Vector2 ground, float radius, Color color);       // Ground Slam
void VfxLightningStrike(Vector2 ground, float radius);             // Nedslag: blink, gnister, glød
void VfxExplosion(Vector2 ground, float radius);                   // Kamikaze-eksplosjon
void VfxBubble(Vector2 ground, float height, Color color);         // Giftbobler som stiger opp

#endif // VFX_HPP
