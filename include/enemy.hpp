#ifndef ENEMY_HPP
#define ENEMY_HPP

#include <raylib.h>
#include <raymath.h>
#include <cmath>
#include <vector>
#include "sprites.hpp"

enum class EnemyType {
    FOOTMAN,
    GOON,
    LACKEY,
    EXPLODER,
    ARCHER,     // Armbrøstskytter: holder avstand og skyter piler
    HOUND,      // Kongens hund: kommer i flokk, kryper sammen og hopper på deg
    PRIEST,     // Hoffprest: holder seg bak og helbreder fiendene rundt seg
    DRUMMER,    // Trommeslager: fiendene rundt ham går mye fortere
    CANNONEER   // Kanonér: lobber granater som lander der du står (rød sirkel)
};

struct Pickup;

// --- Fiende-prosjektiler (armbrøstpiler) ---
// Samles globalt, så de lever videre selv om skytteren dør.
void SpawnEnemyShot(Vector2 from, Vector2 dir, float speed, float damage);
float UpdateEnemyShots(float deltaTime, Vector2 playerPos, float playerRadius); // Returnerer skade på spilleren
void DrawEnemyShots3D();
void DrawEnemyShotsVfx();
void ClearEnemyShots();

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
    // Kritiske treff: settes fra spilleren hver frame. Gjelder alle direkte treff (ikke DoT).
    static inline float critChance = 0.05f;
    static inline float critMultiplier = 2.0f;

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
    // Papirfiguren: snuing (-1..1), hvor mye den går (0..1) og hvor den stod forrige frame
    float spriteTurn = 0.0f;
    float spriteStride = 0.0f;
    Vector2 spriteLastPos = { 1e9f, 1e9f };

    void makeElite();
    float walkCycle() const;

    // --- Statuseffekter fra abilities (brukes i spill-løkka etter update()) ---
    float slowTimer = 0.0f;           // Frost: fienden går tregere så lenge denne er > 0
    float slowAmount = 0.0f;          // 0.5 = halv fart
    Vector2 knockVelocity = { 0, 0 }; // Dytt (bjeller, bumerang), dør ut raskt
    float knockbackScale = 1.0f;      // Store fiender dyttes mindre (bossen nesten ikke)
    float damageTakenMult = 1.0f;     // < 1 = rustning (kongen i fase 2)
    bool chestCarrier = false;        // Slipper alltid en skattekiste (horde-kaptein)
    bool miniboss = false;            // Miniboss: slipper Kongens septer (se miniboss.hpp)
    float hasteTimer = 0.0f;          // Trommeslagerens takt: går 40 % fortere så lenge denne er > 0
    // Aura (prest og trommeslager): spill-løkka bruker den på fiendene rundt
    enum class Aura { NONE, HEAL, HASTE };
    virtual Aura aura() const { return Aura::NONE; }
    virtual float auraRadius() const { return 0.0f; }
    bool auraPulse = false;           // Presten: satt når den skal helbrede (spill-løkka nullstiller)
    const char* title = "";           // Navn som vises over HP-baren (minibosser)
    void applySlow(float amount, float duration);
    void knockBack(Vector2 direction, float strength);
    // Kalles etter update(): demper bevegelsen fra frost og legger på dytt
    void applyStatusMovement(Vector2 positionBeforeUpdate, float deltaTime);

    // Kister fra elites har en felles nedkjøling, så de ikke regner ned sent i runden.
    // luck (Firkløver) gjør både kister og sjeldne drops vanligere.
    static inline float chestCooldown = 0.0f;
    static inline float luck = 1.0f; // Fase for gangeanimasjonen (forskjellig for hver fiende)

    // Tegning i to lag (se render3d.hpp):
    //  draw()   – på gulvet: skygge, varsel-linjer osv. (2D-koordinater)
    //  draw3D() – selve figuren i 3D
    virtual void draw() const;
    virtual void draw3D() const;
    virtual void drawVfx() const;                       // Glød/aura i det additive VFX-passet
    virtual float modelHeight() const { return 36.0f * modelScale; } // Brukes for å plassere HP-bar over hodet

    // Tegnet figur (sprites.hpp). COUNT = ingen, bruk 3D-modellen.
    virtual SpriteId spriteId() const { return SpriteId::COUNT; }
    virtual Color spriteTint() const { return WHITE; }
};

class Footman : public Enemy {
public:
    Footman(Vector2 spawnPos, Texture2D tex);
    void update(Vector2 playerPosition) override;
    void draw3D() const override;
    SpriteId spriteId() const override { return SpriteId::FOOTMAN; }
};

class Goon : public Enemy {
public:
    Goon(Vector2 spawnPos, Texture2D tex);
    void update(Vector2 playerPosition) override;
    void draw3D() const override;
    SpriteId spriteId() const override { return SpriteId::GOON; }
};

class Lackey : public Enemy {
private:
    float waveTimer = 0.0f;

public:
    Lackey(Vector2 spawnPos, Texture2D tex);
    void update(Vector2 playerPosition) override;
    void draw3D() const override;
    SpriteId spriteId() const override { return SpriteId::LACKEY; }
};

// Boss som venter i boss-arenaen når echelon-timeren er ferdig.
// Jager spilleren, stopper opp for å varsle, og dasher så mot spilleren.
class Boss : public Enemy {
private:
    // Fase 1 (TRONE): sitter på tronen og skyter mønstre, sender dekreter og kaller inn vakter.
    // Under 55 % HP: HOPPER ned (LEAP) og går over til fase 2 (JAKT): jager, stormer og tramper.
    // Under 25 % HP: rasende – raskere, og skyter spiraler mens han jager.
    enum class Mode { THRONE, LEAP, HUNT };
    enum class Phase { CHASE, WINDUP, DASH, STOMP };
    Mode mode = Mode::THRONE;
    Phase phase = Phase::CHASE;
    float phaseTimer = 0.0f;
    Vector2 dashDirection = { 0, 0 };
    Vector2 lastPlayerPos = { 0, 0 };
    float summonTimer = 0.0f;

    // Tronemønstre
    int throneAttack = 0;       // Hvilket mønster som går nå
    float attackTimer = 1.5f;   // Pause før neste mønster
    float patternTime = 0.0f;   // Hvor lenge mønsteret har gått
    float shotTimer = 0.0f;
    float spiralAngle = 0.0f;
    int volleysLeft = 0;
    int dashesLeft = 0;
    float chaseShotTimer = 0.0f;
    struct Decree { Vector2 pos; float t; };     // Rød sirkel som eksploderer når t når 1
    std::vector<Decree> decrees;
    Vector2 leapFrom = { 0, 0 }, leapTo = { 0, 0 };

    void announce(const char* text);
    void shootRing(int count, float speed, float offset, float dmg);
    void shootFan(Vector2 dir, int count, float spreadDeg, float speed, float dmg);

public:
    Vector2 thronePos = { 0, 0 };   // Settes av spillet: hvor tronen står
    bool enraged = false;
    float enragedAt = -100.0f;      // Når siste store fasebytte skjedde (for varselteksten)
    const char* announcement = "";  // Teksten som vises ("KONGEN REISER SEG!" osv.)
    int summonsRequested = 0;       // Hvor mange vakter spillet skal kalle inn rundt kongen
    bool onThrone() const { return mode == Mode::THRONE; }

    Boss(Vector2 spawnPos, Texture2D tex);
    void update(Vector2 playerPosition) override;
    void draw() const override;
    void draw3D() const override;
    void drawVfx() const override;
    float modelHeight() const override { return 110.0f; }
    SpriteId spriteId() const override { return SpriteId::KING; }
    Color spriteTint() const override { return enraged ? Color{ 255, 150, 140, 255 } : WHITE; }
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
    SpriteId spriteId() const override { return SpriteId::EXPLODER; }
    Color spriteTint() const override;
};

// Armbrøstskytter: løper til passe avstand, sikter (rød linje på gulvet) og skyter
class Archer : public Enemy {
private:
    float shootTimer = 1.0f;
    float aimTimer = 0.0f;   // > 0 mens den sikter
    Vector2 aimDir = { 0, 1 };

public:
    Archer(Vector2 spawnPos, Texture2D tex);
    void update(Vector2 playerPosition) override;
    void draw() const override;
    void draw3D() const override;
    void drawVfx() const override;
    int contactDamage() const override { return damage / 2; }
    SpriteId spriteId() const override { return SpriteId::ARCHER; }
};

// Kongens hund: løper i flokk, kryper sammen (varsel) og kaster seg mot deg
class Hound : public Enemy {
    enum class State { RUN, CROUCH, LUNGE };
    State state = State::RUN;
    float timer = 0.0f;
    float lungeCooldown = 1.0f;
    Vector2 lungeDir = { 0, 1 };
public:
    Hound(Vector2 spawnPos, Texture2D tex);
    void update(Vector2 playerPosition) override;
    void draw() const override;
    void draw3D() const override;
    SpriteId spriteId() const override { return SpriteId::HOUND; }
    Color spriteTint() const override;
};

// Hoffprest: holder avstand og helbreder alle fiender i nærheten hvert 1.6 sek. Drep ham først!
class Priest : public Enemy {
    float pulseTimer = 1.6f;
public:
    Priest(Vector2 spawnPos, Texture2D tex);
    void update(Vector2 playerPosition) override;
    void draw3D() const override;
    float modelHeight() const override { return 46.0f * modelScale; }
    void drawVfx() const override;
    Aura aura() const override { return Aura::HEAL; }
    float auraRadius() const override { return 190.0f; }
    SpriteId spriteId() const override { return SpriteId::PRIEST; }
};

// Trommeslager: marsjerer med de andre, og alle rundt ham går 40 % fortere
class Drummer : public Enemy {
public:
    Drummer(Vector2 spawnPos, Texture2D tex);
    void update(Vector2 playerPosition) override;
    void draw3D() const override;
    float modelHeight() const override { return 52.0f * modelScale; }
    void drawVfx() const override;
    Aura aura() const override { return Aura::HASTE; }
    float auraRadius() const override { return 210.0f; }
    SpriteId spriteId() const override { return SpriteId::DRUMMER; }
};

// Kanonér: holder god avstand og lobber granater der du står. Rød sirkel = flytt deg!
class Cannoneer : public Enemy {
    float shootTimer = 2.5f;
    float windup = 0.0f;
public:
    Cannoneer(Vector2 spawnPos, Texture2D tex);
    void update(Vector2 playerPosition) override;
    void draw3D() const override;
    float modelHeight() const override { return 44.0f * modelScale; }
    void drawVfx() const override;
    int contactDamage() const override { return damage / 3; }
    SpriteId spriteId() const override { return SpriteId::CANNONEER; }
};

enum class PickupType {
    XP,
    COIN,
    CHEST,  // Skattekiste: gir et item (eller en item-kombinasjon)
    SCEPTER,// Kongens septer fra minibosser: gir en ability sin septer-oppgradering
    VACUUM, // Sjelden magnet: suger inn all XP på bakken
    FOOD    // Sjeldent kyllinglår: gir liv
};

// Ting som ligger på bakken og kan plukkes opp (XP-orbs og gullmynter)
struct Pickup {
    Vector2 position;
    int value;
    Color color;
    float radius;
    float lifetime;
    PickupType type = PickupType::XP;
    float age = 0.0f;   // Sekunder siden den ble sluppet (liten "hopp"-animasjon)
    float pull = 0.0f;  // Hvor lenge den har blitt trukket mot spilleren (akselererer)
};

// Farge og størrelse på XP-krystaller etter hvor mye de er verdt
Color XpTierColor(int value);
float XpTierRadius(int value);

#endif // ENEMY_HPP