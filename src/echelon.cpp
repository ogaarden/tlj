#include "echelon.hpp"
#include <vector>
#include <algorithm>

namespace {
    // Timer i minutter. Echelon 1 er 10 minutter, og det blir lenger jo dypere man kommer.
    const std::vector<EchelonData> ECHELONS = {
        {  1, "Echelon I",    10.0f * 60.0f },
        {  2, "Echelon II",   11.0f * 60.0f },
        {  3, "Echelon III",  12.0f * 60.0f },
        {  4, "Echelon IV",   13.0f * 60.0f },
        {  5, "Echelon V",    14.0f * 60.0f },
        {  6, "Echelon VI",   15.0f * 60.0f },
        {  7, "Echelon VII",  16.0f * 60.0f },
        {  8, "Echelon VIII", 17.0f * 60.0f },
        {  9, "Echelon IX",   18.0f * 60.0f },
        { 10, "Echelon X",    20.0f * 60.0f },
    };
}

const EchelonData& GetEchelon(int echelon) {
    int index = std::clamp(echelon, 1, MAX_ECHELON) - 1;
    return ECHELONS[index];
}
