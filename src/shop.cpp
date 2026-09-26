#include "shop.hpp"
#include "settings.hpp"
#include "audio.hpp"
#include "ui.hpp"
#include "icons.hpp"
#include <string>
#include <cmath>
#include <algorithm>

// =====================================================================
// BALANSE
// Gull er metaprogresjon: målet er at det tar ~30-40 gode runs å kjøpe alt,
// og at spillet da er ganske enkelt. Prisen øker med COST_GROWTH per nivå.
// =====================================================================
namespace {
    constexpr float COST_GROWTH = 1.4f;
}

int ShopItem::cost() const {
    return (int)std::round(baseCost * std::pow(COST_GROWTH, (float)currentLevel));
}

Shop::Shop() {
    //            oppgradering              lagringsnøkkel  navn                      beskrivelse (per nivå)          pris  maks
    items.push_back({ ShopUpgrade::VITALITY,     "vitality",   "Vitality",               "+10% maks HP",                  60,  5 });
    items.push_back({ ShopUpgrade::ARMOR,        "armor",      "Reinforced Armor",       "+3 armor",                      50,  5 });
    items.push_back({ ShopUpgrade::REGEN,        "regen",      "Regeneration",           "+0.4 HP per sekund",            60,  5 });
    items.push_back({ ShopUpgrade::EVASION,      "evasion",    "Float like a butterfly", "+4% evasion",                   60,  5 });
    items.push_back({ ShopUpgrade::SPEED,        "speed",      "Swift Boots",            "+5% fart",                      50,  5 });
    items.push_back({ ShopUpgrade::MIGHT,        "might",      "Might",                  "+10% skade",                    80,  5 });
    items.push_back({ ShopUpgrade::HASTE,        "haste",      "Haste",                  "-5% cooldown",                  80,  5 });
    items.push_back({ ShopUpgrade::AREA,         "area",       "Area",                   "+10% radius og treffomraade",   60,  5 });
    items.push_back({ ShopUpgrade::PROJECTILE,   "projectile", "Projectile count",       "+1 prosjektil",                200,  3 });
    items.push_back({ ShopUpgrade::MAGNET,       "magnet",     "Loot Magnet",            "+30 pickup-radius",             40,  5 });
    items.push_back({ ShopUpgrade::GROWTH,       "growth",     "Growth",                 "+8% XP",                        60,  5 });
    items.push_back({ ShopUpgrade::GREED,        "greed",      "Greed",                  "+10% gull",                     70,  5 });
    items.push_back({ ShopUpgrade::EXTRA_CHOICE, "choice",     "Flere valg",             "+1 valg ved level up",         400,  1 });
    items.push_back({ ShopUpgrade::AEGIS,        "aegis",      "Aegis Protection",       "+1 ekstra liv",                500,  1 });
}

int Shop::level(ShopUpgrade upgrade) const {
    for (const auto& item : items) {
        if (item.upgrade == upgrade) return item.currentLevel;
    }
    return 0;
}

float Shop::hpMult() const { return 1.0f + 0.10f * level(ShopUpgrade::VITALITY); }
float Shop::speedMult() const { return 1.0f + 0.05f * level(ShopUpgrade::SPEED); }
float Shop::armorBonus() const { return 3.0f * level(ShopUpgrade::ARMOR); }
int Shop::aegisBonus() const { return level(ShopUpgrade::AEGIS); }

void Shop::applyToPlayer(const CharacterData& baseChar, Player& player) const {
    // 1. Player arver basestats fra Character og legger på shop-bonusene
    player.maxHp = baseChar.maxHp * hpMult();
    player.hp = player.maxHp;
    player.speed = baseChar.speed * speedMult();
    player.armor = baseChar.armor + armorBonus();
    player.spellAmp = baseChar.spellAmp;
    player.lootRadius = baseChar.lootRadius + 30.0f * level(ShopUpgrade::MAGNET);
    player.xpMultiplier = baseChar.xpMultiplier * (1.0f + 0.08f * level(ShopUpgrade::GROWTH));

    // 2. Universelle shop-stats
    player.evasion = 0.05f + 0.04f * level(ShopUpgrade::EVASION);
    player.hpRegen = 0.4f * level(ShopUpgrade::REGEN);
    player.damageMult = 1.0f + 0.10f * level(ShopUpgrade::MIGHT);
    player.cooldownMult = std::pow(0.95f, (float)level(ShopUpgrade::HASTE));
    player.areaMult = 1.0f + 0.10f * level(ShopUpgrade::AREA);
    player.projectileCount = 1 + level(ShopUpgrade::PROJECTILE);
    player.goldMultiplier = 1.0f + 0.10f * level(ShopUpgrade::GREED);
    player.levelUpChoices = 3 + level(ShopUpgrade::EXTRA_CHOICE);
    player.aegis = aegisBonus();
}

// Samlet effekt av en oppgradering på et gitt nivå (vises i detaljpanelet)
static std::string totalEffect(ShopUpgrade upgrade, int level) {
    switch (upgrade) {
        case ShopUpgrade::VITALITY:     return TextFormat("+%d%% maks HP", 10 * level);
        case ShopUpgrade::ARMOR:        return TextFormat("+%d armor", 3 * level);
        case ShopUpgrade::REGEN:        return TextFormat("+%.1f HP per sekund", 0.4f * level);
        case ShopUpgrade::EVASION:      return TextFormat("+%d%% evasion", 4 * level);
        case ShopUpgrade::SPEED:        return TextFormat("+%d%% fart", 5 * level);
        case ShopUpgrade::MIGHT:        return TextFormat("+%d%% skade", 10 * level);
        case ShopUpgrade::HASTE:        return TextFormat("-%.0f%% cooldown", (1.0f - std::pow(0.95f, (float)level)) * 100.0f);
        case ShopUpgrade::AREA:         return TextFormat("+%d%% radius", 10 * level);
        case ShopUpgrade::PROJECTILE:   return TextFormat("+%d prosjektil", level);
        case ShopUpgrade::MAGNET:       return TextFormat("+%d pickup-radius", 30 * level);
        case ShopUpgrade::GROWTH:       return TextFormat("+%d%% XP", 8 * level);
        case ShopUpgrade::GREED:        return TextFormat("+%d%% gull", 10 * level);
        case ShopUpgrade::EXTRA_CHOICE: return TextFormat("+%d valg ved level up", level);
        case ShopUpgrade::AEGIS:        return TextFormat("+%d ekstra liv", level);
        default:                        return "";
    }
}

void Shop::handleInput(int& totalGold) {
    // Navigering i grid: A/D flytter ett kort, W/S en hel rad
    int count = static_cast<int>(items.size());
    if ((IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) && selectedOption + 1 < count) selectedOption++;
    if ((IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) && selectedOption > 0) selectedOption--;
    if ((IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) && selectedOption + GRID_COLUMNS < count) selectedOption += GRID_COLUMNS;
    if ((IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) && selectedOption - GRID_COLUMNS >= 0) selectedOption -= GRID_COLUMNS;

    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
        ShopItem& item = items[selectedOption];
        if (!item.isMaxed() && totalGold >= item.cost()) {
            totalGold -= item.cost();
            item.currentLevel++;
            purchaseTime = GetTime();
            PlaySfx(Sfx::COIN);
        } else {
            denyTime = GetTime(); // Kjøp-knappen rister
        }
    }

    // DEBUG: gi deg selv gull for å teste shoppen. Fjern før release!
    if (IsKeyPressed(KEY_F9)) totalGold += 500;
}

void Shop::draw(int totalGold) {
    const float W = (float)Settings::SCREEN_WIDTH;
    const float time = (float)GetTime();
    const Color PARCHMENT = { 240, 232, 214, 255 };
    const Color GREEN_OK = { 120, 230, 120, 255 };
    const Color TOO_POOR = { 235, 80, 80, 255 };

    UI::DrawCenteredText("POWER-UP SHOP", W / 2.0f, 24.0f, 40.0f, UI::GOLD_LIGHT, 3.0f);

    // Gullbeholdning oppe til høyre
    {
        const char* goldText = TextFormat("%d g", totalGold);
        float gw = MeasureText(goldText, 24);
        Rectangle gp = { 1240.0f - gw - 70.0f, 26.0f, gw + 70.0f, 46.0f };
        UI::DrawPanel(gp);
        DrawCircleV({ gp.x + 26.0f, gp.y + 23.0f }, 12.0f, UI::GOLD_DARK);
        DrawCircleV({ gp.x + 25.0f, gp.y + 22.0f }, 10.0f, GOLD);
        DrawRectangle((int)gp.x + 23, (int)gp.y + 16, 4, 12, UI::GOLD_DARK);
        UI::DrawOutlinedText(goldText, gp.x + 46.0f, gp.y + 12.0f, 24.0f, UI::GOLD_LIGHT);
    }

    // ---------------- GRID MED KORT ----------------
    const Rectangle gridPanel = { 40.0f, 92.0f, 770.0f, 520.0f };
    UI::DrawPanel(gridPanel);
    const float pad = 18.0f, gap = 12.0f;
    const int rows = ((int)items.size() + GRID_COLUMNS - 1) / GRID_COLUMNS;
    const float cardW = (gridPanel.width - pad * 2.0f - gap * (GRID_COLUMNS - 1)) / GRID_COLUMNS;
    const float cardH = (gridPanel.height - pad * 2.0f - gap * (rows - 1)) / rows;

    // Tegn det valgte kortet sist, så gløden og løftet havner over naboene
    for (int pass = 0; pass < 2; pass++) {
        for (int i = 0; i < (int)items.size(); i++) {
            bool isSelected = i == selectedOption;
            if ((pass == 1) != isSelected) continue;
            const ShopItem& item = items[i];
            bool canAfford = totalGold >= item.cost();
            int col = i % GRID_COLUMNS, row = i / GRID_COLUMNS;
            Rectangle card = { gridPanel.x + pad + col * (cardW + gap), gridPanel.y + pad + row * (cardH + gap), cardW, cardH };

            // Kjøpt akkurat nå: kortet spretter
            float sincePurchase = (float)(GetTime() - purchaseTime);
            float pop = (isSelected && sincePurchase < 0.35f) ? sinf(sincePurchase / 0.35f * PI) * 6.0f : 0.0f;
            if (isSelected) {
                card.y -= 4.0f + pop;
                UI::DrawGlow({ card.x + cardW / 2.0f, card.y + cardH / 2.0f }, cardW * 0.85f, Fade(UI::GOLD_LIGHT, 0.22f), Fade(UI::GOLD_LIGHT, 0.0f));
            }

            Color edge = item.isMaxed() ? UI::GOLD_DARK : (isSelected ? UI::GOLD_LIGHT : UI::PANEL_EDGE);
            Color fill = isSelected ? Color{ 64, 38, 50, 245 } : Color{ 34, 28, 44, 240 };
            UI::DrawPanel(card, 1.0f, edge, fill);

            // Ikon i et rundt merke
            Vector2 ic = { card.x + cardW / 2.0f, card.y + 38.0f };
            DrawCircleV(ic, 28.0f, UI::INK);
            DrawCircleV(ic, 26.0f, isSelected ? Color{ 90, 56, 70, 255 } : Color{ 54, 44, 66, 255 });
            DrawUpgradeIcon(item.upgrade, ic, isSelected ? 20.0f + sinf(time * 5.0f) * 0.8f : 19.0f);

            // Navn
            Color nameColor = item.isMaxed() ? UI::GOLD_LIGHT : (isSelected ? UI::GOLD_LIGHT : PARCHMENT);
            int nameSize = MeasureText(item.name.c_str(), 14) > cardW - 12.0f ? 12 : 14;
            DrawText(item.name.c_str(), (int)(card.x + cardW / 2.0f - MeasureText(item.name.c_str(), nameSize) / 2.0f), (int)card.y + 72, nameSize, nameColor);

            // Nivå-prikker
            const float pip = 9.0f, pipGap = 4.0f;
            float pipsW = item.maxLevel * pip + (item.maxLevel - 1) * pipGap;
            float px = card.x + cardW / 2.0f - pipsW / 2.0f;
            for (int l = 0; l < item.maxLevel; l++) {
                Rectangle pr = { px + l * (pip + pipGap), card.y + cardH - 17.0f, pip, pip };
                DrawRectangleRec({ pr.x - 1, pr.y - 1, pr.width + 2, pr.height + 2 }, UI::INK);
                DrawRectangleRec(pr, l < item.currentLevel ? UI::GOLD_LIGHT : Color{ 60, 54, 70, 255 });
            }

            // Pris-merke oppe til høyre (eller MAKS-bånd)
            if (item.isMaxed()) {
                Rectangle tag = { card.x + cardW - 52.0f, card.y + 6.0f, 46.0f, 18.0f };
                DrawRectangleRec(tag, UI::GOLD_DARK);
                DrawText("MAKS", (int)tag.x + 7, (int)tag.y + 4, 12, UI::INK);
            } else {
                const char* price = TextFormat("%dg", item.cost());
                float tw = MeasureText(price, 12) + 12.0f;
                Rectangle tag = { card.x + cardW - tw - 6.0f, card.y + 6.0f, tw, 18.0f };
                DrawRectangleRec(tag, Fade(UI::INK, 0.8f));
                DrawText(price, (int)tag.x + 6, (int)tag.y + 4, 12, canAfford ? UI::GOLD_LIGHT : TOO_POOR);
            }
        }
    }

    // Tomme plasser i siste rad: svake narre-ruter så griden ser hel ut
    for (int i = (int)items.size(); i < rows * GRID_COLUMNS; i++) {
        int col = i % GRID_COLUMNS, row = i / GRID_COLUMNS;
        Rectangle card = { gridPanel.x + pad + col * (cardW + gap), gridPanel.y + pad + row * (cardH + gap), cardW, cardH };
        DrawRectangleRec(card, Fade(UI::INK, 0.35f));
        DrawRectangleLinesEx(card, 1.0f, Fade(UI::PANEL_EDGE, 0.5f));
        Vector2 c = { card.x + cardW / 2.0f, card.y + cardH / 2.0f };
        DrawTriangle({ c.x, c.y - 16 }, { c.x - 12, c.y }, { c.x, c.y + 16 }, Fade(UI::PANEL_EDGE, 0.5f));
        DrawTriangle({ c.x, c.y - 16 }, { c.x, c.y + 16 }, { c.x + 12, c.y }, Fade(UI::PANEL_EDGE, 0.3f));
    }

    // ---------------- DETALJPANEL ----------------
    const ShopItem& sel = items[selectedOption];
    const Rectangle info = { 830.0f, 92.0f, 410.0f, 520.0f };
    UI::DrawPanel(info);
    float cx = info.x + info.width / 2.0f;

    UI::DrawCenteredText(sel.name.c_str(), cx, info.y + 22.0f, sel.name.size() > 16 ? 22.0f : 28.0f, UI::GOLD_LIGHT);

    // Stort ikon med strålekrans
    Vector2 big = { cx, info.y + 140.0f };
    UI::DrawSunburst(big, 95.0f, 12, time * 0.3f, Fade(UI::GOLD_LIGHT, 0.18f));
    DrawCircleV(big, 58.0f, UI::INK);
    DrawCircleV(big, 55.0f, UI::GOLD_DARK);
    DrawCircleV(big, 50.0f, Color{ 60, 40, 60, 255 });
    DrawUpgradeIcon(sel.upgrade, big, 38.0f);

    // Nivå
    const char* levelText = TextFormat("NIVAA %d / %d", sel.currentLevel, sel.maxLevel);
    UI::DrawCenteredText(levelText, cx, info.y + 222.0f, 18.0f, PARCHMENT);
    {
        const float pip = 22.0f, pipGap = 8.0f;
        float pipsW = sel.maxLevel * pip + (sel.maxLevel - 1) * pipGap;
        for (int l = 0; l < sel.maxLevel; l++) {
            Rectangle pr = { cx - pipsW / 2.0f + l * (pip + pipGap), info.y + 250.0f, pip, pip };
            DrawRectangleRec({ pr.x - 2, pr.y - 2, pr.width + 4, pr.height + 4 }, UI::INK);
            DrawRectangleRec(pr, l < sel.currentLevel ? UI::GOLD_LIGHT : Color{ 60, 54, 70, 255 });
            if (l < sel.currentLevel) DrawRectangleRec({ pr.x, pr.y, pr.width, pr.height * 0.4f }, Fade(WHITE, 0.35f));
        }
    }

    // Effekt nå -> neste nivå
    Rectangle fx = { info.x + 24.0f, info.y + 292.0f, info.width - 48.0f, 118.0f };
    DrawRectangleRec(fx, Fade(UI::INK, 0.55f));
    DrawRectangleLinesEx(fx, 1.0f, Fade(UI::GOLD_DARK, 0.8f));
    DrawText(TextFormat("Per nivaa: %s", sel.description.c_str()), (int)fx.x + 14, (int)fx.y + 12, 16, Color{ 200, 190, 170, 255 });
    DrawText("Naa:", (int)fx.x + 14, (int)fx.y + 46, 18, Color{ 190, 180, 165, 255 });
    DrawText(sel.currentLevel > 0 ? totalEffect(sel.upgrade, sel.currentLevel).c_str() : "-", (int)fx.x + 90, (int)fx.y + 46, 18, WHITE);
    DrawText("Neste:", (int)fx.x + 14, (int)fx.y + 78, 18, Color{ 190, 180, 165, 255 });
    DrawText(sel.isMaxed() ? "Fullt oppgradert!" : totalEffect(sel.upgrade, sel.currentLevel + 1).c_str(), (int)fx.x + 90, (int)fx.y + 78, 18, sel.isMaxed() ? UI::GOLD_LIGHT : GREEN_OK);

    // Kjøp-knapp (rister hvis man ikke har råd)
    float sinceDeny = (float)(GetTime() - denyTime);
    float shake = sinceDeny < 0.3f ? sinf(sinceDeny * 60.0f) * 8.0f * (1.0f - sinceDeny / 0.3f) : 0.0f;
    Rectangle btn = { info.x + 34.0f + shake, info.y + info.height - 86.0f, info.width - 68.0f, 58.0f };
    bool canBuy = !sel.isMaxed() && totalGold >= sel.cost();
    const char* btnText;
    Color btnFill, btnEdge, btnInk;
    if (sel.isMaxed()) {
        btnText = "MAKSET";
        btnFill = Color{ 60, 50, 30, 235 }; btnEdge = UI::GOLD_DARK; btnInk = UI::GOLD_LIGHT;
    } else if (canBuy) {
        btnText = TextFormat("KJOEP  -  %d g", sel.cost());
        btnFill = Color{ 150, 24, 36, 240 }; btnEdge = UI::GOLD_LIGHT; btnInk = UI::GOLD_LIGHT;
        UI::DrawGlow({ btn.x + btn.width / 2.0f, btn.y + btn.height / 2.0f }, btn.width * 0.6f, Fade(UI::GOLD_LIGHT, 0.12f + 0.06f * sinf(time * 4.0f)), Fade(UI::GOLD_LIGHT, 0.0f));
    } else {
        btnText = TextFormat("MANGLER %d g", sel.cost() - totalGold);
        btnFill = Color{ 40, 34, 44, 235 }; btnEdge = Color{ 110, 60, 60, 255 }; btnInk = TOO_POOR;
    }
    UI::DrawPanel(btn, 1.0f, btnEdge, btnFill);
    UI::DrawCenteredText(btnText, btn.x + btn.width / 2.0f, btn.y + 17.0f, 26.0f, btnInk);

    // "+1"-tekst som svever opp etter et kjøp
    float sincePurchase = (float)(GetTime() - purchaseTime);
    if (sincePurchase < 0.8f) {
        float a = 1.0f - sincePurchase / 0.8f;
        UI::DrawCenteredText("+1 NIVAA!", cx, info.y + 190.0f - sincePurchase * 60.0f, 26.0f, Fade(GREEN_OK, a));
    }

    UI::DrawCenteredText("[WASD] velg   [ENTER] kjoep   [ESC] tilbake", W / 2.0f, Settings::SCREEN_HEIGHT - 38.0f, 18.0f, Color{ 220, 210, 190, 255 });
}

void Shop::writeLevels(std::ostream& out) const {
    for (const auto& item : items) {
        out << item.saveKey << " " << item.currentLevel << "\n";
    }
}

bool Shop::readLevel(const std::string& key, int value) {
    for (auto& item : items) {
        if (item.saveKey == key) {
            item.currentLevel = std::max(0, std::min(value, item.maxLevel));
            return true;
        }
    }
    return false;
}

int Shop::totalCostOfEverything() const {
    int total = 0;
    for (ShopItem item : items) {
        for (item.currentLevel = 0; item.currentLevel < item.maxLevel; item.currentLevel++) {
            total += item.cost();
        }
    }
    return total;
}
