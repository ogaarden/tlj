#ifndef WEAPON_HPP
#define WEAPON_HPP

#include <raylib.h>
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include "enemy.hpp"
#include "ability_types.hpp"

// Bonuser fra spilleren som gjelder alle abilities
struct CombatModifiers {
    int extraProjectiles = 0;  // Fra shop ("Projectile count")
    float damageMult = 1.0f;   // Spillerens spellAmp * shop "Might"
    float cooldownMult = 1.0f; // Shop "Haste" (0.9 = 10% kortere cooldown)
    float areaMult = 1.0f;     // Shop "Area" (radius og treffområde)
};

// Baseklassen for ALLE abilities/våpen.
// Alle tall ligger i `stats`, som settes og oppgraderes fra level-tabellene i abilities.cpp.
class Weapon {
protected:
    float fireTimer = 0.0f;
    CombatModifiers mods; // Settes hver frame av update()

    // Stats med spillerens bonuser lagt på. Bruk disse i tick() i stedet for stats direkte.
    int scaledDamage() const { return (int)(stats.damage * mods.damageMult); }
    float cooldown() const { return stats.cooldown * mods.cooldownMult; }
    float radius() const { return stats.radius * mods.areaMult; }
    float area() const { return stats.area * mods.areaMult; }

    // Hver ability implementerer sin egen logikk her
    virtual void tick(float deltaTime, Vector2 playerPos, std::vector<std::unique_ptr<Enemy>>& enemies, std::vector<Pickup>& pickups) = 0;

public:
    AbilityId id = AbilityId::TREFORK;
    std::string name;
    Color color = WHITE;
    int level = 1;
    AbilityStats stats;

    // Virtuell destruktør er obligatorisk når man bruker arv i C++
    virtual ~Weapon() = default;

    void update(float deltaTime, Vector2 playerPos, std::vector<std::unique_ptr<Enemy>>& enemies, std::vector<Pickup>& pickups, const CombatModifiers& modifiers) {
        mods = modifiers;
        tick(deltaTime, playerPos, enemies, pickups);
    }

    // Tegning i to lag (se render3d.hpp):
    //  draw()   – på gulvet: skygger, AOE-ringer osv. (2D-koordinater)
    //  draw3D() – prosjektiler, blader, lyn osv. i 3D
    // "Pure virtual" betyr at subklassene MÅ skrive sin egen versjon
    virtual void draw() const = 0;
    virtual void draw3D() const {}

    // 0.0 = nettopp brukt, 1.0 = klar. Brukes av HUD-en.
    virtual float cooldownProgress() const {
        if (cooldown() <= 0.0f) return 1.0f;
        float p = fireTimer / cooldown();
        return p > 1.0f ? 1.0f : p;
    }
};

// --- Rette prosjektiler (Trefork, Dagger) ---
struct Projectile {
    Vector2 position;
    Vector2 direction;
    float speed;
    int damage;
    float lifetime;
    int pierceLeft;
    std::vector<int> hitEnemyIds;
};

class ProjectileWeapon : public Weapon {
private:
    bool spread; // true = vifte mot nærmeste fiende (Trefork), false = én kule per nærmeste fiende (Dagger)
    std::vector<Projectile> projectiles;

public:
    explicit ProjectileWeapon(bool fireInSpread);

    void tick(float deltaTime, Vector2 playerPos, std::vector<std::unique_ptr<Enemy>>& enemies, std::vector<Pickup>& pickups) override;
    void draw() const override;
    void draw3D() const override;
};

// --- AOE-slag rundt spilleren (Ground Slam) ---
class MeleeWeapon : public Weapon {
private:
    Vector2 lastPlayerPos = { 0, 0 }; // Lagrer spillerens posisjon slik at draw() kan bruke den
    float effectTimer = 0.0f;

public:
    void tick(float deltaTime, Vector2 playerPos, std::vector<std::unique_ptr<Enemy>>& enemies, std::vector<Pickup>& pickups) override;
    void draw() const override;
};

// --- Prosjektiler som spretter videre ved treff (Ricochet, Magic Missile) ---
struct BouncingProjectile {
    Vector2 position;
    Vector2 direction;
    float speed;
    float damage;
    float lifetime;
    int bouncesLeft;
    float bounceRange;
    int targetId;                 // Fienden vi styrer mot (kun homing)
    std::vector<int> hitEnemyIds; // Så den ikke spretter tilbake til samme fiende
};

class BouncingProjectileWeapon : public Weapon {
private:
    bool homing; // true = svinger mot målet (Magic Missile), false = flyr rett (Ricochet)
    std::vector<BouncingProjectile> projectiles;

public:
    explicit BouncingProjectileWeapon(bool isHoming);

    void tick(float deltaTime, Vector2 playerPos, std::vector<std::unique_ptr<Enemy>>& enemies, std::vector<Pickup>& pickups) override;
    void draw() const override;
    void draw3D() const override;
};

// --- Aura som gjør skade HVER FRAME (Rot) ---
// stats.damage er skade per sekund, og akkumuleres slik at den blir lik uansett FPS.
class RotWeapon : public Weapon {
private:
    float damageAccumulator = 0.0f;
    float pulseTimer = 0.0f;
    Vector2 lastPlayerPos = { 0, 0 };

public:
    void tick(float deltaTime, Vector2 playerPos, std::vector<std::unique_ptr<Enemy>>& enemies, std::vector<Pickup>& pickups) override;
    void draw() const override;
    void draw3D() const override;
    float cooldownProgress() const override { return 1.0f; }
};

// --- Blader som sirkler rundt spilleren (Orbiting Blades) ---
class OrbitWeapon : public Weapon {
private:
    float angle = 0.0f;
    float time = 0.0f;
    int bladeCount = 0;
    Vector2 lastPlayerPos = { 0, 0 };
    std::unordered_map<int, float> lastHitTime; // Fiende-id -> tidspunkt for siste treff

    Vector2 bladePosition(int index) const;

public:
    void tick(float deltaTime, Vector2 playerPos, std::vector<std::unique_ptr<Enemy>>& enemies, std::vector<Pickup>& pickups) override;
    void draw() const override;
    void draw3D() const override;
    float cooldownProgress() const override { return 1.0f; }
};

// --- Lyn som slår ned på tilfeldige fiender i nærheten (Lightning) ---
struct LightningBolt {
    Vector2 target;
    float radius;
    float timer;
    std::vector<Vector3> points; // Hakkete strek fra himmelen og ned til bakken (3D)
};

class LightningWeapon : public Weapon {
private:
    std::vector<LightningBolt> bolts;

public:
    void tick(float deltaTime, Vector2 playerPos, std::vector<std::unique_ptr<Enemy>>& enemies, std::vector<Pickup>& pickups) override;
    void draw() const override;
    void draw3D() const override;
};

#endif // WEAPON_HPP
