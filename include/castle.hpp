#ifndef CASTLE_HPP
#define CASTLE_HPP

#include <raylib.h>

// Tegning av slottsmiljøet: porselensgulv, løpere, pynt og tronsalen.
// Slottet har faste saler med pynt (statuer, faner, rustninger ...) og en murvegg rundt.
// Pynten og veggen er det eneste med kollisjon (se ResolvePillarCollision).

// Slottsgulvet (4 x 4 saler med murvegg rundt). Tegner flisene innenfor viewRadius rundt center.
// Porselensflisene (assets/floor). Uten dem tegnes det gamle marmorgulvet.
void InitCastleTextures();
void UnloadCastleTextures();

void DrawCastleFloor(Vector2 center, float viewRadius);

// Tronsalen (boss-arenaen) i to lag:
//  - Gulvet (2D, tegnes i gulvlaget): sjakkbrett, løper og mørk kant
//  - 3D: murvegg, søyler med bannere og tronen
void DrawThroneRoomFloor(Vector2 center, float radius);
void DrawThroneRoom3D(Vector2 center, float radius);

// Fyrfat og pynt i storsalen. center = spilleren: pynt som skjuler spilleren hoppes over
// her og tegnes gjennomsiktige med DrawCastlePillarsFaded etter figurene.
void DrawCastleProps3D(Vector2 center, float viewRadius, const Camera3D& camera);
void DrawCastlePillarsFaded(Vector2 center, float viewRadius, const Camera3D& camera);
// Flammer, glør og varme lyspøler (kalles i VFX-passet, se vfx.hpp)
void DrawCastlePropsVfx(Vector2 center, float viewRadius);
// Samme for tronsalen: fyrfat langs muren
void DrawThroneRoomVfx(Vector2 center, float radius);

// Pynt i storsalen (ikke i tronsalen). PillarsNear fyller `out` med pynt innenfor radius.
int PillarsNear(Vector2 center, float radius, Vector2* out, int maxCount);
// Skyver en sirkel (spiller/fiende) ut av pynten. slide > 0: glir også sidelengs rundt
// den mot `goal`, så fiender finner veien rundt. Returnerer true hvis den traff noe.
bool ResolvePillarCollision(Vector2& pos, float radius, Vector2 goal, float slide);

// Slottet er avgrenset av en murvegg (4 x 4 saler). Holder en posisjon innenfor veggen.
Vector2 ClampToCastle(Vector2 pos, float margin);
bool InsideCastle(Vector2 pos, float margin);
float CastleHalfSize();                 // Veggen står i +-CastleHalfSize() på begge akser
const char* CastleRoomName(Vector2 pos); // Navnet på salen man står i ("Rustkammeret" osv.)

// Salbonus: hver sal gir en liten fordel så lenge man står i den (så det lønner seg å flytte seg)
enum class RoomBonus { NONE, PICKUP, CRIT, DAMAGE, SPEED, REGEN, XP };
RoomBonus CastleRoomBonus(Vector2 pos);
const char* RoomBonusText(RoomBonus bonus);

// Myk skygge under en figur – gir en enkel følelse av dybde
void DrawShadow(Vector2 feet, float width, float height);

#endif // CASTLE_HPP
