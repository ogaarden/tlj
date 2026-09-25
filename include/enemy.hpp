#ifndef ENEMY_HPP
#define ENEMY_HPP

#include <raylib.h>
#include <raymath.h>
#include <cmath>

enum class EnemyType {
    FOOTMAN,
    GOON,
    LACKEY
};

class Enemy {
public:
    Vector2 position;
    float speed;
    int hp;
    int maxHp;
    int damage;
    int xpValue;
    Color orbColor;
    float orbRadius;
    Texture2D texture;

    virtual ~Enemy() = default;

    // Kun deklarasjoner – ingen { ... } her
    virtual void update(Vector2 playerPosition) = 0;
    void takeDamage(int amount);
    bool isDead() const;
    virtual void draw() const;
};

class Footman : public Enemy {
public:
    Footman(Vector2 spawnPos, Texture2D tex);
    void update(Vector2 playerPosition) override;
};

class Goon : public Enemy {
public:
    Goon(Vector2 spawnPos, Texture2D tex);
    void update(Vector2 playerPosition) override;
};

class Lackey : public Enemy {
private:
    float waveTimer = 0.0f;

public:
    Lackey(Vector2 spawnPos, Texture2D tex);
    void update(Vector2 playerPosition) override;
};

class Exploder : public Enemy {
private:
    bool hasExploded = false;
    float explosionTimer = 0.0f;
    float explosionRadius = 80.0f; // Hvor stor radius eksplosjonen har

public:
    Exploder(Vector2 spawnPos, Texture2D tex);
    void update(Vector2 playerPosition) override;
    // Vi kan også override draw hvis vi vil at den skal lyse oransje/rødt
};

struct XPorb {
    Vector2 position;
    int value;
    Color color;
    float radius;
    float lifetime;
};

#endif // ENEMY_HPP