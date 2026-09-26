#include <raylib.h>
#include <rlgl.h>
#include <vector>
#include <string>
#include <memory>
#include <algorithm>
#include <cstdlib>

#include "player.hpp"
#include "settings.hpp"
#include "character.hpp"
#include "shop.hpp"
#include "spawner.hpp"
#include "enemy.hpp"
#include "weapon.hpp"
#include "damage_numbers.hpp"
#include "abilities.hpp"
#include "echelon.hpp"
#include "save.hpp"
#include "curses.hpp"
#include "explosions.hpp"
#include "castle.hpp"
#include "render3d.hpp"
#include "audio.hpp"
#include "clowns.hpp"
#include "ui.hpp"
#include "hud.hpp"
#include "icons.hpp"
#include "vfx.hpp"
#include "music.hpp"

enum GameState {
    MAIN_MENU,
    CHARACTER_SELECT,
    ECHELON_SELECT,
    CURSE_SELECT,
    SHOP,
    SETTINGS,
    GAMEPLAY,
    LEVEL_UP,
    ITEM_SELECT,
    PAUSED,
    GAME_OVER
};

// =====================================================================
// GULL-BELØNNING PER RUN (metaprogresjon)
// Mynter fra fiender er sjeldne (se goldChance i enemy.cpp). I tillegg får man
// litt gull for hvor lenge man overlevde og hvilket level man nådde.
// Et typisk 10-minutters run gir ca. 250-300 gull (~120 mynter + 100 tid + ~60 level),
// og alt i shoppen koster ~9100 gull -> ca. 30-35 gode runs for å kjøpe alt.
// =====================================================================
namespace Rewards {
    constexpr float GOLD_PER_MINUTE = 10.0f;
    constexpr int GOLD_PER_LEVEL = 3;
    constexpr int BOSS_GOLD_PER_ECHELON = 100; // Bonus for å slå bossen (x echelon-nummer)
    const char* SAVE_FILE = "save.txt";
}

// Boss-arenaen ligger langt unna vanlig spillområde, og er en sirkel man ikke kan gå ut av
namespace Arena {
    const Vector2 CENTER = { 0.0f, 20000.0f };
    constexpr float RADIUS = 650.0f;
    constexpr float INTRO_TIME = 2.0f; // Hvor lenge "KONGENS TRONSAL"-teksten vises
}

// Holder en posisjon innenfor arenaen
Vector2 ClampToArena(Vector2 pos, float margin) {
    Vector2 offset = Vector2Subtract(pos, Arena::CENTER);
    float maxDist = Arena::RADIUS - margin;
    if (Vector2Length(offset) > maxDist) {
        offset = Vector2Scale(Vector2Normalize(offset), maxDist);
    }
    return Vector2Add(Arena::CENTER, offset);
}

struct RunSummary {
    bool died = false;
    bool bossDefeated = false;
    int echelon = 1;
    bool unlockedNewEchelon = false;
    float timeSurvived = 0.0f;
    int level = 1;
    int kills = 0;
    int coinGold = 0;     // Mynter plukket opp
    int survivalGold = 0; // Bonus for overlevd tid
    int levelGold = 0;    // Bonus for level
    int bossGold = 0;     // Bonus for å slå bossen
    float greedMult = 1.0f;
    int totalGold = 0;
};

RunSummary CalculateRunSummary(bool died, bool bossDefeated, int echelon, float time, int level, int kills, int coins, float greedMult) {
    RunSummary r;
    r.died = died;
    r.bossDefeated = bossDefeated;
    r.echelon = echelon;
    r.bossGold = bossDefeated ? Rewards::BOSS_GOLD_PER_ECHELON * echelon : 0;
    r.timeSurvived = time;
    r.level = level;
    r.kills = kills;
    r.coinGold = coins;
    r.survivalGold = (int)(time / 60.0f * Rewards::GOLD_PER_MINUTE);
    r.levelGold = (level - 1) * Rewards::GOLD_PER_LEVEL;
    r.greedMult = greedMult;
    r.totalGold = (int)((r.coinGold + r.survivalGold + r.levelGold + r.bossGold) * greedMult);
    return r;
}

int main() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(Settings::SCREEN_WIDTH, Settings::SCREEN_HEIGHT, "The Last Jester");
    SetTargetFPS(Settings::FPS);
    SetWindowMinSize(800, 450); // Alt skalerer med vinduet, men under dette blir teksten for liten
    SetExitKey(KEY_NULL); // ESC skal gå tilbake i menyer, ikke lukke hele spillet
    InitRenderer3D();
    InitVfx();
    InitGameAudio();
    InitGameMusic();

    GameState currentState = MAIN_MENU;
    int mainOption = 0;

    // Karakterer fra character.hpp (rene basestats)
    std::vector<CharacterData> characters = GetAvailableCharacters();
    int selectedCharacter = 0;

    std::vector<Texture2D> characterTextures;
    for (const auto& charData : characters) {
        characterTextures.push_back(LoadTexture(charData.texturePath.c_str()));
    }

    // 3D-forhåndsvisning av klovnene på karaktervalg-skjermen (én liten scene per kort).
    // Rendres i dobbel oppløsning så de er skarpe også når vinduet er stort.
    const int PREVIEW_W = 200, PREVIEW_H = 190;
    std::vector<RenderTexture2D> clownPreviews;
    for (size_t i = 0; i < characters.size(); i++) {
        clownPreviews.push_back(LoadRenderTexture(PREVIEW_W * 2, PREVIEW_H * 2));
        SetTextureFilter(clownPreviews.back().texture, TEXTURE_FILTER_BILINEAR);
    }

    // Portrett av klovnen i HUD-en
    RenderTexture2D portraitRT = LoadRenderTexture(160, 160);
    SetTextureFilter(portraitRT.texture, TEXTURE_FILTER_BILINEAR);
    bool showMinimap = true;

    // Last inn felles tekstur for fiender
    Texture2D enemyTexture = LoadTexture("assets/jester_real.png"); // Bytt ut med egen enemy.png om du har

    // Gull, shop-kjøp og opplåste echelons lagres i save.txt slik at de akkumuleres over runs og økter
    Shop shop;
    SaveData saveData;
    LoadGame(Rewards::SAVE_FILE, saveData, shop);
    SetGameVolume(saveData.volume / 100.0f);
    SetMusicVolume01(saveData.musicVolume / 100.0f);
    int settingsRow = 0; // 0 = volum, 1 = musikk
    int& totalGold = saveData.gold;
    auto saveProgress = [&]() { SaveGame(Rewards::SAVE_FILE, saveData, shop); };

    // --- ECHELON OG BOSS-ARENA ---
    int selectedEchelon = saveData.unlockedEchelon; // Starter på den dypeste man har låst opp
    EchelonModifiers runModifiers;                  // Alle stackede effekter for denne runden
    bool inBossArena = false;
    int bossId = -1;
    float arenaIntroTimer = 0.0f;

    // --- CURSES (echelon 5+) ---
    std::vector<CurseId> activeCurses;
    std::vector<CurseId> curseChoices;
    int selectedCurse = 0;
    int cursesToPick = 0;

    int runCoins = 0;       // Mynter plukket opp denne runden
    RunSummary lastRun;     // Vises på game over-skjermen

    Player player{};
    Camera2D camera{}; // Brukes bare for rotasjonen (Q/E) – selve kameraet er 3D (se MakeGameCamera)

    int lastPlayerLevel = 1;
    std::vector<AbilityChoice> activeUpgradeChoices;
    int selectedUpgradeOption = 0;
    int pauseOption = 0;        // 0 = Fortsett, 1 = Gi opp
    bool giveUpRequested = false;
    double levelUpStart = 0.0; // For animasjonen når level-up-kortene kommer inn
    int chestsPending = 0;     // Skattekister som er plukket opp, men ikke åpnet ennå
    bool levelUpFromChest = false;

    // --- SPAWNER OG FIENDER ---
    WaveSpawner spawner;
    std::vector<std::unique_ptr<Enemy>> enemies;
    std::vector<Pickup> pickups;

    // Gjør klar en ny runde med valgt karakter og echelon
    auto startRun = [&]() {
        const CharacterData& choice = characters[selectedCharacter];
        runModifiers = GetEchelonModifiers(selectedEchelon);

        // 1. Nullstill progresjon fra forrige runde
        player.position = { 0.0f, 0.0f };
        player.level = 1;
        player.currentXp = 0;
        player.xpToNextLevel = 100;
        player.weapons.clear();
        player.invulnerableTimer = 0.0f;
        player.slowTimer = 0.0f;
        player.critChance = 0.05f;
        for (int& b : player.statBoosts) b = 0;
        lastPlayerLevel = 1;
        runCoins = 0;
        Enemy::killCount = 0;
        inBossArena = false;
        bossId = -1;
        arenaIntroTimer = 0.0f;
        activeCurses.clear();
        chestsPending = 0;

        // 2. La shoppen påføre arvede basestats + shop-bonuser, deretter echelon-effekter
        shop.applyToPlayer(choice, player);
        player.xpMultiplier *= runModifiers.xpMult;
        if (runModifiers.noRegen) player.hpRegen = 0.0f;

        player.texture = characterTextures[selectedCharacter];
        player.clown = choice.clown;
        player.facingDir = { 0.0f, 1.0f };
        player.walkTime = 0.0f;

        // Karakterens unike ability tar alltid første slot
        player.innateAbility = choice.innateAbility;
        player.addWeapon(CreateAbility(choice.innateAbility));

        // Kamera-oppsett
        camera.target = player.position;
        camera.offset = { Settings::SCREEN_WIDTH / 2.0f, Settings::SCREEN_HEIGHT / 2.0f };
        camera.zoom = 1.0f;
        camera.rotation = 0.0f;

        // 3. Tilbakestill spawner og fiender for ny runde
        spawner.reset(runModifiers);
        enemies.clear();
        pickups.clear();
        ClearDamageNumbers();
        ClearExplosions();
        ClearVfx();
        ClearEnemyShots();
    };

    // Skade på spilleren fra fiender (kontakt og eksplosjoner)
    auto hurtPlayer = [&](float rawDamage) {
        float taken = player.takeDamage(rawDamage);
        if (taken > 0.0f) {
            SpawnDamageNumber(player.position, std::max(1, (int)(taken + 0.5f)), RED);
            PlaySfx(Sfx::PLAYER_HURT);
            AddCameraShake(0.35f);
            // Echelon 3+: fiender slower deg ved treff
            if (runModifiers.slowOnHit > 0.0f) {
                player.slowTimer = runModifiers.slowDuration;
                player.slowAmount = runModifiers.slowOnHit;
            }
        }
        player.invulnerableTimer = 0.5f; // Kort pause så man ikke smeltes av en klump fiender
    };

    while (!WindowShouldClose()) {
        float deltaTime = GetFrameTime();
        UI::HandleWindowShortcuts();

        // Musikk: menyvals i menyene, drivende spor i spillet, bossmusikk i tronsalen
        bool playing = currentState == GAMEPLAY || currentState == LEVEL_UP || currentState == PAUSED;
        SetMusicTrack(playing ? (inBossArena ? MusicTrack::BOSS : MusicTrack::GAME) : MusicTrack::MENU);
        UpdateGameMusic(deltaTime);

        // --- MENYLYDER: felles for alle menyskjermer ---
        bool inMenu = currentState != GAMEPLAY;
        if (inMenu) {
            if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_RIGHT) ||
                IsKeyPressed(KEY_W) || IsKeyPressed(KEY_S) || IsKeyPressed(KEY_A) || IsKeyPressed(KEY_D)) {
                PlaySfx(Sfx::UI_MOVE);
            }
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) PlaySfx(Sfx::UI_SELECT);
        }

        // -------------------------------------------------------------
        // INPUT & LOGIKK PER GAMESTATE
        // -------------------------------------------------------------
        if (currentState == MAIN_MENU) {
            if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) {
                mainOption = (mainOption + 1) % 4;
            }
            if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) {
                mainOption = (mainOption - 1 + 4) % 4;
            }

            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
                if (mainOption == 0) currentState = CHARACTER_SELECT; // PLAY -> Character Select
                else if (mainOption == 1) currentState = SHOP;         // SHOP
                else if (mainOption == 2) currentState = SETTINGS;     // SETTINGS
                else if (mainOption == 3) break;                       // QUIT
            }
        }
        else if (currentState == CHARACTER_SELECT) {
            if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) {
                selectedCharacter = (selectedCharacter + 1) % static_cast<int>(characters.size());
            }
            if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) {
                selectedCharacter = (selectedCharacter - 1 + static_cast<int>(characters.size())) % static_cast<int>(characters.size());
            }

            // Gå tilbake til Hovedmeny med P eller ESC
            if (IsKeyPressed(KEY_P) || IsKeyPressed(KEY_ESCAPE)) {
                currentState = MAIN_MENU;
            }

            // Videre til echelon-menyen
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
                currentState = ECHELON_SELECT;
            }
        }
        else if (currentState == ECHELON_SELECT) {
            // Bare opplåste echelons kan velges
            if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) {
                selectedEchelon = std::min(selectedEchelon + 1, saveData.unlockedEchelon);
            }
            if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) {
                selectedEchelon = std::max(selectedEchelon - 1, 1);
            }

            if (IsKeyPressed(KEY_P) || IsKeyPressed(KEY_ESCAPE)) {
                currentState = CHARACTER_SELECT;
            }

            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
                startRun();
                cursesToPick = runModifiers.curses;
                if (cursesToPick > 0) {
                    curseChoices = GetCurseChoices(activeCurses);
                    selectedCurse = 0;
                    currentState = CURSE_SELECT;
                } else {
                    currentState = GAMEPLAY;
                }
            }
        }
        else if (currentState == CURSE_SELECT) {
            int count = (int)curseChoices.size();
            if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) selectedCurse = (selectedCurse + 1) % count;
            if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) selectedCurse = (selectedCurse - 1 + count) % count;

            if (IsKeyPressed(KEY_ESCAPE)) {
                currentState = ECHELON_SELECT; // startRun() nullstiller alt når man prøver igjen
            }

            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
                CurseId chosen = curseChoices[selectedCurse];
                GetCurse(chosen).apply(player);
                activeCurses.push_back(chosen);
                cursesToPick--;

                if (cursesToPick > 0) {
                    curseChoices = GetCurseChoices(activeCurses);
                    selectedCurse = 0;
                } else {
                    currentState = GAMEPLAY;
                }
            }
        }
        else if (currentState == SHOP) {
            // Tilbake til Hovedmeny
            if (IsKeyPressed(KEY_P) || IsKeyPressed(KEY_B) || IsKeyPressed(KEY_ESCAPE)) {
                saveProgress();
                currentState = MAIN_MENU;
            }

            shop.handleInput(totalGold);
        }
        else if (currentState == SETTINGS) {
            // W/S velger rad, A/D justerer i steg på 10 %
            if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S) || IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) settingsRow = 1 - settingsRow;
            int& value = settingsRow == 0 ? saveData.volume : saveData.musicVolume;
            if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) value = std::min(100, value + 10);
            if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) value = std::max(0, value - 10);
            SetGameVolume(saveData.volume / 100.0f);
            SetMusicVolume01(saveData.musicVolume / 100.0f);

            if (IsKeyPressed(KEY_P) || IsKeyPressed(KEY_B) || IsKeyPressed(KEY_ESCAPE)) {
                saveProgress();
                currentState = MAIN_MENU;
            }
        }
        else if (currentState == GAMEPLAY) {

            if(player.level > lastPlayerLevel){
                lastPlayerLevel = player.level;
                currentState = LEVEL_UP;
                levelUpFromChest = false;
                PlaySfx(Sfx::LEVEL_UP);
                VfxShockwave(player.position, 90.0f, GOLD);
                selectedUpgradeOption = 0;
                levelUpStart = GetTime();
                activeUpgradeChoices = GenerateLevelUpChoices(player, player.levelUpChoices);
            } else if (chestsPending > 0) {
                // Skattekiste: et gratis oppgraderingsvalg
                chestsPending--;
                currentState = LEVEL_UP;
                levelUpFromChest = true;
                PlaySfx(Sfx::VICTORY);
                VfxShockwave(player.position, 110.0f, GOLD);
                selectedUpgradeOption = 0;
                levelUpStart = GetTime();
                activeUpgradeChoices = GenerateLevelUpChoices(player, player.levelUpChoices);
            }

            // 1. INPUT & OPPDRATERING
            // Roter kameraet med Q og E (60 grader i sekundet)
            if (IsKeyDown(KEY_Q)) camera.rotation -= 60.0f * deltaTime;
            if (IsKeyDown(KEY_E)) camera.rotation += 60.0f * deltaTime;
            if (IsKeyPressed(KEY_M)) showMinimap = !showMinimap;

            // Oppdater spilleren (sender inn gjeldende kamerarotasjon så WASD matcher skjermen)
            player.update(camera.rotation);

            // --- TIMEREN ER FERDIG: TELEPORTER TIL BOSS-ARENAEN ---
            if (!inBossArena && spawner.gameTime >= GetBossTimer(selectedEchelon)) {
                inBossArena = true;
                arenaIntroTimer = Arena::INTRO_TIME;
                PlaySfx(Sfx::BOSS_GONG);

                // Alt som ligger igjen på bakken suges opp automatisk
                for (const auto& p : pickups) {
                    if (p.type == PickupType::COIN) runCoins += p.value;
                    else if (p.type == PickupType::CHEST) chestsPending++;
                    else player.addXP(static_cast<int>(p.value * player.xpMultiplier));
                }
                pickups.clear();
                enemies.clear();
                ClearExplosions();
                ClearVfx();
                ClearEnemyShots();

                // Spilleren nederst i arenaen, bossen øverst
                player.position = { Arena::CENTER.x, Arena::CENTER.y + Arena::RADIUS * 0.6f };
                player.invulnerableTimer = Arena::INTRO_TIME;

                auto boss = std::make_unique<Boss>(Vector2{ Arena::CENTER.x, Arena::CENTER.y - Arena::RADIUS * 0.6f }, enemyTexture);
                boss->applyEchelonModifiers(runModifiers.enemyHpMult, runModifiers.enemyDamageMult, runModifiers.enemySpeedMult);
                bossId = boss->id;
                enemies.push_back(std::move(boss));
            }

            if (inBossArena) {
                // Ingen vanlige fiender i arenaen, men klokka går fortsatt (teller for gull)
                spawner.gameTime += deltaTime;
                if (arenaIntroTimer > 0.0f) arenaIntroTimer -= deltaTime;
                player.position = ClampToArena(player.position, 20.0f);
            } else {
                // Oppdater spawneren (spawner fiender rundt spilleren)
                spawner.update(deltaTime, player.position, enemies, enemyTexture);
            }
            camera.target = player.position;

            // Animasjon: alder (for å stige opp av gulvet) og hvitt treffglimt som dør ut
            for (auto& enemy : enemies) {
                enemy->age += deltaTime;
                enemy->hitFlash = std::max(0.0f, enemy->hitFlash - deltaTime * 7.0f);
            }
            UpdateCameraShake(deltaTime);

            // Oppdater alle fiender (bossen står stille mens intro-teksten vises)
            if (arenaIntroTimer <= 0.0f) {
                for (auto& enemy : enemies) {
                    enemy->update(player.position);
                    if (inBossArena) enemy->position = ClampToArena(enemy->position, enemy->hitRadius);
                }
            }

            // Den rasende kongen kaller inn lakeier i en ring rundt seg
            Boss* king = nullptr;
            for (auto& e : enemies) if (e->id == bossId) king = static_cast<Boss*>(e.get());
            if (king && king->summonsRequested > 0) {
                int n = king->summonsRequested;
                king->summonsRequested = 0;
                Vector2 kingPos = king->position;
                for (int i = 0; i < n; i++) {
                    float a = (float)i / n * 2.0f * PI;
                    Vector2 pos = ClampToArena({ kingPos.x + cosf(a) * 110.0f, kingPos.y + sinf(a) * 110.0f }, 20.0f);
                    spawner.spawnEnemy(EnemyType::LACKEY, pos, enemies, enemyTexture);
                    VfxDeath(pos, Color{ 255, 80, 60, 255 });
                }
            }

            // --- FIENDER SKADER SPILLEREN VED KONTAKT ---
            const float playerHitRadius = 20.0f;
            if (player.invulnerableTimer <= 0.0f) {
                for (auto& enemy : enemies) {
                    if (enemy->contactDamage() <= 0) continue; // F.eks. kamikaze skader bare med eksplosjonen
                    if (CheckCollisionCircles(player.position, playerHitRadius, enemy->position, enemy->hitRadius)) {
                        hurtPlayer((float)enemy->contactDamage());
                        break;
                    }
                }
            }

            bool runEnded = false;
            if (player.hp <= 0.0f) {
                if (player.aegis > 0) {
                    // Aegis: gjenoppstå med halv HP og blås bort fiender rundt deg (men ikke bossen!)
                    player.aegis--;
                    player.hp = player.maxHp * 0.5f;
                    player.invulnerableTimer = 2.0f;
                    enemies.erase(std::remove_if(enemies.begin(), enemies.end(), [&](const std::unique_ptr<Enemy>& e) {
                        return e->id != bossId && Vector2Distance(e->position, player.position) < 250.0f;
                    }), enemies.end());
                } else {
                    runEnded = true;
                }
            }

            // --- OPPDATER OG PLUKK OPP XP-ORBS ---
            for (auto it = pickups.begin(); it != pickups.end(); ) {
                float distance = Vector2Distance(it->position, player.position);

                // Hvis orben er innenfor spillerens lootRadius, sug den til deg!
                if (distance < player.lootRadius) {
                    float magnetSpeed = 400.0f; // Hvor fort den fyker mot spilleren
                    it->position = Vector2MoveTowards(it->position, player.position, magnetSpeed * deltaTime);

                    // Når den er helt nær (f.eks. innenfor 15 piksler), saml den opp
                    if (distance < 15.0f) {
                        if (it->type == PickupType::COIN) {
                            runCoins += it->value;
                            PlaySfx(Sfx::COIN);
                        } else if (it->type == PickupType::CHEST) {
                            chestsPending++;
                        } else {
                            player.addXP(static_cast<int>(it->value * player.xpMultiplier));
                            PlaySfx(Sfx::XP);
                        }

                        // Slett orben fra listen
                        it = pickups.erase(it);
                    } else {
                        ++it;
                    }
                } else {
                    ++it;
                }
            }

            // Oppdater alle abilities
            Enemy::critChance = player.critChance;
            CombatModifiers mods = player.combatModifiers();
            for (auto& w : player.weapons) {
                w->update(deltaTime, player.position, enemies, pickups, mods);
            }

            UpdateDamageNumbers(deltaTime);
            UpdateVfx(deltaTime);

            // Fjerne døde fiender (f.eks. kamikaze som har sprengt seg selv)
            for (auto& e : enemies) {
                if (e->isDead()) e->onDeath();
            }
            enemies.erase(
                std::remove_if(enemies.begin(), enemies.end(),
                    [](const std::unique_ptr<Enemy>& e) { return e->isDead(); }),
                enemies.end()
            );

            // Piler fra armbrøstskyttere
            float shotDamage = UpdateEnemyShots(deltaTime, player.position, playerHitRadius);
            if (shotDamage > 0.0f && player.invulnerableTimer <= 0.0f) hurtPlayer(shotDamage);

            // Eksplosjoner som treffer spilleren
            float explosionDamage = UpdateExplosions(deltaTime, player.position, playerHitRadius);
            if (explosionDamage > 0.0f && player.invulnerableTimer <= 0.0f) {
                hurtPlayer(explosionDamage);
            }

            // Bossen er slått når den ikke lenger finnes i fiende-lista
            bool bossDefeated = inBossArena && bossId >= 0 &&
                std::none_of(enemies.begin(), enemies.end(), [&](const std::unique_ptr<Enemy>& e) { return e->id == bossId; });

            // ESC/P åpner pausemenyen. "Gi opp" der avslutter runden (man får fortsatt gullet man har tjent).
            bool gaveUp = giveUpRequested;
            giveUpRequested = false;
            if (!gaveUp && (IsKeyPressed(KEY_P) || IsKeyPressed(KEY_ESCAPE))) {
                currentState = PAUSED;
                pauseOption = 0;
                PlaySfx(Sfx::UI_SELECT);
            }

            if (runEnded || gaveUp || bossDefeated) {
                lastRun = CalculateRunSummary(runEnded, bossDefeated, selectedEchelon, spawner.gameTime, player.level, Enemy::killCount, runCoins, player.goldMultiplier);
                totalGold += lastRun.totalGold;

                // Slå bossen på den dypeste echelonen -> lås opp neste
                if (bossDefeated && selectedEchelon == saveData.unlockedEchelon && saveData.unlockedEchelon < MAX_ECHELON) {
                    saveData.unlockedEchelon++;
                    selectedEchelon = saveData.unlockedEchelon;
                    lastRun.unlockedNewEchelon = true;
                }

                if (bossDefeated) PlaySfx(Sfx::VICTORY);
                else if (runEnded) PlaySfx(Sfx::DEATH);

                saveProgress();
                currentState = GAME_OVER;
            }
        }
        else if (currentState == PAUSED) {
            if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S) || IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) pauseOption = 1 - pauseOption;
            if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_P)) currentState = GAMEPLAY;
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
                if (pauseOption == 1) giveUpRequested = true; // Håndteres i GAMEPLAY neste frame
                currentState = GAMEPLAY;
            }
        }
        else if (currentState == GAME_OVER) {
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ESCAPE)) {
                currentState = MAIN_MENU;
            }
        }
            else if(currentState == LEVEL_UP) {
                // Kortene ligger ved siden av hverandre: A/D (og W/S) blar
                if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D) || IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) {
                    selectedUpgradeOption = (selectedUpgradeOption + 1) % activeUpgradeChoices.size();
            }
                if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A) || IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) {
                    selectedUpgradeOption = (selectedUpgradeOption - 1 + activeUpgradeChoices.size()) % activeUpgradeChoices.size();
            }
            // Bekreft valg
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
                ApplyAbilityChoice(player, activeUpgradeChoices[selectedUpgradeOption]);

                currentState = GAMEPLAY; // Tilbake til spillet!
            }
    }

        // -------------------------------------------------------------
        // 3D-PORTRETTER (tegnes til teksturer før selve skjermen)
        // -------------------------------------------------------------
        float uiTime = (float)GetTime();
        if (currentState == CHARACTER_SELECT) {
            // Hver klovn i sin egen lille 3D-scene
            Camera3D previewCam{};
            previewCam.position = { 0.0f, 60.0f, 140.0f };
            previewCam.target = { 0.0f, 36.0f, 0.0f };
            previewCam.up = { 0.0f, 1.0f, 0.0f };
            previewCam.fovy = 42.0f; // Plass til Westers hevede hammer
            previewCam.projection = CAMERA_PERSPECTIVE;
            SetShadeViewDir(Vector3Subtract(previewCam.position, previewCam.target));
            for (size_t i = 0; i < characters.size(); i++) {
                bool isSelected = (static_cast<int>(i) == selectedCharacter);
                float spin = uiTime * (isSelected ? 1.6f : 0.5f) + i * 2.0f;

                BeginTextureMode(clownPreviews[i]);
                ClearBackground(Color{ 0, 0, 0, 0 });
                BeginMode3D(previewCam);
                    // Liten sokkel med rød løper-farge og gullkant
                    ShadedCylinder({ 0.0f, -6.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, 34.0f, 32.0f, Color{ 212, 175, 55, 255 }, 24);
                    ShadedCylinder({ 0.0f, -0.5f, 0.0f }, { 0.0f, 0.2f, 0.0f }, 30.0f, 30.0f, Color{ 130, 20, 30, 255 }, 24);

                    ClownPose pose;
                    pose.position = { 0.0f, 0.0f };
                    pose.facing = { sinf(spin), cosf(spin) };
                    pose.moving = isSelected;               // Den valgte klovnen går på stedet
                    pose.walkTime = uiTime * 0.8f;
                    DrawClown(characters[i].clown, pose);
                EndMode3D();
                EndTextureMode();
            }
        }
        if (currentState == GAMEPLAY || currentState == LEVEL_UP || currentState == PAUSED) {
            // Portrett av klovnens hode til HUD-en
            float headHeight = (player.clown == ClownStyle::GEEK) ? 58.0f : 47.0f;
            Camera3D portraitCam{};
            portraitCam.position = { 18.0f, headHeight + 6.0f, 64.0f };
            portraitCam.target = { 0.0f, headHeight + 1.0f, 0.0f };
            portraitCam.up = { 0.0f, 1.0f, 0.0f };
            portraitCam.fovy = 26.0f;
            portraitCam.projection = CAMERA_PERSPECTIVE;
            SetShadeViewDir(Vector3Subtract(portraitCam.position, portraitCam.target));
            BeginTextureMode(portraitRT);
            ClearBackground(Color{ 58, 40, 70, 255 });
            BeginMode3D(portraitCam);
                ClownPose pose;
                pose.position = { 0.0f, 0.0f };
                pose.facing = { 0.15f, 1.0f };
                pose.tint = (player.invulnerableTimer > 0.0f && fmodf(uiTime * 10.0f, 1.0f) < 0.5f) ? Color{ 255, 150, 150, 255 } : WHITE;
                DrawClown(player.clown, pose);
            EndMode3D();
            EndTextureMode();
        }

        // -------------------------------------------------------------
        // TEGNING ON SCREEN
        // Menyer: slottet i bakgrunnen + et 1280x700-lerret som skaleres til vinduet.
        // Spilling: 3D-verdenen fyller hele vinduet, HUD-en festes til kantene.
        // -------------------------------------------------------------
        BeginDrawing();
        ClearBackground(BLACK);

        const float VW = (float)Settings::SCREEN_WIDTH;
        const float VH = (float)Settings::SCREEN_HEIGHT;
        const float CX = VW / 2.0f;
        auto hint = [&](const char* text) {
            UI::DrawCenteredText(text, CX, VH - 38.0f, 18.0f, Color{ 220, 210, 190, 255 });
        };
        auto heading = [&](const char* text, float y, Color color) {
            UI::DrawCenteredText(text, CX, y, 36.0f, color, 3.0f);
        };

        if (currentState == MAIN_MENU) {
            UI::DrawCastleBackdrop(uiTime, 0.0f);
            UI::BeginCanvas();
                UI::DrawTitleLogo(CX, 22.0f, 700.0f, uiTime);

                // Knapper som bannere: den valgte er kongerød med gullkant
                const char* options[] = { "PLAY", "SHOP", "SETTINGS", "QUIT" };
                for (int i = 0; i < 4; i++) {
                    bool sel = (i == mainOption);
                    float bw = sel ? 300.0f + 6.0f * sinf(uiTime * 4.0f) : 280.0f;
                    Rectangle r = { CX - bw / 2.0f, 350.0f + i * 58.0f, bw, 46.0f };
                    if (sel) {
                        UI::DrawPanel(r, 1.0f, UI::GOLD_LIGHT, Color{ 150, 24, 36, 235 });
                        // Diamanter som peker inn mot valget
                        for (int side = -1; side <= 1; side += 2) {
                            float dx = r.x + (side < 0 ? -22.0f : r.width + 22.0f) + side * 3.0f * sinf(uiTime * 6.0f);
                            float dy = r.y + r.height / 2.0f;
                            DrawTriangle({ dx, dy - 9 }, { dx - 9, dy }, { dx, dy + 9 }, UI::GOLD_LIGHT);
                            DrawTriangle({ dx, dy - 9 }, { dx, dy + 9 }, { dx + 9, dy }, UI::GOLD_DARK);
                        }
                    } else {
                        UI::DrawPanel(r, 1.0f, UI::PANEL_EDGE, Color{ 22, 18, 30, 190 });
                    }
                    UI::DrawCenteredText(options[i], CX, r.y + 11.0f, 26.0f, sel ? UI::GOLD_LIGHT : Color{ 230, 222, 205, 255 });
                }
            UI::EndCanvas();

            // Gull nede i venstre hjørne og hjelpetekst nede i midten (festet til vinduskanten)
            float s = HudScale();
            const char* goldText = TextFormat("%d g", totalGold);
            float gs = std::round(22.0f * s);
            float gw = MeasureTextEx(GetFontDefault(), goldText, gs, gs / 10.0f).x;
            Rectangle gp = { 16.0f * s, GetScreenHeight() - 58.0f * s, gw + 64.0f * s, 42.0f * s };
            UI::DrawPanel(gp, s);
            DrawCircleV({ gp.x + 24.0f * s, gp.y + 21.0f * s }, 10.0f * s, UI::GOLD_DARK);
            DrawCircleV({ gp.x + 23.0f * s, gp.y + 20.0f * s }, 8.0f * s, GOLD);
            UI::DrawOutlinedText(goldText, gp.x + 42.0f * s, gp.y + 10.0f * s, gs, UI::GOLD_LIGHT, 2.0f);
            const char* help = "[W/S] Velg   [ENTER] Bekreft   [F11] Fullskjerm";
            float hs = std::round(14.0f * s);
            float hw = MeasureTextEx(GetFontDefault(), help, hs, hs / 10.0f).x;
            UI::DrawOutlinedText(help, GetScreenWidth() - hw - 18.0f * s, GetScreenHeight() - 30.0f * s, hs, Fade(Color{ 230, 222, 205, 255 }, 0.8f), 1.5f);
        }
        else if (currentState == CHARACTER_SELECT) {
            UI::DrawCastleBackdrop(uiTime, 0.6f);
            UI::BeginCanvas();
            heading("VELG KARAKTER", 36.0f, UI::GOLD_LIGHT);

            const float cardWidth = 240.0f;
            const float cardHeight = 470.0f;
            const float cardGap = 26.0f;
            int count = static_cast<int>(characters.size());
            float startX = CX - (count * cardWidth + (count - 1) * cardGap) / 2.0f;

            for (int i = 0; i < count; i++) {
                bool isSelected = (i == selectedCharacter);
                float posX = startX + i * (cardWidth + cardGap);
                float posY = 100.0f - (isSelected ? 8.0f : 0.0f);
                Rectangle card = { posX, posY, cardWidth, cardHeight };
                UI::DrawPanel(card, 1.0f, isSelected ? UI::GOLD_LIGHT : UI::PANEL_EDGE,
                              isSelected ? Color{ 48, 30, 40, 235 } : Color{ 22, 18, 30, 215 });

                // Navn på et lite banner
                UI::DrawCenteredText(characters[i].name.c_str(), posX + cardWidth / 2.0f, posY + 18.0f, 26.0f, isSelected ? UI::GOLD_LIGHT : WHITE);

                // 3D-klovnen (render-teksturer er lagret opp-ned, derav negativ høyde)
                Rectangle srcRect = { 0.0f, 0.0f, (float)clownPreviews[i].texture.width, -(float)clownPreviews[i].texture.height };
                Rectangle destRect = { posX + (cardWidth - PREVIEW_W) / 2.0f, posY + 52.0f, (float)PREVIEW_W, (float)PREVIEW_H };
                if (isSelected) UI::DrawGlow({ (destRect.x + PREVIEW_W / 2.0f), (destRect.y + PREVIEW_H * 0.6f) }, 110.0f, Fade(UI::GOLD_LIGHT, 0.25f), Fade(UI::GOLD_LIGHT, 0.0f));
                DrawTexturePro(clownPreviews[i].texture, srcRect, destRect, { 0.0f, 0.0f }, 0.0f, WHITE);

                // Beskrivelse og oppgangende stats med Shop-bonuser
                float finalHp = characters[i].maxHp * shop.hpMult();
                float finalSpeed = characters[i].speed * shop.speedMult();
                float finalArmor = characters[i].armor + shop.armorBonus();

                float textX = posX + 20.0f;
                float textY = posY + 252.0f;
                DrawText(characters[i].description.c_str(), (int)textX, (int)textY, 14, Color{ 200, 190, 170, 255 });
                DrawLineEx({ textX, textY + 24.0f }, { posX + cardWidth - 20.0f, textY + 24.0f }, 1.0f, Fade(UI::GOLD_DARK, 0.8f));
                DrawText(TextFormat("Innate: %s", GetAbilityDefinition(characters[i].innateAbility).name.c_str()), (int)textX, (int)textY + 34, 16, GOLD);
                const char* labels[4] = { "HP", "Fart", "Armor", "Radius" };
                const char* values[4] = { TextFormat("%.0f", finalHp), TextFormat("%.0f", finalSpeed), TextFormat("%.0f (-%.0f%%)", finalArmor, finalArmor / (finalArmor + 30.0f) * 100.0f), TextFormat("%.0f", characters[i].lootRadius) };
                for (int k = 0; k < 4; k++) {
                    int ly = (int)textY + 62 + k * 24;
                    DrawText(labels[k], (int)textX, ly, 18, Color{ 190, 180, 165, 255 });
                    DrawText(values[k], (int)(posX + cardWidth - 20.0f) - MeasureText(values[k], 18), ly, 18, WHITE);
                }
                if (shop.aegisBonus() > 0) DrawText(TextFormat("Aegis: +%d", shop.aegisBonus()), (int)textX, (int)textY + 162, 18, Color{ 90, 200, 120, 255 });
            }

            hint("[A/D] Velg karakter   |   [ENTER] Videre   |   [ESC] Tilbake");
            UI::EndCanvas();
        }
        else if (currentState == ECHELON_SELECT) {
            UI::DrawCastleBackdrop(uiTime, 0.7f);
            UI::BeginCanvas();
            heading("VELG ECHELON", 26.0f, Color{ 255, 170, 70, 255 });

            // --- Liste over alle 10 echelons (låste er mørke) ---
            const int listX = 60;
            const int rowHeight = 50;
            const int listY = 86;
            UI::DrawPanel({ listX - 14.0f, listY - 14.0f, 648.0f, MAX_ECHELON * rowHeight + 22.0f });
            for (int e = 1; e <= MAX_ECHELON; e++) {
                const EchelonData& info = GetEchelon(e);
                bool unlocked = e <= saveData.unlockedEchelon;
                bool isSelected = e == selectedEchelon;
                int y = listY + (e - 1) * rowHeight;

                Color bg = isSelected ? Color{ 150, 24, 36, 200 } : Fade(DARKGRAY, unlocked ? 0.35f : 0.12f);
                DrawRectangle(listX, y, 620, rowHeight - 6, bg);
                if (isSelected) DrawRectangleLinesEx({ (float)listX, (float)y, 620.0f, rowHeight - 6.0f }, 2.0f, UI::GOLD_LIGHT);

                if (unlocked) {
                    int bossTime = (int)GetBossTimer(e);
                    DrawText(info.name.c_str(), listX + 12, y + 6, 20, isSelected ? UI::GOLD_LIGHT : WHITE);
                    DrawText(info.description.c_str(), listX + 12, y + 27, 14, LIGHTGRAY);
                    const char* timeText = TextFormat("Boss %02d:%02d", bossTime / 60, bossTime % 60);
                    DrawText(timeText, listX + 610 - MeasureText(timeText, 16), y + 14, 16, isSelected ? UI::GOLD_LIGHT : GRAY);
                } else {
                    DrawText(info.name.c_str(), listX + 12, y + 12, 20, Fade(GRAY, 0.4f));
                    DrawText("LAAST", listX + 610 - MeasureText("LAAST", 18), y + 12, 18, Fade(GRAY, 0.4f));
                }
            }

            // --- Alle effekter som gjelder for valgt echelon (de stacker) ---
            const int panelX = 730;
            UI::DrawPanel({ panelX - 20.0f, listY - 14.0f, 510.0f, MAX_ECHELON * rowHeight + 22.0f });
            DrawText(TextFormat("%s - aktive effekter:", GetEchelon(selectedEchelon).name.c_str()), panelX, listY + 6, 20, WHITE);
            int lineY = listY + 42;
            for (int e = 1; e <= selectedEchelon; e++) {
                Color c = (e == selectedEchelon) ? UI::GOLD_LIGHT : LIGHTGRAY;
                DrawText(TextFormat("E%d: %s", e, GetEchelon(e).description.c_str()), panelX, lineY, 18, c);
                lineY += 28;
            }
            int bossTime = (int)GetBossTimer(selectedEchelon);
            DrawText(TextFormat("Boss etter %02d:%02d", bossTime / 60, bossTime % 60), panelX, lineY + 15, 20, Color{ 255, 170, 70, 255 });

            hint("[W/S] Velg   |   [ENTER] Start   |   [ESC] Tilbake");
            UI::EndCanvas();
        }
        else if (currentState == CURSE_SELECT) {
            UI::DrawCastleBackdrop(uiTime, 0.75f);
            UI::BeginCanvas();
            heading(TextFormat("VELG EN CURSE (%d igjen)", cursesToPick), 110.0f, Color{ 190, 120, 255, 255 });
            UI::DrawCenteredText(GetEchelon(selectedEchelon).name.c_str(), CX, 158.0f, 20.0f, Color{ 255, 170, 70, 255 });

            const float cardWidth = 460.0f;
            const float cardHeight = 80.0f;
            for (size_t i = 0; i < curseChoices.size(); i++) {
                const Curse& curse = GetCurse(curseChoices[i]);
                bool isSelected = (int)i == selectedCurse;
                float x = CX - cardWidth / 2.0f;
                float y = 220.0f + i * (cardHeight + 20.0f);

                UI::DrawPanel({ x, y, cardWidth, cardHeight }, 1.0f, isSelected ? VIOLET : UI::PANEL_EDGE,
                              isSelected ? Color{ 60, 24, 90, 230 } : Color{ 22, 18, 30, 215 });
                DrawText(curse.name.c_str(), (int)x + 22, (int)y + 15, 24, isSelected ? Color{ 210, 160, 255, 255 } : WHITE);
                DrawText(curse.description.c_str(), (int)x + 22, (int)y + 48, 16, LIGHTGRAY);
            }

            hint("[W/S] Velg   |   [ENTER] Bekreft   |   [ESC] Tilbake");
            UI::EndCanvas();
        }
        else if (currentState == SHOP) {
            UI::DrawCastleBackdrop(uiTime, 0.7f);
            UI::BeginCanvas();
            shop.draw(totalGold);
            UI::EndCanvas();
        }
        else if (currentState == SETTINGS) {
            UI::DrawCastleBackdrop(uiTime, 0.7f);
            UI::BeginCanvas();
            heading("INNSTILLINGER", 100.0f, UI::GOLD_LIGHT);
            UI::DrawPanel({ CX - 290.0f, 180.0f, 580.0f, 250.0f });
            const char* rowNames[2] = { "Volum", "Musikk" };
            int rowValues[2] = { saveData.volume, saveData.musicVolume };
            for (int i = 0; i < 2; i++) {
                int y = 215 + i * 50;
                bool sel = i == settingsRow;
                if (sel) DrawRectangleLinesEx({ CX - 270.0f, (float)y - 10, 540.0f, 44.0f }, 2.0f, UI::GOLD_LIGHT);
                DrawText(rowNames[i], (int)CX - 250, y, 24, sel ? UI::GOLD_LIGHT : WHITE);
                UI::DrawBar({ CX - 120.0f, (float)y + 2, 280.0f, 22.0f }, rowValues[i] / 100.0f, i == 0 ? GOLD : Color{ 150, 110, 230, 255 }, Color{ 40, 34, 30, 255 });
                DrawText(TextFormat("%d%%", rowValues[i]), (int)CX + 180, y + 2, 22, WHITE);
            }
            DrawText("[W/S] velg   [A/D] juster", (int)CX - 250, 320, 18, GRAY);
            DrawText("Fullskjerm", (int)CX - 250, 370, 24, WHITE);
            DrawText(IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE) ? "PAA  [F11]" : "AV  [F11]", (int)CX - 50, 373, 20, UI::GOLD_LIGHT);
            hint("Trykk [ESC] for aa gaa tilbake");
            UI::EndCanvas();
        }
        else if (currentState == GAMEPLAY || currentState == LEVEL_UP || currentState == PAUSED) {
            // 2. TEGNING PÅ SKJERMEN (2.5D – se render3d.hpp)
            Camera3D view = MakeGameCamera(player.position, camera.rotation);

            // --- GULVLAGET: slottsgulv, skygger, AOE-ringer og varsler (vanlig 2D-tegning) ---
            BeginGroundLayer(player.position);
                if (inBossArena) {
                    DrawThroneRoomFloor(Arena::CENTER, Arena::RADIUS);
                } else {
                    DrawCastleFloor(player.position, View3D::GROUND_SIZE / 2.0f);
                }
                for (const auto& pickup : pickups) {
                    DrawEllipse((int)pickup.position.x + 2, (int)pickup.position.y + 2, pickup.radius, pickup.radius * 0.6f, Fade(BLACK, 0.3f));
                }
                player.drawShadow();
                for (auto& w : player.weapons) w->draw();
                for (auto& enemy : enemies) enemy->draw();
                DrawExplosions();
            EndGroundLayer();

            // --- 3D-LAGET: figurer, prosjektiler, søyler osv. ---
            BeginMode3D(view);
                DrawGroundLayer();
                if (inBossArena) DrawThroneRoom3D(Arena::CENTER, Arena::RADIUS);
                else DrawCastleProps3D(player.position, 1100.0f);

                // Pickups svever og vipper litt opp og ned
                float bob = (float)GetTime() * 4.0f;
                for (const auto& pickup : pickups) {
                    float h = 8.0f + 3.0f * sinf(bob + pickup.position.x * 0.05f);
                    if (pickup.type == PickupType::CHEST) {
                        // Skattekiste som snurrer sakte på gulvet
                        float yaw = (float)GetTime() * 40.0f;
                        Color wood = { 120, 70, 35, 255 }, gold = { 235, 190, 60, 255 };
                        ShadedCube(ToWorld3D(pickup.position, 7.0f), { 24.0f, 14.0f, 16.0f }, yaw, wood);
                        ShadedCube(ToWorld3D(pickup.position, 16.0f), { 25.0f, 5.0f, 17.0f }, yaw, Color{ 140, 85, 40, 255 });
                        ShadedCube(ToWorld3D(pickup.position, 10.0f), { 26.0f, 3.0f, 18.0f }, yaw, gold);
                        ShadedCube(ToWorld3D(pickup.position, 12.0f), { 5.0f, 6.0f, 18.5f }, yaw, gold);
                    } else if (pickup.type == PickupType::COIN) {
                        // Mynt som snurrer rundt seg selv
                        float spin = bob * 0.8f + pickup.position.y * 0.05f;
                        Vector3 axis = { cosf(spin) * 1.5f, 0.0f, sinf(spin) * 1.5f };
                        Vector3 center = ToWorld3D(pickup.position, h + 2.0f);
                        ShadedCylinder(Vector3Subtract(center, axis), Vector3Add(center, axis), 6.0f, 6.0f, GOLD, 12);
                    } else {
                        ShadedSphere(ToWorld3D(pickup.position, h), pickup.radius, pickup.color, 5, 8);
                    }
                }

                // Mange fiender -> færre trekanter per fiende (de er små på skjermen uansett)
                SetShapeDetail(1.0f - Clamp(((float)enemies.size() - 80.0f) / 300.0f, 0.0f, 0.45f));
                for (auto& enemy : enemies) {
                    // Nye fiender stiger opp av gulvet; elite-fiender er større. Treff gir hvitt glimt.
                    float rise = std::min(1.0f, enemy->age / 0.45f);
                    rise = 1.0f - (1.0f - rise) * (1.0f - rise);
                    float scale = enemy->modelScale * (0.5f + 0.5f * rise);
                    SetShadeFlash(enemy->hitFlash * 0.85f);
                    rlPushMatrix();
                        rlTranslatef(enemy->position.x, -(1.0f - rise) * 25.0f, enemy->position.y);
                        rlScalef(scale, scale, scale);
                        rlTranslatef(-enemy->position.x, 0.0f, -enemy->position.y);
                        enemy->draw3D();
                    rlPopMatrix();
                }
                SetShadeFlash(0.0f);
                SetShapeDetail(1.0f);
                for (auto& w : player.weapons) w->draw3D();
                player.drawModel();
                DrawEnemyShots3D();
                DrawExplosions3D();

                // --- VFX: glød, lyn, sjokkbølger og partikler (additivt, etter alt solid) ---
                VfxBegin(view);
                    if (inBossArena) DrawThroneRoomVfx(Arena::CENTER, Arena::RADIUS);
                    else DrawCastlePropsVfx(player.position, 1100.0f);
                    // Lysende ring under spilleren, så man finner seg selv i mylderet
                    {
                        float pulse = 0.85f + 0.15f * sinf(uiTime * 3.0f);
                        Color hero = { (unsigned char)(120 * pulse), (unsigned char)(100 * pulse), (unsigned char)(40 * pulse), 255 };
                        VfxDecal(VfxTex::GLOW, player.position, 80.0f, hero, 0.0f, 0.9f);
                        VfxDecal(VfxTex::SHOCKWAVE, player.position, 62.0f, Color{ 110, 90, 40, 255 }, uiTime * 30.0f, 1.0f);
                    }
                    for (const auto& pickup : pickups) {
                        if (pickup.type == PickupType::CHEST) {
                            // Lyssøyle så kista synes på avstand
                            float pulse = 0.7f + 0.3f * sinf(uiTime * 4.0f);
                            Color beam = { (unsigned char)(180 * pulse), (unsigned char)(140 * pulse), 40, 255 };
                            VfxBeam(VfxTex::GLOW, ToWorld3D(pickup.position, 0.0f), ToWorld3D(pickup.position, 260.0f), 50.0f, beam);
                            VfxBillboard(VfxTex::GLOW, ToWorld3D(pickup.position, 14.0f), 70.0f, Color{ 200, 160, 60, 255 });
                            VfxBillboard(VfxTex::SPARK, ToWorld3D(pickup.position, 22.0f), 40.0f, Color{ 255, 230, 150, 255 }, uiTime * 90.0f);
                            continue;
                        }
                        float h = 8.0f + 3.0f * sinf(bob + pickup.position.x * 0.05f);
                        Color glow = pickup.type == PickupType::COIN ? Color{ 255, 190, 60, 255 } : pickup.color;
                        VfxBillboard(VfxTex::GLOW, ToWorld3D(pickup.position, h), pickup.type == PickupType::COIN ? 26.0f : pickup.radius * 5.0f, Fade(glow, 0.55f));
                    }
                    for (auto& enemy : enemies) enemy->drawVfx();
                    DrawEnemyShotsVfx();
                    for (auto& w : player.weapons) w->drawVfx();
                    DrawVfxParticles();
                VfxEnd();
            EndMode3D();

            // --- HP-BARER over skadde fiender (ikke bossen – den har egen bar øverst) ---
            float barScale = HudScale();
            for (const auto& e : enemies) {
                if (e->hp >= e->maxHp || e->id == bossId) continue;
                Vector2 screen = GroundToScreen(view, e->position, e->modelHeight() + 8.0f);
                float pct = std::max(0.0f, (float)e->hp / (float)e->maxHp);
                float bw = 30.0f * barScale, bh = 4.0f * barScale;
                DrawRectangleRec({ screen.x - bw / 2.0f - 1.0f, screen.y - 1.0f, bw + 2.0f, bh + 2.0f }, Fade(BLACK, 0.7f));
                DrawRectangleRec({ screen.x - bw / 2.0f, screen.y, bw * pct, bh }, Color{ 90, 220, 90, 255 });
            }

            // --- SKADETALL (projiseres fra 3D-posisjonen, så teksten alltid er rett vei) ---
            DrawDamageNumbers(view);

            // --- HUD (festet til vinduskantene, skalerer med vindusstørrelsen) ---
            HudState hud;
            hud.player = &player;
            hud.enemies = &enemies;
            hud.pickups = &pickups;
            hud.curses = &activeCurses;
            hud.portrait = portraitRT.texture;
            hud.characterName = characters[selectedCharacter].name.c_str();
            hud.echelonName = GetEchelon(selectedEchelon).name.c_str();
            hud.gameTime = spawner.gameTime;
            hud.bossTime = GetBossTimer(selectedEchelon);
            hud.inBossArena = inBossArena;
            hud.bossId = bossId;
            hud.arenaCenter = Arena::CENTER;
            hud.arenaRadius = Arena::RADIUS;
            hud.runCoins = runCoins;
            hud.kills = Enemy::killCount;
            hud.cameraYaw = camera.rotation;
            hud.showMinimap = showMinimap;
            DrawGameHud(hud);

            // --- HORDE-VARSEL ---
            float sinceHorde = spawner.gameTime - spawner.lastHordeTime;
            if (!inBossArena && sinceHorde >= 0.0f && sinceHorde < 2.5f) {
                float a = sinceHorde < 2.0f ? 1.0f : (2.5f - sinceHorde) / 0.5f;
                float pulse = 0.7f + 0.3f * sinf(uiTime * 12.0f);
                UI::BeginCanvas();
                UI::DrawCenteredText("EN HORDE OMRINGER DEG!", CX, 190.0f, 40.0f, Fade(Color{ 255, (unsigned char)(80 * pulse), 60, 255 }, a), 3.0f);
                UI::EndCanvas();
            }

            // --- "KONGEN ER RASENDE!" ---
            for (const auto& e : enemies) {
                if (e->id != bossId) continue;
                float since = (float)GetTime() - static_cast<const Boss*>(e.get())->enragedAt;
                if (since >= 0.0f && since < 2.5f) {
                    float a = since < 2.0f ? 1.0f : (2.5f - since) / 0.5f;
                    UI::BeginCanvas();
                    UI::DrawCenteredText("KONGEN ER RASENDE!", CX, 190.0f, 44.0f, Fade(Color{ 255, 70, 50, 255 }, a), 3.0f);
                    UI::EndCanvas();
                }
            }

            // --- "KONGENS TRONSAL"-INTRO ETTER TELEPORT ---
            if (currentState == GAMEPLAY && inBossArena && arenaIntroTimer > 0.0f) {
                float t = arenaIntroTimer / Arena::INTRO_TIME; // 1 -> 0
                // Hvitt blink som fader ut, så teksten
                DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(WHITE, std::max(0.0f, (t - 0.7f) / 0.3f)));
                UI::BeginCanvas();
                float a = std::min(1.0f, t * 2.0f);
                UI::DrawCenteredText("KONGENS TRONSAL", CX, VH / 2.0f - 80.0f, 64.0f, Fade(Color{ 230, 40, 50, 255 }, a), 4.0f);
                UI::DrawCenteredText("Kongen venter...", CX, VH / 2.0f, 24.0f, Fade(UI::GOLD_LIGHT, a));
                UI::EndCanvas();
            }

            if (currentState == PAUSED) {
                DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(Color{ 8, 6, 14, 255 }, 0.8f));
                UI::BeginCanvas();
                UI::DrawCenteredText("PAUSE", 290.0f, 120.0f, 44.0f, UI::GOLD_LIGHT, 3.0f);

                // --- Valg til venstre ---
                const char* options[2] = { "FORTSETT", "GI OPP" };
                for (int i = 0; i < 2; i++) {
                    bool sel = i == pauseOption;
                    Rectangle r = { 140.0f, 200.0f + i * 70.0f, 300.0f, 52.0f };
                    UI::DrawPanel(r, 1.0f, sel ? UI::GOLD_LIGHT : UI::PANEL_EDGE, sel ? Color{ 150, 24, 36, 235 } : Color{ 22, 18, 30, 220 });
                    UI::DrawCenteredText(options[i], r.x + r.width / 2.0f, r.y + 14.0f, 26.0f, sel ? UI::GOLD_LIGHT : WHITE);
                }
                DrawText("Gi opp: du beholder gullet du har tjent", 140, 350, 16, Color{ 190, 180, 165, 255 });
                DrawText(TextFormat("Tid %02d:%02d   Level %d   Kills %d", (int)spawner.gameTime / 60, (int)spawner.gameTime % 60, player.level, Enemy::killCount), 140, 380, 18, WHITE);

                // --- Stats til høyre: alt som påvirker runden, med forklarte tall ---
                Rectangle sp = { 520.0f, 130.0f, 620.0f, 480.0f };
                UI::DrawPanel(sp);
                UI::DrawCenteredText("DINE STATS", sp.x + sp.width / 2.0f, sp.y + 16.0f, 24.0f, UI::GOLD_LIGHT);
                // NB: TextFormat gjenbruker noen få interne buffere, så verdiene må kopieres til std::string
                struct Row { const char* label; std::string value; };
                CombatModifiers cm = player.combatModifiers();
                Row rows[] = {
                    { "HP", TextFormat("%.0f / %.0f", player.hp, player.maxHp) },
                    { "Armor", TextFormat("%.0f  (-%.0f%% skade)", player.armor, player.armorReduction() * 100.0f) },
                    { "Dodge", TextFormat("%.0f%%", player.evasion * 100.0f) },
                    { "Regen", TextFormat("%.1f HP/s", player.hpRegen) },
                    { "Fart", TextFormat("%.0f", player.speed) },
                    { "Skade", TextFormat("x%.2f", cm.damageMult) },
                    { "Cooldown", TextFormat("x%.2f", cm.cooldownMult) },
                    { "Omraade", TextFormat("x%.2f", cm.areaMult) },
                    { "Ekstra prosjektiler", TextFormat("+%d", cm.extraProjectiles) },
                    { "Pickup-radius", TextFormat("%.0f", player.lootRadius) },
                    { "XP", TextFormat("x%.2f", player.xpMultiplier) },
                    { "Aegis", TextFormat("%d", player.aegis) },
                    { "Kritisk treff", TextFormat("%.0f%%  (x2 skade)", player.critChance * 100.0f) },
                };
                int rowCount = (int)(sizeof(rows) / sizeof(rows[0]));
                for (int i = 0; i < rowCount; i++) {
                    int col = i / 7, row = i % 7;
                    float x = sp.x + 30.0f + col * 300.0f;
                    float y = sp.y + 60.0f + row * 27.0f;
                    DrawText(rows[i].label, (int)x, (int)y, 18, Color{ 190, 180, 165, 255 });
                    DrawText(rows[i].value.c_str(), (int)(x + 270.0f) - MeasureText(rows[i].value.c_str(), 18), (int)y, 18, WHITE);
                }

                // Stat-oppgraderinger tatt denne runden (ikon + prikker)
                DrawLineEx({ sp.x + 30.0f, sp.y + 256.0f }, { sp.x + sp.width - 30.0f, sp.y + 256.0f }, 1.0f, Fade(UI::GOLD_DARK, 0.8f));
                DrawText("Stat-oppgraderinger fra level up:", (int)sp.x + 30, (int)sp.y + 266, 16, Color{ 190, 180, 165, 255 });
                for (int i = 0; i < (int)StatBoost::COUNT; i++) {
                    int col = i % 5, row = i / 5;
                    float x = sp.x + 22.0f + col * 116.0f;
                    float y = sp.y + 296.0f + row * 80.0f;
                    Vector2 ic = { x + 22.0f, y + 22.0f };
                    bool has = player.statBoosts[i] > 0;
                    DrawCircleV(ic, 22.0f, UI::INK);
                    DrawCircleV(ic, 20.0f, has ? Color{ 70, 50, 70, 255 } : Color{ 40, 36, 48, 255 });
                    DrawStatBoostIcon((StatBoost)i, ic, 15.0f);
                    if (!has) DrawCircleV(ic, 20.0f, Fade(BLACK, 0.55f));
                    DrawText(GetStatBoostInfo((StatBoost)i).name, (int)x, (int)y + 48, 12, has ? WHITE : GRAY);
                    for (int l = 0; l < MAX_STAT_BOOST; l++) {
                        DrawRectangle((int)x + 50 + l * 11, (int)y + 16, 8, 8, l < player.statBoosts[i] ? UI::GOLD_LIGHT : Color{ 60, 54, 70, 255 });
                    }
                }

                hint("[W/S] Velg   |   [ENTER] Bekreft   |   [ESC] Fortsett");
                UI::EndCanvas();
            }

            if (currentState == LEVEL_UP) {
                float since = (float)(GetTime() - levelUpStart);

                // Mørklegg hele vinduet bak menyen
                DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(Color{ 8, 6, 14, 255 }, std::min(0.82f, since * 4.0f)));
                UI::BeginCanvas();

                // --- Tittel: "LEVEL UP!" som spretter inn, med strålekrans bak ---
                UI::DrawSunburst({ CX, 88.0f }, 260.0f, 18, uiTime * 0.25f, Fade(UI::GOLD_LIGHT, 0.16f));
                float popT = std::min(1.0f, since / 0.35f);
                float titleSize = std::round(64.0f * (0.6f + 0.4f * popT + 0.12f * sinf(popT * PI)));
                UI::DrawCenteredText(levelUpFromChest ? "SKATTEKISTE!" : "LEVEL UP!", CX, 88.0f - titleSize * 0.45f, titleSize, UI::GOLD_LIGHT, 4.0f);
                const char* sub = levelUpFromChest ? "En elite-fiende slapp en kiste  -  velg en gratis belonning"
                                                   : TextFormat("Du er naa level %d  -  velg en belonning", player.level);
                UI::DrawCenteredText(sub, CX, 134.0f, 20.0f, Color{ 230, 220, 200, 255 });

                // --- Kortene ---
                const int count = (int)activeUpgradeChoices.size();
                const float cardW = count > 3 ? 240.0f : 260.0f;
                const float cardH = 360.0f;
                const float gap = 26.0f;
                const float totalW = count * cardW + (count - 1) * gap;
                const float baseY = 188.0f;

                // Tegn det valgte kortet sist så det havner øverst
                for (int pass = 0; pass < 2; pass++) {
                    for (int i = 0; i < count; i++) {
                        bool isSelected = (i == selectedUpgradeOption);
                        if ((pass == 1) != isSelected) continue;
                        const AbilityChoice& choice = activeUpgradeChoices[i];

                        // Kortene glir opp ett og ett
                        float enter = Clamp((since - 0.08f * i) / 0.3f, 0.0f, 1.0f);
                        float ease = 1.0f - (1.0f - enter) * (1.0f - enter);
                        float lift = isSelected ? 16.0f + 3.0f * sinf(uiTime * 3.0f) : 0.0f;
                        float cardX = CX - totalW / 2.0f + i * (cardW + gap);
                        float cardY = baseY + (1.0f - ease) * 80.0f - lift;
                        Rectangle card = { cardX, cardY, cardW, cardH };
                        Color accent = choice.color;

                        if (isSelected) {
                            UI::DrawSunburst({ cardX + cardW / 2.0f, cardY + 110.0f }, cardW * 0.95f, 14, -uiTime * 0.4f, Fade(accent, 0.22f * ease));
                            UI::DrawGlow({ cardX + cardW / 2.0f, cardY + cardH / 2.0f }, cardW, Fade(accent, 0.25f * ease), Fade(accent, 0.0f));
                        }

                        UI::DrawPanel(card, 1.0f, isSelected ? UI::GOLD_LIGHT : UI::PANEL_EDGE,
                                      isSelected ? Color{ 44, 32, 46, 250 } : Color{ 26, 22, 34, 245 });

                        // Fargebanner øverst med rutemønster (narredrakt)
                        Rectangle banner = { cardX + 6.0f, cardY + 6.0f, cardW - 12.0f, 78.0f };
                        DrawRectangleRec(banner, accent);
                        for (int d = 0; d < 9; d++) {
                            float dx = banner.x + 14.0f + d * (banner.width - 28.0f) / 8.0f;
                            float dy = banner.y + banner.height / 2.0f;
                            DrawTriangle({ dx, dy - 10 }, { dx - 8, dy }, { dx, dy + 10 }, Fade(WHITE, 0.12f));
                            DrawTriangle({ dx, dy - 10 }, { dx, dy + 10 }, { dx + 8, dy }, Fade(BLACK, 0.12f));
                        }
                        DrawRectangleGradientV((int)banner.x, (int)banner.y, (int)banner.width, (int)banner.height, Fade(WHITE, 0.18f), Fade(BLACK, 0.25f));
                        DrawRectangleRec({ banner.x, banner.y + banner.height - 3.0f, banner.width, 3.0f }, UI::GOLD_DARK);

                        // Merke: NY! / LV x > y / HEAL
                        std::string title;
                        std::string badge;
                        Color badgeColor;
                        Weapon* existing = choice.type == ChoiceType::UPGRADE_ABILITY ? player.findAbility(choice.ability) : nullptr;
                        if (choice.type == ChoiceType::NEW_ABILITY) {
                            title = GetAbilityDefinition(choice.ability).name;
                            badge = "NY!";
                            badgeColor = Color{ 110, 220, 110, 255 };
                        } else if (choice.type == ChoiceType::STAT) {
                            title = choice.title;
                            badge = "STAT";
                            badgeColor = Color{ 150, 200, 255, 255 };
                        } else if (choice.type == ChoiceType::UPGRADE_ABILITY && existing) {
                            title = GetAbilityDefinition(choice.ability).name;
                            badge = TextFormat("LV %d > %d", existing->level, existing->level + 1);
                            badgeColor = UI::GOLD_LIGHT;
                        } else {
                            title = choice.title;
                            badge = "HEAL";
                            badgeColor = Color{ 255, 120, 120, 255 };
                        }
                        float bw = MeasureText(badge.c_str(), 16) + 18.0f;
                        Rectangle badgeRect = { cardX + cardW - bw - 12.0f, cardY + 12.0f, bw, 24.0f };
                        DrawRectangleRounded(badgeRect, 0.5f, 6, UI::INK);
                        DrawRectangleRoundedLinesEx(badgeRect, 0.5f, 6, 2.0f, badgeColor);
                        DrawText(badge.c_str(), (int)(badgeRect.x + 9), (int)badgeRect.y + 5, 16, badgeColor);

                        // Stort ikon som overlapper banneret
                        Vector2 ic = { cardX + cardW / 2.0f, cardY + 104.0f };
                        DrawCircleV(ic, 50.0f, UI::INK);
                        DrawCircleV(ic, 47.0f, isSelected ? UI::GOLD_LIGHT : UI::GOLD_DARK);
                        DrawCircleV(ic, 42.0f, Color{ 40, 30, 46, 255 });
                        UI::DrawGlow(ic, 42.0f, Fade(accent, 0.45f), Fade(accent, 0.0f));
                        float iconSize = isSelected ? 30.0f + sinf(uiTime * 5.0f) : 29.0f;
                        if (choice.type == ChoiceType::STAT) DrawStatBoostIcon(choice.stat, ic, iconSize);
                        else DrawAbilityIcon(choice.ability, ic, iconSize);

                        // Navn
                        UI::DrawCenteredText(title.c_str(), cardX + cardW / 2.0f, cardY + 166.0f, title.size() > 12 ? 22.0f : 26.0f, isSelected ? UI::GOLD_LIGHT : WHITE);

                        // Nivå-prikker: fylte = nåværende, blinkende grønn = den du får
                        if (choice.type != ChoiceType::HEAL) {
                            bool isStat = choice.type == ChoiceType::STAT;
                            int current = isStat ? player.statBoosts[(int)choice.stat] : (existing ? existing->level : 0);
                            int maxPips = isStat ? MAX_STAT_BOOST : MAX_ABILITY_LEVEL;
                            const float pip = 12.0f, pipGap = 5.0f;
                            float pipsW = maxPips * pip + (maxPips - 1) * pipGap;
                            float px = cardX + cardW / 2.0f - pipsW / 2.0f;
                            for (int l = 0; l < maxPips; l++) {
                                Rectangle pr = { px + l * (pip + pipGap), cardY + 202.0f, pip, pip };
                                Color pc = Color{ 60, 54, 70, 255 };
                                if (l < current) pc = UI::GOLD_LIGHT;
                                else if (l == current) pc = Fade(Color{ 120, 235, 120, 255 }, 0.55f + 0.45f * sinf(uiTime * 8.0f));
                                DrawRectangleRec({ pr.x - 1.5f, pr.y - 1.5f, pr.width + 3.0f, pr.height + 3.0f }, UI::INK);
                                DrawRectangleRec(pr, pc);
                            }
                        }

                        // Beskrivelse
                        DrawLineEx({ cardX + 24.0f, cardY + 228.0f }, { cardX + cardW - 24.0f, cardY + 228.0f }, 1.0f, Fade(UI::GOLD_DARK, 0.8f));
                        UI::DrawWrappedText(choice.description.c_str(), cardX + 20.0f, cardY + 242.0f, cardW - 40.0f, 16.0f, Color{ 215, 205, 190, 255 }, true);

                        if (isSelected) {
                            // Tast-hint nederst på det valgte kortet
                            Rectangle pick = { cardX + 30.0f, cardY + cardH - 50.0f, cardW - 60.0f, 36.0f };
                            DrawRectangleRounded(pick, 0.3f, 6, UI::ROYAL_RED);
                            DrawRectangleRoundedLinesEx(pick, 0.3f, 6, 2.0f, UI::GOLD_LIGHT);
                            UI::DrawCenteredText("[ENTER] VELG", pick.x + pick.width / 2.0f, pick.y + 9.0f, 18.0f, UI::GOLD_LIGHT);
                        } else {
                            // Litt mørkere når de ikke er valgt, så det valgte kortet popper
                            DrawRectangleRec(card, Fade(BLACK, 0.25f));
                        }

                        // Fade inn
                        if (ease < 1.0f) DrawRectangleRec({ card.x - 4, card.y - 4, card.width + 12, card.height + 12 }, Fade(Color{ 8, 6, 14, 255 }, (1.0f - ease) * 0.82f));
                    }
                }

                hint("[A/D] eller [Piltaster] for aa bla   |   [ENTER] for aa velge");
                UI::EndCanvas();
            }
        }

        else if (currentState == GAME_OVER) {
            UI::DrawCastleBackdrop(uiTime, 0.72f);
            UI::BeginCanvas();
            const char* title = lastRun.bossDefeated ? TextFormat("%s FULLFOERT!", GetEchelon(lastRun.echelon).name.c_str())
                                : lastRun.died ? "DU DOEDE" : "RUN AVSLUTTET";
            Color headingColor = lastRun.bossDefeated ? Color{ 110, 230, 120, 255 } : (lastRun.died ? Color{ 235, 60, 60, 255 } : UI::GOLD_LIGHT);
            UI::DrawCenteredText(title, CX, 60.0f, 48.0f, headingColor, 3.0f);
            if (lastRun.unlockedNewEchelon) {
                UI::DrawCenteredText(TextFormat("%s er laast opp!", GetEchelon(lastRun.echelon + 1).name.c_str()), CX, 118.0f, 24.0f, Color{ 255, 170, 70, 255 });
            }

            float px = CX - 260.0f;
            UI::DrawPanel({ px, 155.0f, 520.0f, 430.0f });
            int minutes = (int)lastRun.timeSurvived / 60;
            int seconds = (int)lastRun.timeSurvived % 60;
            const char* stats = TextFormat("Tid: %02d:%02d    Level: %d    Kills: %d", minutes, seconds, lastRun.level, lastRun.kills);
            DrawText(stats, (int)CX - MeasureText(stats, 22) / 2, 180, 22, WHITE);
            DrawLineEx({ px + 30.0f, 218.0f }, { px + 490.0f, 218.0f }, 1.0f, Fade(UI::GOLD_DARK, 0.8f));

            // Gull-linjer: navn til venstre, beløp høyrejustert
            int y = 240;
            auto goldLine = [&](const char* label, const char* value, Color color) {
                DrawText(label, (int)px + 40, y, 22, color);
                DrawText(value, (int)px + 480 - MeasureText(value, 22), y, 22, color);
                y += 35;
            };
            goldLine("Mynter plukket opp", TextFormat("%d g", lastRun.coinGold), LIGHTGRAY);
            goldLine("Overlevd tid", TextFormat("%d g", lastRun.survivalGold), LIGHTGRAY);
            goldLine("Level-bonus", TextFormat("%d g", lastRun.levelGold), LIGHTGRAY);
            if (lastRun.bossGold > 0) goldLine("Boss-bonus", TextFormat("%d g", lastRun.bossGold), GREEN);
            if (lastRun.greedMult > 1.0f) goldLine("Greed", TextFormat("x%.1f", lastRun.greedMult), LIGHTGRAY);
            DrawLineEx({ px + 40.0f, (float)y }, { px + 480.0f, (float)y }, 2.0f, UI::GOLD_DARK);
            UI::DrawOutlinedText("TOTALT", px + 40.0f, y + 15.0f, 30.0f, UI::GOLD_LIGHT);
            const char* total = TextFormat("+%d g", lastRun.totalGold);
            UI::DrawOutlinedText(total, px + 480.0f - MeasureText(total, 30), y + 15.0f, 30.0f, UI::GOLD_LIGHT);
            DrawText(TextFormat("Gull i banken: %d g", totalGold), (int)px + 40, y + 62, 20, GOLD);

            hint("[ENTER] Tilbake til menyen");
            UI::EndCanvas();
        }

        EndDrawing();
    }

    // Rydd opp teksturer i minnet
    for (auto& tex : characterTextures) {
        UnloadTexture(tex);
    }
    UnloadTexture(enemyTexture);

    saveProgress();
    for (auto& rt : clownPreviews) UnloadRenderTexture(rt);
    UnloadRenderTexture(portraitRT);
    UnloadVfx();
    UnloadRenderer3D();
    UnloadGameMusic();
    UnloadGameAudio();
    CloseWindow();
    return 0;
}