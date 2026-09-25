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

// Boss som venter i boss-arenaen når echelon-timeren er ferdig.
// Jager spilleren, stopper opp for å varsle, og dasher så mot spilleren.
class Boss : public Enemy {
private:
    enum class Phase { CHASE, WINDUP, DASH };
    Phase phase = Phase::CHASE;
    float phaseTimer = 0.0f;
    Vector2 dashDirection = { 0, 0 };
    Vector2 lastPlayerPos = { 0, 0 };

public:
    Boss(Vector2 spawnPos, Texture2D tex);
    void update(Vector2 playerPosition) override;
    void draw() const override;
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
    void onDeath() override;
    int contactDamage() const override { return 0; } // Skader bare med eksplosjonen
    void applyEchelonModifiers(float hpMult, float damageMult, float speedMult) override;
};

enum class PickupType {
    XP,
    COIN
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