#ifndef CASTLE_HPP
#define CASTLE_HPP

#include <raylib.h>

// Tegning av slottsmiljøet: porselensgulv, løpere, søyler og tronsalen.
// Søylene er det eneste med kollisjon (se ResolvePillarCollision).

// Uendelig slottsgulv. Tegner flisene innenfor viewRadius rundt center.
// Porselensflisene (assets/floor). Uten dem tegnes det gamle marmorgulvet.
void InitCastleTextures();
void UnloadCastleTextures();

void DrawCastleFloor(Vector2 center, float viewRadius);

// Tronsalen (boss-arenaen) i to lag:
//  - Gulvet (2D, tegnes i gulvlaget): sjakkbrett, løper og mørk kant
//  - 3D: murvegg, søyler med bannere og tronen
void DrawThroneRoomFloor(Vector2 center, float radius);
void DrawThroneRoom3D(Vector2 center, float radius);

// Fyrfat og søyler i storsalen. center = spilleren: søyler som skjuler spilleren hoppes over
// her og tegnes gjennomsiktige med DrawCastlePillarsFaded etter figurene.
void DrawCastleProps3D(Vector2 center, float viewRadius, const Camera3D& camera);
void DrawCastlePillarsFaded(Vector2 center, float viewRadius, const Camera3D& camera);
// Flammer, glør og varme lyspøler (kalles i VFX-passet, se vfx.hpp)
void DrawCastlePropsVfx(Vector2 center, float viewRadius);
// Samme for tronsalen: fyrfat langs muren
void DrawThroneRoomVfx(Vector2 center, float radius);

// Søyler i storsalen (ikke i tronsalen). PillarsNear fyller `out` med søyler innenfor radius.
int PillarsNear(Vector2 center, float radius, Vector2* out, int maxCount);
// Skyver en sirkel (spiller/fiende) ut av søylene. slide > 0: glir også sidelengs rundt
// søylen mot `goal`, så fiender finner veien rundt. Returnerer true hvis den traff en søyle.
bool ResolvePillarCollision(Vector2& pos, float radius, Vector2 goal, float slide);

// Myk skygge under en figur – gir en enkel følelse av dybde
void DrawShadow(Vector2 feet, float width, float height);

#endif // CASTLE_HPP
