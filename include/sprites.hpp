#ifndef SPRITES_HPP
#define SPRITES_HPP

#include <raylib.h>

// =====================================================================
// PAPIRFIGURER (sprites)
// Alle figurene er tegnet med Higgsfield i samme stil: malte papirteater-dukker med mørk
// blekkontur, flate farger og én felles palett (kobolt, krem, karmosin, gull) som passer
// med porselensgulvet. Hver figur har en kremhvit papirkant (assets/sprites/*.png,
// 3 piksler per verdensenhet). De vises som flate figurer i 3D-verdenen:
// vender alltid mot kameraet, speilvendes etter hvilken vei de går, og hopper/vugger.
// Man kan bytte tilbake til 3D-modellene i innstillingene.
// =====================================================================

enum class SpriteId {
    JESTER, WESTER, GEEK, PIERROT,                       // Spillbare klovner
    FOOTMAN, GOON, LACKEY, EXPLODER, ARCHER,             // Vanlige fiender
    HOUND, PRIEST, DRUMMER, CANNONEER,                   // Kongens hunder og tjenere (fra 2-4 min)
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
    float widthScale = 1.0f; // 0..1: figuren snur seg (smalner inn og vider ut igjen, som en papirfigur)
    float walk = 0.0f;     // Fase i gangen (radianer): styrer hvilket bein som er fremme
    float stride = 0.0f;   // 0 = står stille, 1 = full gange
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
// Myk snuing: `turn` går mot -1 (venstre) eller 1 (høyre) når figuren bytter retning.
// Gir flip og widthScale til SpriteDraw. Litt dødsone, så figurer som går rett opp/ned ikke flakker.
void UpdateSpriteTurn(float& turn, const Camera3D& camera, Vector2 facing, float deltaTime);
inline bool TurnFlip(float turn) { return turn < 0.0f; }
inline float TurnWidth(float turn) { return turn < 0.0f ? (-turn > 0.12f ? -turn : 0.12f) : (turn > 0.12f ? turn : 0.12f); }

#endif // SPRITES_HPP
