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
    "assets/sprites/hound.png", "assets/sprites/priest.png", "assets/sprites/drummer.png", "assets/sprites/cannoneer.png",
    "assets/sprites/executioner.png", "assets/sprites/magus.png", "assets/sprites/iron_knight.png", "assets/sprites/king.png",
};

// Høyde i verden (hele bildet, inkludert papirkanten). Bildene har 3 piksler per enhet.
const float HEIGHTS[(int)SpriteId::COUNT] = {
    88.0f, 90.0f, 92.0f, 82.0f,
    76.0f, 98.0f, 54.0f, 60.0f, 72.0f,
    58.0f, 84.0f, 92.0f, 86.0f,
    96.0f, 88.0f, 108.0f, 175.0f,
};

// Hodet i bildet (til HUD-portrettet) som andeler av bildet: midtpunkt x, midtpunkt y og størrelse
// (andel av høyden). x < 0: regnes ut automatisk. Klovnene har våpen over hodet, så de settes for hånd.
const float HEADS[(int)SpriteId::COUNT][3] = {
    { 0.40f, 0.26f, 0.40f }, { 0.53f, 0.26f, 0.36f }, { 0.42f, 0.17f, 0.32f }, { 0.39f, 0.20f, 0.34f },
    { -1, 0, 0 }, { -1, 0, 0 }, { -1, 0, 0 }, { -1, 0, 0 }, { -1, 0, 0 },
    { -1, 0, 0 }, { -1, 0, 0 }, { -1, 0, 0 }, { -1, 0, 0 },
    { -1, 0, 0 }, { -1, 0, 0 }, { -1, 0, 0 }, { -1, 0, 0 },
};

// Gange: hvordan beina beveger seg, og hvor i bildet beina begynner (andel fra toppen).
//  SPLIT = to bein som går hver sin vei (bildet deles på midten mellom føttene)
//  SWAY  = kappe/kjole: hele underdelen svinger litt fra side til side
enum class Legs { SPLIT, SWAY };
struct LegSetup { Legs mode; float top; };
const LegSetup LEGS[(int)SpriteId::COUNT] = {
    { Legs::SPLIT, 0.66f }, { Legs::SPLIT, 0.78f }, { Legs::SPLIT, 0.62f }, { Legs::SPLIT, 0.66f }, // Klovnene
    { Legs::SPLIT, 0.80f }, { Legs::SPLIT, 0.80f }, { Legs::SPLIT, 0.80f }, { Legs::SPLIT, 0.88f }, { Legs::SPLIT, 0.80f },
    { Legs::SPLIT, 0.60f }, { Legs::SWAY, 0.62f }, { Legs::SPLIT, 0.80f }, { Legs::SPLIT, 0.80f }, // Hund: for- og bakbein
    { Legs::SPLIT, 0.74f }, { Legs::SWAY, 0.55f }, { Legs::SPLIT, 0.74f }, { Legs::SWAY, 0.62f },
};
float legSplitX[(int)SpriteId::COUNT] = {};   // Hvor bildet deles mellom beina (andel fra venstre)

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
        // Delingen mellom beina: tyngdepunktet til pikslene i de nederste radene (der føttene er)
        {
            Color* px = (Color*)img.data;
            double sum = 0, sx = 0;
            for (int y = img.height * 90 / 100; y < img.height; y++)
                for (int x = 0; x < img.width; x++) { float a = px[y * img.width + x].a / 255.0f; sum += a; sx += a * x; }
            legSplitX[i] = sum > 0 ? (float)(sx / sum) / img.width : 0.5f;
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

void UpdateSpriteTurn(float& turn, const Camera3D& camera, Vector2 facing, float deltaTime) {
    Vector3 fwd = Vector3Subtract(camera.target, camera.position);
    Vector2 right = Vector2Normalize({ -fwd.z, fwd.x });
    float side = facing.x * right.x + facing.y * right.y;
    float target = side < -0.2f ? -1.0f : (side > 0.2f ? 1.0f : (turn < 0.0f ? -1.0f : 1.0f));
    if (turn == 0.0f) turn = target;                     // Første frame: ingen snuing
    float step = 9.0f * deltaTime;                       // Ca. 0.2 sek for en hel snuing
    turn = target > turn ? fminf(target, turn + step) : fmaxf(target, turn - step);
    if (turn == 0.0f) turn = target * 0.001f;
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
        const LegSetup& legs = LEGS[(int)s.id];
        float h = s.height * (1.0f + 0.05f * s.squash);
        float w = s.height * (float)t.width / (float)t.height * (1.0f - 0.04f * s.squash) * s.widthScale;
        // Vipping rundt føttene i bildeplanet
        float a = s.lean * DEG2RAD;
        Vector3 r = Vector3Add(Vector3Scale(right, cosf(a)), Vector3Scale(up, sinf(a)));
        Vector3 u = Vector3Add(Vector3Scale(up, cosf(a)), Vector3Scale(right, -sinf(a)));
        Vector3 base = { s.feet.x, s.hop - s.sink, s.feet.y };
        // Litt mot kameraet så føttene ikke skjæres av gulvet
        base = Vector3Subtract(base, Vector3Scale(fwd, 6.0f));
        unsigned char alpha = (unsigned char)(255.0f * (1.0f - Clamp(s.flash, 0.0f, 1.0f)));

        // Gange: kroppen hever seg når et bein står rett under (to ganger per steg-syklus),
        // og føttene svinger frem og tilbake. Figurene ser mot høyre i bildet, så "frem" er +x.
        float st = Clamp(s.stride, 0.0f, 1.0f);
        float bodyLift = fabsf(cosf(s.walk)) * 0.035f * s.height * st;
        float swing = sinf(s.walk) * st;
        float dir = s.flip ? -1.0f : 1.0f;              // Bildets x-akse i verden (speilvendt eller ikke)

        // Punkt i bildet (ix, iy i 0..1, iy = 0 øverst) -> verden. dx/dy: forskyvning i verdensenheter.
        auto P = [&](float ix, float iy, float dx, float dy) {
            float x = (ix - 0.5f) * w * dir + dx * dir * s.widthScale;
            float y = (1.0f - iy) * h + dy;
            return Vector3Add(base, Vector3Add(Vector3Scale(r, x), Vector3Scale(u, y)));
        };
        // Én firkant av bildet: [x0,x1] x [y0,y1], med egen forskyvning av bunnkanten (føttene)
        auto piece = [&](float x0, float x1, float y0, float y1, float topLift, float botDx, float botLift) {
            Vector3 tl = P(x0, y0, 0.0f, topLift), tr = P(x1, y0, 0.0f, topLift);
            Vector3 bl = P(x0, y1, botDx, botLift), br = P(x1, y1, botDx, botLift);
            float u0 = s.flip ? 1.0f - x0 : x0, u1 = s.flip ? 1.0f - x1 : x1;
            rlColor4ub(s.tint.r, s.tint.g, s.tint.b, alpha);
            rlTexCoord2f(u0, y0); rlVertex3f(tl.x, tl.y, tl.z);
            rlTexCoord2f(u0, y1); rlVertex3f(bl.x, bl.y, bl.z);
            rlTexCoord2f(u1, y1); rlVertex3f(br.x, br.y, br.z);
            rlTexCoord2f(u1, y0); rlVertex3f(tr.x, tr.y, tr.z);
        };

        rlSetTexture(t.id);
        rlBegin(RL_QUADS);
        if (st <= 0.001f) {
            piece(0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f);
        } else {
            float top = legs.top;
            piece(0.0f, 1.0f, 0.0f, top, bodyLift, 0.0f, bodyLift);                    // Overkroppen
            if (legs.mode == Legs::SWAY) {
                piece(0.0f, 1.0f, top, 1.0f, bodyLift, swing * 0.05f * s.height, 0.0f);  // Kappen svinger
            } else {
                // To bein: det ene frem og løftet, det andre bak på bakken, så bytter de
                float step = 0.075f * s.height;
                float lift = 0.05f * s.height;
                float split = legSplitX[(int)s.id];
                piece(0.0f, split, top, 1.0f, bodyLift, swing * step, fmaxf(0.0f, cosf(s.walk)) * lift * st);
                piece(split, 1.0f, top, 1.0f, bodyLift, -swing * step, fmaxf(0.0f, -cosf(s.walk)) * lift * st);
            }
        }
        rlEnd();
    }
    rlSetTexture(0);
    if (shaderLoaded) EndShaderMode();
    rlDrawRenderBatchActive();
    rlEnableBackfaceCulling();
    queue.clear();
}
