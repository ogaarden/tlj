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

void triangle(Vector3 a, Vector3 b, Vector3 c, Color ca, Color cb, Color cc) {
    vertex(a, ca);
    vertex(b, cb);
    vertex(c, cc);
}

} // namespace

void InitRenderer3D() {
    // Kameraet står ~830 enheter fra bakken. Standard nærplan (0.01-0.05) gir for dårlig
    // dybdepresisjon på den avstanden, så figurer kan "forsvinne" inn i gulvet.
    // rlSetClipPlanes finnes fra raylib 5.5.
#if defined(RAYLIB_VERSION_MAJOR) && (RAYLIB_VERSION_MAJOR > 5 || (RAYLIB_VERSION_MAJOR == 5 && RAYLIB_VERSION_MINOR >= 5))
    rlSetClipPlanes(10.0, 5000.0);
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
    Camera3D cam{};
    cam.target = ToWorld3D(focus, 0.0f);
    cam.position = {
        focus.x + sinf(yaw) * View3D::CAMERA_DISTANCE,
        View3D::CAMERA_HEIGHT,
        focus.y + cosf(yaw) * View3D::CAMERA_DISTANCE
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
    rlDisableBackfaceCulling();
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

            triangle(p00, p10, p11, lit(color, n00), lit(color, n10), lit(color, n11));
            triangle(p00, p11, p01, lit(color, n00), lit(color, n11), lit(color, n01));
        }
    }
    rlEnd();
    rlEnableBackfaceCulling();
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
    rlDisableBackfaceCulling();
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
        triangle(b0, t0, t1, lit(color, n0), lit(color, n0), lit(color, n1));
        triangle(b0, t1, b1, lit(color, n0), lit(color, n1), lit(color, n1));

        // Lokk i begge ender
        if (startRadius > 0.0f) triangle(start, b1, b0, capStart, capStart, capStart);
        if (endRadius > 0.0f) triangle(end, t0, t1, capEnd, capEnd, capEnd);
    }
    rlEnd();
    rlEnableBackfaceCulling();
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
        triangle(a, b, c, col, col, col);
        triangle(a, c, d, col, col, col);
    };

    rlCheckRenderBatchLimit(36);
    rlDisableBackfaceCulling();
    rlBegin(RL_TRIANGLES);
    face(corner(-1, 1, -1), corner(1, 1, -1), corner(1, 1, 1), corner(-1, 1, 1), ay);                        // Topp
    face(corner(-1, -1, -1), corner(1, -1, -1), corner(1, -1, 1), corner(-1, -1, 1), Vector3Negate(ay));     // Bunn
    face(corner(-1, -1, 1), corner(1, -1, 1), corner(1, 1, 1), corner(-1, 1, 1), az);                        // Sør
    face(corner(-1, -1, -1), corner(1, -1, -1), corner(1, 1, -1), corner(-1, 1, -1), Vector3Negate(az));     // Nord
    face(corner(1, -1, -1), corner(1, -1, 1), corner(1, 1, 1), corner(1, 1, -1), ax);                        // Øst
    face(corner(-1, -1, -1), corner(-1, -1, 1), corner(-1, 1, 1), corner(-1, 1, -1), Vector3Negate(ax));     // Vest
    rlEnd();
    rlEnableBackfaceCulling();
}

void DrawSpriteStanding(const Camera3D& camera, Texture2D texture, Vector2 feet, float height, bool flipX, Color tint) {
    // Kameraets "opp"-retning, så bunnen av spriten treffer bakken akkurat ved føttene
    Vector3 forward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, { 0, 1, 0 }));
    Vector3 up = Vector3CrossProduct(right, forward);

    float width = height * (float)texture.width / (float)texture.height;
    Vector3 center = Vector3Add(ToWorld3D(feet, 0.0f), Vector3Scale(up, height / 2.0f));
    Rectangle source = { 0.0f, 0.0f, flipX ? -(float)texture.width : (float)texture.width, (float)texture.height };
    // DrawBillboardRec holder spriten loddrett i verden (blir sammenklemt sett ovenfra).
    // Med kameraets egen opp-retning vender spriten rett mot kameraet, som i Vampire Survivors.
    Vector2 size = { width, height };
    DrawBillboardPro(camera, texture, source, center, up, size, Vector2Scale(size, 0.5f), 0.0f, tint);
}
