#include "spawner.hpp"
#include <cmath>
#include <algorithm>
#include <random>

// =====================================================================
// VANSKELIGHETSGRAD
// Hver wave varer 30 sekunder. Echelon 1 har boss etter 10 min (20 waves),
// og hver echelon legger til 30 sek = én ekstra wave.
//
// Starten er rolig (ca. 13 fiender den første halve minuttet), men antallet
// vokser kvadratisk: ~100 i wave 10 og ~300 i wave 20. I tillegg blir hver
// fiende sterkere med tiden (se Difficulty under), elite-fiender dukker opp
// etter 2 minutter, og hvert 2. minutt kommer en horde som omringer deg.
// Exploder-grupper tas bare med når echelon 2+ er aktiv.
// =====================================================================
namespace Difficulty {
    constexpr float SPAWN_DISTANCE = 950.0f;   // Utenfor skjermen, også med kameraet zoomet ut
    constexpr int MAX_ALIVE = 500;             // Tak på fiender samtidig (ytelse)

    int waveSize(int n) { return (int)(8 + 4 * n + 0.55f * n * n); }

    // Fiender blir sterkere jo lenger runden varer (m = minutter)
    float hpMult(float m)     { return 1.0f + 0.10f * m + 0.02f * m * m; }   // 10 min: x4
    float damageMult(float m) { return 1.0f + 0.06f * m; }                   // 10 min: x1.6
    float speedMult(float m)  { return fminf(1.3f, 1.0f + 0.015f * m); }     // Maks +30 %

    // Sjanse for elite: 0 de første 2 minuttene, så 2 % + 0.6 % per minutt (maks 10 %)
    float eliteChance(float m) { return m < 2.0f ? 0.0f : fminf(0.10f, 0.02f + 0.006f * (m - 2.0f)); }

    // Horde hvert 2. minutt, midt i waven (1:30, 3:30, 5:30 ...)
    bool isHordeWave(int waveIndex) { return waveIndex % 4 == 3; }
    int hordeSize(int n) { return 10 + 3 * n; }
}

std::vector<WaveDefinition> BuildWaves() {
    std::vector<WaveDefinition> waves;
    for (int n = 1; n <= 30; n++) {
        int total = Difficulty::waveSize(n);
        auto part = [&](float fraction) { return std::max(0, (int)(total * fraction + 0.5f)); };
        if (n == 1) {
            // Wave 1: små lakeier og noen få soldater
            waves.push_back({ n, { { EnemyType::LACKEY, part(0.7f) }, { EnemyType::FOOTMAN, part(0.3f) } } });
        } else if (n == 2) {
            waves.push_back({ n, { { EnemyType::LACKEY, part(0.55f) }, { EnemyType::FOOTMAN, part(0.4f) },
                                   { EnemyType::GOON, part(0.05f) }, { EnemyType::EXPLODER, part(0.05f) } } });
        } else if (n < 5) {
            waves.push_back({ n, { { EnemyType::FOOTMAN, part(0.38f) }, { EnemyType::LACKEY, part(0.34f) },
                                   { EnemyType::GOON, part(0.18f) }, { EnemyType::EXPLODER, part(0.10f) } } });
        } else {
            // Fra 2 minutter: armbrøstskyttere som tvinger deg til å bevege deg
            waves.push_back({ n, { { EnemyType::FOOTMAN, part(0.33f) }, { EnemyType::LACKEY, part(0.31f) },
                                   { EnemyType::GOON, part(0.16f) }, { EnemyType::EXPLODER, part(0.10f) },
                                   { EnemyType::ARCHER, part(0.10f) } } });
        }
    }
    return waves;
}

const std::vector<WaveDefinition> GAME_WAVES = BuildWaves();

void WaveSpawner::reset(const EchelonModifiers& echelonModifiers) {
    modifiers = echelonModifiers;
    gameTime = 0.0f;
    spawnTimer = 0.0f;
    currentWaveIndex = -1;
    hordeSpawnedWave = -1;
    lastHordeTime = -100.0f;
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

    // Horde: midt i hver 4. wave kommer en ring av fiender fra alle kanter samtidig
    bool hordeDue = Difficulty::isHordeWave(currentWaveIndex) && fmodf(gameTime, 30.0f) >= 15.0f;
    if (hordeDue && hordeSpawnedWave != currentWaveIndex) {
        hordeSpawnedWave = currentWaveIndex;
        lastHordeTime = gameTime;
        int n = currentWaveIndex + 1;
        int count = Difficulty::hordeSize(n);
        EnemyType type = n < 8 ? EnemyType::LACKEY : EnemyType::FOOTMAN;
        for (int i = 0; i < count; i++) {
            float angle = (float)i / count * 2.0f * PI;
            Vector2 pos = { playerPos.x + cosf(angle) * Difficulty::SPAWN_DISTANCE, playerPos.y + sinf(angle) * Difficulty::SPAWN_DISTANCE };
            spawnEnemy(type, pos, enemies, enemyTexture);
        }
    }

    if (spawnTimer >= spawnInterval && !spawnQueue.empty() && (int)enemies.size() < Difficulty::MAX_ALIVE) {
        spawnTimer = 0.0f;

        EnemyType nextType = spawnQueue.back();
        spawnQueue.pop_back();

        float angle = (float)GetRandomValue(0, 360) * DEG2RAD;
        Vector2 spawnPos = {
            playerPos.x + cosf(angle) * Difficulty::SPAWN_DISTANCE,
            playerPos.y + sinf(angle) * Difficulty::SPAWN_DISTANCE
        };
        spawnEnemy(nextType, spawnPos, enemies, enemyTexture);
    }
}

void WaveSpawner::spawnEnemy(EnemyType type, Vector2 spawnPos, std::vector<std::unique_ptr<Enemy>>& enemies, Texture2D enemyTexture) {
    // Polymorf instansiering basert på type
    std::unique_ptr<Enemy> enemy;
    if (type == EnemyType::FOOTMAN) {
        enemy = std::make_unique<Footman>(spawnPos, enemyTexture);
    } else if (type == EnemyType::GOON) {
        enemy = std::make_unique<Goon>(spawnPos, enemyTexture);
    } else if (type == EnemyType::LACKEY) {
        enemy = std::make_unique<Lackey>(spawnPos, enemyTexture);
    } else if (type == EnemyType::EXPLODER) {
        enemy = std::make_unique<Exploder>(spawnPos, enemyTexture);
    } else if (type == EnemyType::ARCHER) {
        enemy = std::make_unique<Archer>(spawnPos, enemyTexture);
    }
    if (!enemy) return;

    // Echelon-effekter og tidsskalering stacker
    float m = gameTime / 60.0f;
    enemy->applyEchelonModifiers(modifiers.enemyHpMult * Difficulty::hpMult(m),
                                 modifiers.enemyDamageMult * Difficulty::damageMult(m),
                                 modifiers.enemySpeedMult * Difficulty::speedMult(m));
    if (type != EnemyType::EXPLODER && GetRandomValue(1, 1000) <= (int)(Difficulty::eliteChance(m) * 1000.0f)) {
        enemy->makeElite();
    }
    enemies.push_back(std::move(enemy));
}