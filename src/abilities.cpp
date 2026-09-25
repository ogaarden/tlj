#include "abilities.hpp"
#include "character.hpp"
#include "player.hpp"
#include <algorithm>

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
AbilityLevel addArea(float a, const char* text) { return { text, [a](AbilityStats& s) { s.area += a; } }; }

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
        AbilityId::LIGHTNING, "Lightning", "Lyn slaar ned paa tilfeldige fiender i naerheten.", YELLOW,
        { .damage = 120.0f, .cooldown = 2.5f, .projectiles = 1, .radius = 450.0f /* rekkevidde */, .area = 50.0f },
        {
            addProjectiles(1, "+1 lyn"),
            damageMult(1.4f, "+40% skade"),
            addArea(25.0f, "+25 treffomraade"),
            cooldownMult(0.8f, "-20% cooldown"),
            addProjectiles(1, "+1 lyn"),
            damageMult(2.0f, "2x skade"),
            { "+1 lyn og +25 treffomraade", [](AbilityStats& s) { s.projectiles += 1; s.area += 25.0f; } },
            { "-30% cooldown og +1 lyn", [](AbilityStats& s) { s.cooldown *= 0.7f; s.projectiles += 1; } },
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

std::vector<AbilityChoice> GenerateLevelUpChoices(const Player& player, int count) {
    std::vector<AbilityChoice> candidates;

    // 1. Oppgraderinger for abilities vi allerede har (inkl. innate)
    for (const auto& w : player.weapons) {
        if (w->level >= MAX_ABILITY_LEVEL) continue;
        const AbilityDefinition& def = GetAbilityDefinition(w->id);
        candidates.push_back({
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
            candidates.push_back({ ChoiceType::NEW_ABILITY, id, def.name + "  (NY)", def.description, def.color });
        }
    }

    // Plukk tilfeldige valg
    std::vector<AbilityChoice> chosen;
    while ((int)chosen.size() < count && !candidates.empty()) {
        int idx = GetRandomValue(0, (int)candidates.size() - 1);
        chosen.push_back(candidates[idx]);
        candidates.erase(candidates.begin() + idx);
    }

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
        case ChoiceType::HEAL:
            player.hp = player.maxHp;
            break;
    }
}

// =====================================================================
// HUD
// =====================================================================

void DrawAbilityHud(const Player& player, int screenWidth, int screenHeight) {
    const int slotSize = 64;
    const int gap = 10;
    const int totalWidth = MAX_ABILITY_SLOTS * slotSize + (MAX_ABILITY_SLOTS - 1) * gap;
    const int startX = screenWidth / 2 - totalWidth / 2;
    const int y = screenHeight - slotSize - 28;

    for (int i = 0; i < MAX_ABILITY_SLOTS; i++) {
        int x = startX + i * (slotSize + gap);
        Rectangle slot = { (float)x, (float)y, (float)slotSize, (float)slotSize };

        if (i >= (int)player.weapons.size()) {
            // Låst slot: bare mørk
            DrawRectangleRec(slot, Fade(BLACK, 0.85f));
            DrawRectangleLinesEx(slot, 2.0f, Fade(DARKGRAY, 0.8f));
            continue;
        }

        const Weapon& w = *player.weapons[i];
        bool isInnate = (w.id == player.innateAbility);

        // Bakgrunn i abilityens farge
        DrawRectangleRec(slot, Fade(w.color, 0.35f));

        // Forkortelse av navnet i midten (f.eks. "MM" for Magic Missile)
        std::string initials;
        initials += w.name[0];
        size_t space = w.name.find(' ');
        if (space != std::string::npos && space + 1 < w.name.size()) initials += w.name[space + 1];
        else if (w.name.size() > 1) initials += w.name[1];
        int initialsWidth = MeasureText(initials.c_str(), 24);
        DrawText(initials.c_str(), x + slotSize / 2 - initialsWidth / 2, y + 12, 24, WHITE);

        // Cooldown: mørk overlay som krymper nedover mens abilityen lader
        float cd = w.cooldownProgress();
        if (cd < 1.0f) {
            float h = slotSize * (1.0f - cd);
            DrawRectangle(x, y, slotSize, (int)h, Fade(BLACK, 0.5f));
        }

        // Level-prikker (1-9) nederst i slotten
        const int pipSize = 4;
        const int pipGap = 2;
        int pipsWidth = MAX_ABILITY_LEVEL * pipSize + (MAX_ABILITY_LEVEL - 1) * pipGap;
        int pipX = x + slotSize / 2 - pipsWidth / 2;
        for (int l = 0; l < MAX_ABILITY_LEVEL; l++) {
            Color pipColor = (l < w.level) ? w.color : Fade(DARKGRAY, 0.8f);
            DrawRectangle(pipX + l * (pipSize + pipGap), y + slotSize - 10, pipSize, pipSize, pipColor);
        }

        // Innate har gull-ramme
        DrawRectangleLinesEx(slot, isInnate ? 3.0f : 2.0f, isInnate ? GOLD : w.color);

        // Navn under slotten
        int nameWidth = MeasureText(w.name.c_str(), 10);
        DrawText(w.name.c_str(), x + slotSize / 2 - nameWidth / 2, y + slotSize + 4, 10, LIGHTGRAY);
    }
}
