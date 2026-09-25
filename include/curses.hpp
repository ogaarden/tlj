#ifndef CURSES_HPP
#define CURSES_HPP

#include <string>
#include <vector>
#include <functional>

struct Player;

// Curses velges før runden starter på echelon 5+ (to stk på echelon 10).
// De er rene ulemper som gjør runden vanskeligere.
enum class CurseId {
    FRAILTY,
    SLUGGISH,
    FAMINE,
    BLUNT,
    BRITTLE,
    MYOPIA,
    COUNT
};

struct Curse {
    CurseId id;
    std::string name;
    std::string description;
    std::function<void(Player&)> apply;
};

const Curse& GetCurse(CurseId id);

// Tilfeldige curses å velge mellom (uten de man allerede har)
std::vector<CurseId> GetCurseChoices(const std::vector<CurseId>& alreadyChosen, int count = 3);

#endif // CURSES_HPP
