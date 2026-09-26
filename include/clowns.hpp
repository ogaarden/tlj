#ifndef CLOWNS_HPP
#define CLOWNS_HPP

#include <raylib.h>
#include "character.hpp"

// 3D-modellene til de spillbare klovnene. Bygget av skyggelagte kuler,
// sylindre og ellipsoider (se render3d.hpp), med enkel gangeanimasjon.

struct ClownPose {
    Vector2 position;       // Føttene på gulvet
    Vector2 facing;         // Retningen klovnen ser (på gulvet)
    float walkTime = 0.0f;  // Driver gangeanimasjonen
    bool moving = false;
    Color tint = WHITE;     // F.eks. rødt når man er truffet, blått når man er slowet
    float attack = 0.0f;    // 1 rett etter at standardvåpenet ble brukt, faller til 0 (klar)
};

void DrawClown(ClownStyle style, const ClownPose& pose);

// Hvor bred skyggen under klovnen skal være
float ClownShadowWidth(ClownStyle style);

#endif // CLOWNS_HPP
