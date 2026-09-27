#ifndef SPAWNER_HPP
#define SPAWNER_HPP

#include <vector>
#include <memory>
#include <raylib.h>
#include "enemy.hpp"
#include "echelon.hpp"

// Kongelige dekreter: hvert 2. minutt fra 2:30 bestemmer kongen noe nytt i 30 sekunder.
// Alle gjør runden farligere, men de fleste gir også noe tilbake.
enum class Decree {
    NONE,
    FEAST,       // Kongens fest: fiendene går 40 % fortere, men gir dobbelt XP
    BLOOD_MOON,  // Blodmåne: nye fiender slår 50 % hardere, men slipper mye mer gull
    HUNT,        // Den store jakten: tre hundeflokker slippes løs rundt deg
    MUSTER,      // Mobilisering: dobbelt så mange fiender, +50 % XP
    DARKNESS,    // Mørklegging: lysene slukkes, du ser bare rett rundt deg, men +50 % XP
    GOLD_RAIN,   // Gullregn: mynter regner ned rundt deg, men nye fiender har +40 % HP
    COUNT
};
const char* DecreeTitle(Decree d);
const char* DecreeText(Decree d);

struct EnemyGroup {
    EnemyType type;
    int count;
};

struct WaveDefinition {
    int waveNumber;
    std::vector<EnemyGroup> groups;
};

class WaveSpawner {
public:
    float gameTime = 0.0f;
    float spawnTimer = 0.0f;
    float spawnInterval = 1.0f;
    float lastHordeTime = -100.0f; // Når siste horde kom (HUD-en viser et varsel)

    // Dekretet som gjelder nå (NONE mellom dekretene), og når det startet
    static constexpr float DECREE_FIRST = 150.0f;
    static constexpr float DECREE_EVERY = 120.0f;
    static constexpr float DECREE_LENGTH = 30.0f;
    Decree decree = Decree::NONE;
    float decreeStart = -100.0f;
    // Skattmesteren kommer kl. 3:15 og deretter hvert 3. minutt
    static constexpr float TREASURER_FIRST = 195.0f;
    static constexpr float TREASURER_EVERY = 180.0f;
    float lastTreasurerTime = -100.0f;   // Når siste skattmester kom (HUD-en viser et varsel)
    int treasurersSpawned = 0;
    float decreeLeft() const { return decree == Decree::NONE ? 0.0f : decreeStart + DECREE_LENGTH - gameTime; }

private:
    int currentWaveIndex = -1;
    EchelonModifiers modifiers;
    std::vector<EnemyType> spawnQueue;

    int hordeSpawnedWave = -1;
    int decreesStarted = 0;
    Decree lastDecree = Decree::NONE;

    void spawnWave(int waveIndex);

public:
    // Lager én fiende med echelon- og tidsskalering (brukes også når kongen kaller inn hjelp)
    void spawnEnemy(EnemyType type, Vector2 spawnPos, std::vector<std::unique_ptr<Enemy>>& enemies, Texture2D enemyTexture, float xpMult = 1.0f);

public:
    // Nullstill for en ny runde med echelon-effektene som gjelder
    void reset(const EchelonModifiers& echelonModifiers);

    void update(float deltaTime, Vector2 playerPos, std::vector<std::unique_ptr<Enemy>>& enemies, Texture2D enemyTexture);
};

#endif