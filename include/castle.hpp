#ifndef CASTLE_HPP
#define CASTLE_HPP

#include <raylib.h>

// Tegning av slottsmiljøet: rutete marmorgulv, løpere og tronsalen.
// Alt her er ren grafikk – ingen kollisjon eller spillogikk.

// Uendelig slottsgulv. Tegner flisene innenfor viewRadius rundt center.
void DrawCastleFloor(Vector2 center, float viewRadius);

// Tronsalen (boss-arenaen) i to lag:
//  - Gulvet (2D, tegnes i gulvlaget): sjakkbrett, løper og mørk kant
//  - 3D: murvegg, søyler med bannere og tronen
void DrawThroneRoomFloor(Vector2 center, float radius);
void DrawThroneRoom3D(Vector2 center, float radius);

// Fyrfat og andre 3D-detaljer i storsalen (rundt der løperne krysser hverandre)
void DrawCastleProps3D(Vector2 center, float viewRadius);
// Flammer, glør og varme lyspøler (kalles i VFX-passet, se vfx.hpp)
void DrawCastlePropsVfx(Vector2 center, float viewRadius);
// Samme for tronsalen: fyrfat langs muren
void DrawThroneRoomVfx(Vector2 center, float radius);

// Myk skygge under en figur – gir en enkel følelse av dybde
void DrawShadow(Vector2 feet, float width, float height);

#endif // CASTLE_HPP
