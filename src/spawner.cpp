#include "spawner.hpp"
#include <cmath>
#include <algorithm>
#include <random>

const std::vector<WaveDefinition> GAME_WAVES = {
    // Wave 1: 6 Footmen
    {
        1,
        {
            { EnemyType::FOOTMAN, 70 }
        }
    },
    // Wave 2: 10 Footmen, 4 Lackeys
    {
        2,
        {
            { EnemyType::FOOTMAN, 35 },
            { EnemyType::LACKEY, 35 }
        }
    },
    // Wave 3: 12 Lackeys, 3 Goons
    {
        3,
        {
            { EnemyType::LACKEY, 40 },
            { EnemyType::GOON, 25 }
        }
    }
};

void WaveSpawner::spawnWave(int waveIndex) {
    currentWaveIndex = waveIndex;
    spawnQueue.clear();

    int safeIndex = std::min(waveIndex, (int)GAME_WAVES.size() - 1);
    const WaveDefinition& wave = GAME_WAVES[safeIndex];

    for (const auto& group : wave.groups) {
        for (int i = 0; i < group.count; ++i) {
            spawnQueue.push_back(group.type);
        }
    }

    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(spawnQueue.begin(), spawnQueue.end(), g);

    if (!spawnQueue.empty()) {
        spawnInterval = 30.0f / static_cast<float>(spawnQueue.size());
    } else {
        spawnInterval = 1.0f;
    }

    spawnTimer = spawnInterval; 
}

void WaveSpawner::update(float deltaTime, Vector2 playerPos, std::vector<std::unique_ptr<Enemy>>& enemies, Texture2D enemyTexture) {
    gameTime += deltaTime;
    spawnTimer += deltaTime;

    int targetWaveIndex = (int)(gameTime / 30.0f);

    if (targetWaveIndex != currentWaveIndex) {
        spawnWave(targetWaveIndex);
    }

    if (spawnTimer >= spawnInterval && !spawnQueue.empty()) {
        spawnTimer = 0.0f;

        EnemyType nextType = spawnQueue.back();
        spawnQueue.pop_back();

        float angle = (float)GetRandomValue(0, 360) * DEG2RAD;
        float spawnDistance = 700.0f;

        Vector2 spawnPos = {
            playerPos.x + cosf(angle) * spawnDistance,
            playerPos.y + sinf(angle) * spawnDistance
        };

        // Polymorf instansiering basert på type
        if (nextType == EnemyType::FOOTMAN) {
            enemies.push_back(std::make_unique<Footman>(spawnPos, enemyTexture));
        } else if (nextType == EnemyType::GOON) {
            enemies.push_back(std::make_unique<Goon>(spawnPos, enemyTexture));
        } else if (nextType == EnemyType::LACKEY) {
            enemies.push_back(std::make_unique<Lackey>(spawnPos, enemyTexture));
        }
    }
}