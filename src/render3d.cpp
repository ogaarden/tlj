#include "render3d.hpp"
#include <raymath.h>
#include <rlgl.h>
#include <cmath>
#include <vector>
#include <unordered_map>

namespace {

RenderTexture2D groundTexture{};
Vector2 groundCenter = { 0, 0 };

// Fast lys fra nordvest og ovenfra (samme retning som skyggene på gulvet)
const Vector3 LIGHT_DIR = Vector3Normalize({ -0.4f, 0.85f, -0.35f });
constexpr float AMBIENT = 0.42f;
constexpr float DIFFUSE = 0.70f;
constexpr float RIM = 0.55f;      // Hvor sterkt kantlyset er

Vector3 viewDir = Vector3Normalize({ 0.0f, 0.95f, 0.3f });
float flashAmount = 0.0f;
float shapeDetail = 1.0f;

int detailed(int n, int minimum) {
    int d = (int)(n * shapeDetail + 0.5f);
    return d < minimum ? minimum : d;
}
float shakeTrauma = 0.0f;

Color lit(Color base, Vector3 normal) {
    float d = Vector3DotProduct(normal, LIGHT_DIR);
    float k = AMBIENT + DIFFUSE * fmaxf(d, 0.0f);
    // Kantlys: flater som vender bort fra kameraet (konturen) blir lysere
    float facing = fmaxf(Vector3DotProduct(normal, viewDir), 0.0f);
    float edge = 1.0f - facing;
    float rim = edge * edge * edge * RIM;
    auto ch = [&](unsigned char c) {
        float v = c * k + (70.0f + c * 0.6f) * rim;
        v = v + (255.0f - v) * flashAmount;
        return (unsigned char)(v > 255.0f ? 255.0f : v);
    };
    return { ch(base.r), ch(base.g), ch(base.b), base.a };
}

void vertex(Vector3 p, Color c) {
    rlColor4ub(c.r, c.g, c.b, c.a);
    rlVertex3f(p.x, p.y, p.z);
}

// Trekant som alltid vender utover (mot klokka sett fra `outward`-siden).
// Viktig: rlDisableBackfaceCulling() gjelder ikke for trekanter som ligger i batchen –
// de tegnes først senere, når culling er på igjen. Trekanter med feil rekkefølge blir
// da borte, og man ser innsiden av formen (f.eks. nesa gjennom bakhodet).
void triangle(Vector3 a, Vector3 b, Vector3 c, Color ca, Color cb, Color cc, Vector3 outward) {
    Vector3 n = Vector3CrossProduct(Vector3Subtract(b, a), Vector3Subtract(c, a));
    if (Vector3DotProduct(n, outward) < 0.0f) {
        Vector3 tp = b; b = c; c = tp;
        Color tc = cb; cb = cc; cc = tc;
    }
    vertex(a, ca);
    vertex(b, cb);
    vertex(c, cc);
}

} // namespace

void InitRenderer3D() {
    // Kameraet står ~1550 enheter fra bakken. Standard nærplan (0.01-0.05) gir for dårlig
    // dybdepresisjon på den avstanden, så figurer kan "forsvinne" inn i gulvet.
    // rlSetClipPlanes finnes fra raylib 5.5.
#if defined(RAYLIB_VERSION_MAJOR) && (RAYLIB_VERSION_MAJOR > 5 || (RAYLIB_VERSION_MAJOR == 5 && RAYLIB_VERSION_MINOR >= 5))
    rlSetClipPlanes(50.0, 6000.0);
#endif

    groundTexture = LoadRenderTexture(View3D::GROUND_SIZE, View3D::GROUND_SIZE);
    SetTextureFilter(groundTexture.texture, TEXTURE_FILTER_ANISOTROPIC_8X);
}

void UnloadRenderer3D() {
    UnloadRenderTexture(groundTexture);
}

Camera3D MakeGameCamera(Vector2 focus, float yawDegrees) {
    // Samme konvensjon som 2D-kameraet: "opp" på skjermen er (-sin, -cos) i verden,
    // så kameraet står motsatt vei, bak spilleren.
    float yaw = yawDegrees * DEG2RAD;
    float pitch = View3D::CAMERA_PITCH * DEG2RAD;
    float height = sinf(pitch) * View3D::CAMERA_DISTANCE;
    float back = cosf(pitch) * View3D::CAMERA_DISTANCE;

    // Skjermristing: liten forskyvning som følger to sinusbølger (jevnere enn ren støy)
    float t = (float)GetTime();
    float shake = shakeTrauma * shakeTrauma * 14.0f;
    Vector2 jolt = { sinf(t * 57.0f) * shake, cosf(t * 43.0f + 1.3f) * shake };
    focus = Vector2Add(focus, jolt);

    Camera3D cam{};
    cam.target = ToWorld3D(focus, 0.0f);
    cam.position = {
        focus.x + sinf(yaw) * back,
        height,
        focus.y + cosf(yaw) * back
    };
    viewDir = Vector3Normalize(Vector3Subtract(cam.position, cam.target));
    cam.up = { 0.0f, 1.0f, 0.0f };
    cam.fovy = View3D::FOVY;
    cam.projection = CAMERA_PERSPECTIVE;
    return cam;
}

void AddCameraShake(float amount) {
    shakeTrauma = fminf(1.0f, shakeTrauma + amount);
}

void UpdateCameraShake(float deltaTime) {
    shakeTrauma = fmaxf(0.0f, shakeTrauma - deltaTime * 2.2f);
}

void SetShadeViewDir(Vector3 towardCamera) {
    viewDir = Vector3Normalize(towardCamera);
}

void SetShapeDetail(float detail) {
    shapeDetail = detail < 0.3f ? 0.3f : (detail > 1.0f ? 1.0f : detail);
}

void SetShadeFlash(float amount) {
    flashAmount = amount < 0.0f ? 0.0f : (amount > 1.0f ? 1.0f : amount);
}

void BeginGroundLayer(Vector2 center) {
    groundCenter = center;
    BeginTextureMode(groundTexture);
    ClearBackground(Color{ 12, 10, 16, 255 });

    Camera2D cam{};
    cam.target = center;
    cam.offset = { View3D::GROUND_SIZE / 2.0f, View3D::GROUND_SIZE / 2.0f };
    cam.zoom = 1.0f;
    BeginMode2D(cam);
}

void EndGroundLayer() {
    EndMode2D();
    EndTextureMode();
}

void DrawGroundLayer() {
    float half = View3D::GROUND_SIZE / 2.0f;
    float x0 = groundCenter.x - half, x1 = groundCenter.x + half;
    float z0 = groundCenter.y - half, z1 = groundCenter.y + half;

    // Render-teksturer er lagret opp-ned, så v går fra 1 (nord) til 0 (sør)
    rlSetTexture(groundTexture.texture.id);
    rlBegin(RL_QUADS);
        rlColor4ub(255, 255, 255, 255);
        rlNormal3f(0.0f, 1.0f, 0.0f);
        rlTexCoord2f(0.0f, 1.0f); rlVertex3f(x0, 0.0f, z0);
        rlTexCoord2f(0.0f, 0.0f); rlVertex3f(x0, 0.0f, z1);
        rlTexCoord2f(1.0f, 0.0f); rlVertex3f(x1, 0.0f, z1);
        rlTexCoord2f(1.0f, 1.0f); rlVertex3f(x1, 0.0f, z0);
    rlEnd();
    rlSetTexture(0);
}

Vector2 GroundToScreen(const Camera3D& camera, Vector2 ground, float height) {
    return GetWorldToScreen(ToWorld3D(ground, height), camera);
}

// ---------------------------------------------------------------------
// Skyggelagte former. Dette er det som bygger ALLE figurene hver frame, så det
// må gå fort: enhetskuler og sirkeltabeller regnes ut én gang og caches, og
// lyset regnes én gang per hjørnepunkt (ikke per trekant-hjørne).
// ---------------------------------------------------------------------
namespace {

// Normaler på en enhetskule: (rings+1) x (slices+1) punkter, fra sørpolen til nordpolen
const std::vector<Vector3>& unitSphere(int rings, int slices) {
    static std::unordered_map<int, std::vector<Vector3>> cache;
    int key = rings * 1000 + slices;
    auto it = cache.find(key);
    if (it != cache.end()) return it->second;
    std::vector<Vector3> grid;
    grid.reserve((rings + 1) * (slices + 1));
    for (int i = 0; i <= rings; i++) {
        float lat = -PI / 2.0f + PI * i / rings;
        for (int j = 0; j <= slices; j++) {
            float lon = 2.0f * PI * j / slices;
            grid.push_back({ cosf(lat) * cosf(lon), sinf(lat), cosf(lat) * sinf(lon) });
        }
    }
    return cache.emplace(key, std::move(grid)).first->second;
}

// cos/sin for hver slice rundt en sirkel
const std::vector<Vector2>& unitCircle(int slices) {
    static std::unordered_map<int, std::vector<Vector2>> cache;
    auto it = cache.find(slices);
    if (it != cache.end()) return it->second;
    std::vector<Vector2> ring;
    for (int i = 0; i <= slices; i++) {
        float a = 2.0f * PI * i / slices;
        ring.push_back({ cosf(a), sinf(a) });
    }
    return cache.emplace(slices, std::move(ring)).first->second;
}

// Felles for kule og ellipsoide: et rutenett av punkter og farger -> trekanter
std::vector<Vector3> gridPos;
std::vector<Color> gridCol;

void emitGrid(int rings, int slices, Vector3 center) {
    int w = slices + 1;
    // Finn vindingen én gang (fra en rute midt på kula) i stedet for per trekant
    int mi = rings / 2, mj = 0;
    Vector3 a = gridPos[mi * w + mj], b = gridPos[(mi + 1) * w + mj], c = gridPos[(mi + 1) * w + mj + 1];
    Vector3 n = Vector3CrossProduct(Vector3Subtract(b, a), Vector3Subtract(c, a));
    bool flip = Vector3DotProduct(n, Vector3Subtract(a, center)) < 0.0f;

    rlCheckRenderBatchLimit(rings * slices * 6);
    rlBegin(RL_TRIANGLES);
    for (int i = 0; i < rings; i++) {
        for (int j = 0; j < slices; j++) {
            int i00 = i * w + j, i01 = i00 + 1, i10 = i00 + w, i11 = i10 + 1;
            if (!flip) {
                vertex(gridPos[i00], gridCol[i00]); vertex(gridPos[i10], gridCol[i10]); vertex(gridPos[i11], gridCol[i11]);
                vertex(gridPos[i00], gridCol[i00]); vertex(gridPos[i11], gridCol[i11]); vertex(gridPos[i01], gridCol[i01]);
            } else {
                vertex(gridPos[i00], gridCol[i00]); vertex(gridPos[i11], gridCol[i11]); vertex(gridPos[i10], gridCol[i10]);
                vertex(gridPos[i00], gridCol[i00]); vertex(gridPos[i01], gridCol[i01]); vertex(gridPos[i11], gridCol[i11]);
            }
        }
    }
    rlEnd();
}

} // namespace

void ShadedSphere(Vector3 center, float radius, Color color, int rings, int slices) {
    rings = detailed(rings, 3);
    slices = detailed(slices, 4);
    const std::vector<Vector3>& unit = unitSphere(rings, slices);
    size_t count = unit.size();
    gridPos.resize(count);
    gridCol.resize(count);
    for (size_t k = 0; k < count; k++) {
        gridPos[k] = { center.x + unit[k].x * radius, center.y + unit[k].y * radius, center.z + unit[k].z * radius };
        gridCol[k] = lit(color, unit[k]);
    }
    emitGrid(rings, slices, center);
}

void ShadedCylinder(Vector3 start, Vector3 end, float startRadius, float endRadius, Color color, int slices) {
    Vector3 axis = Vector3Subtract(end, start);
    if (Vector3Length(axis) < 0.001f) return;
    Vector3 dir = Vector3Normalize(axis);

    // To vektorer vinkelrett på aksen
    Vector3 helper = (fabsf(dir.y) < 0.95f) ? Vector3{ 0, 1, 0 } : Vector3{ 1, 0, 0 };
    Vector3 u = Vector3Normalize(Vector3CrossProduct(dir, helper));
    Vector3 v = Vector3CrossProduct(dir, u);

    // Normal, farge og punkter for hver kant rundt sylinderen (regnes én gang)
    slices = detailed(slices, 4);
    const std::vector<Vector2>& circle = unitCircle(slices);
    Vector3 normals[65];
    Color colors[65];
    if (slices > 64) slices = 64;
    for (int i = 0; i <= slices; i++) {
        normals[i] = Vector3Add(Vector3Scale(u, circle[i].x), Vector3Scale(v, circle[i].y));
        colors[i] = lit(color, normals[i]);
    }
    // Vindingen: sjekk én gang om (b0, t0, t1) vender utover
    Vector3 b0 = Vector3Add(start, Vector3Scale(normals[0], startRadius));
    Vector3 t0 = Vector3Add(end, Vector3Scale(normals[0], endRadius));
    Vector3 t1 = Vector3Add(end, Vector3Scale(normals[1], endRadius));
    Vector3 b1 = Vector3Add(start, Vector3Scale(normals[1], startRadius));
    Vector3 faceN = Vector3CrossProduct(Vector3Subtract(t0, b0), Vector3Subtract(t1, b0));
    if (Vector3Length(faceN) < 1e-6f) faceN = Vector3CrossProduct(Vector3Subtract(t1, b0), Vector3Subtract(b1, b0)); // Kjegle-spiss
    bool flip = Vector3DotProduct(faceN, Vector3Add(normals[0], normals[1])) < 0.0f;
    Color capStart = lit(color, Vector3Negate(dir));
    Color capEnd = lit(color, dir);

    rlCheckRenderBatchLimit(slices * 12);
    rlBegin(RL_TRIANGLES);
    for (int i = 0; i < slices; i++) {
        b0 = Vector3Add(start, Vector3Scale(normals[i], startRadius));
        b1 = Vector3Add(start, Vector3Scale(normals[i + 1], startRadius));
        t0 = Vector3Add(end, Vector3Scale(normals[i], endRadius));
        t1 = Vector3Add(end, Vector3Scale(normals[i + 1], endRadius));
        Color c0 = colors[i], c1 = colors[i + 1];

        // Sidene
        if (!flip) {
            vertex(b0, c0); vertex(t0, c0); vertex(t1, c1);
            vertex(b0, c0); vertex(t1, c1); vertex(b1, c1);
        } else {
            vertex(b0, c0); vertex(t1, c1); vertex(t0, c0);
            vertex(b0, c0); vertex(b1, c1); vertex(t1, c1);
        }
        // Lokk i begge ender (motsatt vinding av sidene sett utenfra)
        if (startRadius > 0.0f) {
            if (!flip) { vertex(start, capStart); vertex(b1, capStart); vertex(b0, capStart); }
            else       { vertex(start, capStart); vertex(b0, capStart); vertex(b1, capStart); }
        }
        if (endRadius > 0.0f) {
            if (!flip) { vertex(end, capEnd); vertex(t0, capEnd); vertex(t1, capEnd); }
            else       { vertex(end, capEnd); vertex(t1, capEnd); vertex(t0, capEnd); }
        }
    }
    rlEnd();
}

void ShadedCube(Vector3 center, Vector3 size, float yawDegrees, Color color) {
    float yaw = yawDegrees * DEG2RAD;
    Vector3 ax = { cosf(yaw), 0.0f, -sinf(yaw) };  // Lokal x-akse
    Vector3 ay = { 0.0f, 1.0f, 0.0f };
    Vector3 az = { sinf(yaw), 0.0f, cosf(yaw) };   // Lokal z-akse
    Vector3 hx = Vector3Scale(ax, size.x / 2.0f);
    Vector3 hy = Vector3Scale(ay, size.y / 2.0f);
    Vector3 hz = Vector3Scale(az, size.z / 2.0f);

    auto corner = [&](float sx, float sy, float sz) {
        return Vector3Add(center, Vector3Add(Vector3Scale(hx, sx), Vector3Add(Vector3Scale(hy, sy), Vector3Scale(hz, sz))));
    };
    auto face = [&](Vector3 a, Vector3 b, Vector3 c, Vector3 d, Vector3 normal) {
        Color col = lit(color, normal);
        triangle(a, b, c, col, col, col, normal);
        triangle(a, c, d, col, col, col, normal);
    };

    rlCheckRenderBatchLimit(36);
    rlBegin(RL_TRIANGLES);
    face(corner(-1, 1, -1), corner(1, 1, -1), corner(1, 1, 1), corner(-1, 1, 1), ay);                        // Topp
    face(corner(-1, -1, -1), corner(1, -1, -1), corner(1, -1, 1), corner(-1, -1, 1), Vector3Negate(ay));     // Bunn
    face(corner(-1, -1, 1), corner(1, -1, 1), corner(1, 1, 1), corner(-1, 1, 1), az);                        // Sør
    face(corner(-1, -1, -1), corner(1, -1, -1), corner(1, 1, -1), corner(-1, 1, -1), Vector3Negate(az));     // Nord
    face(corner(1, -1, -1), corner(1, -1, 1), corner(1, 1, 1), corner(1, 1, -1), ax);                        // Øst
    face(corner(-1, -1, -1), corner(-1, -1, 1), corner(-1, 1, 1), corner(-1, 1, -1), Vector3Negate(ax));     // Vest
    rlEnd();
}

void ShadedEllipsoid(Vector3 center, Vector2 forward, Vector3 radii, Color color, int rings, int slices) {
    // Lokale akser: fremover (f), opp (u) og sidelengs (s)
    Vector3 f = { forward.x, 0.0f, forward.y };
    if (Vector3Length(f) < 0.001f) f = { 0.0f, 0.0f, 1.0f };
    f = Vector3Normalize(f);
    Vector3 u = { 0.0f, 1.0f, 0.0f };
    Vector3 sd = Vector3CrossProduct(u, f);

    rings = detailed(rings, 3);
    slices = detailed(slices, 4);
    const std::vector<Vector3>& unit = unitSphere(rings, slices);
    size_t count = unit.size();
    gridPos.resize(count);
    gridCol.resize(count);
    float irx = 1.0f / radii.x, iry = 1.0f / radii.y, irz = 1.0f / radii.z;
    for (size_t k = 0; k < count; k++) {
        // Enhetskula (x = fremover, y = opp, z = sidelengs) strekkes til ellipsoiden
        float nf = unit[k].x, nu = unit[k].y, ns = unit[k].z;
        gridPos[k] = {
            center.x + f.x * nf * radii.x + sd.x * ns * radii.z,
            center.y + nu * radii.y,
            center.z + f.z * nf * radii.x + sd.z * ns * radii.z
        };
        // Normalen på en strukket kule: del på radiene
        Vector3 n = Vector3Normalize({ f.x * nf * irx + sd.x * ns * irz, nu * iry, f.z * nf * irx + sd.z * ns * irz });
        gridCol[k] = lit(color, n);
    }
    emitGrid(rings, slices, center);
}
