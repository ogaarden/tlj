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
    bool evolved = false; // Evolusjon: ability på maks level + riktig item + en skattekiste
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
    // drawVfx() – glød, lyn og spor. Tegnes additivt etter alt annet (se vfx.hpp)
    virtual void drawVfx() const {}

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
    void drawVfx() const override;
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
    void drawVfx() const override;
};

// --- Aura som gjør skade HVER FRAME (Rot) ---
// stats.damage er skade per sekund, og akkumuleres slik at den blir lik uansett FPS.
class RotWeapon : public Weapon {
private:
    float damageAccumulator = 0.0f;
    float pulseTimer = 0.0f;
    float bubbleTimer = 0.0f;
    Vector2 lastPlayerPos = { 0, 0 };

public:
    void tick(float deltaTime, Vector2 playerPos, std::vector<std::unique_ptr<Enemy>>& enemies, std::vector<Pickup>& pickups) override;
    void draw() const override;
    void draw3D() const override;
    void drawVfx() const override;
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
    void drawVfx() const override;
    float cooldownProgress() const override { return 1.0f; }
};

// --- Kremkaker som lobbes i en bue og spruter (Pierrot) ---
struct Pie {
    Vector2 from, to;   // Fra spilleren til landingsstedet
    float t;            // 0 -> 1 underveis i lufta
    float flightTime;
    int damage;
    float spin;
};
struct CreamSplat {
    Vector2 position;
    float radius;
    float timer;        // Hvor lenge kremflekken ligger igjen
    float tickTimer;
};

class PieWeapon : public Weapon {
private:
    std::vector<Pie> pies;
    std::vector<CreamSplat> splats;

public:
    void tick(float deltaTime, Vector2 playerPos, std::vector<std::unique_ptr<Enemy>>& enemies, std::vector<Pickup>& pickups) override;
    void draw() const override;
    void draw3D() const override;
    void drawVfx() const override;
};

// --- Lyn som slår ned på tilfeldige fiender i nærheten (Lightning) ---
// Visuell lynstrek: enten fra himmelen og ned (nedslag), eller en bue mellom to fiender (kjede)
struct LightningBolt {
    Vector2 target;
    float radius;                // Treffområde på gulvet (0 for kjede-buer)
    float timer;
    float maxTimer;
    std::vector<Vector3> points; // Hakkete strek i 3D
};

// Et kjede-hopp som venter på å bli utført (litt forsinkelse per hopp ser kulere ut)
struct ChainJump {
    Vector2 from;             // Hvor buen starter
    int targetId;             // Fienden den skal treffe
    float damage;
    int jumpsLeft;            // Hvor mange hopp som er igjen ETTER dette
    float delay;              // Tid til hoppet utføres
    std::vector<int> hitIds;  // Fiender som allerede er truffet av denne kjeden
};

class LightningWeapon : public Weapon {
private:
    std::vector<LightningBolt> bolts;
    std::vector<ChainJump> pendingJumps;

    // Starter et kjede-hopp fra `from` til nærmeste fiende som ikke er truffet
    void queueNextJump(Vector2 from, float damage, int jumpsLeft, const std::vector<int>& hitIds,
                       const std::vector<std::unique_ptr<Enemy>>& enemies);

public:
    void tick(float deltaTime, Vector2 playerPos, std::vector<std::unique_ptr<Enemy>>& enemies, std::vector<Pickup>& pickups) override;
    void draw() const override;
    void draw3D() const override;
    void drawVfx() const override;
};

#endif // WEAPON_HPP
