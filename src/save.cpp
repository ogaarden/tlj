#include "save.hpp"
#include "echelon.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>

// Formatet er enkel tekst, én verdi per linje: "nøkkel verdi"
// Ukjente nøkler ignoreres, så gamle lagringsfiler fungerer fortsatt når vi legger til nye ting.

void SaveGame(const std::string& path, const SaveData& data, const Shop& shop) {
    std::ofstream file(path);
    if (!file) return;

    file << "gold " << data.gold << "\n";
    file << "echelon " << data.unlockedEchelon << "\n";
    file << "volume " << data.volume << "\n";
    shop.writeLevels(file);
}

void LoadGame(const std::string& path, SaveData& data, Shop& shop) {
    std::ifstream file(path);
    if (!file) return; // Ingen lagring ennå – ny spiller

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream in(line);
        std::string key;
        int value = 0;
        if (!(in >> key >> value)) continue;

        if (key == "gold") data.gold = std::max(0, value);
        else if (key == "echelon") data.unlockedEchelon = std::clamp(value, 1, MAX_ECHELON);
        else if (key == "volume") data.volume = std::clamp(value, 0, 100);
        else shop.readLevel(key, value);
    }
}
