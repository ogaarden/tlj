#include "vfx.hpp"
#include "render3d.hpp"
#include <raymath.h>
#include <rlgl.h>
#include <vector>
#include <cmath>

namespace {

const char* FILE_NAMES[(int)VfxTex::COUNT] = {
    "assets/vfx/lightning_bolt.png",
    "assets/vfx/lightning_impact.png",
    "assets/vfx/magic_orb.png",
    "assets/vfx/explosion.png",
    "assets/vfx/shockwave.png",
    "assets/vfx/poison_mist.png",
    "assets/vfx/slash.png",
    "assets/vfx/spark.png",
    nullptr, // GLOW lages alltid i kode
};

Texture2D textures[(int)VfxTex::COUNT] = {};
Camera3D currentCamera{};

float frand(float a, float b) { return a + (b - a) * (GetRandomValue(0, 10000) / 10000.0f); }

// ---------------------------------------------------------------------
// Erstatnings-teksturer laget i kode (brukes hvis PNG-en mangler)
// ---------------------------------------------------------------------
Color addColor(Color a, Color b) {
    auto c = [](int v) { return (unsigned char)(v > 255 ? 255 : v); };
    return { c(a.r + b.r), c(a.g + b.g), c(a.b + b.b), 255 };
}

Image proceduralImage(VfxTex tex) {
    const int S = 128;
    Image img = GenImageColor(S, S, BLACK);
    Color* px = (Color*)img.data;
    auto put = [&](int x, int y, Color c) {
        if (x < 0 || y < 0 || x >= S || y >= S) return;
        px[y * S + x] = addColor(px[y * S + x], c);
    };
    auto scale = [](Color c, float k) {
        k = Clamp(k, 0.0f, 1.0f);
        return Color{ (unsigned char)(c.r * k), (unsigned char)(c.g * k), (unsigned char)(c.b * k), 255 };
    };
    // Glød rundt et punkt (brukes til å "male" streker og flekker)
    auto blob = [&](float cx, float cy, float r, Color c) {
        for (int y = (int)(cy - r); y <= (int)(cy + r); y++)
            for (int x = (int)(cx - r); x <= (int)(cx + r); x++) {
                float d = sqrtf((x - cx) * (x - cx) + (y - cy) * (y - cy)) / r;
                if (d < 1.0f) put(x, y, scale(c, (1.0f - d) * (1.0f - d)));
            }
    };
    auto stroke = [&](Vector2 a, Vector2 b, float r, Color c) {
        float len = Vector2Distance(a, b);
        for (float t = 0.0f; t <= len; t += r * 0.5f) {
            Vector2 p = Vector2Lerp(a, b, len > 0.0f ? t / len : 0.0f);
            blob(p.x, p.y, r, c);
        }
    };
    float c = S / 2.0f;

    switch (tex) {
        case VfxTex::GLOW:
            for (int y = 0; y < S; y++)
                for (int x = 0; x < S; x++) {
                    float d = sqrtf((x - c + 0.5f) * (x - c + 0.5f) + (y - c + 0.5f) * (y - c + 0.5f)) / c;
                    float k = d < 1.0f ? powf(1.0f - d, 2.2f) : 0.0f;
                    px[y * S + x] = scale(WHITE, k);
                }
            break;
        case VfxTex::SPARK:
            blob(c, c, S * 0.22f, Color{ 160, 160, 160, 255 });
            stroke({ c, 4.0f }, { c, S - 4.0f }, 3.0f, Color{ 90, 90, 90, 255 });
            stroke({ 4.0f, c }, { S - 4.0f, c }, 3.0f, Color{ 90, 90, 90, 255 });
            blob(c, c, 8.0f, WHITE);
            break;
        case VfxTex::MAGIC_ORB:
            blob(c, c, S * 0.48f, Color{ 90, 30, 140, 255 });
            blob(c, c, S * 0.25f, Color{ 180, 90, 255, 255 });
            blob(c, c, S * 0.12f, WHITE);
            break;
        case VfxTex::LIGHTNING_BOLT: {
            Vector2 p = { c, 0.0f };
            while (p.y < S) {
                Vector2 n = { Clamp(p.x + frand(-14.0f, 14.0f), 20.0f, S - 20.0f), p.y + frand(8.0f, 18.0f) };
                stroke(p, n, 6.0f, Color{ 40, 90, 200, 255 });
                stroke(p, n, 2.0f, WHITE);
                p = n;
            }
        } break;
        case VfxTex::LIGHTNING_IMPACT:
            blob(c, c, S * 0.3f, Color{ 60, 120, 220, 255 });
            for (int i = 0; i < 10; i++) {
                float a = i * 2.0f * PI / 10.0f + frand(-0.2f, 0.2f);
                Vector2 p = { c, c };
                for (int k = 0; k < 4; k++) {
                    float r = (k + 1) * S * 0.11f;
                    Vector2 n = { c + cosf(a + frand(-0.3f, 0.3f)) * r, c + sinf(a + frand(-0.3f, 0.3f)) * r };
                    stroke(p, n, 2.5f, Color{ 150, 200, 255, 255 });
                    p = n;
                }
            }
            blob(c, c, 10.0f, WHITE);
            break;
        case VfxTex::EXPLOSION:
            for (int i = 0; i < 40; i++) {
                float a = frand(0.0f, 2.0f * PI), r = frand(0.0f, S * 0.32f);
                blob(c + cosf(a) * r, c + sinf(a) * r, frand(8.0f, 18.0f), Color{ 140, 50, 10, 255 });
            }
            blob(c, c, S * 0.3f, Color{ 255, 170, 40, 255 });
            blob(c, c, S * 0.15f, Color{ 255, 255, 200, 255 });
            break;
        case VfxTex::SHOCKWAVE:
            for (int y = 0; y < S; y++)
                for (int x = 0; x < S; x++) {
                    float d = sqrtf((x - c) * (x - c) + (y - c) * (y - c)) / c;
                    float k = expf(-powf((d - 0.82f) / 0.08f, 2.0f));
                    px[y * S + x] = scale(Color{ 255, 190, 80, 255 }, k);
                }
            break;
        case VfxTex::POISON_MIST:
            for (int i = 0; i < 30; i++) {
                float a = frand(0.0f, 2.0f * PI), r = frand(0.0f, S * 0.38f);
                blob(c + cosf(a) * r, c + sinf(a) * r, frand(10.0f, 20.0f), Color{ 30, 90, 20, 255 });
            }
            break;
        case VfxTex::SLASH:
            for (int i = 0; i < 60; i++) {
                float t = i / 59.0f;
                float a = PI * (0.15f + 0.7f * t);
                blob(c + cosf(a) * S * 0.36f, c - sinf(a) * S * 0.2f, 6.0f + 6.0f * sinf(t * PI), Color{ 120, 30, 90, 255 });
            }
            break;
        default: break;
    }
    return img;
}

// Mørk støy i "svarte" bakgrunner blir til et grått slør med additiv blending – kutt den bort
void cleanBlack(Image& img) {
    ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    Color* px = (Color*)img.data;
    const int cut = 4; // Filene i assets/vfx er allerede renset, dette tar bare bort siste rest av støy
    for (int i = 0; i < img.width * img.height; i++) {
        auto f = [cut](unsigned char v) { int o = (v - cut) * 255 / (255 - cut); return (unsigned char)(o < 0 ? 0 : o); };
        px[i] = { f(px[i].r), f(px[i].g), f(px[i].b), 255 };
    }
}

// ---------------------------------------------------------------------
// Partikler
// ---------------------------------------------------------------------
struct Particle {
    VfxTex tex;
    Vector3 pos;
    Vector3 vel;
    float gravity;
    float drag;
    float life, maxLife;
    float size, sizeEnd;
    float rot, rotSpeed;
    Color color;
    bool decal;       // Ligger flatt på gulvet i stedet for å se mot kameraet
};

std::vector<Particle> particles;
constexpr size_t MAX_PARTICLES = 4000;

// Korte elektriske buer (hakkete linje mellom to punkter)
struct Zap {
    std::vector<Vector3> points;
    Color color;
    float life, maxLife;
};
std::vector<Zap> zaps;

void spawn(const Particle& p) {
    if (particles.size() >= MAX_PARTICLES) return;
    particles.push_back(p);
}

// Tegner et teksturert firkant-kvad (fire hjørner, UV 0..1)
void texturedQuad(Texture2D tex, Vector3 a, Vector3 b, Vector3 c, Vector3 d, Color tint) {
    rlSetTexture(tex.id);
    rlBegin(RL_QUADS);
    rlColor4ub(tint.r, tint.g, tint.b, tint.a);
    rlTexCoord2f(0.0f, 0.0f); rlVertex3f(a.x, a.y, a.z);
    rlTexCoord2f(0.0f, 1.0f); rlVertex3f(b.x, b.y, b.z);
    rlTexCoord2f(1.0f, 1.0f); rlVertex3f(c.x, c.y, c.z);
    rlTexCoord2f(1.0f, 0.0f); rlVertex3f(d.x, d.y, d.z);
    rlEnd();
    rlSetTexture(0);
}

} // namespace

void InitVfx() {
    for (int i = 0; i < (int)VfxTex::COUNT; i++) {
        Image img{};
        const char* path = FILE_NAMES[i];
        if (path && FileExists(path)) {
            img = LoadImage(path);
            cleanBlack(img);
        } else {
            img = proceduralImage((VfxTex)i);
        }
        textures[i] = LoadTextureFromImage(img);
        GenTextureMipmaps(&textures[i]);
        SetTextureFilter(textures[i], TEXTURE_FILTER_TRILINEAR);
        UnloadImage(img);
    }
}

void UnloadVfx() {
    for (auto& t : textures) UnloadTexture(t);
    particles.clear();
}

Texture2D VfxTexture(VfxTex tex) { return textures[(int)tex]; }

void VfxBegin(const Camera3D& camera) {
    currentCamera = camera;
    // Tøm batchen FØR tilstandene endres, ellers gjelder de feil tegninger (se render3d.cpp)
    rlDrawRenderBatchActive();
    rlDisableBackfaceCulling();
    rlDisableDepthMask();              // Glød skal ikke skjule ting bak seg
    BeginBlendMode(BLEND_ADDITIVE);
}

void VfxEnd() {
    EndBlendMode();                    // Tegner batchen med additiv blending
    rlDrawRenderBatchActive();
    rlEnableDepthMask();
    rlEnableBackfaceCulling();
}

void VfxBillboard(VfxTex tex, Vector3 pos, float size, Color tint, float rotationDeg) {
    Texture2D t = textures[(int)tex];
    Rectangle src = { 0.0f, 0.0f, (float)t.width, (float)t.height };
    float aspect = (float)t.height / (float)t.width;
    Vector2 sz = { size, size * aspect };
    DrawBillboardPro(currentCamera, t, src, pos, { 0.0f, 1.0f, 0.0f }, sz, { sz.x / 2.0f, sz.y / 2.0f }, rotationDeg, tint);
}

void VfxDecal(VfxTex tex, Vector2 ground, float size, Color tint, float rotationDeg, float height) {
    float r = rotationDeg * DEG2RAD;
    float h = size / 2.0f;
    Vector2 ax = { cosf(r) * h, sinf(r) * h };
    Vector2 ay = { -sinf(r) * h, cosf(r) * h };
    auto corner = [&](float sx, float sy) {
        return Vector3{ ground.x + ax.x * sx + ay.x * sy, height, ground.y + ax.y * sx + ay.y * sy };
    };
    texturedQuad(textures[(int)tex], corner(-1, -1), corner(-1, 1), corner(1, 1), corner(1, -1), tint);
}

void VfxBeam(VfxTex tex, Vector3 from, Vector3 to, float width, Color tint) {
    // Kvadet vendes mot kameraet rundt aksen fra -> to
    Vector3 axis = Vector3Subtract(to, from);
    Vector3 mid = Vector3Lerp(from, to, 0.5f);
    Vector3 toCam = Vector3Subtract(currentCamera.position, mid);
    Vector3 side = Vector3CrossProduct(axis, toCam);
    if (Vector3Length(side) < 0.001f) return;
    side = Vector3Scale(Vector3Normalize(side), width / 2.0f);
    texturedQuad(textures[(int)tex],
                 Vector3Subtract(from, side), Vector3Subtract(to, side),
                 Vector3Add(to, side), Vector3Add(from, side), tint);
}

// ---------------------------------------------------------------------
// Partikler: oppdatering og tegning
// ---------------------------------------------------------------------
void UpdateVfx(float dt) {
    for (size_t i = 0; i < zaps.size(); ) {
        zaps[i].life -= dt;
        if (zaps[i].life <= 0.0f) { zaps[i] = zaps.back(); zaps.pop_back(); }
        else i++;
    }
    for (size_t i = 0; i < particles.size(); ) {
        Particle& p = particles[i];
        p.life -= dt;
        if (p.life <= 0.0f) {
            particles[i] = particles.back();
            particles.pop_back();
            continue;
        }
        p.vel.y -= p.gravity * dt;
        float d = 1.0f / (1.0f + p.drag * dt);
        p.vel = Vector3Scale(p.vel, d);
        p.pos = Vector3Add(p.pos, Vector3Scale(p.vel, dt));
        if (p.pos.y < 0.5f && !p.decal) { p.pos.y = 0.5f; p.vel.y = fabsf(p.vel.y) * 0.3f; } // Små sprett på gulvet
        p.rot += p.rotSpeed * dt;
        i++;
    }
}

void DrawVfxParticles() {
    for (const Zap& z : zaps) {
        float a = z.life / z.maxLife;
        Color glow = { (unsigned char)(z.color.r * a), (unsigned char)(z.color.g * a), (unsigned char)(z.color.b * a), 255 };
        Color core = { (unsigned char)(255 * a), (unsigned char)(255 * a), (unsigned char)(255 * a), 255 };
        for (size_t i = 1; i < z.points.size(); i++) {
            VfxBeam(VfxTex::GLOW, z.points[i - 1], z.points[i], 16.0f, glow);
            VfxBeam(VfxTex::GLOW, z.points[i - 1], z.points[i], 5.0f, core);
        }
    }
    for (const Particle& p : particles) {
        float t = 1.0f - p.life / p.maxLife;       // 0 -> 1
        float size = p.size + (p.sizeEnd - p.size) * t;
        float fade = t < 0.15f ? t / 0.15f : 1.0f - (t - 0.15f) / 0.85f; // Rask inn, rolig ut
        Color c = { (unsigned char)(p.color.r * fade), (unsigned char)(p.color.g * fade), (unsigned char)(p.color.b * fade), 255 };
        if (p.decal) VfxDecal(p.tex, { p.pos.x, p.pos.z }, size, c, p.rot, p.pos.y);
        else VfxBillboard(p.tex, p.pos, size, c, p.rot);
    }
}

void ClearVfx() { particles.clear(); zaps.clear(); }

// ---------------------------------------------------------------------
// Ferdige effekter
// ---------------------------------------------------------------------
void VfxHit(Vector2 g, Color color) {
    Vector3 at = ToWorld3D(g, 20.0f);
    spawn({ VfxTex::SPARK, at, { 0, 0, 0 }, 0, 0, 0.14f, 0.14f, 26.0f, 8.0f, frand(0, 90), 0, color, false });
    for (int i = 0; i < 5; i++) {
        float a = frand(0, 2 * PI);
        Vector3 v = { cosf(a) * frand(60, 160), frand(40, 140), sinf(a) * frand(60, 160) };
        spawn({ VfxTex::GLOW, at, v, 380.0f, 2.0f, 0.3f, 0.3f, 6.0f, 1.0f, 0, 0, color, false });
    }
}

void VfxDeath(Vector2 g, Color color) {
    Vector3 at = ToWorld3D(g, 18.0f);
    spawn({ VfxTex::GLOW, at, { 0, 20, 0 }, 0, 0, 0.3f, 0.3f, 30.0f, 60.0f, 0, 0, Color{ 255, 240, 220, 255 }, false });
    spawn({ VfxTex::SPARK, at, { 0, 0, 0 }, 0, 0, 0.22f, 0.22f, 40.0f, 10.0f, frand(0, 90), 120.0f, color, false });
    for (int i = 0; i < 10; i++) {
        float a = frand(0, 2 * PI);
        Vector3 v = { cosf(a) * frand(80, 220), frand(80, 220), sinf(a) * frand(80, 220) };
        spawn({ VfxTex::GLOW, at, v, 420.0f, 1.5f, 0.5f, 0.5f, 7.0f, 2.0f, 0, 0, color, false });
    }
}

void VfxTrail(Vector3 pos, Color color, float size, float life) {
    spawn({ VfxTex::GLOW, pos, { 0, 0, 0 }, 0, 0, life, life, size, size * 0.3f, 0, 0, color, false });
}

void VfxShockwave(Vector2 g, float radius, Color color) {
    float rot = frand(0, 360);
    // Gyllen ring som vokser utover på gulvet
    spawn({ VfxTex::SHOCKWAVE, ToWorld3D(g, 0.8f), { 0, 0, 0 }, 0, 0, 0.45f, 0.45f, radius * 0.6f, radius * 2.5f, rot, 40.0f, WHITE, true });
    spawn({ VfxTex::GLOW, ToWorld3D(g, 1.0f), { 0, 0, 0 }, 0, 0, 0.3f, 0.3f, radius * 1.2f, radius * 2.2f, 0, 0, Fade(color, 0.6f), true });
    // Støv og steinbiter som kastes ut
    for (int i = 0; i < 26; i++) {
        float a = frand(0, 2 * PI);
        float sp = frand(radius * 1.2f, radius * 2.6f);
        Vector3 v = { cosf(a) * sp, frand(60, 200), sinf(a) * sp };
        Vector3 start = ToWorld3D({ g.x + cosf(a) * radius * 0.2f, g.y + sinf(a) * radius * 0.2f }, 4.0f);
        spawn({ VfxTex::GLOW, start, v, 500.0f, 3.0f, frand(0.35f, 0.6f), 0.6f, frand(6, 12), 2.0f, 0, 0, Color{ 255, 200, 110, 255 }, false });
    }
}

void VfxLightningStrike(Vector2 g, float radius) {
    Vector3 at = ToWorld3D(g, 6.0f);
    spawn({ VfxTex::LIGHTNING_IMPACT, ToWorld3D(g, 1.0f), { 0, 0, 0 }, 0, 0, 0.35f, 0.35f, radius * 2.4f, radius * 3.0f, frand(0, 360), 0, WHITE, true });
    spawn({ VfxTex::GLOW, at, { 0, 0, 0 }, 0, 0, 0.2f, 0.2f, 110.0f, 60.0f, 0, 0, Color{ 200, 230, 255, 255 }, false });
    for (int i = 0; i < 14; i++) {
        float a = frand(0, 2 * PI);
        Vector3 v = { cosf(a) * frand(100, 300), frand(80, 260), sinf(a) * frand(100, 300) };
        spawn({ VfxTex::SPARK, at, v, 600.0f, 2.0f, frand(0.25f, 0.45f), 0.45f, 12.0f, 3.0f, frand(0, 90), 200.0f, Color{ 170, 215, 255, 255 }, false });
    }
}

void VfxExplosion(Vector2 g, float radius) {
    Vector3 at = ToWorld3D(g, radius * 0.4f);
    spawn({ VfxTex::EXPLOSION, at, { 0, 30, 0 }, 0, 0, 0.5f, 0.5f, radius * 1.4f, radius * 3.0f, frand(0, 360), 60.0f, WHITE, false });
    spawn({ VfxTex::EXPLOSION, ToWorld3D(g, 1.0f), { 0, 0, 0 }, 0, 0, 0.6f, 0.6f, radius * 1.8f, radius * 2.6f, frand(0, 360), -30.0f, Color{ 200, 120, 60, 255 }, true });
    spawn({ VfxTex::GLOW, at, { 0, 0, 0 }, 0, 0, 0.18f, 0.18f, radius * 3.0f, radius * 1.5f, 0, 0, Color{ 255, 220, 150, 255 }, false });
    for (int i = 0; i < 24; i++) {
        float a = frand(0, 2 * PI);
        Vector3 v = { cosf(a) * frand(120, 320), frand(100, 320), sinf(a) * frand(120, 320) };
        spawn({ VfxTex::GLOW, at, v, 450.0f, 1.5f, frand(0.5f, 0.9f), 0.9f, frand(5, 9), 1.5f, 0, 0, Color{ 255, 150, 50, 255 }, false });
    }
}

void VfxBubble(Vector2 g, float height, Color color) {
    Vector3 v = { frand(-8, 8), frand(20, 45), frand(-8, 8) };
    spawn({ VfxTex::GLOW, ToWorld3D(g, height), v, 0.0f, 0.5f, frand(0.6f, 1.1f), 1.1f, frand(4, 8), frand(8, 12), 0, 0, color, false });
}

void VfxZap(Vector2 from, Vector2 to, float height, Color color) {
    Zap z;
    z.color = color;
    z.life = z.maxLife = 0.18f;
    const int segments = 6;
    for (int i = 0; i <= segments; i++) {
        Vector2 p = Vector2Lerp(from, to, (float)i / segments);
        float jitter = (i == 0 || i == segments) ? 0.0f : 7.0f;
        z.points.push_back(ToWorld3D({ p.x + frand(-jitter, jitter), p.y + frand(-jitter, jitter) }, height + frand(-jitter, jitter) * 0.5f));
    }
    zaps.push_back(z);
    spawn({ VfxTex::SPARK, z.points.back(), { 0, 0, 0 }, 0, 0, 0.15f, 0.15f, 30.0f, 8.0f, frand(0, 90), 0, color, false });
}

void VfxMuzzle(Vector2 g, Vector2 dir, Color color) {
    Vector3 at = ToWorld3D({ g.x + dir.x * 16.0f, g.y + dir.y * 16.0f }, 24.0f);
    spawn({ VfxTex::GLOW, at, { 0, 0, 0 }, 0, 0, 0.12f, 0.12f, 34.0f, 12.0f, 0, 0, color, false });
    spawn({ VfxTex::SPARK, at, { 0, 0, 0 }, 0, 0, 0.12f, 0.12f, 30.0f, 10.0f, frand(0, 90), 0, WHITE, false });
    for (int i = 0; i < 4; i++) {
        Vector3 v = { dir.x * frand(150, 300) + frand(-60, 60), frand(20, 90), dir.y * frand(150, 300) + frand(-60, 60) };
        spawn({ VfxTex::GLOW, at, v, 300.0f, 3.0f, 0.25f, 0.25f, 5.0f, 1.0f, 0, 0, color, false });
    }
}
