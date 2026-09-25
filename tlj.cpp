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

enum GameState {
    MAIN_MENU,
    CHARACTER_SELECT,
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
    const char* SAVE_FILE = "save.txt";
}

struct RunSummary {
    bool died = false;
    float timeSurvived = 0.0f;
    int level = 1;
    int kills = 0;
    int coinGold = 0;     // Mynter plukket opp
    int survivalGold = 0; // Bonus for overlevd tid
    int levelGold = 0;    // Bonus for level
    float greedMult = 1.0f;
    int totalGold = 0;
};

RunSummary CalculateRunSummary(bool died, float time, int level, int kills, int coins, float greedMult) {
    RunSummary r;
    r.died = died;
    r.timeSurvived = time;
    r.level = level;
    r.kills = kills;
    r.coinGold = coins;
    r.survivalGold = (int)(time / 60.0f * Rewards::GOLD_PER_MINUTE);
    r.levelGold = (level - 1) * Rewards::GOLD_PER_LEVEL;
    r.greedMult = greedMult;
    r.totalGold = (int)((r.coinGold + r.survivalGold + r.levelGold) * greedMult);
    return r;
}

int main() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(Settings::SCREEN_WIDTH, Settings::SCREEN_HEIGHT, "The Last Jester");
    SetTargetFPS(Settings::FPS);
    SetExitKey(KEY_NULL); // ESC skal gå tilbake i menyer, ikke lukke hele spillet

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

    // Instans av shoppen og permanent gull
    // Gull og kjøp lagres i save.txt slik at de akkumuleres over runs og økter
    Shop shop;
    int totalGold = 0;
    shop.load(Rewards::SAVE_FILE, totalGold);

    int runCoins = 0;       // Mynter plukket opp denne runden
    RunSummary lastRun;     // Vises på game over-skjermen

    Player player{};
    Camera2D camera{};

    int lastPlayerLevel = 1;
    std::vector<AbilityChoice> activeUpgradeChoices;
    int selectedUpgradeOption = 0;

    // --- SPAWNER OG FIENDER ---
    WaveSpawner spawner;
    std::vector<std::unique_ptr<Enemy>> enemies;
    std::vector<Pickup> pickups;

    while (!WindowShouldClose()) {
        float deltaTime = GetFrameTime();

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

            // Start spillet med valgt karakter!
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
                CharacterData choice = characters[selectedCharacter];

                // 1. Nullstill progresjon fra forrige runde
                player.position = { 0.0f, 0.0f };
                player.level = 1;
                player.currentXp = 0;
                player.xpToNextLevel = 100;
                player.weapons.clear();
                player.invulnerableTimer = 0.0f;
                lastPlayerLevel = 1;
                runCoins = 0;
                Enemy::killCount = 0;

                // 2. La shoppen påføre arvede basestats + shop-multiplikatorer
                shop.applyToPlayer(choice, player);

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
                spawner.gameTime = 0.0f;
                spawner.spawnTimer = 0.0f;
                enemies.clear();
                pickups.clear();
                ClearDamageNumbers();

                currentState = GAMEPLAY;
            }
        }
        else if (currentState == SHOP) {
            // Tilbake til Hovedmeny
            if (IsKeyPressed(KEY_P) || IsKeyPressed(KEY_B) || IsKeyPressed(KEY_ESCAPE)) {
                shop.save(Rewards::SAVE_FILE, totalGold);
                currentState = MAIN_MENU;
            }

            shop.handleInput(totalGold);
        }
        else if (currentState == SETTINGS) {
            if (IsKeyPressed(KEY_P) || IsKeyPressed(KEY_B) || IsKeyPressed(KEY_ESCAPE)) {
                currentState = MAIN_MENU;
            }
        }
        else if (currentState == GAMEPLAY) {

            if(player.level > lastPlayerLevel){
                lastPlayerLevel = player.level;
                currentState = LEVEL_UP;
                selectedUpgradeOption = 0;
                activeUpgradeChoices = GenerateLevelUpChoices(player, player.levelUpChoices);
            }

            // 1. INPUT & OPPDRATERING
            // Roter kameraet med Q og E (60 grader i sekundet)
            if (IsKeyDown(KEY_Q)) camera.rotation -= 60.0f * deltaTime;
            if (IsKeyDown(KEY_E)) camera.rotation += 60.0f * deltaTime;

            // Oppdater spilleren (sender inn gjeldende kamerarotasjon så WASD matcher skjermen)
            player.update(camera.rotation);
            camera.target = player.position;

            // Oppdater spawneren (spawner fiender rundt spilleren)
            spawner.update(deltaTime, player.position, enemies, enemyTexture);

            // Oppdater alle fiender
            for (auto& enemy : enemies) {
                enemy->update(player.position);
            }

            // --- FIENDER SKADER SPILLEREN VED KONTAKT ---
            const float playerHitRadius = 20.0f;
            if (player.invulnerableTimer <= 0.0f) {
                for (auto& enemy : enemies) {
                    if (CheckCollisionCircles(player.position, playerHitRadius, enemy->position, 15.0f)) {
                        float taken = player.takeDamage((float)enemy->damage);
                        if (taken > 0.0f) SpawnDamageNumber(player.position, std::max(1, (int)(taken + 0.5f)), RED);
                        player.invulnerableTimer = 0.5f; // Kort pause så man ikke smeltes av en klump fiender
                        break;
                    }
                }
            }

            bool runEnded = false;
            if (player.hp <= 0.0f) {
                if (player.aegis > 0) {
                    // Aegis: gjenoppstå med halv HP og blås bort fiender rundt deg
                    player.aegis--;
                    player.hp = player.maxHp * 0.5f;
                    player.invulnerableTimer = 2.0f;
                    enemies.erase(std::remove_if(enemies.begin(), enemies.end(), [&](const std::unique_ptr<Enemy>& e) {
                        return Vector2Distance(e->position, player.position) < 250.0f;
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
                        } else {
                            player.addXP(static_cast<int>(it->value * player.xpMultiplier));
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

            // Fjerne døde fiender
            enemies.erase(
                std::remove_if(enemies.begin(), enemies.end(),
                    [](const std::unique_ptr<Enemy>& e) { return e->isDead(); }),
                enemies.end()
            );

            // ESC/P avslutter runden (man får fortsatt gullet man har tjent)
            bool gaveUp = IsKeyPressed(KEY_P) || IsKeyPressed(KEY_ESCAPE);

            if (runEnded || gaveUp) {
                lastRun = CalculateRunSummary(runEnded, spawner.gameTime, player.level, Enemy::killCount, runCoins, player.goldMultiplier);
                totalGold += lastRun.totalGold;
                shop.save(Rewards::SAVE_FILE, totalGold);
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

            DrawText("[ENTER] Start Game   |   [ESC] Tilbake", Settings::SCREEN_WIDTH / 2 - 180, 480, 20, GRAY);
        }
        else if (currentState == SHOP) {
            shop.draw(totalGold);
        }
        else if (currentState == SETTINGS) {
            DrawText("INNSTILLINGER", Settings::SCREEN_WIDTH / 2 - 120, 100, 32, WHITE);
            DrawText("Lyd / Grafikk innstillinger her...", Settings::SCREEN_WIDTH / 2 - 160, 220, 20, GRAY);
            DrawText("Trykk [ESC] for a ga tilbake", Settings::SCREEN_WIDTH / 2 - 140, 450, 20, GRAY);
        }
        else if (currentState == GAMEPLAY || currentState == LEVEL_UP) {
            // 2. TEGNING PÅ SKJERMEN
            BeginMode2D(camera);

                // --- TEGN BAKGRUNN (GRID) ---
                int gridSize = 100;
                int gridExtent = 2000;

                for (int x = -gridExtent; x <= gridExtent; x += gridSize) {
                    DrawLine(x, -gridExtent, x, gridExtent, DARKGRAY);
                }
                for (int y = -gridExtent; y <= gridExtent; y += gridSize) {
                    DrawLine(-gridExtent, y, gridExtent, y, DARKGRAY);
                }

                for (const auto& pickup : pickups) {
                    DrawCircleV(pickup.position, pickup.radius, pickup.color);
                    if (pickup.type == PickupType::COIN) {
                        DrawCircleLines((int)pickup.position.x, (int)pickup.position.y, pickup.radius, ORANGE);
                    }
                }

                // --- REFRENSERUBRIKKER / OBJEKTER I VERDEN ---
                DrawRectangle(-300, -300, 80, 80, RED);
                DrawRectangle(400, 200, 100, 100, GREEN);
                DrawCircle(0, -500, 60.0f, PURPLE);

                // --- SPILLER OG VÅPEN ---
                player.draw(camera.rotation);
                
                for (auto& w : player.weapons) {
                    w->draw();
                }
                // --- TEGN ALLE FIENDER ---
                for (auto& enemy : enemies) {
                    enemy->draw();
                }



            EndMode2D();

            // --- SKADETALL (tegnes i skjerm-koordinater så de ikke roterer med kameraet) ---
            DrawDamageNumbers(camera);

            // --- UI / TEKST (Festet til skjermen, roterer ikke) ---
            DrawText("GAMEPLAY (ESC for meny)", 20, 20, 20, GREEN);
            DrawText("Roter kamera: [Q] / [E]", 20, 50, 18, LIGHTGRAY);
            DrawText(TextFormat("Vinkel: %.1f deg", camera.rotation), 20, 75, 18, YELLOW);
            DrawText(TextFormat("HP: %.0f / %.0f", player.hp, player.maxHp), 20, 105, 18, RED);
            DrawText(TextFormat("Aegis: %d", player.aegis), 20, 130, 18, GREEN);
            DrawText(TextFormat("Gull: %d", runCoins), 20, 155, 18, GOLD);
            DrawText(TextFormat("Kills: %d", Enemy::killCount), 20, 180, 18, LIGHTGRAY);

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
            const char* heading = lastRun.died ? "DU DOEDE" : "RUN AVSLUTTET";
            DrawText(heading, cx - MeasureText(heading, 44) / 2, 90, 44, lastRun.died ? RED : YELLOW);

            int minutes = (int)lastRun.timeSurvived / 60;
            int seconds = (int)lastRun.timeSurvived % 60;
            DrawText(TextFormat("Tid: %02d:%02d    Level: %d    Kills: %d", minutes, seconds, lastRun.level, lastRun.kills),
                     cx - 230, 170, 22, WHITE);

            int y = 240;
            DrawText(TextFormat("Mynter plukket opp:   %d g", lastRun.coinGold), cx - 200, y, 22, LIGHTGRAY);
            DrawText(TextFormat("Overlevd tid:              %d g", lastRun.survivalGold), cx - 200, y + 35, 22, LIGHTGRAY);
            DrawText(TextFormat("Level-bonus:                %d g", lastRun.levelGold), cx - 200, y + 70, 22, LIGHTGRAY);
            if (lastRun.greedMult > 1.0f) {
                DrawText(TextFormat("Greed:                          x%.1f", lastRun.greedMult), cx - 200, y + 105, 22, LIGHTGRAY);
            }
            DrawLine(cx - 200, y + 140, cx + 200, y + 140, GRAY);
            DrawText(TextFormat("TOTALT:  +%d g", lastRun.totalGold), cx - 200, y + 155, 30, GOLD);
            DrawText(TextFormat("Gull i banken: %d g", totalGold), cx - 200, y + 200, 20, GOLD);

            DrawText("[ENTER] Tilbake til menyen", cx - 140, Settings::SCREEN_HEIGHT - 80, 20, GRAY);
        }

        if (currentState == GAMEPLAY || currentState == LEVEL_UP) {
            // --- KLOKKE / TIMER ØVERST I MIDTEN ---
            int minutes = (int)spawner.gameTime / 60;
            int seconds = (int)spawner.gameTime % 60;
            const char* timeText = TextFormat("%02d:%02d", minutes, seconds);
            int fontSize = 32;
            int textWidth = MeasureText(timeText, fontSize);
            DrawText(timeText, (Settings::SCREEN_WIDTH / 2) - (textWidth / 2), 20, fontSize, WHITE);
        }

        EndDrawing();
    }

    // Rydd opp teksturer i minnet
    for (auto& tex : characterTextures) {
        UnloadTexture(tex);
    }
    UnloadTexture(enemyTexture);

    shop.save(Rewards::SAVE_FILE, totalGold);
    CloseWindow();
    return 0;
}