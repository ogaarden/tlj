#ifndef SPAWNER_HPP
#define SPAWNER_HPP

#include <vector>
#include <memory>
#include <raylib.h>
#include "enemy.hpp"

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

private:
    int currentWaveIndex = -1;
    std::vector<EnemyType> spawnQueue;

    void spawnWave(int waveIndex);

public:
    void update(float deltaTime, Vector2 playerPos, std::vector<std::unique_ptr<Enemy>>& enemies, Texture2D enemyTexture);
};

#endif