#include "sprites.hpp"
#include <raymath.h>
#include <rlgl.h>
#include <vector>
#include <algorithm>
#include <cmath>

namespace {

const char* FILES[(int)SpriteId::COUNT] = {
    "assets/sprites/jester.png", "assets/sprites/wester.png", "assets/sprites/geek.png", "assets/sprites/pierrot.png",
    "assets/sprites/footman.png", "assets/sprites/goon.png", "assets/sprites/lackey.png", "assets/sprites/exploder.png",
    "assets/sprites/archer.png",
    "assets/sprites/executioner.png", "assets/sprites/magus.png", "assets/sprites/iron_knight.png", "assets/sprites/king.png",
};

// Høyde i verden (hele bildet, inkludert papirkanten). Bildene har 3 piksler per enhet.
const float HEIGHTS[(int)SpriteId::COUNT] = {
    88.0f, 90.0f, 92.0f, 82.0f,
    76.0f, 98.0f, 54.0f, 60.0f, 72.0f,
    96.0f, 88.0f, 108.0f, 175.0f,
};

// Hodet i bildet (til HUD-portrettet) som andeler av bildet: midtpunkt x, midtpunkt y og størrelse
// (andel av høyden). x < 0: regnes ut automatisk. Klovnene har våpen over hodet, så de settes for hånd.
const float HEADS[(int)SpriteId::COUNT][3] = {
    { 0.40f, 0.26f, 0.40f }, { 0.53f, 0.26f, 0.36f }, { 0.42f, 0.17f, 0.32f }, { 0.39f, 0.20f, 0.34f },
    { -1, 0, 0 }, { -1, 0, 0 }, { -1, 0, 0 }, { -1, 0, 0 }, { -1, 0, 0 },
    { -1, 0, 0 }, { -1, 0, 0 }, { -1, 0, 0 }, { -1, 0, 0 },
};

Texture2D textures[(int)SpriteId::COUNT] = {};
Rectangle heads[(int)SpriteId::COUNT] = {};   // Hvor hodet er i bildet (for portrettet)
bool loaded[(int)SpriteId::COUNT] = {};
bool enabled = true;
std::vector<SpriteDraw> queue;
Shader shader{};
bool shaderLoaded = false;

// Alfa-test (så figurene kan tegnes med dybde uten rare kanter) og hvitt treffglimt.
// Glimtet sendes i vertex-fargens alfa: 255 = normalt, 0 = helt hvit.
const char* FRAGMENT = R"(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
out vec4 finalColor;
void main() {
    vec4 t = texture(texture0, fragTexCoord);
    if (t.a < 0.35) discard;
    float flash = 1.0 - fragColor.a;
    vec3 c = t.rgb * fragColor.rgb * colDiffuse.rgb;
    finalColor = vec4(mix(c, vec3(1.0), flash), t.a);
}
)";

} // namespace

void InitSprites() {
    for (int i = 0; i < (int)SpriteId::COUNT; i++) {
        if (!FileExists(FILES[i])) continue;
        Image img = LoadImage(FILES[i]);
        ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
        // Hodet: tyngdepunktet til de synlige pikslene i øverste tredjedel
        {
            Color* px = (Color*)img.data;
            double sum = 0, sx = 0;
            int rows = img.height * 35 / 100;
            for (int y = 0; y < rows; y++)
                for (int x = 0; x < img.width; x++) { float a = px[y * img.width + x].a / 255.0f; sum += a; sx += a * x; }
            float cx = sum > 0 ? (float)(sx / sum) : img.width / 2.0f;
            float size = img.height * 0.46f;
            heads[i] = { cx - size / 2.0f, img.height * 0.01f, size, size };
            if (HEADS[i][0] >= 0.0f) {
                float sz = img.height * HEADS[i][2];
                heads[i] = { img.width * HEADS[i][0] - sz / 2.0f, img.height * HEADS[i][1] - sz / 2.0f, sz, sz };
            }
        }
        textures[i] = LoadTextureFromImage(img);
        UnloadImage(img);
        // Malte papirfigurer: mipmaps og myk filtrering, så de er rene også når de er små
        GenTextureMipmaps(&textures[i]);
        SetTextureFilter(textures[i], TEXTURE_FILTER_TRILINEAR);
        loaded[i] = textures[i].id != 0;
    }
    shader = LoadShaderFromMemory(nullptr, FRAGMENT);
    shaderLoaded = shader.id != 0;
}

void UnloadSprites() {
    for (int i = 0; i < (int)SpriteId::COUNT; i++) if (loaded[i]) UnloadTexture(textures[i]);
    if (shaderLoaded) UnloadShader(shader);
}

bool HasSprite(SpriteId id) { return id != SpriteId::COUNT && loaded[(int)id] && enabled; }
Texture2D SpriteTexture(SpriteId id) { return textures[(int)id]; }
float SpriteBaseHeight(SpriteId id) { return HEIGHTS[(int)id]; }
Rectangle SpriteHeadRect(SpriteId id) { return heads[(int)id]; }
bool SpritesEnabled() { return enabled; }
void SetSpritesEnabled(bool e) { enabled = e; }

void QueueSprite(const SpriteDraw& s) { queue.push_back(s); }

bool FacesLeftOnScreen(const Camera3D& camera, Vector2 facing) {
    Vector3 fwd = Vector3Subtract(camera.target, camera.position);
    Vector2 right = { -fwd.z, fwd.x }; // Kameraets høyre, projisert på gulvet
    return facing.x * right.x + facing.y * right.y < -0.05f;
}

void DrawQueuedSprites(const Camera3D& camera) {
    if (queue.empty()) return;

    // Kameraets høyre- og opp-vektor: figurene står vinkelrett på synsretningen
    Vector3 fwd = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    Vector3 right = Vector3Normalize(Vector3CrossProduct(fwd, { 0, 1, 0 }));
    Vector3 up = Vector3CrossProduct(right, fwd);

    // Bakerst først, så kantene blandes riktig
    std::sort(queue.begin(), queue.end(), [&](const SpriteDraw& a, const SpriteDraw& b) {
        Vector3 pa = { a.feet.x, 0, a.feet.y }, pb = { b.feet.x, 0, b.feet.y };
        return Vector3DotProduct(Vector3Subtract(pa, camera.position), fwd) > Vector3DotProduct(Vector3Subtract(pb, camera.position), fwd);
    });

    rlDrawRenderBatchActive();
    rlDisableBackfaceCulling();
    if (shaderLoaded) BeginShaderMode(shader);
    for (const SpriteDraw& s : queue) {
        Texture2D t = textures[(int)s.id];
        float h = s.height * (1.0f + 0.07f * s.squash);
        float w = s.height * (float)t.width / (float)t.height * (1.0f - 0.06f * s.squash);
        // Vipping rundt føttene i bildeplanet
        float a = s.lean * DEG2RAD;
        Vector3 r = Vector3Add(Vector3Scale(right, cosf(a)), Vector3Scale(up, sinf(a)));
        Vector3 u = Vector3Add(Vector3Scale(up, cosf(a)), Vector3Scale(right, -sinf(a)));
        Vector3 base = { s.feet.x, s.hop - s.sink, s.feet.y };
        // Litt mot kameraet så føttene ikke skjæres av gulvet
        base = Vector3Subtract(base, Vector3Scale(fwd, 6.0f));
        Vector3 bl = Vector3Subtract(base, Vector3Scale(r, w / 2.0f));
        Vector3 br = Vector3Add(base, Vector3Scale(r, w / 2.0f));
        Vector3 tl = Vector3Add(bl, Vector3Scale(u, h));
        Vector3 tr = Vector3Add(br, Vector3Scale(u, h));
        float u0 = s.flip ? 1.0f : 0.0f, u1 = s.flip ? 0.0f : 1.0f;
        unsigned char alpha = (unsigned char)(255.0f * (1.0f - Clamp(s.flash, 0.0f, 1.0f)));

        rlSetTexture(t.id);
        rlBegin(RL_QUADS);
        rlColor4ub(s.tint.r, s.tint.g, s.tint.b, alpha);
        rlTexCoord2f(u0, 0.0f); rlVertex3f(tl.x, tl.y, tl.z);
        rlTexCoord2f(u0, 1.0f); rlVertex3f(bl.x, bl.y, bl.z);
        rlTexCoord2f(u1, 1.0f); rlVertex3f(br.x, br.y, br.z);
        rlTexCoord2f(u1, 0.0f); rlVertex3f(tr.x, tr.y, tr.z);
        rlEnd();
    }
    rlSetTexture(0);
    if (shaderLoaded) EndShaderMode();
    rlDrawRenderBatchActive();
    rlEnableBackfaceCulling();
    queue.clear();
}
