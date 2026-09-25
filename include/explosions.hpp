#ifndef EXPLOSIONS_HPP
#define EXPLOSIONS_HPP

#include <raylib.h>

// Eksplosjoner (f.eks. fra kamikaze-fiender). Fiender vet ikke hvor spilleren er
// når de dør, så de legger en eksplosjon i køen her, og hovedløkka sjekker
// om spilleren står innenfor radiusen.

void SpawnExplosion(Vector2 position, float radius, float damage);

// Oppdaterer animasjoner og returnerer total skade spilleren tar denne framen
// fra nye eksplosjoner som treffer (0 hvis ingen).
float UpdateExplosions(float deltaTime, Vector2 playerPos, float playerRadius);

void DrawExplosions();
void ClearExplosions();

#endif // EXPLOSIONS_HPP
