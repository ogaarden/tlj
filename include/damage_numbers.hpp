#ifndef DAMAGE_NUMBERS_HPP
#define DAMAGE_NUMBERS_HPP

#include <raylib.h>

// Flytende skadetall som vises over fiender hver gang de tar skade.
// Tallene lagres i verdens-koordinater (posisjon på gulvet + høyde), men tegnes
// i skjerm-koordinater slik at teksten alltid er rett vei.

// crit = kritisk treff: større, gult tall med utropstegn
void SpawnDamageNumber(Vector2 worldPos, int amount, Color color, bool crit = false);
void UpdateDamageNumbers(float deltaTime);
void DrawDamageNumbers(const Camera3D& camera); // Kalles ETTER EndMode3D()
void ClearDamageNumbers();

#endif // DAMAGE_NUMBERS_HPP
