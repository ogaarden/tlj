#include "spawner.hpp"
#include <cmath>
#include <algorithm>
#include <random>

// Hver wave varer 30 sekunder. Echelon 1 har boss etter 10 min (20 waves),
// og hver echelon legger til 30 sek = én ekstra wave. Echelon 10 trenger 29 waves.
// Exploder-grupper tas bare med når echelon 2+ er aktiv.
std::vector<WaveDefinition> BuildWaves() {
    std::vector<WaveDefinition> waves = {
        // Wave 1: Bare footmen
        { 1, { { EnemyType::FOOTMAN, 70 } } },
        // Wave 2: Footmen og lackeys, første kamikaze
        { 2, { { EnemyType::FOOTMAN, 35 }, { EnemyType::LACKEY, 35 }, { EnemyType::EXPLODER, 4 } } },
        // Wave 3: Lackeys og goons
        { 3, { { EnemyType::LACKEY, 40 }, { EnemyType::GOON, 25 }, { EnemyType::EXPLODER, 6 } } },
    };

    // Wave 4-30: blanding som gradvis blir større
    for (int n = 4; n <= 30; n++) {
        waves.push_back({ n, {
            { EnemyType::FOOTMAN,  30 + n },
            { EnemyType::LACKEY,   25 + n },
            { EnemyType::GOON,     10 + n / 2 },
            { EnemyType::EXPLODER, 4 + n / 3 },
        } });
    }
    return waves;
}

const std::vector<WaveDefinition> GAME_WAVES = BuildWaves();

void WaveSpawner::reset(const EchelonModifiers& echelonModifiers) {
    modifiers = echelonModifiers;
    gameTime = 0.0f;
    spawnTimer = 0.0f;
    currentWaveIndex = -1;
    spawnQueue.clear();
}

void WaveSpawner::spawnWave(int waveIndex) {
    currentWaveIndex = waveIndex;
    spawnQueue.clear();

    int safeIndex = std::min(waveIndex, (int)GAME_WAVES.size() - 1);
    const WaveDefinition& wave = GAME_WAVES[safeIndex];

    for (const auto& group : wave.groups) {
        if (group.type == EnemyType::EXPLODER && !modifiers.exploders) continue;
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
        std::unique_ptr<Enemy> enemy;
        if (nextType == EnemyType::FOOTMAN) {
            enemy = std::make_unique<Footman>(spawnPos, enemyTexture);
        } else if (nextType == EnemyType::GOON) {
            enemy = std::make_unique<Goon>(spawnPos, enemyTexture);
        } else if (nextType == EnemyType::LACKEY) {
            enemy = std::make_unique<Lackey>(spawnPos, enemyTexture);
        } else if (nextType == EnemyType::EXPLODER) {
            enemy = std::make_unique<Exploder>(spawnPos, enemyTexture);
        }

        if (enemy) {
            enemy->applyEchelonModifiers(modifiers.enemyHpMult, modifiers.enemyDamageMult, modifiers.enemySpeedMult);
            enemies.push_back(std::move(enemy));
        }
    }
}