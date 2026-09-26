#include "abilities.hpp"
#include "character.hpp"
#include "player.hpp"
#include "ui.hpp"
#include "icons.hpp"
#include "shop.hpp"
#include "items.hpp"
#include <algorithm>
#include <cmath>

// =====================================================================
// ABILITY-DEFINISJONER
// Her bestemmes basestats og hva som skjer på HVERT level (2-9).
// Vil du endre balansen, er det bare å endre tallene her.
// =====================================================================

namespace {

// Små hjelpere så level-tabellene blir lette å lese
AbilityLevel damageMult(float mult, const char* text) { return { text, [mult](AbilityStats& s) { s.damage *= mult; } }; }
AbilityLevel cooldownMult(float mult, const char* text) { return { text, [mult](AbilityStats& s) { s.cooldown *= mult; } }; }
AbilityLevel addProjectiles(int n, const char* text) { return { text, [n](AbilityStats& s) { s.projectiles += n; } }; }
AbilityLevel addPierce(int n, const char* text) { return { text, [n](AbilityStats& s) { s.pierce += n; } }; }
AbilityLevel addBounces(int n, const char* text) { return { text, [n](AbilityStats& s) { s.bounces += n; } }; }
AbilityLevel addRadius(float r, const char* text) { return { text, [r](AbilityStats& s) { s.radius += r; } }; }

std::vector<AbilityDefinition> buildDefinitions() {
    std::vector<AbilityDefinition> defs;

    // ---------------- INNATE ----------------

    defs.push_back({
        AbilityId::TREFORK, "Trefork", "Skyter en vifte av prongs mot naermeste fiende.", GOLD,
        { .damage = 100.0f, .cooldown = 1.0f, .speed = 500.0f, .projectiles = 3 },
        {
            addPierce(1, "+1 gjennomboring"),
            damageMult(1.3f, "+30% skade"),
            cooldownMult(0.85f, "-15% cooldown"),
            addProjectiles(2, "+2 prongs"),
            addPierce(1, "+1 gjennomboring"),
            damageMult(2.0f, "2x skade"),
            cooldownMult(0.8f, "-20% cooldown"),
            { "+2 prongs og +1 gjennomboring", [](AbilityStats& s) { s.projectiles += 2; s.pierce += 1; } },
        }
    });

    defs.push_back({
        AbilityId::GROUND_SLAM, "Ground Slam", "Slaar i bakken og skader alt rundt deg.", ORANGE,
        { .damage = 80.0f, .cooldown = 2.0f, .radius = 130.0f },
        {
            addRadius(20.0f, "+20 radius"),
            damageMult(1.4f, "+40% skade"),
            cooldownMult(0.85f, "-15% cooldown"),
            addRadius(30.0f, "+30 radius"),
            damageMult(2.0f, "2x skade"),
            cooldownMult(0.8f, "-20% cooldown"),
            addRadius(40.0f, "+40 radius"),
            { "2x skade og -20% cooldown", [](AbilityStats& s) { s.damage *= 2.0f; s.cooldown *= 0.8f; } },
        }
    });

    defs.push_back({
        AbilityId::RICOCHET, "Ricochet", "Kule som spretter videre, svakere for hvert sprett.", SKYBLUE,
        { .damage = 60.0f, .cooldown = 0.9f, .speed = 550.0f, .projectiles = 1,
          .bounces = 3, .bounceRange = 250.0f, .bounceFalloff = 0.7f },
        {
            addBounces(1, "+1 sprett"),
            damageMult(1.3f, "+30% skade"),
            { "Sprett mister mindre skade (-20% i stedet for -30%)", [](AbilityStats& s) { s.bounceFalloff = 0.8f; } },
            addProjectiles(1, "+1 kule"),
            addBounces(2, "+2 sprett"),
            damageMult(2.0f, "2x skade"),
            cooldownMult(0.75f, "-25% cooldown"),
            { "Sprett mister ingen skade, +2 sprett", [](AbilityStats& s) { s.bounceFalloff = 1.0f; s.bounces += 2; } },
        }
    });

    defs.push_back({
        AbilityId::PIE, "Kakekast", "Lobber kremkaker som spruter og lar krem ligge igjen.", Color{ 255, 180, 200, 255 },
        { .damage = 70.0f, .cooldown = 1.3f, .projectiles = 2, .radius = 55.0f },
        {
            addProjectiles(1, "+1 kake"),
            damageMult(1.3f, "+30% skade"),
            addRadius(15.0f, "+15 sprut-radius"),
            cooldownMult(0.85f, "-15% cooldown"),
            addProjectiles(1, "+1 kake"),
            damageMult(2.0f, "2x skade"),
            addRadius(20.0f, "+20 sprut-radius"),
            { "+2 kaker og -20% cooldown", [](AbilityStats& s) { s.projectiles += 2; s.cooldown *= 0.8f; } },
        }
    });

    // ---------------- FELLES POOL ----------------

    defs.push_back({
        AbilityId::MAGIC_MISSILE, "Magic Missile", "Maalsoekende missil som spretter videre ved treff.", VIOLET,
        { .damage = 45.0f, .cooldown = 1.2f, .speed = 350.0f, .projectiles = 1,
          .bounces = 1, .bounceRange = 300.0f, .bounceFalloff = 1.0f },
        {
            addBounces(1, "+1 sprett"),
            addProjectiles(1, "+1 missil"),
            damageMult(1.4f, "+40% skade"),
            addBounces(1, "+1 sprett"),
            cooldownMult(0.8f, "-20% cooldown"),
            addProjectiles(1, "+1 missil"),
            damageMult(2.0f, "2x skade"),
            { "+2 sprett og +1 missil", [](AbilityStats& s) { s.bounces += 2; s.projectiles += 1; } },
        }
    });

    defs.push_back({
        AbilityId::ROT, "Rot", "Giftaura som skader alle fiender rundt deg hver frame.", LIME,
        { .damage = 40.0f /* per sekund */, .radius = 100.0f },
        {
            addRadius(15.0f, "+15 radius"),
            damageMult(1.5f, "+50% skade"),
            addRadius(20.0f, "+20 radius"),
            damageMult(1.5f, "+50% skade"),
            addRadius(25.0f, "+25 radius"),
            damageMult(2.0f, "2x skade"),
            addRadius(30.0f, "+30 radius"),
            damageMult(2.0f, "2x skade"),
        }
    });

    defs.push_back({
        AbilityId::DAGGER, "Dagger", "Kaster raske dolker mot de naermeste fiendene.", LIGHTGRAY,
        { .damage = 30.0f, .cooldown = 0.4f, .speed = 700.0f, .projectiles = 1 },
        {
            addProjectiles(1, "+1 dolk"),
            damageMult(1.4f, "+40% skade"),
            addPierce(1, "+1 gjennomboring"),
            cooldownMult(0.8f, "-20% cooldown"),
            addProjectiles(1, "+1 dolk"),
            damageMult(2.0f, "2x skade"),
            { "+1 dolk og +1 gjennomboring", [](AbilityStats& s) { s.projectiles += 1; s.pierce += 1; } },
            cooldownMult(0.7f, "-30% cooldown"),
        }
    });

    defs.push_back({
        AbilityId::ORBIT_BLADES, "Orbit Blades", "Blader som sirkler rundt deg.", PINK,
        { .damage = 35.0f, .cooldown = 0.5f /* tid mellom treff paa samme fiende */, .speed = 180.0f, .projectiles = 2, .radius = 90.0f },
        {
            addProjectiles(1, "+1 blad"),
            damageMult(1.4f, "+40% skade"),
            { "+30% rotasjonsfart", [](AbilityStats& s) { s.speed *= 1.3f; } },
            addProjectiles(1, "+1 blad"),
            addRadius(25.0f, "+25 radius"),
            damageMult(2.0f, "2x skade"),
            addProjectiles(1, "+1 blad"),
            { "2x skade og +1 blad", [](AbilityStats& s) { s.damage *= 2.0f; s.projectiles += 1; } },
        }
    });

    defs.push_back({
        AbilityId::LIGHTNING, "Lightning", "Lyn slaar ned og kjeder videre til fiender, svakere for hvert hopp.", YELLOW,
        { .damage = 120.0f, .cooldown = 2.5f, .projectiles = 1,
          .bounces = 3 /* kjede-hopp */, .bounceRange = 200.0f, .bounceFalloff = 0.7f,
          .radius = 450.0f /* rekkevidde */, .area = 50.0f },
        {
            addBounces(1, "+1 kjede-hopp"),
            damageMult(1.4f, "+40% skade"),
            addProjectiles(1, "+1 lyn"),
            { "Kjeden mister mindre skade (-20% per hopp)", [](AbilityStats& s) { s.bounceFalloff = 0.8f; } },
            addBounces(2, "+2 kjede-hopp"),
            damageMult(2.0f, "2x skade"),
            { "-25% cooldown og +1 lyn", [](AbilityStats& s) { s.cooldown *= 0.75f; s.projectiles += 1; } },
            { "+2 kjede-hopp og lengre hopp", [](AbilityStats& s) { s.bounces += 2; s.bounceRange += 60.0f; } },
        }
    });

    return defs;
}

const std::vector<AbilityDefinition>& allDefinitions() {
    static const std::vector<AbilityDefinition> defs = buildDefinitions();
    return defs;
}

} // namespace

const AbilityDefinition& GetAbilityDefinition(AbilityId id) {
    for (const auto& def : allDefinitions()) {
        if (def.id == id) return def;
    }
    return allDefinitions()[0];
}

std::unique_ptr<Weapon> CreateAbility(AbilityId id) {
    std::unique_ptr<Weapon> ability;
    switch (id) {
        case AbilityId::TREFORK:       ability = std::make_unique<ProjectileWeapon>(true); break;
        case AbilityId::DAGGER:        ability = std::make_unique<ProjectileWeapon>(false); break;
        case AbilityId::GROUND_SLAM:   ability = std::make_unique<MeleeWeapon>(); break;
        case AbilityId::RICOCHET:      ability = std::make_unique<BouncingProjectileWeapon>(false); break;
        case AbilityId::MAGIC_MISSILE: ability = std::make_unique<BouncingProjectileWeapon>(true); break;
        case AbilityId::ROT:           ability = std::make_unique<RotWeapon>(); break;
        case AbilityId::ORBIT_BLADES:  ability = std::make_unique<OrbitWeapon>(); break;
        case AbilityId::LIGHTNING:     ability = std::make_unique<LightningWeapon>(); break;
        case AbilityId::PIE:           ability = std::make_unique<PieWeapon>(); break;
        case AbilityId::COUNT:         return nullptr;
    }

    const AbilityDefinition& def = GetAbilityDefinition(id);
    ability->id = id;
    ability->name = def.name;
    ability->color = def.color;
    ability->stats = def.baseStats;
    ability->level = 1;
    return ability;
}

void LevelUpAbility(Weapon& ability) {
    if (ability.level >= MAX_ABILITY_LEVEL) return;

    const AbilityDefinition& def = GetAbilityDefinition(ability.id);
    int upgradeIndex = ability.level - 1; // Level 1 -> 2 bruker levels[0]
    if (upgradeIndex < (int)def.levels.size()) {
        def.levels[upgradeIndex].apply(ability.stats);
    }
    ability.level++;
}

std::vector<AbilityId> GetSharedAbilityPool() {
    std::vector<AbilityId> innates;
    for (const auto& c : GetAvailableCharacters()) innates.push_back(c.innateAbility);

    std::vector<AbilityId> pool;
    for (const auto& def : allDefinitions()) {
        if (std::find(innates.begin(), innates.end(), def.id) == innates.end()) {
            pool.push_back(def.id);
        }
    }
    return pool;
}

// =====================================================================
// LEVEL-UP-VALG
// =====================================================================

// ---------------------------------------------------------------------
// EVOLUSJONER: hver ability har ett item som er nøkkelen.
// Ability på level 9 + itemet (hvilket som helst nivå) -> neste skattekiste
// gjør den om til en superversjon.
// ---------------------------------------------------------------------
namespace {
struct EvolutionData {
    Evolution info;
    void (*apply)(AbilityStats&);
};

const std::vector<EvolutionData>& evolutions() {
    static const std::vector<EvolutionData> list = {
        { { AbilityId::TREFORK, ItemId::JUGGLING_BALL, "Poseidons trefork", "+4 prongs, +3 gjennomboring og +50% skade", Color{ 80, 220, 255, 255 } },
          [](AbilityStats& s) { s.projectiles += 4; s.pierce += 3; s.damage *= 1.5f; } },
        { { AbilityId::GROUND_SLAM, ItemId::CHAINMAIL, "Jordskjelv", "+80 radius, 2x skade og -20% cooldown", Color{ 255, 110, 40, 255 } },
          [](AbilityStats& s) { s.radius += 80.0f; s.damage *= 2.0f; s.cooldown *= 0.8f; } },
        { { AbilityId::RICOCHET, ItemId::LUCKY_DIE, "Kaoskule", "+2 kuler, +6 sprett og ingen svekkelse per sprett", Color{ 255, 80, 220, 255 } },
          [](AbilityStats& s) { s.projectiles += 2; s.bounces += 6; s.bounceFalloff = 1.0f; } },
        { { AbilityId::PIE, ItemId::ROYAL_CAPE, "Bryllupskake", "+3 kaker, +45 sprut-radius og +50% skade", Color{ 255, 235, 245, 255 } },
          [](AbilityStats& s) { s.projectiles += 3; s.radius += 45.0f; s.damage *= 1.5f; } },
        { { AbilityId::MAGIC_MISSILE, ItemId::OWL_FEATHER, "Stjerneregn", "+4 missiler, +3 sprett og +30% skade", Color{ 150, 120, 255, 255 } },
          [](AbilityStats& s) { s.projectiles += 4; s.bounces += 3; s.damage *= 1.3f; } },
        { { AbilityId::ROT, ItemId::HEART_AMULET, "Svartedauden", "+60 radius og 2.5x skade", Color{ 150, 60, 200, 255 } },
          [](AbilityStats& s) { s.radius += 60.0f; s.damage *= 2.5f; } },
        { { AbilityId::DAGGER, ItemId::JESTER_SHOES, "Tusen kniver", "+5 dolker, +2 gjennomboring og -40% cooldown", Color{ 230, 235, 255, 255 } },
          [](AbilityStats& s) { s.projectiles += 5; s.pierce += 2; s.cooldown *= 0.6f; } },
        { { AbilityId::ORBIT_BLADES, ItemId::WHETSTONE, "Staalvirvel", "+4 blader, 2x skade, raskere og videre", Color{ 200, 225, 255, 255 } },
          [](AbilityStats& s) { s.projectiles += 4; s.damage *= 2.0f; s.speed *= 1.6f; s.radius += 30.0f; } },
        { { AbilityId::LIGHTNING, ItemId::HOURGLASS, "Tordenguden", "+3 lyn, +4 kjede-hopp, -40% cooldown og +50% skade", Color{ 255, 250, 140, 255 } },
          [](AbilityStats& s) { s.projectiles += 3; s.bounces += 4; s.cooldown *= 0.6f; s.damage *= 1.5f; } },
    };
    return list;
}
} // namespace

const Evolution* GetEvolution(AbilityId ability) {
    for (const auto& e : evolutions()) if (e.info.ability == ability) return &e.info;
    return nullptr;
}

const Evolution* GetEvolutionForItem(ItemId item) {
    for (const auto& e : evolutions()) if (e.info.item == item) return &e.info;
    return nullptr;
}

bool CanEvolve(const Player& player, const Weapon& weapon) {
    if (weapon.evolved || weapon.level < MAX_ABILITY_LEVEL) return false;
    const Evolution* evo = GetEvolution(weapon.id);
    return evo && player.itemLevels[(int)evo->item] > 0;
}

void EvolveAbility(Weapon& weapon) {
    for (const auto& e : evolutions()) {
        if (e.info.ability != weapon.id) continue;
        e.apply(weapon.stats);
        weapon.evolved = true;
        weapon.name = e.info.name;
        weapon.color = e.info.color;
        return;
    }
}

std::vector<AbilityChoice> GenerateLevelUpChoices(const Player& player, int count) {
    std::vector<AbilityChoice> abilityCandidates;
    std::vector<AbilityChoice> itemCandidates;

    // 1. Oppgraderinger for abilities vi allerede har (inkl. innate)
    for (const auto& w : player.weapons) {
        if (w->level >= MAX_ABILITY_LEVEL) continue;
        const AbilityDefinition& def = GetAbilityDefinition(w->id);
        abilityCandidates.push_back({
            ChoiceType::UPGRADE_ABILITY, w->id,
            TextFormat("%s  Lv %d -> %d", def.name.c_str(), w->level, w->level + 1),
            def.levels[w->level - 1].description,
            def.color
        });
    }

    // 2. Nye abilities fra poolen, hvis det er ledige slots
    if ((int)player.weapons.size() < MAX_ABILITY_SLOTS) {
        for (AbilityId id : GetSharedAbilityPool()) {
            if (player.findAbility(id)) continue; // Har den allerede -> tilbys som oppgradering over
            const AbilityDefinition& def = GetAbilityDefinition(id);
            abilityCandidates.push_back({ ChoiceType::NEW_ABILITY, id, def.name + "  (NY)", def.description, def.color });
        }
    }

    // 3. Items: neste nivå av de man har, og nye så lenge det er ledige item-plasser
    for (int i = 0; i < (int)ItemId::COUNT; i++) {
        int level = player.itemLevels[i];
        if (level >= MAX_ITEM_LEVEL) continue;
        if (level == 0 && !HasItemSlotFree(player)) continue;
        const ItemDef& def = GetItemDef((ItemId)i);
        AbilityChoice c{ ChoiceType::ITEM, AbilityId::COUNT, def.name, def.description, def.color };
        c.item = (ItemId)i;
        itemCandidates.push_back(c);
    }

    // Fordeling: minst ett av hver når det finnes, ellers fyll opp med det som er igjen
    int itemPicks = 0;
    if (!itemCandidates.empty()) {
        itemPicks = abilityCandidates.empty() ? count : (count >= 4 ? 2 : GetRandomValue(1, 2));
        itemPicks = std::min(itemPicks, count - (abilityCandidates.empty() ? 0 : 1));
        itemPicks = std::max(itemPicks, 1);
    }
    std::vector<AbilityChoice> chosen;
    auto pickFrom = [&](std::vector<AbilityChoice>& from, int n) {
        while (n-- > 0 && !from.empty()) {
            int idx = GetRandomValue(0, (int)from.size() - 1);
            chosen.push_back(from[idx]);
            from.erase(from.begin() + idx);
        }
    };
    pickFrom(itemCandidates, itemPicks);
    pickFrom(abilityCandidates, count - (int)chosen.size());
    pickFrom(itemCandidates, count - (int)chosen.size());
    for (int i = (int)chosen.size() - 1; i > 0; i--) std::swap(chosen[i], chosen[GetRandomValue(0, i)]);

    // Alt er maks-level og alle slots er fulle
    if (chosen.empty()) {
        chosen.push_back({ ChoiceType::HEAL, AbilityId::COUNT, "Restituer", "Fyller opp all HP.", RED });
    }
    return chosen;
}

void ApplyAbilityChoice(Player& player, const AbilityChoice& choice) {
    switch (choice.type) {
        case ChoiceType::NEW_ABILITY:
            player.addWeapon(CreateAbility(choice.ability));
            break;
        case ChoiceType::UPGRADE_ABILITY:
            if (Weapon* w = player.findAbility(choice.ability)) LevelUpAbility(*w);
            break;
        case ChoiceType::ITEM:
            ApplyItemLevel(player, choice.item);
            break;
        case ChoiceType::EVOLUTION:
            if (Weapon* w = player.findAbility(choice.ability)) EvolveAbility(*w);
            break;
        case ChoiceType::HEAL:
            player.hp = player.maxHp;
            break;
    }
}

// =====================================================================
// HUD
// =====================================================================

void DrawAbilityHud(const Player& player, float centerX, float bottom, float scale) {
    const float s = scale;
    const float slotSize = 58.0f * s;
    const float gap = 10.0f * s;
    const float pad = 12.0f * s;
    const float totalWidth = MAX_ABILITY_SLOTS * slotSize + (MAX_ABILITY_SLOTS - 1) * gap;
    const float labelH = 16.0f * s;

    // Felles panel rundt alle slotsene
    Rectangle panel = { centerX - totalWidth / 2.0f - pad, bottom - slotSize - labelH - pad * 2.0f, totalWidth + pad * 2.0f, slotSize + labelH + pad * 2.0f };
    UI::DrawPanel(panel, s);

    const float startX = panel.x + pad;
    const float y = panel.y + pad;
    auto text = [&](const char* t, float cx, float ty, float size, Color c) {
        size = std::round(size);
        float w = MeasureTextEx(GetFontDefault(), t, size, size / 10.0f).x;
        UI::DrawOutlinedText(t, std::round(cx - w / 2.0f), std::round(ty), size, c, std::max(1.0f, size / 12.0f));
    };

    for (int i = 0; i < MAX_ABILITY_SLOTS; i++) {
        float x = startX + i * (slotSize + gap);
        Rectangle slot = { x, y, slotSize, slotSize };
        DrawRectangleRec({ x - 2.0f * s, y - 2.0f * s, slotSize + 4.0f * s, slotSize + 4.0f * s }, UI::INK);

        if (i >= (int)player.weapons.size()) {
            // Låst slot: mørk med en liten lås
            DrawRectangleRec(slot, Color{ 16, 14, 20, 255 });
            DrawRectangleLinesEx(slot, 1.5f * s, Fade(GRAY, 0.35f));
            Vector2 c = { x + slotSize / 2.0f, y + slotSize / 2.0f };
            DrawRing({ c.x, c.y - 4.0f * s }, 5.0f * s, 7.5f * s, 180.0f, 360.0f, 12, Fade(GRAY, 0.4f));
            DrawRectangleRec({ c.x - 9.0f * s, c.y - 4.0f * s, 18.0f * s, 13.0f * s }, Fade(GRAY, 0.4f));
            continue;
        }

        const Weapon& w = *player.weapons[i];
        bool isInnate = (w.id == player.innateAbility);

        // Bakgrunn: abilityens farge med lys øverst
        DrawRectangleRec(slot, Color{ 20, 18, 26, 255 });
        DrawRectangleGradientV((int)x, (int)y, (int)slotSize, (int)slotSize, Fade(w.color, 0.55f), Fade(w.color, 0.15f));

        // Ikonet til abilityen i midten
        DrawAbilityIcon(w.id, { x + slotSize / 2.0f, y + slotSize * 0.44f }, slotSize * 0.33f);

        // Cooldown: mørk overlay som krymper nedover mens abilityen lader
        float cd = w.cooldownProgress();
        if (cd < 1.0f) {
            float h = slotSize * (1.0f - cd);
            DrawRectangleRec({ x, y, slotSize, h }, Fade(BLACK, 0.55f));
            DrawRectangleRec({ x, y + h - 1.0f * s, slotSize, 2.0f * s }, Fade(WHITE, 0.5f));
        }

        // Level-prikker (1-9) nederst i slotten
        const float pip = 4.0f * s;
        const float pipGap = 1.5f * s;
        float pipsWidth = MAX_ABILITY_LEVEL * pip + (MAX_ABILITY_LEVEL - 1) * pipGap;
        float pipX = x + slotSize / 2.0f - pipsWidth / 2.0f;
        DrawRectangleRec({ pipX - 2.0f * s, y + slotSize - 11.0f * s, pipsWidth + 4.0f * s, pip + 4.0f * s }, Fade(BLACK, 0.5f));
        for (int l = 0; l < MAX_ABILITY_LEVEL; l++) {
            Color pipColor = (l < w.level) ? UI::GOLD_LIGHT : Fade(DARKGRAY, 0.8f);
            DrawRectangleRec({ pipX + l * (pip + pipGap), y + slotSize - 9.0f * s, pip, pip }, pipColor);
        }

        // Innate har tykk gullramme og en liten krone-prikk
        DrawRectangleLinesEx(slot, isInnate ? 3.0f * s : 2.0f * s, isInnate ? UI::GOLD_LIGHT : w.color);
        if (w.evolved) {
            // Evolvert: magenta ramme og stjerne i hjørnet
            DrawRectangleLinesEx(slot, 3.0f * s, Color{ 255, 120, 255, 255 });
            Vector2 st = { x + slotSize - 8.0f * s, y + 8.0f * s };
            DrawPoly(st, 5, 7.0f * s, (float)GetTime() * 90.0f, UI::INK);
            DrawPoly(st, 5, 5.5f * s, (float)GetTime() * 90.0f, Color{ 255, 140, 255, 255 });
        } else if (CanEvolve(player, w)) {
            // Klar for evolusjon: pulserende gullglød – finn en skattekiste!
            float pulse = 0.5f + 0.5f * sinf((float)GetTime() * 7.0f);
            DrawRectangleLinesEx({ x - 4.0f * s, y - 4.0f * s, slotSize + 8.0f * s, slotSize + 8.0f * s }, 2.5f * s, Fade(UI::GOLD_LIGHT, 0.4f + 0.6f * pulse));
            text("KLAR!", x + slotSize / 2.0f, y - 16.0f * s, 11.0f * s, Fade(UI::GOLD_LIGHT, 0.6f + 0.4f * pulse));
        }
        if (isInnate) {
            DrawCircleV({ x + 7.0f * s, y + 7.0f * s }, 4.0f * s, UI::INK);
            DrawCircleV({ x + 7.0f * s, y + 7.0f * s }, 2.8f * s, UI::GOLD_LIGHT);
        }

        // Navn under slotten
        text(w.name.c_str(), x + slotSize / 2.0f, y + slotSize + 5.0f * s, 10.0f * s, Color{ 225, 218, 200, 255 });
    }
}
