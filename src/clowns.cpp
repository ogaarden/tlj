#include "clowns.hpp"
#include "render3d.hpp"
#include <raymath.h>
#include <rlgl.h>
#include <cmath>

namespace {

// Farger
const Color FACE_PAINT = { 245, 242, 236, 255 };
const Color CLOWN_RED  = { 220, 35, 45, 255 };
const Color NOSE_RED   = { 235, 30, 40, 255 };
const Color GLOVE      = { 250, 250, 250, 255 };
const Color EYE        = { 25, 25, 30, 255 };

// Hjelper som plasserer deler relativt til klovnen:
// fwd = fremover, side = sidelengs (+ = klovnens venstre), up = høyde
struct Rig {
    Vector2 pos;
    Vector2 f;    // Fremover
    Vector2 s;    // Sidelengs
    Color tint;

    Vector3 at(float fwd, float side, float up) const {
        return { pos.x + f.x * fwd + s.x * side, up, pos.y + f.y * fwd + s.y * side };
    }
    Vector2 ground(float fwd, float side) const {
        return { pos.x + f.x * fwd + s.x * side, pos.y + f.y * fwd + s.y * side };
    }
    // Blander inn tint-fargen (for blink/slow-effekter)
    Color c(Color base) const {
        if (tint.r == 255 && tint.g == 255 && tint.b == 255) return base;
        return { (unsigned char)((base.r + tint.r) / 2), (unsigned char)((base.g + tint.g) / 2),
                 (unsigned char)((base.b + tint.b) / 2), base.a };
    }

    void sphere(float fwd, float side, float up, float r, Color col, int rings = 7, int slices = 10) const {
        ShadedSphere(at(fwd, side, up), r, c(col), rings, slices);
    }
    void ellipsoid(float fwd, float side, float up, Vector3 radii, Color col) const {
        ShadedEllipsoid(at(fwd, side, up), f, radii, c(col));
    }
    void limb(Vector3 a, Vector3 b, float r0, float r1, Color col) const {
        ShadedCylinder(a, b, r0, r1, c(col), 10);
    }
};

Rig makeRig(const ClownPose& pose) {
    Vector2 f = Vector2Length(pose.facing) > 0.001f ? Vector2Normalize(pose.facing) : Vector2{ 0.0f, 1.0f };
    return { pose.position, f, { -f.y, f.x }, pose.tint };
}

// Gange: hopp opp og ned, og føttene går frem og tilbake
struct Gait {
    float bob;   // Hvor mye kroppen løftes
    float step;  // -1..1, hvilken fot som er foran
};

Gait gait(const ClownPose& pose, float bounce) {
    if (!pose.moving) {
        float breathe = sinf((float)GetTime() * 2.5f) * 0.4f; // Litt pust når man står stille
        return { breathe, 0.0f };
    }
    float t = pose.walkTime * 11.0f;
    return { fabsf(sinf(t)) * bounce, sinf(t) };
}

// Klovnefjes: hvit sminke, rød nese, øyne og smil. `faceFwd` er hvor langt frem ansiktet er.
void clownFace(const Rig& r, float headUp, float headR, bool bigNose) {
    r.sphere(0.0f, 0.0f, headUp, headR, FACE_PAINT, 9, 12);
    float front = headR * 0.92f;
    // Øyne med blå diamant-sminke over
    for (int side = -1; side <= 1; side += 2) {
        r.sphere(front - 0.6f, side * headR * 0.36f, headUp + headR * 0.28f, headR * 0.13f, EYE, 4, 6);
        r.ellipsoid(front - 1.0f, side * headR * 0.36f, headUp + headR * 0.52f, { 0.6f, headR * 0.16f, headR * 0.07f }, Color{ 60, 110, 220, 255 });
    }
    // Stort rødt smil
    r.ellipsoid(front - 0.8f, 0.0f, headUp - headR * 0.38f, { headR * 0.14f, headR * 0.14f, headR * 0.5f }, CLOWN_RED);
    // Rød nese
    r.sphere(front + (bigNose ? 1.5f : 0.8f), 0.0f, headUp, headR * (bigNose ? 0.42f : 0.32f), NOSE_RED, 6, 8);
}

// =====================================================================
// JESTER – helt vanlig sirkusklovn: regnbuehår, krage, pomponger og store sko
// =====================================================================
void drawJester(const ClownPose& pose) {
    Rig r = makeRig(pose);
    Gait g = gait(pose, 3.0f);
    const Color SUIT = { 225, 45, 60, 255 };
    const Color PANTS = { 250, 200, 40, 255 };
    const Color SHOE = { 200, 25, 35, 255 };
    const Color POMPOM = { 255, 235, 90, 255 };
    const Color HAIR[] = { { 255, 90, 60, 255 }, { 255, 170, 40, 255 }, { 80, 190, 255, 255 }, { 120, 220, 90, 255 } };

    // Store sko som går frem og tilbake
    for (int side = -1; side <= 1; side += 2) {
        float step = g.step * 4.0f * side;
        r.ellipsoid(3.0f + step, side * 5.5f, 2.6f, { 7.5f, 2.8f, 3.6f }, SHOE);
        r.limb(r.at(step * 0.5f, side * 4.5f, 3.0f), r.at(0.0f, side * 4.0f, 15.0f + g.bob), 4.8f, 4.2f, PANTS);
    }

    float up = g.bob;
    // Kropp med pomponger
    r.limb(r.at(0, 0, 14.0f + up), r.at(0, 0, 33.0f + up), 11.5f, 9.0f, SUIT);
    for (int i = 0; i < 3; i++) r.sphere(10.2f - i * 0.8f, 0.0f, 18.0f + i * 5.5f + up, 2.4f, POMPOM, 5, 7);

    // Armer med hvite hansker (svinger litt når han går)
    for (int side = -1; side <= 1; side += 2) {
        float swing = -g.step * 3.0f * side;
        Vector3 shoulder = r.at(0.0f, side * 9.5f, 31.0f + up);
        Vector3 hand = r.at(3.0f + swing, side * 13.5f, 19.0f + up);
        r.limb(shoulder, hand, 3.2f, 2.8f, SUIT);
        ShadedSphere(hand, 3.8f, r.c(GLOVE), 6, 8);
    }

    // Krage (hvite rysjer)
    for (int i = 0; i < 12; i++) {
        float a = i * (2.0f * PI / 12.0f);
        r.sphere(cosf(a) * 9.0f, sinf(a) * 9.0f, 34.0f + up, 3.3f, GLOVE, 5, 7);
    }

    // Hode og regnbuehår som stritter ut på sidene
    float headUp = 44.0f + up;
    clownFace(r, headUp, 10.0f, false);
    for (int side = -1; side <= 1; side += 2) {
        for (int i = 0; i < 4; i++) {
            float a = (-40.0f + i * 30.0f) * DEG2RAD;
            r.sphere(-2.0f + cosf(a) * 3.0f, side * (9.5f + fabsf(sinf(a)) * 2.0f), headUp + 2.0f + sinf(a) * 6.0f, 4.2f, HAIR[i], 5, 7);
        }
    }
    // Liten hatt på skrå
    r.limb(r.at(-1.0f, 1.5f, headUp + 8.5f), r.at(-1.5f, 2.5f, headUp + 10.0f), 7.0f, 7.0f, Color{ 40, 40, 50, 255 });
    r.limb(r.at(-1.5f, 2.5f, headUp + 10.0f), r.at(-2.0f, 3.0f, headUp + 17.0f), 4.5f, 4.0f, Color{ 40, 40, 50, 255 });
    r.limb(r.at(-1.6f, 2.6f, headUp + 11.0f), r.at(-1.7f, 2.7f, headUp + 12.5f), 4.7f, 4.6f, CLOWN_RED); // Hattebånd
    r.sphere(-1.8f, 7.0f, headUp + 13.0f, 1.8f, POMPOM, 4, 6);                                          // Blomst
}

// =====================================================================
// WESTER – feit, Wario-aktig klovn: gul og lilla, stor mage, bart og caps
// =====================================================================
void drawWester(const ClownPose& pose) {
    Rig r = makeRig(pose);
    Gait g = gait(pose, 2.0f);
    const Color OVERALLS = { 120, 50, 160, 255 };
    const Color SHIRT = { 250, 205, 30, 255 };
    const Color SHOE = { 40, 130, 60, 255 };
    const Color BUTTON = { 240, 190, 40, 255 };
    const Color MUSTACHE = { 30, 25, 25, 255 };

    // Vugger fra side til side når han går
    float sway = pose.moving ? sinf(pose.walkTime * 11.0f) * 1.8f : 0.0f;

    // Korte bein og store grønne sko
    for (int side = -1; side <= 1; side += 2) {
        float step = g.step * 3.0f * side;
        r.ellipsoid(4.0f + step, side * 8.0f, 3.2f, { 8.5f, 3.4f, 5.0f }, SHOE);
        r.limb(r.at(step * 0.5f, side * 7.0f, 3.5f), r.at(0.0f, side * 7.0f, 12.0f + g.bob), 5.8f, 6.2f, OVERALLS);
    }

    float up = g.bob;
    // Stor mage (overall) og gul skjorte over
    r.ellipsoid(1.5f, sway, 22.0f + up, { 17.0f, 14.5f, 17.5f }, OVERALLS);
    r.ellipsoid(0.0f, sway, 33.0f + up, { 13.0f, 8.0f, 15.0f }, SHIRT);
    // Seler og knapper
    for (int side = -1; side <= 1; side += 2) {
        r.limb(r.at(12.0f, sway + side * 6.0f, 27.0f + up), r.at(3.0f, sway + side * 8.0f, 39.0f + up), 1.6f, 1.6f, OVERALLS);
        r.sphere(15.5f, sway + side * 6.0f, 27.0f + up, 2.2f, BUTTON, 4, 6);
    }

    // Tykke gule armer og store hansker
    for (int side = -1; side <= 1; side += 2) {
        float swing = -g.step * 2.5f * side;
        Vector3 shoulder = r.at(0.0f, sway + side * 14.0f, 34.0f + up);
        Vector3 hand = r.at(4.0f + swing, sway + side * 19.0f, 20.0f + up);
        r.limb(shoulder, hand, 5.0f, 4.2f, SHIRT);
        ShadedSphere(hand, 5.5f, r.c(GLOVE), 6, 8);
    }

    // Hode: klovnefjes med stor nese og svart zigzag-bart
    float headUp = 45.0f + up;
    Rig head = r; head.pos = r.ground(0.0f, sway);
    clownFace(head, headUp, 10.5f, true);
    for (int side = -1; side <= 1; side += 2) {
        head.ellipsoid(9.8f, side * 3.8f, headUp - 3.0f, { 1.6f, 1.8f, 4.2f }, MUSTACHE);
        head.ellipsoid(9.2f, side * 7.4f, headUp - 1.5f, { 1.4f, 2.2f, 1.8f }, MUSTACHE); // Oppbrettede tupper
    }

    // Gul caps med skygge og emblem
    head.ellipsoid(-0.5f, 0.0f, headUp + 5.5f, { 11.0f, 7.5f, 11.0f }, SHIRT);
    head.ellipsoid(8.5f, 0.0f, headUp + 5.5f, { 7.0f, 1.2f, 8.0f }, SHIRT);
    head.sphere(9.0f, 0.0f, headUp + 9.0f, 3.2f, GLOVE, 5, 7);
    head.sphere(10.6f, 0.0f, headUp + 9.0f, 1.8f, OVERALLS, 4, 6);
}

// =====================================================================
// TOK GEEK – lang og tynn nerde-klovn: briller, sløyfe, strikkevest og propellcaps
// =====================================================================
void drawGeek(const ClownPose& pose) {
    Rig r = makeRig(pose);
    Gait g = gait(pose, 2.5f);
    const Color PANTS = { 150, 120, 80, 255 };
    const Color VEST = { 40, 150, 140, 255 };
    const Color SHIRT = { 235, 235, 240, 255 };
    const Color SHOE = { 35, 30, 30, 255 };
    const Color FRAME = { 20, 20, 25, 255 };
    const Color LENS = { 180, 220, 255, 255 };
    const Color BOWTIE = { 210, 30, 40, 255 };
    const Color CAP_A = { 230, 60, 60, 255 };
    const Color CAP_B = { 60, 110, 230, 255 };

    // Lange tynne bein og smale sko
    for (int side = -1; side <= 1; side += 2) {
        float step = g.step * 5.0f * side;
        r.ellipsoid(3.0f + step, side * 3.5f, 1.8f, { 7.0f, 2.0f, 2.8f }, SHOE);
        r.limb(r.at(step * 0.6f, side * 3.2f, 2.5f), r.at(0.0f, side * 3.0f, 28.0f + g.bob), 2.4f, 2.8f, PANTS);
    }

    float up = g.bob;
    // Smal overkropp: skjorte med strikkevest over
    r.limb(r.at(0, 0, 27.0f + up), r.at(0, 0, 45.0f + up), 6.4f, 6.8f, SHIRT);
    r.limb(r.at(0, 0, 27.5f + up), r.at(0, 0, 41.0f + up), 6.9f, 7.1f, VEST);
    // Lommebeskytter med penner
    r.ellipsoid(6.8f, -2.5f, 37.5f + up, { 0.6f, 2.2f, 1.8f }, SHIRT);
    r.limb(r.at(7.2f, -3.2f, 38.0f + up), r.at(7.2f, -3.2f, 41.0f + up), 0.5f, 0.5f, CAP_B);
    r.limb(r.at(7.2f, -1.8f, 38.0f + up), r.at(7.2f, -1.8f, 41.5f + up), 0.5f, 0.5f, CAP_A);

    // Sløyfe
    for (int side = -1; side <= 1; side += 2) {
        r.limb(r.at(6.5f, 0.0f, 44.0f + up), r.at(6.8f, side * 3.8f, 44.0f + up), 0.6f, 2.2f, BOWTIE);
    }
    r.sphere(6.8f, 0.0f, 44.0f + up, 1.2f, BOWTIE, 4, 6);

    // Lange tynne armer
    for (int side = -1; side <= 1; side += 2) {
        float swing = -g.step * 4.0f * side;
        Vector3 shoulder = r.at(0.0f, side * 7.0f, 43.0f + up);
        Vector3 hand = r.at(2.5f + swing, side * 9.0f, 25.0f + up);
        r.limb(shoulder, hand, 2.0f, 1.8f, SHIRT);
        ShadedSphere(hand, 2.8f, r.c(GLOVE), 5, 7);
    }

    // Lang hals og avlangt hode
    r.limb(r.at(0, 0, 44.0f + up), r.at(0, 0, 49.0f + up), 2.4f, 2.4f, FACE_PAINT);
    float headUp = 56.0f + up;
    r.ellipsoid(0.0f, 0.0f, headUp, { 8.5f, 10.0f, 8.5f }, FACE_PAINT);
    // Ansikt: smil og liten rød nese under brillene
    r.ellipsoid(7.6f, 0.0f, headUp - 4.5f, { 1.0f, 1.0f, 3.2f }, CLOWN_RED);
    r.sphere(8.6f, 0.0f, headUp - 1.5f, 2.3f, NOSE_RED, 5, 7);

    // Store runde briller: svart innfatning, lyseblå glass og øyne bak
    for (int side = -1; side <= 1; side += 2) {
        r.limb(r.at(7.2f, side * 3.6f, headUp + 1.5f), r.at(8.2f, side * 3.6f, headUp + 1.5f), 3.4f, 3.4f, FRAME);
        r.limb(r.at(8.2f, side * 3.6f, headUp + 1.5f), r.at(8.5f, side * 3.6f, headUp + 1.5f), 2.6f, 2.6f, LENS);
        r.sphere(8.2f, side * 3.6f, headUp + 1.5f, 1.0f, EYE, 4, 6);
        r.limb(r.at(7.4f, side * 7.0f, headUp + 1.5f), r.at(0.0f, side * 8.2f, headUp + 1.5f), 0.5f, 0.5f, FRAME); // Brillestang
    }
    r.limb(r.at(8.0f, -0.8f, headUp + 1.8f), r.at(8.0f, 0.8f, headUp + 1.8f), 0.6f, 0.6f, FRAME); // Neseklype

    // Propellcaps i to farger, med propell som snurrer
    r.ellipsoid(-0.3f, 0.0f, headUp + 6.5f, { 8.8f, 5.0f, 8.8f }, CAP_A);
    r.ellipsoid(0.2f, 0.0f, headUp + 7.2f, { 5.0f, 4.6f, 9.0f }, CAP_B);
    r.limb(r.at(0, 0, headUp + 11.0f), r.at(0, 0, headUp + 14.0f), 0.6f, 0.6f, FRAME);
    float spin = (float)GetTime() * (pose.moving ? 18.0f : 5.0f);
    Vector2 blade = { cosf(spin), sinf(spin) };
    ShadedEllipsoid(r.at(0, 0, headUp + 14.2f), blade, { 6.0f, 0.5f, 1.4f }, r.c(Color{ 250, 210, 40, 255 }), 4, 8);
    r.sphere(0, 0, headUp + 14.5f, 1.1f, CAP_A, 4, 6);
}

} // namespace

void DrawClown(ClownStyle style, const ClownPose& pose) {
    switch (style) {
        case ClownStyle::JESTER: drawJester(pose); break;
        case ClownStyle::WESTER: drawWester(pose); break;
        case ClownStyle::GEEK:   drawGeek(pose); break;
    }
}

float ClownShadowWidth(ClownStyle style) {
    switch (style) {
        case ClownStyle::JESTER: return 16.0f;
        case ClownStyle::WESTER: return 23.0f;
        case ClownStyle::GEEK:   return 12.0f;
    }
    return 16.0f;
}
