#ifndef ENEMY_HPP
#define ENEMY_HPP

#include <raylib.h>
#include <raymath.h>
#include <cmath>
#include <vector>

enum class EnemyType {
    FOOTMAN,
    GOON,
    LACKEY,
    EXPLODER
};

struct Pickup;

class Enemy {
private:
    static inline int nextId = 0;

    // Damage-over-time (f.eks. Rot) samles opp og vises som ett tall
    // med jevne mellomrom, i stedet for ett tall per frame.
    int pendingDotDamage = 0;
    double lastDotPopupTime = 0.0;
    Color dotColor = WHITE;

public:
    static inline int killCount = 0; // Antall drepte fiender denne runden

    const int id = nextId++; // Unik ID, brukes f.eks. av ricochet for å huske hvem som er truffet
    Vector2 position;
    float speed;
    int hp;
    int maxHp;
    int damage;
    int xpValue;
    int goldValue = 1;        // Hvor mye gull en mynt fra denne fienden er verdt
    float goldChance = 0.0f;  // Sjanse (0-1) for at fienden dropper en mynt
    Color orbColor;
    float orbRadius;
    float hitRadius = 16.0f;  // Kollisjonsradius for treff og kontaktskade
    Texture2D texture;

    virtual ~Enemy() = default;

    // Kun deklarasjoner – ingen { ... } her
    virtual void update(Vector2 playerPosition) = 0;
    void takeDamage(int amount, Color numberColor = WHITE, bool isDamageOverTime = false);
    bool isDead() const;
    void dropLoot(std::vector<Pickup>& pickups) const; // XP + evt. gull når fienden dør
    virtual void onDeath() {}                          // Kalles når fienden dør (uansett årsak)
    virtual int contactDamage() const { return damage; } // Skade ved berøring

    // Echelon-effekter som gjør fienden sterkere (kalles når den spawner)
    virtual void applyEchelonModifiers(float hpMult, float damageMult, float speedMult);
    Vector2 facing = { 0.0f, 1.0f }; // Retningen fienden går/ser (brukes av 3D-modellen)

    // --- Utseende ---
    float hitFlash = 0.0f;   // 1 -> 0 etter et treff (fienden blinker hvitt)
    float age = 0.0f;        // Sekunder siden den spawnet (brukes til å stige opp av gulvet)
    bool elite = false;      // Elite: større, mye mer HP og loot, gyllen aura
    float modelScale = 1.0f; // Hele 3D-modellen skaleres rundt føttene

    void makeElite();
    float walkCycle() const; // Fase for gangeanimasjonen (forskjellig for hver fiende)

    // Tegning i to lag (se render3d.hpp):
    //  draw()   – på gulvet: skygge, varsel-linjer osv. (2D-koordinater)
    //  draw3D() – selve figuren i 3D
    virtual void draw() const;
    virtual void draw3D() const;
    virtual void drawVfx() const;                       // Glød/aura i det additive VFX-passet
    virtual float modelHeight() const { return 36.0f * modelScale; } // Brukes for å plassere HP-bar over hodet
};

class Footman : public Enemy {
public:
    Footman(Vector2 spawnPos, Texture2D tex);
    void update(Vector2 playerPosition) override;
    void draw3D() const override;
};

class Goon : public Enemy {
public:
    Goon(Vector2 spawnPos, Texture2D tex);
    void update(Vector2 playerPosition) override;
    void draw3D() const override;
};

class Lackey : public Enemy {
private:
    float waveTimer = 0.0f;

public:
    Lackey(Vector2 spawnPos, Texture2D tex);
    void update(Vector2 playerPosition) override;
    void draw3D() const override;
};

// Boss som venter i boss-arenaen når echelon-timeren er ferdig.
// Jager spilleren, stopper opp for å varsle, og dasher så mot spilleren.
class Boss : public Enemy {
private:
    enum class Phase { CHASE, WINDUP, DASH };
    Phase phase = Phase::CHASE;
    float phaseTimer = 0.0f;
    Vector2 dashDirection = { 0, 0 };
    Vector2 lastPlayerPos = { 0, 0 };
    float summonTimer = 0.0f;

public:
    // Under 50 % HP blir kongen rasende: raskere dash, kortere pauser og hjelpere
    bool enraged = false;
    float enragedAt = -100.0f;  // Når raseriet startet (for varselteksten)
    int summonsRequested = 0;   // Hvor mange lakeier spillet skal kalle inn rundt kongen

    Boss(Vector2 spawnPos, Texture2D tex);
    void update(Vector2 playerPosition) override;
    void draw() const override;
    void draw3D() const override;
    void drawVfx() const override;
    float modelHeight() const override { return 110.0f; }
};

// Kamikaze-fiende (echelon 2+): løper mot spilleren, stopper opp og blinker
// når den er nær, og eksploderer. Den eksploderer OGSÅ hvis du dreper den,
// så pass på å ikke drepe den rett ved siden av deg!
class Exploder : public Enemy {
private:
    bool fuseLit = false;
    float fuseTimer = 0.0f;
    float explosionRadius = 80.0f;
    float explosionDamage = 25.0f;

public:
    Exploder(Vector2 spawnPos, Texture2D tex);
    void update(Vector2 playerPosition) override;
    void draw() const override;
    void draw3D() const override;
    void drawVfx() const override;
    void onDeath() override;
    int contactDamage() const override { return 0; } // Skader bare med eksplosjonen
    void applyEchelonModifiers(float hpMult, float damageMult, float speedMult) override;
};

enum class PickupType {
    XP,
    COIN,
    CHEST // Skattekiste fra elite-fiender: gir et gratis oppgraderingsvalg
};

// Ting som ligger på bakken og kan plukkes opp (XP-orbs og gullmynter)
struct Pickup {
    Vector2 position;
    int value;
    Color color;
    float radius;
    float lifetime;
    PickupType type = PickupType::XP;
};

#endif // ENEMY_HPP