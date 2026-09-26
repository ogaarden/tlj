#ifndef MINIBOSS_HPP
#define MINIBOSS_HPP

#include <memory>
#include "enemy.hpp"

// =====================================================================
// MINIBOSSER
// Ved 3, 6 og 9 minutter dukker en innkallingssirkel opp et sted i slottet.
// Går du inn i sirkelen, stiger en tilfeldig valgt miniboss opp av gulvet.
// De er svært sterke, og slipper Kongens septer (septer-oppgradering for en ability).
// =====================================================================

enum class MinibossKind {
    EXECUTIONER, // Bøddelen: treg, men løfter øksa og knuser bakken rundt seg
    MAGUS,       // Hoffmagikeren: holder avstand, skyter ringer av magi og teleporterer
    IRON_KNIGHT, // Jernridderen: stormer mot deg i full fart
    COUNT
};

// index = hvilken miniboss i runden (0, 1, 2) – de blir sterkere for hver
std::unique_ptr<Enemy> CreateMiniboss(MinibossKind kind, int index, Vector2 position, Texture2D texture,
                                      float hpMult, float damageMult);

class Executioner : public Goon {
    enum class Phase { CHASE, WINDUP, RECOVER };
    Phase phase = Phase::CHASE;
    float timer = 2.5f;
public:
    using Goon::Goon;
    SpriteId spriteId() const override { return SpriteId::EXECUTIONER; }
    void update(Vector2 playerPosition) override;
    void draw() const override;
    void drawVfx() const override;
};

class Magus : public Archer {
    float volleyTimer = 2.0f;
    float teleportTimer = 6.0f;
    float volleyOffset = 0.0f;
public:
    using Archer::Archer;
    SpriteId spriteId() const override { return SpriteId::MAGUS; }
    void update(Vector2 playerPosition) override;
    void draw() const override;
    void drawVfx() const override;
};

class IronKnight : public Footman {
    enum class Phase { CHASE, WINDUP, DASH, REST };
    Phase phase = Phase::CHASE;
    float timer = 2.0f;
    Vector2 dashDir = { 0, 1 };
public:
    using Footman::Footman;
    SpriteId spriteId() const override { return SpriteId::IRON_KNIGHT; }
    void update(Vector2 playerPosition) override;
    void draw() const override;
    void drawVfx() const override;
};

#endif // MINIBOSS_HPP
