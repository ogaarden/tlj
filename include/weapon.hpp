#ifndef WEAPON_HPP
#define WEAPON_HPP

#include <raylib.h>
#include <vector>
#include <memory>
#include <string>
#include "enemy.hpp"

// Baseklassen for ALLE våpen
class Weapon {
protected:
    float fireTimer = 0.0f;
    float fireRate = 0.3f;
    int damage = 25;

public:
    std::string name;
    int level = 1;

    // Virtuell destruktør er obligatorisk når man bruker arv i C++
    virtual ~Weapon() = default;

    // "Pure virtual" funksjoner betyr at subklassene Må skrive sin egen versjon av disse!
    virtual void update(float deltaTime, Vector2 playerPos, std::vector<std::unique_ptr<Enemy>>& enemies, std::vector<XPorb>& xpOrbs, int projectileCount = 1) = 0;
    virtual void draw() const = 0;
    virtual void upgrade() {
        level++;
        damage += 10.0;
        fireRate *= 0.9f; // Standard oppgradering: litt raskere
    }
};

// Subklasse for vanlige avstandsvåpen som skyter kuler/prosjektiler
struct Projectile {
    Vector2 position;
    Vector2 direction;
    float speed;
    int damage;
    float lifetime;
};

class ProjectileWeapon : public Weapon {
private:
    float projectileSpeed;
    std::vector<Projectile> projectiles;

public:
    ProjectileWeapon(std::string weaponName, float rate, float speed, float dmg);

    // Override betyr at vi implementerer logikken spesifikt for avstandsvåpen
    void update(float deltaTime, Vector2 playerPos, std::vector<std::unique_ptr<Enemy>>& enemies, std::vector<XPorb>& xpOrbs, int projectileCount = 1) override;
    void draw() const override;
};

class MeleeWeapon : public Weapon {
private: 
    float radius;
    Vector2 lastPlayerPos; // Lagrer spillerens posisjon slik at draw() kan bruke den

public:
    MeleeWeapon(std::string weaponName, float rate, float hitRadius, float dmg);

    void update(float deltaTime, Vector2 playerPos, std::vector<std::unique_ptr<Enemy>>& enemies, std::vector<XPorb>& xpOrbs, int projectileCount = 1) override;
    void draw() const override;
};

#endif // WEAPON_HPP