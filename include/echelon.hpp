#ifndef ECHELON_HPP
#define ECHELON_HPP

#include <string>

// Spillet har 10 echelons som blir progressivt vanskeligere.
// Man starter på echelon 1, og låser opp neste ved å slå bossen.
//
// Hver echelon har en timer: når den er ferdig blir spilleren teleportert til
// boss-arenaen. Dypere echelons har lengre timer.
//
// Senere skal hver echelon også få egne vanskelighets-modifikatorer
// (f.eks. echelon 3: fiender slower deg ved treff, echelon 4: mindre XP-drop).
// De legges til som nye felt i EchelonData og fylles ut i echelon.cpp.

constexpr int MAX_ECHELON = 10;

struct EchelonData {
    int number;
    std::string name;
    float bossTimerSeconds; // Tid før man teleporteres til boss-arenaen
};

// echelon: 1 til MAX_ECHELON
const EchelonData& GetEchelon(int echelon);

#endif // ECHELON_HPP
