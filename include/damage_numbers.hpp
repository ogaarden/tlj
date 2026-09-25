#ifndef DAMAGE_NUMBERS_HPP
#define DAMAGE_NUMBERS_HPP

#include <raylib.h>

// Flytende skadetall som vises over fiender hver gang de tar skade.
// Tallene lagres i verdens-koordinater, men tegnes i skjerm-koordinater
// slik at teksten ikke roterer sammen med kameraet.

void SpawnDamageNumber(Vector2 worldPos, int amount, Color color);
void UpdateDamageNumbers(float deltaTime);
void DrawDamageNumbers(const Camera2D& camera); // Kalles ETTER EndMode2D()
void ClearDamageNumbers();

#endif // DAMAGE_NUMBERS_HPP
