#ifndef CASTLE_HPP
#define CASTLE_HPP

#include <raylib.h>

// Tegning av slottsmiljøet: rutete marmorgulv, løpere og tronsalen.
// Alt her er ren grafikk – ingen kollisjon eller spillogikk.

// Uendelig slottsgulv. Tegner bare flisene som er synlige for kameraet.
void DrawCastleFloor(const Camera2D& camera);

// Tronsalen (boss-arenaen): sirkulært gulv, løper, trone, murvegg med søyler og bannere
void DrawThroneRoom(Vector2 center, float radius);

// Myk skygge under en figur – gir en enkel følelse av dybde
void DrawShadow(Vector2 feet, float width, float height);

#endif // CASTLE_HPP
