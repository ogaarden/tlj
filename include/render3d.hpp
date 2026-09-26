#ifndef RENDER3D_HPP
#define RENDER3D_HPP

#include <raylib.h>

// =====================================================================
// 2.5D-RENDERING
//
// Spillogikken er ren 2D: alt har en posisjon (x, y) på gulvet.
// Her gjøres det om til 3D:  2D (x, y)  ->  3D (x, høyde, y)
//
// Tegningen skjer i to lag:
//  1. Gulvlaget: fliser, løpere, skygger og AOE-effekter tegnes med vanlig
//     2D-kode til en tekstur, som så legges på bakken som et 3D-plan.
//  2. 3D-laget: figurer, prosjektiler, søyler osv. tegnes som skyggelagte
//     3D-former (kuler, sylindre, bokser) med et fast lys fra nordvest.
// =====================================================================

namespace View3D {
    // Kameraet står langt unna med smal linse. Da blir perspektivet nesten flatt
    // (som et 2D-spill), mens figurene fortsatt har volum, lys og skygge.
    constexpr float CAMERA_PITCH = 72.0f;      // Vinkel ned mot bakken (90 = rett ovenfra)
    constexpr float CAMERA_DISTANCE = 1550.0f; // Avstand fra kameraet til spilleren
    constexpr float FOVY = 28.0f;              // Smal linse = lite perspektiv-forvrengning (lavere = mer zoom)
    constexpr int GROUND_SIZE = 2560;         // Hvor stort område av gulvet som tegnes rundt spilleren
}

void InitRenderer3D();
void UnloadRenderer3D();

// Kameraet ser mot `focus` på bakken. yawDegrees er rotasjonen fra Q/E.
// Setter også retningen til kantlyset (se SetShadeViewDir) og legger på skjermristing.
Camera3D MakeGameCamera(Vector2 focus, float yawDegrees);

// --- Skjermristing (slag, eksplosjoner, lyn) ---
void AddCameraShake(float amount);   // amount ~ 0.2 (lite) til 1.0 (kraftig)
void UpdateCameraShake(float deltaTime);

// --- Belysning ---
// Retningen mot kameraet brukes til kantlys (rim light), så figurene får en lys kontur
void SetShadeViewDir(Vector3 towardCamera);
// 0 = vanlige farger, 1 = helt hvit. Brukes til å blinke fiender hvite når de blir truffet.
void SetShadeFlash(float amount);

// Detaljnivå for kuler/sylindre (1 = fullt, 0.5 = halvparten så mange trekanter).
// Senkes automatisk når det er mange fiender på skjermen, så spillet holder farten.
void SetShapeDetail(float detail);

inline Vector3 ToWorld3D(Vector2 ground, float height) { return { ground.x, height, ground.y }; }

// --- Gulvlaget ---
// Alt som tegnes mellom Begin/End havner på gulvet, med vanlige 2D-koordinater.
void BeginGroundLayer(Vector2 center);
void EndGroundLayer();
void DrawGroundLayer(); // Kalles inne i BeginMode3D

// Skjermposisjon for et punkt i en viss høyde over bakken (for HP-barer, skadetall osv.)
Vector2 GroundToScreen(const Camera3D& camera, Vector2 ground, float height);

// --- Skyggelagte 3D-former (fast lys, ingen shader nødvendig) ---
void ShadedSphere(Vector3 center, float radius, Color color, int rings = 8, int slices = 12);
void ShadedCylinder(Vector3 start, Vector3 end, float startRadius, float endRadius, Color color, int slices = 12);
void ShadedCube(Vector3 center, Vector3 size, float yawDegrees, Color color);

// Krystall (to pyramider mot hverandre, 4 sider), roterer rundt seg selv. Brukes til XP.
void ShadedCrystal(Vector3 center, float radius, float height, float spinDegrees, Color color);

// Ellipsoide (strukket kule) orientert etter en retning på gulvet.
// radii = { fremover, opp, sidelengs }
void ShadedEllipsoid(Vector3 center, Vector2 forward, Vector3 radii, Color color, int rings = 8, int slices = 12);

#endif // RENDER3D_HPP
