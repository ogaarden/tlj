#ifndef SPAWNER_HPP
#define SPAWNER_HPP

#include <vector>
#include <memory>
#include <raylib.h>
#include "enemy.hpp"
#include "echelon.hpp"

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

private:
    int currentWaveIndex = -1;
    EchelonModifiers modifiers;
    std::vector<EnemyType> spawnQueue;

    int hordeSpawnedWave = -1;

    void spawnWave(int waveIndex);
    void spawnEnemy(EnemyType type, Vector2 spawnPos, std::vector<std::unique_ptr<Enemy>>& enemies, Texture2D enemyTexture);

public:
    // Nullstill for en ny runde med echelon-effektene som gjelder
    void reset(const EchelonModifiers& echelonModifiers);

    void update(float deltaTime, Vector2 playerPos, std::vector<std::unique_ptr<Enemy>>& enemies, Texture2D enemyTexture);
};

#endif