#ifndef ABILITIES_HPP
#define ABILITIES_HPP

#include <string>

enum AbilityType {
    UPGRADE_EXISTING_WEAPON,
    SPAWN_NEW_WEAPON,
    PASSIVE_STAT_BUFF // F.eks. +10% fart eller mer HP
};

struct AbilityChoice {
    std::string title;       // F.eks. "Trefork Level 2" eller "Ildstav"
    std::string description; // "Skyter raskere og hardere"
    AbilityType type;
    int targetWeaponIndex;   // Hvilket våpen i spillerens liste som oppgraderes (hvis relevant)
};

#endif