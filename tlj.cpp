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

enum GameState {
    MAIN_MENU,
    CHARACTER_SELECT,
    SHOP,
    SETTINGS,
    GAMEPLAY,
    LEVEL_UP,
    ITEM_SELECT
};

struct UpgradeOption {
    std::string title;
    std::string description;
    int weaponId;
};

std::vector<UpgradeOption> GetRandomUpgrades() {
    std::vector<UpgradeOption> allPossibleUpgrades = {
        { "Standard Gun", "Skyter kuler mot nærmeste fiende.", 0 },
        { "Melee Sword", "Svinger et sverd rundt deg i nærkamp.", 1 },
        { "Weapon Upgrade", "Øker skade og reduserer cooldown.", 2 }
    };

    std::vector<UpgradeOption> chosenUpgrades;
    while (chosenUpgrades.size() < 3 && !allPossibleUpgrades.empty()) {
        int randomIndex = rand() % allPossibleUpgrades.size();
        chosenUpgrades.push_back(allPossibleUpgrades[randomIndex]);
        allPossibleUpgrades.erase(allPossibleUpgrades.begin() + randomIndex);
    }
    return chosenUpgrades;
}


int main() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(Settings::SCREEN_WIDTH, Settings::SCREEN_HEIGHT, "The Last Jester");
    SetTargetFPS(Settings::FPS);

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
    Shop shop;
    int totalGold = 500; // Starter med litt test-gull

    Player player{};
    Camera2D camera{};

    int lastPlayerLevel = 1;
    std::vector<UpgradeOption> activeUpgradeChoices;
    int selectedUpgradeOption = 0;

    // --- SPAWNER OG FIENDER ---
    WaveSpawner spawner;
    std::vector<std::unique_ptr<Enemy>> enemies;
    std::vector<XPorb> xpOrbs;

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

                // 1. Tilbakestill standard spillerspesifikke variabler
                player.evasion = 0.05f;
                player.xpMultiplier = 1.0f;
                player.goldMultiplier = 1.0f;
                player.cooldownReduction = 0.0f;
                player.projectileCount = 1;

                // 2. La shoppen påføre arvede basestats + shop-multiplikatorer
                shop.applyToPlayer(choice, player);

                player.texture = characterTextures[selectedCharacter];

                player.addWeapon(std::make_unique<ProjectileWeapon>(choice.weaponName, choice.cooldown, choice.weaponSpeed, choice.weaponDamage));

                for (auto& w : player.weapons) {
                    w->update(deltaTime, player.position, enemies, xpOrbs, player.projectileCount);
                }

                // Kamera-oppsett
                camera.target = player.position;
                camera.offset = { Settings::SCREEN_WIDTH / 2.0f, Settings::SCREEN_HEIGHT / 2.0f };
                camera.zoom = 1.0f;
                camera.rotation = 0.0f;

                // 3. Tilbakestill spawner og fiender for ny runde
                spawner.gameTime = 0.0f;
                spawner.spawnTimer = 0.0f;
                enemies.clear();

                currentState = GAMEPLAY;
            }
        }
        else if (currentState == SHOP) {
            // Tilbake til Hovedmeny
            if (IsKeyPressed(KEY_P) || IsKeyPressed(KEY_B) || IsKeyPressed(KEY_ESCAPE)) {
                currentState = MAIN_MENU;
            }

            // Oppgraderer interne shop-multiplikatorer
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
                activeUpgradeChoices = GetRandomUpgrades();
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

            // --- OPPDATER OG PLUKK OPP XP-ORBS ---
            for (auto it = xpOrbs.begin(); it != xpOrbs.end(); ) {
                float distance = Vector2Distance(it->position, player.position);

                // Hvis orben er innenfor spillerens lootRadius, sug den til deg!
                if (distance < player.lootRadius) {
                    float magnetSpeed = 400.0f; // Hvor fort den fyker mot spilleren
                    it->position = Vector2MoveTowards(it->position, player.position, magnetSpeed * deltaTime);

                    // Når den er helt nær (f.eks. innenfor 15 piksler), saml den opp
                    if (distance < 15.0f) {
                        // Multipliser gjerne med spillerens xpMultiplier om du vil ha utbytte av traits!
                        int finalXp = static_cast<int>(it->value * player.xpMultiplier);
                        player.addXP(finalXp);

                        // Slett orben fra listen
                        it = xpOrbs.erase(it);
                    } else {
                        ++it;
                    }
                } else {
                    ++it;
                }
            }

            //Våpen tegn og oppdatering
            for (auto& w : player.weapons) {
                w->update(deltaTime, player.position, enemies, xpOrbs, player.projectileCount);
            }

            // Fjerne døde fiender
            enemies.erase(
                std::remove_if(enemies.begin(), enemies.end(),
                    [](const std::unique_ptr<Enemy>& e) { return e->isDead(); }),
                enemies.end()
            );

            if (IsKeyPressed(KEY_P) || IsKeyPressed(KEY_ESCAPE)) {
                currentState = MAIN_MENU; // Gå ut til menyen igjen
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
                UpgradeOption chosen = activeUpgradeChoices[selectedUpgradeOption];

                if (chosen.weaponId == 0) {
                    if (player.weapons.size() < (size_t)player.maxWeapons) {
                        player.addWeapon(std::make_unique<ProjectileWeapon>("Standard Gun", 0.3f, 500.0f, 25.0f));
                    }
                } 
                else if (chosen.weaponId == 1) {
                    if (player.weapons.size() < (size_t)player.maxWeapons) {
                        player.addWeapon(std::make_unique<MeleeWeapon>("Melee Sword", 0.5f, 120.0f, 35.0f));
                    }
                } 
                else if (chosen.weaponId == 2) {
                    if (!player.weapons.empty()) {
                        player.weapons[0]->upgrade();
                    }
                }

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
                float finalHp = characters[i].maxHp * shop.hpMult;
                float finalSpeed = characters[i].speed * shop.speedMult;
                float finalArmor = characters[i].armor * shop.armorMult;

                DrawText(characters[i].description.c_str(), posX + 15, 240, 12, GRAY);
                DrawText(TextFormat("HP: %.0f", finalHp), posX + 15, 270, 16, WHITE);
                DrawText(TextFormat("Fart: %.0f", finalSpeed), posX + 15, 295, 16, WHITE);
                DrawText(TextFormat("Armor: %.1f", finalArmor), posX + 15, 320, 16, WHITE);
                DrawText(TextFormat("Radius: %.0f", characters[i].lootRadius), posX + 15, 345, 16, WHITE);
                DrawText(TextFormat("Aegis: +%d", shop.aegisBonus), posX + 15, 370, 16, GREEN);
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

                for (const auto& orb : xpOrbs) {
                    DrawCircleV(orb.position, orb.radius, orb.color);
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

            // --- UI / TEKST (Festet til skjermen, roterer ikke) ---
            DrawText("GAMEPLAY (ESC for meny)", 20, 20, 20, GREEN);
            DrawText("Roter kamera: [Q] / [E]", 20, 50, 18, LIGHTGRAY);
            DrawText(TextFormat("Vinkel: %.1f deg", camera.rotation), 20, 75, 18, YELLOW);
            DrawText(TextFormat("HP: %.0f / %.0f", player.hp, player.maxHp), 20, 105, 18, RED);
            DrawText(TextFormat("Aegis: %d", player.aegis), 20, 130, 18, GREEN);

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

            if (currentState == LEVEL_UP) {
                // Mørklegg skjermen bak menyen
                DrawRectangle(0, 0, Settings::SCREEN_WIDTH, Settings::SCREEN_HEIGHT, Fade(BLACK, 0.85f));

                DrawText("LEVEL UP! VELG EN OPPDATERING", Settings::SCREEN_WIDTH / 2 - 210, 120, 30, YELLOW);

                int cardWidth = 450;
                int cardHeight = 80;
                int startY = 220;

                for (size_t i = 0; i < activeUpgradeChoices.size(); i++) {
                    int cardY = startY + (int)i * (cardHeight + 20);
                    bool isSelected = ((int)i == selectedUpgradeOption);

                    // Tegn boks
                    DrawRectangle(Settings::SCREEN_WIDTH / 2 - cardWidth / 2, cardY, cardWidth, cardHeight, isSelected ? DARKGRAY : BLACK);
                    DrawRectangleLines(Settings::SCREEN_WIDTH / 2 - cardWidth / 2, cardY, cardWidth, cardHeight, isSelected ? YELLOW : GRAY);

                    // Innhold i boksen
                    Color textColor = isSelected ? YELLOW : WHITE;
                    DrawText(activeUpgradeChoices[i].title.c_str(), Settings::SCREEN_WIDTH / 2 - cardWidth / 2 + 20, cardY + 15, 22, textColor);
                    DrawText(activeUpgradeChoices[i].description.c_str(), Settings::SCREEN_WIDTH / 2 - cardWidth / 2 + 20, cardY + 45, 14, LIGHTGRAY);
                }

                DrawText("Bruk [W/S] eller [Piltaster] og trykk [ENTER] for å velge", Settings::SCREEN_WIDTH / 2 - 220, Settings::SCREEN_HEIGHT - 100, 18, GRAY);
            }
        }

            // --- KLOKKE / TIMER ØVERST I MIDTEN ---
            int minutes = (int)spawner.gameTime / 60;
            int seconds = (int)spawner.gameTime % 60;
            const char* timeText = TextFormat("%02d:%02d", minutes, seconds);
            int fontSize = 32;
            int textWidth = MeasureText(timeText, fontSize);
            DrawText(timeText, (Settings::SCREEN_WIDTH / 2) - (textWidth / 2), 20, fontSize, WHITE);

        EndDrawing();
    }

    // Rydd opp teksturer i minnet
    for (auto& tex : characterTextures) {
        UnloadTexture(tex);
    }
    UnloadTexture(enemyTexture);

    CloseWindow();
    return 0;
}