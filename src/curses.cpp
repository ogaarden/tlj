#include "curses.hpp"
#include "player.hpp"
#include <raylib.h>
#include <algorithm>

namespace {
    const std::vector<Curse> CURSES = {
        { CurseId::FRAILTY,  "Frailty",  "-25% maks HP",
            [](Player& p) { p.maxHp *= 0.75f; p.hp = p.maxHp; } },
        { CurseId::SLUGGISH, "Sluggish", "-15% fart",
            [](Player& p) { p.speed *= 0.85f; } },
        { CurseId::FAMINE,   "Famine",   "-30% XP",
            [](Player& p) { p.xpMultiplier *= 0.7f; } },
        { CurseId::BLUNT,    "Blunt",    "-20% skade",
            [](Player& p) { p.damageMult *= 0.8f; } },
        { CurseId::BRITTLE,  "Brittle",  "Ingen armor og evasion",
            [](Player& p) { p.armor = 0.0f; p.evasion = 0.0f; } },
        { CurseId::MYOPIA,   "Myopia",   "-50% pickup-radius",
            [](Player& p) { p.lootRadius *= 0.5f; } },
    };
}

const Curse& GetCurse(CurseId id) {
    for (const auto& c : CURSES) {
        if (c.id == id) return c;
    }
    return CURSES[0];
}

std::vector<CurseId> GetCurseChoices(const std::vector<CurseId>& alreadyChosen, int count) {
    std::vector<CurseId> candidates;
    for (const auto& c : CURSES) {
        if (std::find(alreadyChosen.begin(), alreadyChosen.end(), c.id) == alreadyChosen.end()) {
            candidates.push_back(c.id);
        }
    }

    std::vector<CurseId> chosen;
    while ((int)chosen.size() < count && !candidates.empty()) {
        int idx = GetRandomValue(0, (int)candidates.size() - 1);
        chosen.push_back(candidates[idx]);
        candidates.erase(candidates.begin() + idx);
    }
    return chosen;
}
