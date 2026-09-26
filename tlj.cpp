#include <raylib.h>
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
    InitGameAudio();

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
        lastPlayerLevel = 1;
        runCoins = 0;
        Enemy::killCount = 0;
        inBossArena = false;
        bossId = -1;
        arenaIntroTimer = 0.0f;
        activeCurses.clear();

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
    };

    // Skade på spilleren fra fiender (kontakt og eksplosjoner)
    auto hurtPlayer = [&](float rawDamage) {
        float taken = player.takeDamage(rawDamage);
        if (taken > 0.0f) {
            SpawnDamageNumber(player.position, std::max(1, (int)(taken + 0.5f)), RED);
            PlaySfx(Sfx::PLAYER_HURT);
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
            // Volum med venstre/høyre (A/D) i steg på 10%
            if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) saveData.volume = std::min(100, saveData.volume + 10);
            if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) saveData.volume = std::max(0, saveData.volume - 10);
            SetGameVolume(saveData.volume / 100.0f);

            if (IsKeyPressed(KEY_P) || IsKeyPressed(KEY_B) || IsKeyPressed(KEY_ESCAPE)) {
                saveProgress();
                currentState = MAIN_MENU;
            }
        }
        else if (currentState == GAMEPLAY) {

            if(player.level > lastPlayerLevel){
                lastPlayerLevel = player.level;
                currentState = LEVEL_UP;
                PlaySfx(Sfx::LEVEL_UP);
                selectedUpgradeOption = 0;
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
                    else player.addXP(static_cast<int>(p.value * player.xpMultiplier));
                }
                pickups.clear();
                enemies.clear();
                ClearExplosions();

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

            // Oppdater alle fiender (bossen står stille mens intro-teksten vises)
            if (arenaIntroTimer <= 0.0f) {
                for (auto& enemy : enemies) {
                    enemy->update(player.position);
                    if (inBossArena) enemy->position = ClampToArena(enemy->position, enemy->hitRadius);
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
            CombatModifiers mods = player.combatModifiers();
            for (auto& w : player.weapons) {
                w->update(deltaTime, player.position, enemies, pickups, mods);
            }

            UpdateDamageNumbers(deltaTime);

            // Fjerne døde fiender (f.eks. kamikaze som har sprengt seg selv)
            for (auto& e : enemies) {
                if (e->isDead()) e->onDeath();
            }
            enemies.erase(
                std::remove_if(enemies.begin(), enemies.end(),
                    [](const std::unique_ptr<Enemy>& e) { return e->isDead(); }),
                enemies.end()
            );

            // Eksplosjoner som treffer spilleren
            float explosionDamage = UpdateExplosions(deltaTime, player.position, playerHitRadius);
            if (explosionDamage > 0.0f && player.invulnerableTimer <= 0.0f) {
                hurtPlayer(explosionDamage);
            }

            // Bossen er slått når den ikke lenger finnes i fiende-lista
            bool bossDefeated = inBossArena && bossId >= 0 &&
                std::none_of(enemies.begin(), enemies.end(), [&](const std::unique_ptr<Enemy>& e) { return e->id == bossId; });

            // ESC/P avslutter runden (man får fortsatt gullet man har tjent)
            bool gaveUp = IsKeyPressed(KEY_P) || IsKeyPressed(KEY_ESCAPE);

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
        else if (currentState == GAME_OVER) {
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ESCAPE)) {
                currentState = MAIN_MENU;
            }
        }
            else if(currentState == LEVEL_UP) {
                if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) {
                    selectedUpgradeOption = (selectedUpgradeOption + 1) % activeUpgradeChoices.size();
            }
                if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) {
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
            previewCam.position = { 0.0f, 52.0f, 125.0f };
            previewCam.target = { 0.0f, 30.0f, 0.0f };
            previewCam.up = { 0.0f, 1.0f, 0.0f };
            previewCam.fovy = 38.0f;
            previewCam.projection = CAMERA_PERSPECTIVE;
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
        if (currentState == GAMEPLAY || currentState == LEVEL_UP) {
            // Portrett av klovnens hode til HUD-en
            float headHeight = (player.clown == ClownStyle::GEEK) ? 58.0f : 47.0f;
            Camera3D portraitCam{};
            portraitCam.position = { 18.0f, headHeight + 6.0f, 64.0f };
            portraitCam.target = { 0.0f, headHeight + 1.0f, 0.0f };
            portraitCam.up = { 0.0f, 1.0f, 0.0f };
            portraitCam.fovy = 26.0f;
            portraitCam.projection = CAMERA_PERSPECTIVE;
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
                if (isSelected) DrawCircleGradient((int)(destRect.x + PREVIEW_W / 2.0f), (int)(destRect.y + PREVIEW_H * 0.6f), 110.0f, Fade(UI::GOLD_LIGHT, 0.25f), Fade(UI::GOLD_LIGHT, 0.0f));
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
                const char* values[4] = { TextFormat("%.0f", finalHp), TextFormat("%.0f", finalSpeed), TextFormat("%.1f", finalArmor), TextFormat("%.0f", characters[i].lootRadius) };
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
            UI::DrawPanel({ 50.0f, 100.0f, VW - 100.0f, 470.0f });
            shop.draw(totalGold);
            UI::EndCanvas();
        }
        else if (currentState == SETTINGS) {
            UI::DrawCastleBackdrop(uiTime, 0.7f);
            UI::BeginCanvas();
            heading("INNSTILLINGER", 100.0f, UI::GOLD_LIGHT);
            UI::DrawPanel({ CX - 290.0f, 180.0f, 580.0f, 200.0f });
            DrawText("Volum", (int)CX - 250, 220, 24, WHITE);
            UI::DrawBar({ CX - 120.0f, 222.0f, 280.0f, 22.0f }, saveData.volume / 100.0f, GOLD, Color{ 40, 34, 30, 255 });
            DrawText(TextFormat("%d%%", saveData.volume), (int)CX + 180, 222, 22, WHITE);
            DrawText("[A/D] eller [Venstre/Hoeyre] for aa justere", (int)CX - 250, 265, 18, GRAY);
            DrawText("Fullskjerm", (int)CX - 250, 315, 24, WHITE);
            DrawText(IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE) ? "PAA  [F11]" : "AV  [F11]", (int)CX - 50, 318, 20, UI::GOLD_LIGHT);
            hint("Trykk [ESC] for aa gaa tilbake");
            UI::EndCanvas();
        }
        else if (currentState == GAMEPLAY || currentState == LEVEL_UP) {
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

                // Pickups svever og vipper litt opp og ned
                float bob = (float)GetTime() * 4.0f;
                for (const auto& pickup : pickups) {
                    float h = 8.0f + 3.0f * sinf(bob + pickup.position.x * 0.05f);
                    if (pickup.type == PickupType::COIN) {
                        // Mynt som snurrer rundt seg selv
                        float spin = bob * 0.8f + pickup.position.y * 0.05f;
                        Vector3 axis = { cosf(spin) * 1.5f, 0.0f, sinf(spin) * 1.5f };
                        Vector3 center = ToWorld3D(pickup.position, h + 2.0f);
                        ShadedCylinder(Vector3Subtract(center, axis), Vector3Add(center, axis), 6.0f, 6.0f, GOLD, 12);
                    } else {
                        ShadedSphere(ToWorld3D(pickup.position, h), pickup.radius, pickup.color, 5, 8);
                    }
                }

                for (auto& enemy : enemies) enemy->draw3D();
                for (auto& w : player.weapons) w->draw3D();
                player.drawModel();
                DrawExplosions3D();
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

            if (currentState == LEVEL_UP) {
                // Mørklegg hele vinduet bak menyen
                DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(Color{ 8, 6, 14, 255 }, 0.8f));
                UI::BeginCanvas();
                heading("LEVEL UP!", 70.0f, UI::GOLD_LIGHT);
                UI::DrawCenteredText("Velg en oppgradering", CX, 118.0f, 20.0f, Color{ 220, 210, 190, 255 });

                const float cardWidth = 480.0f;
                const float cardHeight = 80.0f;
                float startY = (activeUpgradeChoices.size() > 3) ? 170.0f : 210.0f; // Plass til 4 valg med "Flere valg"-oppgraderingen

                for (size_t i = 0; i < activeUpgradeChoices.size(); i++) {
                    float cardY = startY + i * (cardHeight + 16.0f);
                    float cardX = CX - cardWidth / 2.0f;
                    bool isSelected = ((int)i == selectedUpgradeOption);
                    const AbilityChoice& choice = activeUpgradeChoices[i];

                    UI::DrawPanel({ cardX, cardY, cardWidth, cardHeight }, 1.0f, isSelected ? UI::GOLD_LIGHT : UI::PANEL_EDGE,
                                  isSelected ? Color{ 48, 34, 44, 240 } : Color{ 22, 18, 30, 230 });
                    // Fargestripe som viser hvilken ability det gjelder
                    DrawRectangleRec({ cardX + 6.0f, cardY + 6.0f, 8.0f, cardHeight - 12.0f }, choice.color);

                    Color textColor = isSelected ? UI::GOLD_LIGHT : WHITE;
                    DrawText(choice.title.c_str(), (int)cardX + 28, (int)cardY + 15, 22, textColor);
                    DrawText(choice.description.c_str(), (int)cardX + 28, (int)cardY + 46, 14, LIGHTGRAY);
                    if (isSelected) {
                        float dx = cardX - 18.0f + 3.0f * sinf(uiTime * 6.0f);
                        float dy = cardY + cardHeight / 2.0f;
                        DrawTriangle({ dx - 9, dy - 9 }, { dx - 9, dy + 9 }, { dx + 3, dy }, UI::GOLD_LIGHT);
                    }
                }

                hint("[W/S] eller [Piltaster] og [ENTER] for aa velge");
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
    UnloadRenderer3D();
    UnloadGameAudio();
    CloseWindow();
    return 0;
}