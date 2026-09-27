#include "spawner.hpp"
#include <cmath>
#include <algorithm>
#include <random>

// =====================================================================
// VANSKELIGHETSGRAD
// Hver wave varer 30 sekunder. Echelon 1 har boss etter 10 min (20 waves),
// og hver echelon legger til 30 sek = én ekstra wave.
//
// Starten er rolig og antallet vokser de første minuttene, men så flater det ut:
// sent i runden kommer det FÆRRE, men mye STERKERE fiender (hoffgarder med rustning,
// steintroll som stormer, prester som helbreder), så skjermen ikke drukner i fiender.
//   wave 10: ~100 fiender   wave 20: ~170   (før: ~750)
// I tillegg blir hver fiende sterkere med tiden (se Difficulty under), elite-fiender
// dukker opp etter 2 minutter, og hvert minutt fra 1:45 kommer en horde som omringer deg.
// XP per fiende justeres slik at man får omtrent like mye XP per wave som før.
// Exploder-grupper tas bare med når echelon 2+ er aktiv.
// =====================================================================
namespace Difficulty {
    constexpr float SPAWN_DISTANCE = 950.0f;   // Utenfor skjermen, også med kameraet zoomet ut
    constexpr int MAX_ALIVE = 260;             // Tak på fiender samtidig (lesbarhet og ytelse)

    // Den opprinnelige kurven, som XP-balansen er bygget rundt
    float baseWaveSize(int n) { return 8.0f + 4.0f * n + 0.55f * n * n; }
    // Antall fiender per wave: følger kurven tidlig, men flater ut på 70 + 5 per wave
    int waveSize(int n) { return (int)fminf(baseWaveSize(n), 70.0f + 5.0f * n); }

    // Fiender blir sterkere jo lenger runden varer (m = minutter)
    float hpMult(float m)     { return 1.0f + 0.10f * m + 0.02f * m * m; }   // 10 min: x4
    float damageMult(float m) { return 1.0f + 0.06f * m; }                   // 10 min: x1.6
    float speedMult(float m)  { return fminf(1.3f, 1.0f + 0.015f * m); }     // Maks +30 %

    // Sjanse for elite: 0 de første 2 minuttene, så 2 % + 0.6 % per minutt (maks 10 %)
    float eliteChance(float m) { return m < 2.0f ? 0.0f : fminf(0.10f, 0.02f + 0.006f * (m - 2.0f)); }

    // Horde hvert minutt, midt i waven (1:45, 2:45, 3:45 ...). Maks 50 i én ring.
    bool isHordeWave(int waveIndex) { return waveIndex >= 3 && waveIndex % 2 == 1; }
    int hordeSize(int n) { return std::min(50, 12 + 3 * n); }

    // Grunn-XP per fiendetype (samme som i konstruktørene i enemy.cpp)
    float baseXp(EnemyType t) {
        switch (t) {
            case EnemyType::FOOTMAN:  return 15.0f;
            case EnemyType::GOON:     return 40.0f;
            case EnemyType::LACKEY:   return 8.0f;
            case EnemyType::EXPLODER: return 12.0f;
            case EnemyType::ARCHER:   return 22.0f;
            case EnemyType::GUARD:    return 45.0f;
            case EnemyType::TROLL:    return 120.0f;
            case EnemyType::PRIEST:   return 40.0f;
        }
        return 15.0f;
    }
    // Snitt-XP per fiende med den gamle miksen (fotsoldater, lakeier, troll, bomber, skyttere)
    constexpr float OLD_AVERAGE_XP = 17.2f;
}

// Hvor stor andel av en wave hver fiendetype utgjør (summerer til 1.0)
struct WaveMix { EnemyType type; float fraction; };

std::vector<WaveMix> mixForWave(int n) {
    using T = EnemyType;
    if (n == 1) return { { T::LACKEY, 0.70f }, { T::FOOTMAN, 0.30f } };
    if (n == 2) return { { T::LACKEY, 0.50f }, { T::FOOTMAN, 0.40f }, { T::GOON, 0.05f }, { T::EXPLODER, 0.05f } };
    if (n < 5)  return { { T::FOOTMAN, 0.38f }, { T::LACKEY, 0.34f }, { T::GOON, 0.18f }, { T::EXPLODER, 0.10f } };
    // 2-4 min: armbrøstskyttere, og de første hoffgardene
    if (n < 8)  return { { T::FOOTMAN, 0.30f }, { T::LACKEY, 0.28f }, { T::GOON, 0.16f }, { T::EXPLODER, 0.10f },
                         { T::ARCHER, 0.10f }, { T::GUARD, 0.06f } };
    // 4-6 min: flere garder, prester og de første steintrollene
    if (n < 12) return { { T::FOOTMAN, 0.22f }, { T::LACKEY, 0.18f }, { T::GOON, 0.16f }, { T::EXPLODER, 0.10f },
                         { T::ARCHER, 0.12f }, { T::GUARD, 0.14f }, { T::PRIEST, 0.04f }, { T::TROLL, 0.04f } };
    // 6-8 min: de sterke tar over
    if (n < 17) return { { T::FOOTMAN, 0.16f }, { T::LACKEY, 0.10f }, { T::GOON, 0.14f }, { T::EXPLODER, 0.10f },
                         { T::ARCHER, 0.14f }, { T::GUARD, 0.20f }, { T::PRIEST, 0.06f }, { T::TROLL, 0.10f } };
    // 8 min+: slottets elitestyrker
    return { { T::FOOTMAN, 0.12f }, { T::LACKEY, 0.08f }, { T::GOON, 0.12f }, { T::EXPLODER, 0.10f },
             { T::ARCHER, 0.14f }, { T::GUARD, 0.24f }, { T::PRIEST, 0.08f }, { T::TROLL, 0.12f } };
}

// XP-skalering per wave: like mye XP per wave som den gamle kurven ga, selv om det er
// færre fiender og de sterke gir mer XP hver. Litt mindre fra 3 min (som før).
float xpScaleForWave(int n) {
    float avgXp = 0.0f;
    for (const WaveMix& m : mixForWave(n)) avgXp += m.fraction * Difficulty::baseXp(m.type);
    float scale = (Difficulty::baseWaveSize(n) * Difficulty::OLD_AVERAGE_XP) / (Difficulty::waveSize(n) * avgXp);
    return n >= 6 ? scale * 0.85f : scale;
}

std::vector<WaveDefinition> BuildWaves() {
    std::vector<WaveDefinition> waves;
    for (int n = 1; n <= 30; n++) {
        int total = Difficulty::waveSize(n);
        WaveDefinition wave{ n, {} };
        for (const WaveMix& m : mixForWave(n)) {
            wave.groups.push_back({ m.type, std::max(0, (int)(total * m.fraction + 0.5f)) });
        }
        waves.push_back(wave);
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
        // Én ring som blir tøffere med tiden: lakeier først, så soldater og troll,
        // så armbrøstskyttere, hoffgarder og til slutt et par steintroll.
        float dist = Difficulty::SPAWN_DISTANCE;
        for (int i = 0; i < count; i++) {
            float angle = (float)i / count * 2.0f * PI;
            Vector2 pos = { playerPos.x + cosf(angle) * dist, playerPos.y + sinf(angle) * dist };
            EnemyType type = EnemyType::LACKEY;
            if (n >= 6) type = (i % 4 == 0) ? EnemyType::GOON : EnemyType::FOOTMAN;
            if (n >= 10 && i % 5 == 2) type = EnemyType::ARCHER;
            if (n >= 10 && i % 5 == 4) type = EnemyType::GUARD;
            if (n >= 14 && i % 16 == 8) type = EnemyType::TROLL;
            spawnEnemy(type, pos, enemies, enemyTexture, 0.5f); // Hordefiender gir halv XP
            // Den første er hordens kaptein: en elite som alltid bærer en skattekiste
            if (i == 0 && !enemies.empty()) {
                if (!enemies.back()->elite) enemies.back()->makeElite();
                enemies.back()->chestCarrier = true;
            }
        }
    }

    // Kan spawne flere per frame når waven er stor (sent i runden kommer det 20+ i sekundet)
    while (spawnTimer >= spawnInterval && !spawnQueue.empty() && (int)enemies.size() < Difficulty::MAX_ALIVE) {
        spawnTimer -= spawnInterval;

        EnemyType nextType = spawnQueue.back();
        spawnQueue.pop_back();

        float angle = (float)GetRandomValue(0, 360) * DEG2RAD;
        Vector2 spawnPos = {
            playerPos.x + cosf(angle) * Difficulty::SPAWN_DISTANCE,
            playerPos.y + sinf(angle) * Difficulty::SPAWN_DISTANCE
        };
        spawnEnemy(nextType, spawnPos, enemies, enemyTexture);
    }
    // Når taket er nådd skal det ikke hope seg opp et stort rykk som kommer med én gang
    spawnTimer = std::min(spawnTimer, spawnInterval * 4.0f);
}

void WaveSpawner::spawnEnemy(EnemyType type, Vector2 spawnPos, std::vector<std::unique_ptr<Enemy>>& enemies, Texture2D enemyTexture, float xpMult) {
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
    } else if (type == EnemyType::GUARD) {
        enemy = std::make_unique<RoyalGuard>(spawnPos, enemyTexture);
    } else if (type == EnemyType::TROLL) {
        enemy = std::make_unique<StoneTroll>(spawnPos, enemyTexture);
    } else if (type == EnemyType::PRIEST) {
        enemy = std::make_unique<CourtPriest>(spawnPos, enemyTexture);
    }
    if (!enemy) return;

    // Echelon-effekter og tidsskalering stacker
    float m = gameTime / 60.0f;
    enemy->applyEchelonModifiers(modifiers.enemyHpMult * Difficulty::hpMult(m),
                                 modifiers.enemyDamageMult * Difficulty::damageMult(m),
                                 modifiers.enemySpeedMult * Difficulty::speedMult(m));
    // Mindre XP per fiende sent i runden (det kommer så mange flere). Tilfeldig avrunding,
    // så en fiende med 2.4 XP gir 2 eller 3 (i snitt 2.4).
    {
        int n = (int)(gameTime / 30.0f) + 1;
        float xp = enemy->xpValue * xpScaleForWave(std::min(n, 30)) * xpMult;
        int whole = (int)xp;
        if (GetRandomValue(0, 999) < (int)((xp - whole) * 1000.0f)) whole++;
        enemy->xpValue = std::max(1, whole);
    }
    // Steintrollene er allerede tungvektere – de blir aldri elite (det ville blitt nesten en boss)
    bool canBeElite = type != EnemyType::EXPLODER && type != EnemyType::TROLL;
    if (canBeElite && GetRandomValue(1, 1000) <= (int)(Difficulty::eliteChance(m) * 1000.0f)) {
        enemy->makeElite();
    }
    enemies.push_back(std::move(enemy));
}