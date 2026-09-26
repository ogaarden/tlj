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
        // TEGNING ON SCREEN
        // -------------------------------------------------------------
        BeginDrawing();
        ClearBackground(BLACK);

        if (currentState == MAIN_MENU) {
            DrawText("THE LAST JESTER", Settings::SCREEN_WIDTH / 2 - 180, 120, 40, YELLOW);

            const char* options[] = { "PLAY", "SHOP", "SETTINGS", "QUIT" };
            for (int i = 0; i < 4; i++) {
                Color color = (i == mainOption) ? YELLOW : WHITE;
                const char* prefix = (i == mainOption) ? "> " : "  ";
                DrawText(TextFormat("%s%s", prefix, options[i]), Settings::SCREEN_WIDTH / 2 - 60, 260 + (i * 50), 28, color);
            }

            DrawText(TextFormat("Gull: %d g", totalGold), 30, Settings::SCREEN_HEIGHT - 50, 22, GOLD);
        }
        else if (currentState == CHARACTER_SELECT) {
            DrawText("VELG KARAKTER", Settings::SCREEN_WIDTH / 2 - 130, 50, 30, WHITE);

            int cardWidth = 220;
            int cardHeight = 350;
            int startX = (Settings::SCREEN_WIDTH - (static_cast<int>(characters.size()) * cardWidth + (static_cast<int>(characters.size()) - 1) * 20)) / 2;

            for (size_t i = 0; i < characters.size(); i++) {
                int posX = startX + static_cast<int>(i) * (cardWidth + 20);
                bool isSelected = (static_cast<int>(i) == selectedCharacter);

                // Ramme
                DrawRectangleLinesEx({ (float)posX, 100.0f, (float)cardWidth, (float)cardHeight }, isSelected ? 4.0f : 2.0f, isSelected ? YELLOW : DARKGRAY);
                
                // Navn
                DrawText(characters[i].name.c_str(), posX + 15, 115, 22, isSelected ? YELLOW : WHITE);

                // Karakterbilde (skalert til 80x80 px midt på kortet)
                Texture2D icon = characterTextures[i];
                Rectangle srcRect = { 0.0f, 0.0f, (float)icon.width, (float)icon.height };
                Rectangle destRect = { posX + (cardWidth / 2.0f) - 40.0f, 150.0f, 80.0f, 80.0f };
                DrawTexturePro(icon, srcRect, destRect, { 0.0f, 0.0f }, 0.0f, WHITE);

                // Beskrivelse og oppgangende stats med Shop-bonuser
                float finalHp = characters[i].maxHp * shop.hpMult();
                float finalSpeed = characters[i].speed * shop.speedMult();
                float finalArmor = characters[i].armor + shop.armorBonus();

                DrawText(characters[i].description.c_str(), posX + 15, 240, 12, GRAY);
                DrawText(TextFormat("HP: %.0f", finalHp), posX + 15, 270, 16, WHITE);
                DrawText(TextFormat("Fart: %.0f", finalSpeed), posX + 15, 295, 16, WHITE);
                DrawText(TextFormat("Armor: %.1f", finalArmor), posX + 15, 320, 16, WHITE);
                DrawText(TextFormat("Radius: %.0f", characters[i].lootRadius), posX + 15, 345, 16, WHITE);
                DrawText(TextFormat("Aegis: +%d", shop.aegisBonus()), posX + 15, 370, 16, GREEN);
            }

            DrawText("[A/D] Velg karakter   |   [ENTER] Videre   |   [ESC] Tilbake", Settings::SCREEN_WIDTH / 2 - 290, 480, 20, GRAY);
        }
        else if (currentState == ECHELON_SELECT) {
            DrawText("VELG ECHELON", Settings::SCREEN_WIDTH / 2 - MeasureText("VELG ECHELON", 32) / 2, 30, 32, ORANGE);

            // --- Liste over alle 10 echelons (låste er mørke) ---
            const int listX = 60;
            const int rowHeight = 50;
            const int listY = 90;
            for (int e = 1; e <= MAX_ECHELON; e++) {
                const EchelonData& info = GetEchelon(e);
                bool unlocked = e <= saveData.unlockedEchelon;
                bool isSelected = e == selectedEchelon;
                int y = listY + (e - 1) * rowHeight;

                Color bg = isSelected ? Fade(ORANGE, 0.25f) : Fade(DARKGRAY, unlocked ? 0.35f : 0.15f);
                DrawRectangle(listX, y, 620, rowHeight - 6, bg);
                if (isSelected) DrawRectangleLines(listX, y, 620, rowHeight - 6, ORANGE);

                if (unlocked) {
                    int bossTime = (int)GetBossTimer(e);
                    DrawText(info.name.c_str(), listX + 12, y + 6, 20, isSelected ? YELLOW : WHITE);
                    DrawText(info.description.c_str(), listX + 12, y + 27, 14, LIGHTGRAY);
                    const char* timeText = TextFormat("Boss %02d:%02d", bossTime / 60, bossTime % 60);
                    DrawText(timeText, listX + 610 - MeasureText(timeText, 16), y + 14, 16, GRAY);
                } else {
                    DrawText(info.name.c_str(), listX + 12, y + 12, 20, Fade(GRAY, 0.4f));
                    DrawText("LAAST", listX + 610 - MeasureText("LAAST", 18), y + 12, 18, Fade(GRAY, 0.4f));
                }
            }

            // --- Alle effekter som gjelder for valgt echelon (de stacker) ---
            const int panelX = 730;
            DrawText(TextFormat("%s - aktive effekter:", GetEchelon(selectedEchelon).name.c_str()), panelX, 90, 20, WHITE);
            int lineY = 125;
            for (int e = 1; e <= selectedEchelon; e++) {
                Color c = (e == selectedEchelon) ? YELLOW : LIGHTGRAY;
                DrawText(TextFormat("E%d: %s", e, GetEchelon(e).description.c_str()), panelX, lineY, 18, c);
                lineY += 28;
            }
            int bossTime = (int)GetBossTimer(selectedEchelon);
            DrawText(TextFormat("Boss etter %02d:%02d", bossTime / 60, bossTime % 60), panelX, lineY + 15, 20, ORANGE);

            DrawText("[W/S] Velg   |   [ENTER] Start   |   [ESC] Tilbake", Settings::SCREEN_WIDTH / 2 - 230, Settings::SCREEN_HEIGHT - 45, 20, GRAY);
        }
        else if (currentState == CURSE_SELECT) {
            const char* title = TextFormat("VELG EN CURSE (%d igjen)", cursesToPick);
            DrawText(title, Settings::SCREEN_WIDTH / 2 - MeasureText(title, 32) / 2, 110, 32, PURPLE);
            DrawText(GetEchelon(selectedEchelon).name.c_str(), Settings::SCREEN_WIDTH / 2 - MeasureText(GetEchelon(selectedEchelon).name.c_str(), 20) / 2, 155, 20, ORANGE);

            int cardWidth = 450;
            int cardHeight = 80;
            for (size_t i = 0; i < curseChoices.size(); i++) {
                const Curse& curse = GetCurse(curseChoices[i]);
                bool isSelected = (int)i == selectedCurse;
                int x = Settings::SCREEN_WIDTH / 2 - cardWidth / 2;
                int y = 220 + (int)i * (cardHeight + 20);

                DrawRectangle(x, y, cardWidth, cardHeight, isSelected ? Fade(PURPLE, 0.3f) : BLACK);
                DrawRectangleLines(x, y, cardWidth, cardHeight, isSelected ? VIOLET : GRAY);
                DrawText(curse.name.c_str(), x + 20, y + 15, 24, isSelected ? VIOLET : WHITE);
                DrawText(curse.description.c_str(), x + 20, y + 48, 16, LIGHTGRAY);
            }

            DrawText("[W/S] Velg   |   [ENTER] Bekreft   |   [ESC] Tilbake", Settings::SCREEN_WIDTH / 2 - 240, Settings::SCREEN_HEIGHT - 60, 20, GRAY);
        }
        else if (currentState == SHOP) {
            shop.draw(totalGold);
        }
        else if (currentState == SETTINGS) {
            DrawText("INNSTILLINGER", Settings::SCREEN_WIDTH / 2 - 120, 100, 32, WHITE);
            DrawText("Volum", Settings::SCREEN_WIDTH / 2 - 200, 220, 24, WHITE);
            DrawRectangle(Settings::SCREEN_WIDTH / 2 - 80, 222, 280, 22, DARKGRAY);
            DrawRectangle(Settings::SCREEN_WIDTH / 2 - 80, 222, (int)(280 * saveData.volume / 100.0f), 22, GOLD);
            DrawRectangleLines(Settings::SCREEN_WIDTH / 2 - 80, 222, 280, 22, WHITE);
            DrawText(TextFormat("%d%%", saveData.volume), Settings::SCREEN_WIDTH / 2 + 215, 222, 22, WHITE);
            DrawText("[A/D] eller [Venstre/Hoeyre] for aa justere", Settings::SCREEN_WIDTH / 2 - 200, 265, 18, GRAY);
            DrawText("Trykk [ESC] for a ga tilbake", Settings::SCREEN_WIDTH / 2 - 140, 450, 20, GRAY);
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
                player.drawSprite(view);
                DrawExplosions3D();
            EndMode3D();

            // --- HP-BARER over skadde fiender (ikke bossen – den har egen bar øverst) ---
            for (const auto& e : enemies) {
                if (e->hp >= e->maxHp || e->id == bossId) continue;
                Vector2 screen = GroundToScreen(view, e->position, e->modelHeight() + 8.0f);
                float pct = std::max(0.0f, (float)e->hp / (float)e->maxHp);
                DrawRectangle((int)screen.x - 15, (int)screen.y, 30, 4, Fade(BLACK, 0.6f));
                DrawRectangle((int)screen.x - 15, (int)screen.y, (int)(30 * pct), 4, GREEN);
            }

            // --- SKADETALL (projiseres fra 3D-posisjonen, så teksten alltid er rett vei) ---
            DrawDamageNumbers(view);

            // --- UI / TEKST (Festet til skjermen, roterer ikke) ---
            DrawText("GAMEPLAY (ESC for meny)", 20, 20, 20, GREEN);
            DrawText("Roter kamera: [Q] / [E]", 20, 50, 18, LIGHTGRAY);
            DrawText(TextFormat("Vinkel: %.1f deg", camera.rotation), 20, 75, 18, YELLOW);
            DrawText(TextFormat("HP: %.0f / %.0f", player.hp, player.maxHp), 20, 105, 18, RED);
            DrawText(TextFormat("Aegis: %d", player.aegis), 20, 130, 18, GREEN);
            DrawText(TextFormat("Gull: %d", runCoins), 20, 155, 18, GOLD);
            DrawText(TextFormat("Kills: %d", Enemy::killCount), 20, 180, 18, LIGHTGRAY);
            for (size_t i = 0; i < activeCurses.size(); i++) {
                DrawText(TextFormat("Curse: %s", GetCurse(activeCurses[i]).name.c_str()), 20, 205 + (int)i * 22, 16, VIOLET);
            }

            float barWidth = 400.0f;
            float barHeight = 12.0f;
            float barX = (Settings::SCREEN_WIDTH / 2.0f) - (barWidth / 2.0f);
            float barY = 70.0f; // Litt under timeren



            // Regn utprosent fullført
            float xpProgress = (float)player.currentXp / (float)player.xpToNextLevel;
            if (xpProgress > 1.0f) xpProgress = 1.0f;

            // Bakgrunn (tom bar) ogfyll (blå/lilla)
            DrawRectangle((int)barX, (int)barY, (int)barWidth, (int)barHeight, DARKGRAY);
            DrawRectangle((int)barX, (int)barY, (int)(barWidth * xpProgress), (int)barHeight, BLUE);
            DrawRectangleLines((int)barX, (int)barY, (int)barWidth, (int)barHeight, WHITE);
            DrawText(TextFormat("Lv %d", player.level), (int)(barX + barWidth + 10), (int)barY - 2, 16, WHITE);

            // --- ABILITY-SLOTS (5 stk, låste er mørke) ---
            DrawAbilityHud(player, Settings::SCREEN_WIDTH, Settings::SCREEN_HEIGHT);

            // --- BOSS HP-BAR ---
            if (inBossArena) {
                for (const auto& e : enemies) {
                    if (e->id != bossId) continue;
                    float bossBarWidth = 600.0f;
                    float bossBarX = Settings::SCREEN_WIDTH / 2.0f - bossBarWidth / 2.0f;
                    float bossBarY = 100.0f;
                    float pct = std::max(0.0f, (float)e->hp / (float)e->maxHp);
                    DrawRectangle((int)bossBarX, (int)bossBarY, (int)bossBarWidth, 18, Fade(BLACK, 0.7f));
                    DrawRectangle((int)bossBarX, (int)bossBarY, (int)(bossBarWidth * pct), 18, RED);
                    DrawRectangleLines((int)bossBarX, (int)bossBarY, (int)bossBarWidth, 18, WHITE);
                    const char* bossName = TextFormat("KONGEN - %s", GetEchelon(selectedEchelon).name.c_str());
                    DrawText(bossName, Settings::SCREEN_WIDTH / 2 - MeasureText(bossName, 16) / 2, (int)bossBarY + 22, 16, WHITE);
                }
            }

            if (currentState == LEVEL_UP) {
                // Mørklegg skjermen bak menyen
                DrawRectangle(0, 0, Settings::SCREEN_WIDTH, Settings::SCREEN_HEIGHT, Fade(BLACK, 0.85f));

                DrawText("LEVEL UP! VELG EN OPPDATERING", Settings::SCREEN_WIDTH / 2 - 210, 110, 30, YELLOW);

                int cardWidth = 450;
                int cardHeight = 80;
                int startY = (activeUpgradeChoices.size() > 3) ? 170 : 220; // Plass til 4 valg med "Flere valg"-oppgraderingen

                for (size_t i = 0; i < activeUpgradeChoices.size(); i++) {
                    int cardY = startY + (int)i * (cardHeight + 15);
                    bool isSelected = ((int)i == selectedUpgradeOption);

                    // Tegn boks
                    DrawRectangle(Settings::SCREEN_WIDTH / 2 - cardWidth / 2, cardY, cardWidth, cardHeight, isSelected ? DARKGRAY : BLACK);
                    DrawRectangleLines(Settings::SCREEN_WIDTH / 2 - cardWidth / 2, cardY, cardWidth, cardHeight, isSelected ? YELLOW : GRAY);

                    // Fargestripe som viser hvilken ability det gjelder
                    DrawRectangle(Settings::SCREEN_WIDTH / 2 - cardWidth / 2, cardY, 6, cardHeight, activeUpgradeChoices[i].color);

                    // Innhold i boksen
                    Color textColor = isSelected ? YELLOW : WHITE;
                    DrawText(activeUpgradeChoices[i].title.c_str(), Settings::SCREEN_WIDTH / 2 - cardWidth / 2 + 20, cardY + 15, 22, textColor);
                    DrawText(activeUpgradeChoices[i].description.c_str(), Settings::SCREEN_WIDTH / 2 - cardWidth / 2 + 20, cardY + 45, 14, LIGHTGRAY);
                }

                DrawText("Bruk [W/S] eller [Piltaster] og trykk [ENTER] for å velge", Settings::SCREEN_WIDTH / 2 - 220, Settings::SCREEN_HEIGHT - 100, 18, GRAY);
            }
        }

        else if (currentState == GAME_OVER) {
            int cx = Settings::SCREEN_WIDTH / 2;
            const char* heading = lastRun.bossDefeated ? TextFormat("%s FULLFOERT!", GetEchelon(lastRun.echelon).name.c_str())
                                : lastRun.died ? "DU DOEDE" : "RUN AVSLUTTET";
            Color headingColor = lastRun.bossDefeated ? GREEN : (lastRun.died ? RED : YELLOW);
            DrawText(heading, cx - MeasureText(heading, 44) / 2, 70, 44, headingColor);
            if (lastRun.unlockedNewEchelon) {
                const char* unlockText = TextFormat("%s er laast opp!", GetEchelon(lastRun.echelon + 1).name.c_str());
                DrawText(unlockText, cx - MeasureText(unlockText, 24) / 2, 125, 24, ORANGE);
            }

            int minutes = (int)lastRun.timeSurvived / 60;
            int seconds = (int)lastRun.timeSurvived % 60;
            DrawText(TextFormat("Tid: %02d:%02d    Level: %d    Kills: %d", minutes, seconds, lastRun.level, lastRun.kills),
                     cx - 230, 170, 22, WHITE);

            int y = 240;
            DrawText(TextFormat("Mynter plukket opp:   %d g", lastRun.coinGold), cx - 200, y, 22, LIGHTGRAY);
            DrawText(TextFormat("Overlevd tid:              %d g", lastRun.survivalGold), cx - 200, y + 35, 22, LIGHTGRAY);
            DrawText(TextFormat("Level-bonus:                %d g", lastRun.levelGold), cx - 200, y + 70, 22, LIGHTGRAY);
            int nextLine = y + 105;
            if (lastRun.bossGold > 0) {
                DrawText(TextFormat("Boss-bonus:                  %d g", lastRun.bossGold), cx - 200, nextLine, 22, GREEN);
                nextLine += 35;
            }
            if (lastRun.greedMult > 1.0f) {
                DrawText(TextFormat("Greed:                          x%.1f", lastRun.greedMult), cx - 200, nextLine, 22, LIGHTGRAY);
                nextLine += 35;
            }
            DrawLine(cx - 200, nextLine, cx + 200, nextLine, GRAY);
            DrawText(TextFormat("TOTALT:  +%d g", lastRun.totalGold), cx - 200, nextLine + 15, 30, GOLD);
            DrawText(TextFormat("Gull i banken: %d g", totalGold), cx - 200, nextLine + 60, 20, GOLD);

            DrawText("[ENTER] Tilbake til menyen", cx - 140, Settings::SCREEN_HEIGHT - 80, 20, GRAY);
        }

        if (currentState == GAMEPLAY || currentState == LEVEL_UP) {
            // --- KLOKKE: TELLER ALLTID OPP FRA 0 ---
            int fontSize = 32;
            int minutes = (int)spawner.gameTime / 60;
            int seconds = (int)spawner.gameTime % 60;
            const char* timeText = TextFormat("%02d:%02d", minutes, seconds);
            DrawText(timeText, (Settings::SCREEN_WIDTH / 2) - MeasureText(timeText, fontSize) / 2, 20, fontSize, inBossArena ? RED : WHITE);
            const char* echelonLabel = GetEchelon(selectedEchelon).name.c_str();
            DrawText(echelonLabel, Settings::SCREEN_WIDTH - MeasureText(echelonLabel, 20) - 20, 20, 20, ORANGE);

            // --- "BOSS ARENA"-INTRO ETTER TELEPORT ---
            if (inBossArena && arenaIntroTimer > 0.0f) {
                float t = arenaIntroTimer / Arena::INTRO_TIME; // 1 -> 0
                // Hvitt blink som fader ut, så teksten
                DrawRectangle(0, 0, Settings::SCREEN_WIDTH, Settings::SCREEN_HEIGHT, Fade(WHITE, std::max(0.0f, (t - 0.7f) / 0.3f)));
                const char* introText = "KONGENS TRONSAL";
                int introSize = 64;
                DrawText(introText, Settings::SCREEN_WIDTH / 2 - MeasureText(introText, introSize) / 2, Settings::SCREEN_HEIGHT / 2 - 80, introSize, Fade(RED, std::min(1.0f, t * 2.0f)));
            }
        }

        EndDrawing();
    }

    // Rydd opp teksturer i minnet
    for (auto& tex : characterTextures) {
        UnloadTexture(tex);
    }
    UnloadTexture(enemyTexture);

    saveProgress();
    UnloadRenderer3D();
    UnloadGameAudio();
    CloseWindow();
    return 0;
}