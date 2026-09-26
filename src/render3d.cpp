#include "render3d.hpp"
#include <raymath.h>
#include <rlgl.h>
#include <cmath>

namespace {

RenderTexture2D groundTexture{};
Vector2 groundCenter = { 0, 0 };

// Fast lys fra nordvest og ovenfra (samme retning som skyggene på gulvet)
const Vector3 LIGHT_DIR = Vector3Normalize({ -0.4f, 0.85f, -0.35f });
constexpr float AMBIENT = 0.48f;
constexpr float DIFFUSE = 0.62f;

Color lit(Color base, Vector3 normal) {
    float d = Vector3DotProduct(normal, LIGHT_DIR);
    float k = AMBIENT + DIFFUSE * fmaxf(d, 0.0f);
    auto ch = [k](unsigned char c) { float v = c * k; return (unsigned char)(v > 255.0f ? 255.0f : v); };
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

    Camera3D cam{};
    cam.target = ToWorld3D(focus, 0.0f);
    cam.position = {
        focus.x + sinf(yaw) * back,
        height,
        focus.y + cosf(yaw) * back
    };
    cam.up = { 0.0f, 1.0f, 0.0f };
    cam.fovy = View3D::FOVY;
    cam.projection = CAMERA_PERSPECTIVE;
    return cam;
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

void ShadedSphere(Vector3 center, float radius, Color color, int rings, int slices) {
    rlCheckRenderBatchLimit(rings * slices * 6);
    rlBegin(RL_TRIANGLES);
    for (int i = 0; i < rings; i++) {
        float lat0 = -PI / 2.0f + PI * i / rings;
        float lat1 = -PI / 2.0f + PI * (i + 1) / rings;
        for (int j = 0; j < slices; j++) {
            float lon0 = 2.0f * PI * j / slices;
            float lon1 = 2.0f * PI * (j + 1) / slices;

            Vector3 n00 = { cosf(lat0) * cosf(lon0), sinf(lat0), cosf(lat0) * sinf(lon0) };
            Vector3 n01 = { cosf(lat0) * cosf(lon1), sinf(lat0), cosf(lat0) * sinf(lon1) };
            Vector3 n10 = { cosf(lat1) * cosf(lon0), sinf(lat1), cosf(lat1) * sinf(lon0) };
            Vector3 n11 = { cosf(lat1) * cosf(lon1), sinf(lat1), cosf(lat1) * sinf(lon1) };

            Vector3 p00 = Vector3Add(center, Vector3Scale(n00, radius));
            Vector3 p01 = Vector3Add(center, Vector3Scale(n01, radius));
            Vector3 p10 = Vector3Add(center, Vector3Scale(n10, radius));
            Vector3 p11 = Vector3Add(center, Vector3Scale(n11, radius));

            Vector3 out = Vector3Add(Vector3Add(n00, n01), Vector3Add(n10, n11));
            triangle(p00, p10, p11, lit(color, n00), lit(color, n10), lit(color, n11), out);
            triangle(p00, p11, p01, lit(color, n00), lit(color, n11), lit(color, n01), out);
        }
    }
    rlEnd();
}

void ShadedCylinder(Vector3 start, Vector3 end, float startRadius, float endRadius, Color color, int slices) {
    Vector3 axis = Vector3Subtract(end, start);
    if (Vector3Length(axis) < 0.001f) return;
    Vector3 dir = Vector3Normalize(axis);

    // To vektorer vinkelrett på aksen
    Vector3 helper = (fabsf(dir.y) < 0.95f) ? Vector3{ 0, 1, 0 } : Vector3{ 1, 0, 0 };
    Vector3 u = Vector3Normalize(Vector3CrossProduct(dir, helper));
    Vector3 v = Vector3CrossProduct(dir, u);

    rlCheckRenderBatchLimit(slices * 12);
    rlBegin(RL_TRIANGLES);
    Color capStart = lit(color, Vector3Negate(dir));
    Color capEnd = lit(color, dir);
    for (int i = 0; i < slices; i++) {
        float a0 = 2.0f * PI * i / slices;
        float a1 = 2.0f * PI * (i + 1) / slices;
        Vector3 n0 = Vector3Add(Vector3Scale(u, cosf(a0)), Vector3Scale(v, sinf(a0)));
        Vector3 n1 = Vector3Add(Vector3Scale(u, cosf(a1)), Vector3Scale(v, sinf(a1)));

        Vector3 b0 = Vector3Add(start, Vector3Scale(n0, startRadius));
        Vector3 b1 = Vector3Add(start, Vector3Scale(n1, startRadius));
        Vector3 t0 = Vector3Add(end, Vector3Scale(n0, endRadius));
        Vector3 t1 = Vector3Add(end, Vector3Scale(n1, endRadius));

        // Sidene
        Vector3 out = Vector3Add(n0, n1);
        triangle(b0, t0, t1, lit(color, n0), lit(color, n0), lit(color, n1), out);
        triangle(b0, t1, b1, lit(color, n0), lit(color, n1), lit(color, n1), out);

        // Lokk i begge ender
        if (startRadius > 0.0f) triangle(start, b1, b0, capStart, capStart, capStart, Vector3Negate(dir));
        if (endRadius > 0.0f) triangle(end, t0, t1, capEnd, capEnd, capEnd, dir);
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

    auto point = [&](float lat, float lon, Color& outColor) {
        // Enhetskule-koordinater
        float nf = cosf(lat) * cosf(lon), nu = sinf(lat), ns = cosf(lat) * sinf(lon);
        Vector3 p = Vector3Add(center, Vector3Add(Vector3Scale(f, nf * radii.x), Vector3Add(Vector3Scale(u, nu * radii.y), Vector3Scale(sd, ns * radii.z))));
        // Normalen på en strukket kule: del på radiene
        Vector3 n = Vector3Normalize(Vector3Add(Vector3Scale(f, nf / radii.x), Vector3Add(Vector3Scale(u, nu / radii.y), Vector3Scale(sd, ns / radii.z))));
        outColor = lit(color, n);
        return p;
    };

    rlCheckRenderBatchLimit(rings * slices * 6);
    rlBegin(RL_TRIANGLES);
    for (int i = 0; i < rings; i++) {
        float lat0 = -PI / 2.0f + PI * i / rings;
        float lat1 = -PI / 2.0f + PI * (i + 1) / rings;
        for (int j = 0; j < slices; j++) {
            float lon0 = 2.0f * PI * j / slices;
            float lon1 = 2.0f * PI * (j + 1) / slices;
            Color c00, c01, c10, c11;
            Vector3 p00 = point(lat0, lon0, c00), p01 = point(lat0, lon1, c01);
            Vector3 p10 = point(lat1, lon0, c10), p11 = point(lat1, lon1, c11);
            Vector3 out = Vector3Subtract(Vector3Scale(Vector3Add(Vector3Add(p00, p01), Vector3Add(p10, p11)), 0.25f), center);
            triangle(p00, p10, p11, c00, c10, c11, out);
            triangle(p00, p11, p01, c00, c11, c01, out);
        }
    }
    rlEnd();
}
