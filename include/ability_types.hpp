#ifndef ABILITY_TYPES_HPP
#define ABILITY_TYPES_HPP

// Grunnleggende typer for abilities. Ligger i egen fil slik at både
// character.hpp, weapon.hpp og abilities.hpp kan bruke dem uten sirkulære includes.

constexpr int MAX_ABILITY_LEVEL = 9;
constexpr int MAX_ABILITY_SLOTS = 6; // 1 innate + 5 som låses opp via level up

enum class AbilityId {
    // Innate (unike for hver karakter)
    TREFORK,
    GROUND_SLAM,
    RICOCHET,
    PIE,        // Pierrot: kremkaker som lobbes og spruter

    // Felles pool
    MAGIC_MISSILE,
    ROT,
    DAGGER,
    ORBIT_BLADES,
    LIGHTNING,
    FIRE_BREATH,  // Ildsluker: ildkjegle mot nærmeste fiende
    BOOMERANG,    // Bumerang: flyr ut og kommer tilbake
    CARDS,        // Kortstokk: kort i alle retninger
    FROST_NOVA,   // Frostnova: fryser og bremser alt rundt deg
    CATAPULT,     // Katapult: steinblokker faller ned fra himmelen
    BELLS,        // Narrebjeller: lydbølger som dytter fiender bort
    SPOTLIGHT,    // Rampelys: roterende lysstråler
    SABRE,        // Sabelhugg: brede hugg mot nærmeste fiende
    TORNADO,      // Virvelvind: vandrende virvelvinder som suger inn fiender

    COUNT
};

// Items: passive gjenstander man finner i skattekister. Nullstilles hver runde.
// Hver har 5 nivåer, og man kan bare bære MAX_ITEM_SLOTS forskjellige – så man må velge.
// Kister er sjeldne (ca. 20 skatter i en hel runde), og en kombinasjon koster 11 skatter,
// så man må bestemme seg tidlig for hvilke par man satser på.
// To items på maks nivå kan kombineres til et sterkere item (se items.cpp).
enum class ItemId {
    JUGGLING_BALL,  // Sjonglørball: +1 prosjektil
    ROYAL_CAPE,     // Kongens kappe: +område
    JESTER_SHOES,   // Narreskoene: +fart
    WHETSTONE,      // Slipestein: +skade
    HOURGLASS,      // Timeglass: -cooldown
    HEART_AMULET,   // Hjerteamulett: +maks HP og regen
    CHAINMAIL,      // Ringbrynje: +armor
    LODESTONE,      // Magnetstein: +pickup-radius
    OWL_FEATHER,    // Uglefjær: +XP
    LUCKY_DIE,      // Heldig terning: +kritisk treff
    VAMPIRE_FANG,   // Vampyrtann: liv per drap
    THORN_COLLAR,   // Piggkrage: fiender som treffer deg tar skade
    CROWN_JEWEL,    // Kronjuvel: +gull og XP
    SHADOW_CLOAK,   // Skyggekappe: +unnvikelse
    SPYGLASS,       // Kikkert: +prosjektilfart og rekkevidde
    CANDLE,         // Evighetslys: +varighet
    MAGIC_MIRROR,   // Trollspeil: +1 reroll
    CLOVER,         // Firkløver: +flaks (oftere kister og sjeldne drops)
    HOLY_RELIC,     // Helgenrelikvie: +regen og armor
    ROSARY,         // Rosenkrans: -cooldown og +varighet
    WAR_DRUM,       // Krigstromme: +fart og skade
    FENCING_GLOVE,  // Fekthanske: +krit og prosjektilfart
    BEGGAR_BOWL,    // Tiggerskål: +gull
    JESTER_MASK,    // Narremaske: +XP og område
    COUNT
};
// Item-kombinasjoner: to items på maks nivå smeltes sammen til ett sterkere item
// (og frigjør en item-plass). Se items.cpp.
enum class ComboId {
    EXECUTIONER_AXE,   // Slipestein + Heldig terning
    JUGGLER_SCOPE,     // Sjonglørball + Kikkert
    ROYAL_REGALIA,     // Kongens kappe + Kronjuvel
    SHADOW_DANCER,     // Narreskoene + Skyggekappe
    ETERNAL_GLASS,     // Timeglass + Evighetslys
    BLOOD_HEART,       // Hjerteamulett + Vampyrtann
    SPIKED_ARMOR,      // Ringbrynje + Piggkrage
    SAGE_MAGNET,       // Magnetstein + Uglefjær
    LUCKY_MIRROR,      // Trollspeil + Firkløver
    CATHEDRAL,         // Helgenrelikvie + Rosenkrans
    CAVALRY_MARCH,     // Krigstromme + Fekthanske
    STREET_PERFORMER,  // Tiggerskål + Narremaske
    COUNT
};

constexpr int MAX_ITEM_LEVEL = 5;
constexpr int MAX_ITEM_SLOTS = 6;

// Alle tall en ability kan ha. Hver ability bruker bare de feltene som er relevante for den.
// Level-tabellene i abilities.cpp endrer disse verdiene.
struct AbilityStats {
    float damage = 0.0f;        // Skade per treff (Rot: skade per sekund)
    float cooldown = 1.0f;      // Sekunder mellom hvert angrep (Orbit: tid før samme fiende kan treffes igjen)
    float speed = 0.0f;         // Prosjektilfart (Orbit: grader per sekund)
    int projectiles = 1;        // Antall prosjektiler / blader / lyn
    int pierce = 0;             // Hvor mange fiender et prosjektil går gjennom
    int bounces = 0;            // Antall sprett
    float bounceRange = 0.0f;   // Hvor langt et sprett kan gå
    float bounceFalloff = 1.0f; // Skade-multiplikator per sprett (0.7 = -30% per sprett)
    float radius = 0.0f;        // AOE-radius / orbit-radius / rekkevidde
    float area = 0.0f;          // Treffområde for f.eks. lyn (Sabel: buens vinkel, Ild: kjeglens halvvinkel)
    float duration = 0.0f;      // Hvor lenge noe varer (frost, virvelvind, ildpust)
    float effect = 0.0f;        // Spesialeffekt: frost-styrke, dytt, sug eller livstjeling
};

#endif // ABILITY_TYPES_HPP
