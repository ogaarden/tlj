#include "audio.hpp"
#include <raylib.h>
#include <cmath>
#include <cstdlib>
#include <vector>

// =====================================================================
// LYDSYNTESE
// Hver lyd bygges som en liste med samples (-1.0 til 1.0) og gjøres om
// til en raylib-Sound. Byggeklossene er enkle bølgeformer (sinus, firkant,
// trekant, støy) med en volum-kurve (envelope) og frekvens-sweep.
// =====================================================================

namespace {

constexpr int SAMPLE_RATE = 44100;
constexpr int VOICES = 6; // Hvor mange av samme lyd som kan spilles samtidig

enum class Wave { SINE, SQUARE, TRIANGLE, SAW, NOISE };

using Buffer = std::vector<float>;

float randf() { return (float)rand() / (float)RAND_MAX * 2.0f - 1.0f; }

Buffer makeBuffer(float seconds) { return Buffer((size_t)(seconds * SAMPLE_RATE), 0.0f); }

float oscillator(Wave wave, float phase) {
    float p = phase - floorf(phase); // 0..1
    switch (wave) {
        case Wave::SINE:     return sinf(2.0f * PI * p);
        case Wave::SQUARE:   return p < 0.5f ? 1.0f : -1.0f;
        case Wave::TRIANGLE: return 4.0f * fabsf(p - 0.5f) - 1.0f;
        case Wave::SAW:      return 2.0f * p - 1.0f;
        case Wave::NOISE:    return randf();
    }
    return 0.0f;
}

// Legger til en tone i bufferen: frekvens glir fra f0 til f1, med rask attack og eksponentiell decay
void addTone(Buffer& buf, float start, float duration, float f0, float f1, Wave wave, float volume,
             float attack = 0.005f, float decay = 6.0f) {
    int from = (int)(start * SAMPLE_RATE);
    int count = (int)(duration * SAMPLE_RATE);
    float phase = 0.0f;
    for (int i = 0; i < count && from + i < (int)buf.size(); i++) {
        float t = (float)i / SAMPLE_RATE;
        float progress = t / duration;
        float freq = f0 + (f1 - f0) * progress;
        phase += freq / SAMPLE_RATE;

        float env = (t < attack) ? t / attack : expf(-decay * (t - attack) / duration);
        env *= 1.0f - progress * progress; // Myk slutt så det ikke klikker
        buf[from + i] += oscillator(wave, phase) * env * volume;
    }
}

// Støy som er filtrert (lavpass) – 0.0 = bare dyp rumling, 1.0 = skarp susing
void addNoise(Buffer& buf, float start, float duration, float brightness, float volume, float decay = 5.0f) {
    int from = (int)(start * SAMPLE_RATE);
    int count = (int)(duration * SAMPLE_RATE);
    float filtered = 0.0f;
    for (int i = 0; i < count && from + i < (int)buf.size(); i++) {
        float t = (float)i / SAMPLE_RATE;
        float progress = t / duration;
        filtered += (randf() - filtered) * brightness;
        float env = expf(-decay * progress) * (1.0f - progress);
        buf[from + i] += filtered * env * volume;
    }
}

// Knitring: korte tilfeldige smell (til lyn)
void addCrackle(Buffer& buf, float start, float duration, float volume) {
    int from = (int)(start * SAMPLE_RATE);
    int count = (int)(duration * SAMPLE_RATE);
    for (int i = 0; i < count && from + i < (int)buf.size(); i++) {
        float progress = (float)i / count;
        if (rand() % 90 == 0) {
            int len = 40 + rand() % 200;
            float amp = volume * (1.0f - progress) * (0.5f + 0.5f * randf());
            for (int k = 0; k < len && from + i + k < (int)buf.size(); k++) {
                buf[from + i + k] += randf() * amp * (1.0f - (float)k / len);
            }
        }
    }
}

// Gjør bufferen om til en raylib-Wave (16-bit mono)
::Wave toWave(const Buffer& buf) {
    // Normaliser hvis vi har klippet, og gjør om til 16-bit
    float peak = 0.0f;
    for (float v : buf) peak = fmaxf(peak, fabsf(v));
    float scale = (peak > 1.0f) ? 1.0f / peak : 1.0f;

    short* data = (short*)MemAlloc((unsigned int)(buf.size() * sizeof(short)));
    for (size_t i = 0; i < buf.size(); i++) data[i] = (short)(buf[i] * scale * 32000.0f);

    ::Wave wave{};
    wave.frameCount = (unsigned int)buf.size();
    wave.sampleRate = SAMPLE_RATE;
    wave.sampleSize = 16;
    wave.channels = 1;
    wave.data = data;
    return wave;
}

// --- Selve lydene ---

Buffer buildSound(Sfx sfx) {
    switch (sfx) {
        case Sfx::XP: {
            // Lys, stigende "blipp"
            Buffer b = makeBuffer(0.09f);
            addTone(b, 0.0f, 0.09f, 900.0f, 1500.0f, Wave::SINE, 0.6f, 0.002f, 4.0f);
            addTone(b, 0.0f, 0.06f, 1800.0f, 3000.0f, Wave::SINE, 0.15f, 0.002f, 5.0f);
            return b;
        }
        case Sfx::COIN: {
            // Klassisk to-tone mynt: B5 -> E6
            Buffer b = makeBuffer(0.30f);
            addTone(b, 0.0f, 0.06f, 988.0f, 988.0f, Wave::SQUARE, 0.25f, 0.001f, 1.0f);
            addTone(b, 0.06f, 0.24f, 1319.0f, 1319.0f, Wave::SQUARE, 0.25f, 0.001f, 4.0f);
            return b;
        }
        case Sfx::HIT: {
            // Kort "tikk"
            Buffer b = makeBuffer(0.05f);
            addNoise(b, 0.0f, 0.04f, 0.8f, 0.5f, 8.0f);
            addTone(b, 0.0f, 0.04f, 500.0f, 250.0f, Wave::TRIANGLE, 0.4f, 0.001f, 6.0f);
            return b;
        }
        case Sfx::KILL: {
            // "Pop" med dump klang
            Buffer b = makeBuffer(0.16f);
            addTone(b, 0.0f, 0.15f, 240.0f, 60.0f, Wave::SINE, 0.9f, 0.001f, 4.0f);
            addNoise(b, 0.0f, 0.06f, 0.5f, 0.5f, 6.0f);
            return b;
        }
        case Sfx::PLAYER_HURT: {
            // Hard, lav "au"
            Buffer b = makeBuffer(0.24f);
            addTone(b, 0.0f, 0.22f, 190.0f, 95.0f, Wave::SQUARE, 0.35f, 0.002f, 3.0f);
            addNoise(b, 0.0f, 0.10f, 0.35f, 0.5f, 5.0f);
            return b;
        }
        case Sfx::LEVEL_UP: {
            // Stigende arpeggio C5 E5 G5 C6
            Buffer b = makeBuffer(0.65f);
            const float notes[] = { 523.3f, 659.3f, 784.0f, 1046.5f };
            for (int i = 0; i < 4; i++) {
                float len = (i == 3) ? 0.40f : 0.12f;
                addTone(b, i * 0.08f, len, notes[i], notes[i], Wave::TRIANGLE, 0.55f, 0.003f, i == 3 ? 3.0f : 2.0f);
                addTone(b, i * 0.08f, len, notes[i] * 2.0f, notes[i] * 2.0f, Wave::SINE, 0.12f, 0.003f, 4.0f);
            }
            return b;
        }
        case Sfx::THUNDER: {
            // Skarpt smell, knitring og dyp rumling
            Buffer b = makeBuffer(0.9f);
            addNoise(b, 0.0f, 0.12f, 0.9f, 0.8f, 7.0f);
            addCrackle(b, 0.0f, 0.35f, 0.7f);
            addNoise(b, 0.02f, 0.85f, 0.03f, 1.6f, 3.0f);
            addTone(b, 0.0f, 0.7f, 70.0f, 40.0f, Wave::SINE, 0.5f, 0.01f, 3.0f);
            return b;
        }
        case Sfx::ZAP: {
            // Elektrisk "bzzt"
            Buffer b = makeBuffer(0.08f);
            addTone(b, 0.0f, 0.07f, 1400.0f, 500.0f, Wave::SAW, 0.3f, 0.001f, 3.0f);
            addNoise(b, 0.0f, 0.06f, 0.9f, 0.25f, 4.0f);
            return b;
        }
        case Sfx::EXPLOSION: {
            // Dyp buldrende støy med et smell i starten
            Buffer b = makeBuffer(0.7f);
            addNoise(b, 0.0f, 0.08f, 0.7f, 0.8f, 5.0f);
            addNoise(b, 0.0f, 0.7f, 0.05f, 2.0f, 4.0f);
            addTone(b, 0.0f, 0.4f, 110.0f, 35.0f, Wave::SINE, 0.8f, 0.002f, 3.0f);
            return b;
        }
        case Sfx::BOSS_GONG: {
            // Dyp gong: flere nesten-harmoniske sinuser som klinger lenge
            Buffer b = makeBuffer(2.6f);
            addTone(b, 0.0f, 2.6f, 98.0f, 97.0f, Wave::SINE, 0.7f, 0.004f, 3.0f);
            addTone(b, 0.0f, 2.2f, 196.0f * 1.01f, 196.0f, Wave::SINE, 0.35f, 0.004f, 4.0f);
            addTone(b, 0.0f, 1.6f, 294.0f * 0.98f, 294.0f, Wave::SINE, 0.25f, 0.004f, 5.0f);
            addTone(b, 0.0f, 1.0f, 523.0f * 1.03f, 523.0f, Wave::SINE, 0.15f, 0.004f, 6.0f);
            addNoise(b, 0.0f, 0.05f, 0.4f, 0.4f, 5.0f);
            return b;
        }
        case Sfx::BOSS_CHARGE: {
            // Stigende, truende brøl
            Buffer b = makeBuffer(0.8f);
            addTone(b, 0.0f, 0.8f, 110.0f, 260.0f, Wave::SAW, 0.35f, 0.15f, 0.5f);
            addTone(b, 0.0f, 0.8f, 55.0f, 130.0f, Wave::SQUARE, 0.2f, 0.15f, 0.5f);
            addNoise(b, 0.0f, 0.8f, 0.15f, 0.3f, 1.0f);
            return b;
        }
        case Sfx::VICTORY: {
            // Fanfare G4 C5 E5 G5 ... C6
            Buffer b = makeBuffer(1.4f);
            const float notes[] = { 392.0f, 523.3f, 659.3f, 784.0f };
            for (int i = 0; i < 4; i++) {
                addTone(b, i * 0.12f, 0.14f, notes[i], notes[i], Wave::SQUARE, 0.22f, 0.003f, 1.5f);
            }
            addTone(b, 0.5f, 0.9f, 1046.5f, 1046.5f, Wave::SQUARE, 0.22f, 0.003f, 2.5f);
            addTone(b, 0.5f, 0.9f, 523.3f, 523.3f, Wave::TRIANGLE, 0.35f, 0.003f, 2.5f);
            return b;
        }
        case Sfx::DEATH: {
            // Synkende, trist tone med vibrato-følelse
            Buffer b = makeBuffer(1.1f);
            addTone(b, 0.0f, 0.35f, 440.0f, 392.0f, Wave::TRIANGLE, 0.5f, 0.005f, 1.0f);
            addTone(b, 0.3f, 0.35f, 370.0f, 330.0f, Wave::TRIANGLE, 0.5f, 0.005f, 1.0f);
            addTone(b, 0.6f, 0.5f, 294.0f, 196.0f, Wave::TRIANGLE, 0.5f, 0.005f, 2.0f);
            return b;
        }
        case Sfx::UI_MOVE: {
            Buffer b = makeBuffer(0.04f);
            addTone(b, 0.0f, 0.035f, 1100.0f, 1100.0f, Wave::SINE, 0.4f, 0.001f, 6.0f);
            return b;
        }
        case Sfx::UI_SELECT: {
            Buffer b = makeBuffer(0.12f);
            addTone(b, 0.0f, 0.05f, 700.0f, 700.0f, Wave::TRIANGLE, 0.5f, 0.001f, 2.0f);
            addTone(b, 0.05f, 0.07f, 1050.0f, 1050.0f, Wave::TRIANGLE, 0.5f, 0.001f, 3.0f);
            return b;
        }
        case Sfx::COUNT: break;
    }
    return makeBuffer(0.01f);
}

// Hvor høyt, hvor ofte og hvor mye pitch-variasjon hver lyd skal ha
struct SfxSettings {
    float volume;
    float minInterval;  // Minste tid mellom to avspillinger (hindrer "lydvegg" ved mange samtidige)
    float pitchJitter;  // Tilfeldig pitch-variasjon (+/-)
};

SfxSettings settingsFor(Sfx sfx) {
    switch (sfx) {
        case Sfx::XP:          return { 0.35f, 0.035f, 0.08f };
        case Sfx::COIN:        return { 0.45f, 0.05f, 0.03f };
        case Sfx::HIT:         return { 0.25f, 0.045f, 0.15f };
        case Sfx::KILL:        return { 0.40f, 0.035f, 0.15f };
        case Sfx::PLAYER_HURT: return { 0.60f, 0.10f, 0.05f };
        case Sfx::LEVEL_UP:    return { 0.60f, 0.20f, 0.0f };
        case Sfx::THUNDER:     return { 0.55f, 0.08f, 0.10f };
        case Sfx::ZAP:         return { 0.30f, 0.03f, 0.20f };
        case Sfx::EXPLOSION:   return { 0.65f, 0.06f, 0.10f };
        case Sfx::BOSS_GONG:   return { 0.85f, 1.00f, 0.0f };
        case Sfx::BOSS_CHARGE: return { 0.55f, 0.50f, 0.0f };
        case Sfx::VICTORY:     return { 0.70f, 1.00f, 0.0f };
        case Sfx::DEATH:       return { 0.70f, 1.00f, 0.0f };
        case Sfx::UI_MOVE:     return { 0.35f, 0.03f, 0.0f };
        case Sfx::UI_SELECT:   return { 0.45f, 0.05f, 0.0f };
        case Sfx::COUNT: break;
    }
    return { 0.5f, 0.05f, 0.0f };
}

struct SfxSlot {
    std::vector<Sound> voices; // Flere kopier av samme lyd, så den kan spilles oppå seg selv
    int nextVoice = 0;
    double lastPlayed = -100.0;
    SfxSettings settings{};
};

bool audioReady = false;
float masterVolume = 0.7f;
SfxSlot slots[(int)Sfx::COUNT];

} // namespace

void InitGameAudio() {
    InitAudioDevice();
    audioReady = IsAudioDeviceReady();
    if (!audioReady) return; // Ingen lydenhet – spillet fungerer fortsatt, bare uten lyd

    SetMasterVolume(masterVolume);
    for (int i = 0; i < (int)Sfx::COUNT; i++) {
        Sfx sfx = (Sfx)i;
        SfxSlot& slot = slots[i];
        slot.settings = settingsFor(sfx);
        ::Wave wave = toWave(buildSound(sfx));
        for (int v = 0; v < VOICES; v++) slot.voices.push_back(LoadSoundFromWave(wave));
        UnloadWave(wave);
    }
}

void UnloadGameAudio() {
    if (!audioReady) return;
    for (auto& slot : slots) {
        for (Sound& v : slot.voices) UnloadSound(v);
        slot.voices.clear();
    }
    CloseAudioDevice();
    audioReady = false;
}

void PlaySfx(Sfx sfx) {
    if (!audioReady || sfx == Sfx::COUNT) return;
    SfxSlot& slot = slots[(int)sfx];

    double now = GetTime();
    if (now - slot.lastPlayed < slot.settings.minInterval) return;
    slot.lastPlayed = now;

    Sound& voice = slot.voices[slot.nextVoice];
    slot.nextVoice = (slot.nextVoice + 1) % (int)slot.voices.size();

    SetSoundVolume(voice, slot.settings.volume);
    SetSoundPitch(voice, 1.0f + slot.settings.pitchJitter * randf());
    PlaySound(voice);
}

void PlaySfxPitch(Sfx sfx, float pitch) {
    if (!audioReady || sfx == Sfx::COUNT) return;
    SfxSlot& slot = slots[(int)sfx];
    double now = GetTime();
    if (now - slot.lastPlayed < slot.settings.minInterval) return;
    slot.lastPlayed = now;
    Sound& voice = slot.voices[slot.nextVoice];
    slot.nextVoice = (slot.nextVoice + 1) % (int)slot.voices.size();
    SetSoundVolume(voice, slot.settings.volume);
    SetSoundPitch(voice, pitch);
    PlaySound(voice);
}

void SetGameVolume(float volume) {
    masterVolume = fmaxf(0.0f, fminf(1.0f, volume));
    if (audioReady) SetMasterVolume(masterVolume);
}

float GetGameVolume() {
    return masterVolume;
}
