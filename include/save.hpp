#ifndef SAVE_HPP
#define SAVE_HPP

#include <string>
#include "shop.hpp"

// All metaprogresjon som skal huskes mellom økter
struct SaveData {
    int gold = 0;
    int unlockedEchelon = 1; // Høyeste echelon spilleren har tilgang til
    int volume = 70;         // Hovedvolum i prosent
};

void SaveGame(const std::string& path, const SaveData& data, const Shop& shop);
void LoadGame(const std::string& path, SaveData& data, Shop& shop);

#endif // SAVE_HPP
