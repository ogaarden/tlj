#ifndef SAVE_HPP
#define SAVE_HPP

#include <string>
#include "shop.hpp"

// All metaprogresjon som skal huskes mellom økter
struct SaveData {
    int gold = 0;
    int unlockedEchelon = 1; // Høyeste echelon spilleren har tilgang til
    int volume = 70;         // Hovedvolum i prosent
    int musicVolume = 60;    // Musikkvolum i prosent (i tillegg til hovedvolumet)
    int drawnFigures = 0;    // 1 = tegnede figurer (sprites), 0 = 3D-modeller (standard)
};

void SaveGame(const std::string& path, const SaveData& data, const Shop& shop);
void LoadGame(const std::string& path, SaveData& data, Shop& shop);

#endif // SAVE_HPP
