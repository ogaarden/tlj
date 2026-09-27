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
// dukker opp etter 2 minutter, og hvert 30. sek fra 1:15 kommer en horde som omringer deg
// (hvert 2. minutt et ekstra farlig stormangrep).
// Fordi det kommer så mange flere fiender, gir hver fiende MINDRE XP jo senere det er
// (xpScale), så levelingen går omtrent like fort som før.
// Exploder-grupper tas bare med når echelon 2+ er aktiv.
// =====================================================================
namespace Difficulty {
    constexpr float SPAWN_DISTANCE = 950.0f;   // Utenfor skjermen, også med kameraet zoomet ut
    constexpr int MAX_ALIVE = 480;             // Tak på fiender samtidig (lesbarhet og ytelse)

    // Referansekurve for XP: så mye XP en wave skal gi totalt (uansett hvor mange fiender den har)
    float baseWaveSize(int n) { return 10.0f + 5.0f * n + 0.6f * n * n; }
    // Antall fiender per wave: vokser jevnt, men flater ut sent. Sent i runden kommer i stedet
    // tunge fiender (kyrassere, fanebærere og kjemper) – færre figurer, men farligere.
    int waveSize(int n) {
        return (int)std::min(300.0f, 10.0f + 5.0f * n + 0.45f * n * n);
    }
    // XP per fiende: like mye XP per wave som med den gamle kurven (litt mindre sent)
    float xpScale(int n) {
        float s = baseWaveSize(n) / (float)waveSize(n);
        return n >= 6 ? s * 0.85f : s;
    }

    // Alle vanlige fiender tåler så mye at de rekker å følge etter deg en stund (ingen one-shots).
    // Ganges med HP-en i hver fiendes konstruktør.
    constexpr float BASE_HP = 2.3f;

    // Fiender blir sterkere jo lenger runden varer (m = minutter). Stiger raskere tidlig enn før,
    // så levelene dine ikke løper fra fiendene.
    float hpMult(float m)     { return 1.0f + 0.25f * m + 0.055f * m * m; }  // 2 min: x1.7, 5 min: x3.6, 10 min: x9
    float damageMult(float m) { return 1.0f + 0.14f * m; }                   // 10 min: x2.4
    float speedMult(float m)  { return fminf(1.45f, 1.0f + 0.035f * m); }    // 10 min: +35 %, maks +45 %

    // Sjanse for elite: 0 det første 1.5 minuttet, så 3 % + 1.2 % per minutt (maks 20 %)
    float eliteChance(float m) { return m < 1.5f ? 0.0f : fminf(0.20f, 0.03f + 0.012f * (m - 1.5f)); }

    // Horde midt i hver wave fra 1:15 (1:15, 1:45, 2:15 ...), altså hvert 30. sekund.
    // Hvert 2. minutt fra 2:45 (2:45, 4:45, 6:45 ...) er horden et stormangrep.
    bool isHordeWave(int waveIndex) { return waveIndex >= 2; }
    bool isAssaultWave(int waveIndex) { return waveIndex >= 5 && (waveIndex - 5) % 4 == 0; }
    int hordeSize(int n) { return (int)(12 + 3 * n + 0.08f * n * n); }
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
            // Tunge fiender: få, men de merkes. Kyrassere fra 4 min, fanebærere fra 5 min,
            // beleiringskjemper fra 6:30 (én til å begynne med, så flere)
            if (n >= 9)  g.push_back({ EnemyType::KNIGHT, 1 + (n - 9) / 2 });
            if (n >= 11) g.push_back({ EnemyType::BANNERMAN, 1 + (n - 11) / 4 });
            if (n >= 14) g.push_back({ EnemyType::GIANT, 1 + (n - 14) / 3 });
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

const char* HordeTitle(HordeKind k) {
    switch (k) {
        case HordeKind::GUARD:    return "KONGENS GARDE!";
        case HordeKind::HUNT:     return "HUNDEJAKTEN!";
        case HordeKind::STAMPEDE: return "STORMLOEPET!";
        default:                  return "EN HORDE OMRINGER DEG!";
    }
}

const char* HordeText(HordeKind k) {
    switch (k) {
        case HordeKind::GUARD:    return "Garden lukker ringen rundt deg - slaa deg ut!";
        case HordeKind::HUNT:     return "Hundeflokker fra alle kanter";
        case HordeKind::STAMPEDE: return "En vegg av fiender stormer mot deg fra en kant - kom deg rundt!";
        default:                  return "";
    }
}

void WaveSpawner::reset(const EchelonModifiers& echelonModifiers) {
    modifiers = echelonModifiers;
    gameTime = 0.0f;
    spawnTimer = 0.0f;
    currentWaveIndex = -1;
    hordeSpawnedWave = -1;
    lastHordeTime = -100.0f;
    lastHordeKind = HordeKind::RING;
    assaultsSpawned = 0;
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

    // Horde: midt i HVER wave fra 1:15 (hvert 30. sek) kommer en ring av fiender fra alle kanter.
    // Hvert 2. minutt fra 2:45 er det i stedet et STORMANGREP (se spawnAssault) – de skal være
    // skikkelig farlige. Bare stormangrepene har en kaptein med skattekiste.
    bool hordeDue = Difficulty::isHordeWave(currentWaveIndex) && fmodf(gameTime, 30.0f) >= 15.0f;
    if (hordeDue && hordeSpawnedWave != currentWaveIndex) {
        hordeSpawnedWave = currentWaveIndex;
        lastHordeTime = gameTime;
        int n = currentWaveIndex + 1;
        if (Difficulty::isAssaultWave(currentWaveIndex)) {
            spawnAssault(n, playerPos, enemies, enemyTexture);
        } else {
            lastHordeKind = HordeKind::RING;
            int count = Difficulty::hordeSize(n);
            // Blanding som blir tøffere: lakeier først, så soldater og troll, så armbrøstskyttere.
            int rings = 1; // Én ring: sent i runden er det heller de tunge fiendene som gjør det farlig
            for (int ring = 0; ring < rings; ring++) {
                float dist = Difficulty::SPAWN_DISTANCE + ring * 220.0f;
                for (int i = 0; i < count; i++) {
                    float angle = (float)i / count * 2.0f * PI + ring * 0.13f;
                    Vector2 pos = ClampToCastle({ playerPos.x + cosf(angle) * dist, playerPos.y + sinf(angle) * dist }, 40.0f); // Mot muren hvis den er nær
                    EnemyType type = EnemyType::LACKEY;
                    if (n >= 6) type = (i % 4 == 0) ? EnemyType::GOON : EnemyType::FOOTMAN;
                    if (n >= 10 && i % 5 == 2) type = EnemyType::ARCHER;
                    if (n >= 12 && i % 11 == 6) type = EnemyType::DRUMMER; // Hele hordens ring går fortere
                    if (n >= 12 && ring == 0 && i % 12 == 3) type = EnemyType::KNIGHT;
                    if (ring == 1) type = (i % 2 == 0) ? EnemyType::FOOTMAN : EnemyType::ARCHER;
                    spawnEnemy(type, pos, enemies, enemyTexture, 0.3f); // Hordefiender gir mindre XP
                    // Den første er hordens kaptein: en elite (uten kiste)
                    if (ring == 0 && i == 0 && !enemies.empty() && !enemies.back()->elite) enemies.back()->makeElite();
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
            if (decree == Decree::MUSTER && nextType != EnemyType::GIANT) spawnEnemy(nextType, Vector2Add(spawnPos, { 40.0f, 30.0f }), enemies, enemyTexture);
        }
    }
    // Når taket er nådd skal det ikke hope seg opp et stort rykk som kommer med én gang
    spawnTimer = std::min(spawnTimer, spawnInterval * 4.0f);
}

// =====================================================================
// STORMANGREP – de farlige hordene. Tre typer som går på rundgang (i tilfeldig rekkefølge
// innenfor hver runde på tre), og alle blir større og tøffere utover i runden.
//  - Kongens garde: to tette ringer som starter NÆR deg. Indre ring er troll med mange elites,
//    ytre ring soldater og armbrøstskyttere. Du må slå deg ut før ringen lukker seg.
//  - Hundejakten: hundeflokker fra alle kanter samtidig, pluss en ring med lakeier
//  - Stormløpet: en tykk vegg av lakeier og kamikaze-fiender (med trommeslagere) som stormer
//    mot deg fra én kant. Den er for tykk til å løpe gjennom – kom deg rundt kanten!
// Kapteinen (en elite) bærer alltid en skattekiste.
// =====================================================================
void WaveSpawner::spawnAssault(int n, Vector2 playerPos, std::vector<std::unique_ptr<Enemy>>& enemies, Texture2D enemyTexture) {
    // Ny tilfeldig rekkefølge for hver runde på tre, så man ikke vet hva som kommer
    if (assaultsSpawned % 3 == 0) {
        for (int i = 0; i < 3; i++) assaultOrder[i] = (HordeKind)((int)HordeKind::GUARD + i);
        for (int i = 2; i > 0; i--) std::swap(assaultOrder[i], assaultOrder[GetRandomValue(0, i)]);
    }
    HordeKind kind = assaultOrder[assaultsSpawned % 3];
    assaultsSpawned++;
    lastHordeKind = kind;
    float power = 1.0f + 0.25f * assaultsSpawned; // Hvert stormangrep er større enn det forrige
    size_t first = enemies.size();
    // Stormangrep kan gå over det vanlige taket, men ikke uendelig (ytelse)
    auto full = [&]() { return (int)enemies.size() >= Difficulty::MAX_ALIVE + 250; };

    if (kind == HordeKind::GUARD) {
        int inner = (int)((10 + 1.2f * n) * power);
        int outer = (int)((16 + 2.0f * n) * power);
        for (int i = 0; i < inner && !full(); i++) {
            float a = (float)i / inner * 2.0f * PI;
            Vector2 pos = ClampToCastle({ playerPos.x + cosf(a) * 520.0f, playerPos.y + sinf(a) * 520.0f }, 40.0f);
            spawnEnemy(EnemyType::GOON, pos, enemies, enemyTexture, 0.5f);
            if (i % 4 == 0 && !enemies.empty() && !enemies.back()->elite) enemies.back()->makeElite();
        }
        for (int i = 0; i < outer && !full(); i++) {
            float a = (float)i / outer * 2.0f * PI + 0.1f;
            Vector2 pos = ClampToCastle({ playerPos.x + cosf(a) * 720.0f, playerPos.y + sinf(a) * 720.0f }, 40.0f);
            EnemyType type = (i % 3 == 0) ? EnemyType::ARCHER : EnemyType::FOOTMAN;
            if (i % 12 == 5) type = EnemyType::DRUMMER;
            if (n >= 10 && i % 12 == 11) type = EnemyType::PRIEST;
            spawnEnemy(type, pos, enemies, enemyTexture, 0.5f);
        }
    } else if (kind == HordeKind::HUNT) {
        int packs = (int)((3 + n / 4) * power);
        for (int p = 0; p < packs && !full(); p++) {
            float a = (float)p / packs * 2.0f * PI;
            Vector2 c = SpawnPoint(playerPos, a, 800.0f);
            for (int k = 0; k < 5; k++)
                spawnEnemy(EnemyType::HOUND, ClampToCastle({ c.x + (float)GetRandomValue(-60, 60), c.y + (float)GetRandomValue(-60, 60) }, 40.0f), enemies, enemyTexture, 0.5f);
        }
        int ring = (int)((14 + 2 * n) * power);
        for (int i = 0; i < ring && !full(); i++) {
            float a = (float)i / ring * 2.0f * PI;
            Vector2 pos = ClampToCastle({ playerPos.x + cosf(a) * 900.0f, playerPos.y + sinf(a) * 900.0f }, 40.0f);
            spawnEnemy(n >= 10 && i % 3 == 0 ? EnemyType::FOOTMAN : EnemyType::LACKEY, pos, enemies, enemyTexture, 0.4f);
        }
    } else {
        // Stormløpet: en vegg vinkelrett på en tilfeldig retning, 7-9 rader dyp
        float a = (float)GetRandomValue(0, 628) / 100.0f;
        Vector2 dir = { cosf(a), sinf(a) }, side = { -dir.y, dir.x };
        int rows = std::min(8, 4 + assaultsSpawned);
        int perRow = std::min(40, (int)((18 + n) * std::sqrt(power)));
        float width = 1900.0f;
        for (int r = 0; r < rows; r++) {
            for (int i = 0; i < perRow && !full(); i++) {
                float t = ((float)i / (perRow - 1) - 0.5f) * width + (r % 2) * 25.0f;
                float d = 850.0f + r * 55.0f;
                Vector2 pos = ClampToCastle({ playerPos.x + dir.x * d + side.x * t, playerPos.y + dir.y * d + side.y * t }, 40.0f);
                EnemyType type = EnemyType::LACKEY;
                if (modifiers.exploders && (r + i) % 4 == 0) type = EnemyType::EXPLODER;
                if (r == rows - 1 && i % 6 == 3) type = EnemyType::DRUMMER; // Bakerste rad: hele veggen går fortere
                if (n >= 12 && r % 3 == 1 && i % 3 == 0) type = EnemyType::FOOTMAN;
                spawnEnemy(type, pos, enemies, enemyTexture, 0.3f);
            }
        }
    }

    // Kapteinen: den første som ble laget blir en elite med skattekiste
    if (enemies.size() > first) {
        Enemy& captain = *enemies[first];
        if (!captain.elite) captain.makeElite();
        captain.chestCarrier = true;
    }
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
    } else if (type == EnemyType::KNIGHT) {
        enemy = std::make_unique<Knight>(spawnPos, enemyTexture);
    } else if (type == EnemyType::BANNERMAN) {
        enemy = std::make_unique<Bannerman>(spawnPos, enemyTexture);
    } else if (type == EnemyType::GIANT) {
        enemy = std::make_unique<Giant>(spawnPos, enemyTexture);
    }
    if (!enemy) return;

    // Echelon-effekter og tidsskalering stacker
    float m = gameTime / 60.0f;
    if (decree == Decree::FEAST) xpMult *= 2.0f;
    if (decree == Decree::MUSTER || decree == Decree::DARKNESS) xpMult *= 1.5f;
    float decreeHp = decree == Decree::GOLD_RAIN ? 1.4f : 1.0f;
    float decreeDamage = decree == Decree::BLOOD_MOON ? 1.5f : 1.0f;
    float baseHp = type == EnemyType::TREASURER ? 1.0f : Difficulty::BASE_HP; // Skattmesteren rømmer, han skal kunne tas
    enemy->applyEchelonModifiers(modifiers.enemyHpMult * baseHp * Difficulty::hpMult(m) * decreeHp,
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
    bool canBeElite = type != EnemyType::EXPLODER && type != EnemyType::TREASURER && type != EnemyType::GIANT && type != EnemyType::BANNERMAN;
    if (canBeElite && GetRandomValue(1, 1000) <= (int)(Difficulty::eliteChance(m) * 1000.0f)) {
        enemy->makeElite();
    }
    // Gull skaleres ned på samme måte som XP: flere fiender betyr ikke mer gull per minutt
    enemy->goldChance *= Difficulty::xpScale((int)(gameTime / 30.0f) + 1);
    if (decree == Decree::BLOOD_MOON) enemy->goldChance = fminf(1.0f, enemy->goldChance * 3.0f);
    enemies.push_back(std::move(enemy));
}