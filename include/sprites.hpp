#ifndef SPRITES_HPP
#define SPRITES_HPP

#include <raylib.h>

// =====================================================================
// PIKSELFIGURER (sprites)
// Figurene ble tegnet med Higgsfield og gjort om til pikselkunst (assets/sprites/*.png,
// ca. 1.5 verdensenheter per piksel). De vises som flate figurer i 3D-verdenen:
// vender alltid mot kameraet, speilvendes etter hvilken vei de går, og hopper/vugger.
// Man kan bytte tilbake til 3D-modellene i innstillingene.
// =====================================================================

enum class SpriteId {
    JESTER, WESTER, GEEK, PIERROT,                       // Spillbare klovner
    FOOTMAN, GOON, LACKEY, EXPLODER, ARCHER,             // Vanlige fiender
    EXECUTIONER, MAGUS, IRON_KNIGHT, KING,               // Minibosser og kongen
    COUNT
};

void InitSprites();
void UnloadSprites();
bool HasSprite(SpriteId id);           // Finnes bildet? (ellers brukes 3D-modellen)
Texture2D SpriteTexture(SpriteId id);
float SpriteBaseHeight(SpriteId id);   // Høyde i verden (før modelScale)
Rectangle SpriteHeadRect(SpriteId id); // Firkant rundt hodet i bildet (til HUD-portrettet)

// Av/på fra innstillingene
bool SpritesEnabled();
void SetSpritesEnabled(bool enabled);

// Samler opp figurer i løpet av 3D-passet og tegner dem sortert bakfra og frem.
struct SpriteDraw {
    SpriteId id;
    Vector2 feet;          // Posisjon på gulvet
    float height;          // Høyde i verden
    bool flip;             // Speilvend (figurene ser mot høyre i bildet)
    float flash = 0.0f;    // 0..1 hvitt treffglimt
    float squash = 0.0f;   // -1..1: + = høy og smal, - = lav og bred
    float lean = 0.0f;     // Vipping i grader (gange)
    float hop = 0.0f;      // Hvor høyt over gulvet (hopp)
    float sink = 0.0f;     // Hvor langt under gulvet (stiger opp når de spawner)
    Color tint = WHITE;
};
void QueueSprite(const SpriteDraw& s);
void DrawQueuedSprites(const Camera3D& camera); // Kalles inne i BeginMode3D

// Er figuren vendt mot venstre på skjermen? (for speilvending)
bool FacesLeftOnScreen(const Camera3D& camera, Vector2 facing);

#endif // SPRITES_HPP
