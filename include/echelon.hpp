#ifndef ECHELON_HPP
#define ECHELON_HPP

#include <string>
#include <vector>
#include <functional>

// Spillet har 10 echelons. Man starter med bare echelon 1 låst opp,
// og låser opp neste ved å slå bossen.
//
// Hver echelon legger til ÉN unik ting, og alle stacker: echelon 4 har
// alt fra echelon 1, 2, 3 og 4. Hver echelon gir også +30 sek før bossen
// (= én ekstra wave).

constexpr int MAX_ECHELON = 10;
constexpr float BASE_BOSS_TIMER = 10.0f * 60.0f; // Echelon 1: boss etter 10 min
constexpr float BOSS_TIMER_PER_ECHELON = 30.0f;  // +30 sek per echelon

// Summen av alle aktive echelon-effekter. Standardverdiene = echelon 1 (ingen effekter).
struct EchelonModifiers {
    bool exploders = false;        // Kamikaze-fiender i wavene
    float slowOnHit = 0.0f;        // Hvor mye spilleren slowes ved treff (0.4 = -40% fart)
    float slowDuration = 0.0f;     // Hvor lenge sloweffekten varer
    float enemyDamageMult = 1.0f;
    int curses = 0;                // Antall curses man må velge før runden starter
    float enemySpeedMult = 1.0f;
    float xpMult = 1.0f;
    bool noRegen = false;
    float enemyHpMult = 1.0f;
};

struct EchelonData {
    int number;
    std::string name;
    std::string description; // Hva DENNE echelonen legger til
    std::function<void(EchelonModifiers&)> apply;
};

// echelon: 1 til MAX_ECHELON
const EchelonData& GetEchelon(int echelon);

// Alle effekter fra echelon 1 opp til og med `echelon`, stacket
EchelonModifiers GetEchelonModifiers(int echelon);

// Tid før man sendes til bossen
float GetBossTimer(int echelon);

#endif // ECHELON_HPP
