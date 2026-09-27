#include "spawner.hpp"
#include "castle.hpp"
#include <cmath>
#include <algorithm>
#include <random>

// =====================================================================
// VANSKELIGHETSGRAD
// Hver wave varer 30 sekunder. Echelon 1 har boss etter 10 min (20 waves),
// og hver echelon legger til 30 sek = én ekstra wave.
//
// Starten er rolig (ca. 13 fiender den første halve minuttet), men antallet
// vokser raskt: ~130 i wave 10 og ~750 i wave 20 (en konstant strøm sent i runden).
// I tillegg blir hver fiende sterkere med tiden (se Difficulty under), elite-fiender
// dukker opp etter 2 minutter, og hvert minutt fra 1:45 kommer en horde som omringer deg.
// Fordi det kommer så mange flere fiender, gir hver fiende MINDRE XP jo senere det er
// (xpScale), så levelingen går omtrent like fort som før.
// Exploder-grupper tas bare med når echelon 2+ er aktiv.
// =====================================================================
namespace Difficulty {
    constexpr float SPAWN_DISTANCE = 950.0f;   // Utenfor skjermen, også med kameraet zoomet ut
    constexpr int MAX_ALIVE = 800;             // Tak på fiender samtidig (ytelse)

    // Gammel kurve (8 + 4n + 0.55n²) + et kubisk ledd som først merkes etter ca. 2 min
    float baseWaveSize(int n) { return 10.0f + 5.0f * n + 0.6f * n * n; }
    int waveSize(int n) {
        float late = (float)std::max(0, n - 3);
        return (int)(baseWaveSize(n) + 0.09f * late * late * late);
    }
    // XP per fiende: like mye XP per wave som med den gamle kurven (litt mindre sent)
    float xpScale(int n) {
        float s = baseWaveSize(n) / (float)waveSize(n);
        return n >= 6 ? s * 0.85f : s;
    }

    // Fiender blir sterkere jo lenger runden varer (m = minutter)
    float hpMult(float m)     { return 1.0f + 0.15f * m + 0.035f * m * m; }  // 10 min: x6
    float damageMult(float m) { return 1.0f + 0.10f * m; }                   // 10 min: x2
    float speedMult(float m)  { return fminf(1.35f, 1.0f + 0.02f * m); }     // Maks +35 %

    // Sjanse for elite: 0 det første 1.5 minuttet, så 3 % + 0.8 % per minutt (maks 14 %)
    float eliteChance(float m) { return m < 1.5f ? 0.0f : fminf(0.14f, 0.03f + 0.008f * (m - 1.5f)); }

    // Horde hvert minutt, midt i waven (1:45, 2:45, 3:45 ...)
    bool isHordeWave(int waveIndex) { return waveIndex >= 3 && waveIndex % 2 == 1; }
    int hordeSize(int n) { return (int)(12 + 4 * n + 0.15f * n * n); }
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
            // Fra 2 minutter: armbrøstskyttere som tvinger deg til å bevege deg, og hundeflokker.
            // Fra 3 min: prester (helbreder) og trommeslagere (fart). Fra 4 min: kanonérer.
            // HOUND teller flokker (4-5 hunder hver), ikke enkelthunder.
            std::vector<EnemyGroup> g = { { EnemyType::FOOTMAN, part(0.30f) }, { EnemyType::LACKEY, part(0.27f) },
                                          { EnemyType::GOON, part(0.15f) }, { EnemyType::EXPLODER, part(0.09f) },
                                          { EnemyType::ARCHER, part(0.09f) }, { EnemyType::HOUND, std::max(1, part(0.025f)) } };
            if (n >= 7) { g.push_back({ EnemyType::PRIEST, std::max(1, part(0.02f)) }); g.push_back({ EnemyType::DRUMMER, std::max(1, part(0.02f)) }); }
            if (n >= 9) g.push_back({ EnemyType::CANNONEER, std::max(1, part(0.025f)) });
            waves.push_back({ n, g });
        }
    }
    return waves;
}

const std::vector<WaveDefinition> GAME_WAVES = BuildWaves();

// Et spawnpunkt `dist` fra spilleren, innenfor slottsmuren. Ligger punktet utenfor muren,
// prøves andre retninger (så fiendene kommer fra den åpne siden), ellers skyves det inn.
static Vector2 SpawnPoint(Vector2 playerPos, float angle, float dist) {
    for (int k = 0; k < 12; k++) {
        float a = angle + k * 0.53f;
        Vector2 p = { playerPos.x + cosf(a) * dist, playerPos.y + sinf(a) * dist };
        if (InsideCastle(p, 60.0f)) return p;
    }
    return ClampToCastle({ playerPos.x + cosf(angle) * dist, playerPos.y + sinf(angle) * dist }, 60.0f);
}

const char* DecreeTitle(Decree d) {
    switch (d) {
        case Decree::FEAST:      return "KONGENS FEST";
        case Decree::BLOOD_MOON: return "BLODMAANE";
        case Decree::HUNT:       return "DEN STORE JAKTEN";
        case Decree::MUSTER:     return "MOBILISERING";
        case Decree::DARKNESS:   return "MOERKLEGGING";
        case Decree::GOLD_RAIN:  return "GULLREGN";
        default:                 return "";
    }
}

const char* DecreeText(Decree d) {
    switch (d) {
        case Decree::FEAST:      return "Fiendene er raskere, men gir dobbelt XP";
        case Decree::BLOOD_MOON: return "Fiendene slaar hardere, men slipper mye mer gull";
        case Decree::HUNT:       return "Kongens hunder er sluppet loes!";
        case Decree::MUSTER:     return "Dobbelt saa mange fiender, +50% XP";
        case Decree::DARKNESS:   return "Lysene slukkes! +50% XP saa lenge det er moerkt";
        case Decree::GOLD_RAIN:  return "Gull regner fra taket, men fiendene taaler mer";
        default:                 return "";
    }
}

void WaveSpawner::reset(const EchelonModifiers& echelonModifiers) {
    modifiers = echelonModifiers;
    gameTime = 0.0f;
    spawnTimer = 0.0f;
    currentWaveIndex = -1;
    hordeSpawnedWave = -1;
    lastHordeTime = -100.0f;
    spawnQueue.clear();
    decree = Decree::NONE;
    decreeStart = -100.0f;
    decreesStarted = 0;
    lastDecree = Decree::NONE;
    lastTreasurerTime = -100.0f;
    treasurersSpawned = 0;
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

    // --- KONGELIGE DEKRETER ---
    if (decree != Decree::NONE && gameTime >= decreeStart + DECREE_LENGTH) decree = Decree::NONE;
    if (gameTime >= DECREE_FIRST + decreesStarted * DECREE_EVERY) {
        decreesStarted++;
        Decree d;
        do { d = (Decree)GetRandomValue(1, (int)Decree::COUNT - 1); } while (d == lastDecree);
        decree = lastDecree = d;
        decreeStart = gameTime;
        if (d == Decree::HUNT) {
            // Tre flokker fra tre kanter
            float base = (float)GetRandomValue(0, 360) * DEG2RAD;
            for (int p = 0; p < 3; p++) {
                float a = base + p * 2.0f * PI / 3.0f;
                Vector2 c = SpawnPoint(playerPos, a, Difficulty::SPAWN_DISTANCE);
                for (int k = 0; k < 5; k++)
                    spawnEnemy(EnemyType::HOUND, ClampToCastle({ c.x + (float)GetRandomValue(-60, 60), c.y + (float)GetRandomValue(-60, 60) }, 40.0f), enemies, enemyTexture);
            }
        }
    }

    // --- SKATTMESTEREN ---
    if (gameTime >= TREASURER_FIRST + treasurersSpawned * TREASURER_EVERY) {
        treasurersSpawned++;
        lastTreasurerTime = gameTime;
        float a = (float)GetRandomValue(0, 360) * DEG2RAD;
        spawnEnemy(EnemyType::TREASURER, ClampToCastle({ playerPos.x + cosf(a) * 320.0f, playerPos.y + sinf(a) * 260.0f }, 80.0f), enemies, enemyTexture);
    }

    // Horde: midt i hver 4. wave kommer en ring av fiender fra alle kanter samtidig
    bool hordeDue = Difficulty::isHordeWave(currentWaveIndex) && fmodf(gameTime, 30.0f) >= 15.0f;
    if (hordeDue && hordeSpawnedWave != currentWaveIndex) {
        hordeSpawnedWave = currentWaveIndex;
        lastHordeTime = gameTime;
        int n = currentWaveIndex + 1;
        int count = Difficulty::hordeSize(n);
        // Blanding som blir tøffere: lakeier først, så soldater og troll, så armbrøstskyttere.
        // Etter 7 min kommer hordene i to ringer, den ytre litt lenger ute.
        int rings = n >= 14 ? 2 : 1;
        for (int ring = 0; ring < rings; ring++) {
            float dist = Difficulty::SPAWN_DISTANCE + ring * 220.0f;
            for (int i = 0; i < count; i++) {
                float angle = (float)i / count * 2.0f * PI + ring * 0.13f;
                Vector2 pos = ClampToCastle({ playerPos.x + cosf(angle) * dist, playerPos.y + sinf(angle) * dist }, 40.0f); // Mot muren hvis den er nær
                EnemyType type = EnemyType::LACKEY;
                if (n >= 6) type = (i % 4 == 0) ? EnemyType::GOON : EnemyType::FOOTMAN;
                if (n >= 10 && i % 5 == 2) type = EnemyType::ARCHER;
                if (n >= 12 && i % 11 == 6) type = EnemyType::DRUMMER; // Hele hordens ring går fortere
                if (ring == 1) type = (i % 2 == 0) ? EnemyType::FOOTMAN : EnemyType::ARCHER;
                spawnEnemy(type, pos, enemies, enemyTexture, 0.5f); // Hordefiender gir halv XP
                // Den første er hordens kaptein: en elite. Bare annenhver horde (ca. hvert 2. minutt)
                // har kapteinen en skattekiste, så items forblir sjeldne.
                if (ring == 0 && i == 0 && !enemies.empty()) {
                    if (!enemies.back()->elite) enemies.back()->makeElite();
                    if (currentWaveIndex % 4 == 3) enemies.back()->chestCarrier = true;
                }
            }
        }
    }

    // Kan spawne flere per frame når waven er stor (sent i runden kommer det 20+ i sekundet)
    while (spawnTimer >= spawnInterval && !spawnQueue.empty() && (int)enemies.size() < Difficulty::MAX_ALIVE) {
        spawnTimer -= spawnInterval;

        EnemyType nextType = spawnQueue.back();
        spawnQueue.pop_back();

        float angle = (float)GetRandomValue(0, 360) * DEG2RAD;
        Vector2 spawnPos = SpawnPoint(playerPos, angle, Difficulty::SPAWN_DISTANCE);
        if (nextType == EnemyType::HOUND) {
            // Hundene kommer i flokk fra samme kant
            int pack = GetRandomValue(4, 5);
            for (int k = 0; k < pack; k++) {
                Vector2 p = ClampToCastle({ spawnPos.x + (float)GetRandomValue(-50, 50), spawnPos.y + (float)GetRandomValue(-50, 50) }, 40.0f);
                spawnEnemy(EnemyType::HOUND, p, enemies, enemyTexture);
            }
        } else {
            spawnEnemy(nextType, spawnPos, enemies, enemyTexture);
            // Mobilisering: en ekstra fiende for hver som kommer
            if (decree == Decree::MUSTER) spawnEnemy(nextType, Vector2Add(spawnPos, { 40.0f, 30.0f }), enemies, enemyTexture);
        }
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
    } else if (type == EnemyType::HOUND) {
        enemy = std::make_unique<Hound>(spawnPos, enemyTexture);
    } else if (type == EnemyType::PRIEST) {
        enemy = std::make_unique<Priest>(spawnPos, enemyTexture);
    } else if (type == EnemyType::DRUMMER) {
        enemy = std::make_unique<Drummer>(spawnPos, enemyTexture);
    } else if (type == EnemyType::CANNONEER) {
        enemy = std::make_unique<Cannoneer>(spawnPos, enemyTexture);
    } else if (type == EnemyType::TREASURER) {
        enemy = std::make_unique<Treasurer>(spawnPos, enemyTexture);
    }
    if (!enemy) return;

    // Echelon-effekter og tidsskalering stacker
    float m = gameTime / 60.0f;
    if (decree == Decree::FEAST) xpMult *= 2.0f;
    if (decree == Decree::MUSTER || decree == Decree::DARKNESS) xpMult *= 1.5f;
    float decreeHp = decree == Decree::GOLD_RAIN ? 1.4f : 1.0f;
    float decreeDamage = decree == Decree::BLOOD_MOON ? 1.5f : 1.0f;
    enemy->applyEchelonModifiers(modifiers.enemyHpMult * Difficulty::hpMult(m) * decreeHp,
                                 modifiers.enemyDamageMult * Difficulty::damageMult(m) * decreeDamage,
                                 modifiers.enemySpeedMult * Difficulty::speedMult(m));
    // Mindre XP per fiende sent i runden (det kommer så mange flere). Tilfeldig avrunding,
    // så en fiende med 2.4 XP gir 2 eller 3 (i snitt 2.4).
    {
        int n = (int)(gameTime / 30.0f) + 1;
        float xp = enemy->xpValue * Difficulty::xpScale(n) * xpMult;
        int whole = (int)xp;
        if (GetRandomValue(0, 999) < (int)((xp - whole) * 1000.0f)) whole++;
        enemy->xpValue = std::max(1, whole);
    }
    if (type != EnemyType::EXPLODER && type != EnemyType::TREASURER && GetRandomValue(1, 1000) <= (int)(Difficulty::eliteChance(m) * 1000.0f)) {
        enemy->makeElite();
    }
    // Gull skaleres ned på samme måte som XP: flere fiender betyr ikke mer gull per minutt
    enemy->goldChance *= Difficulty::xpScale((int)(gameTime / 30.0f) + 1);
    if (decree == Decree::BLOOD_MOON) enemy->goldChance = fminf(1.0f, enemy->goldChance * 3.0f);
    enemies.push_back(std::move(enemy));
}