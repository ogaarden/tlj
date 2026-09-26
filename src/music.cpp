#include "music.hpp"
#include <raylib.h>
#include <cmath>
#include <cstdlib>
#include <vector>

// =====================================================================
// MUSIKK-SYNTESE
// Hvert spor er et antall takter med en akkord per takt. Fra akkorden lages
// bass og arpeggio automatisk, og melodien er skrevet inn som MIDI-noter
// (60 = C4, 62 = D4, 69 = A4 ...). 0 betyr pause.
// =====================================================================

namespace {

constexpr int RATE = 22050;
using Buffer = std::vector<float>;

float midiToHz(int note) { return 440.0f * powf(2.0f, (note - 69) / 12.0f); }
float noise() { return (float)rand() / RAND_MAX * 2.0f - 1.0f; }

enum class Instrument { PLUCK, LEAD, BELL, BASS, PAD, STAB };

// Én tone med instrumentets bølgeform, filter og volum-kurve
void addNote(Buffer& buf, float start, float length, int midi, Instrument inst, float volume) {
    if (midi <= 0) return;
    float freq = midiToHz(midi);
    float release = 0.08f;
    int from = (int)(start * RATE);
    int count = (int)((length + release) * RATE);
    float phase = 0.0f, phase2 = 0.0f, phase3 = 0.0f;
    float lp = 0.0f;
    float cutoff = inst == Instrument::PAD ? 900.0f : (inst == Instrument::BASS ? 700.0f : 3200.0f);
    float a = 1.0f - expf(-2.0f * PI * cutoff / RATE);
    for (int i = 0; i < count && from + i < (int)buf.size(); i++) {
        float t = (float)i / RATE;
        float vib = inst == Instrument::LEAD ? 1.0f + 0.004f * sinf(t * 2.0f * PI * 5.5f) : 1.0f;
        phase += freq * vib / RATE;
        phase2 += freq * 2.005f / RATE;
        phase3 += freq * 0.997f / RATE;
        float p = phase - floorf(phase);
        float saw = 2.0f * p - 1.0f;
        float sq = p < 0.5f ? 1.0f : -1.0f;
        float tri = 4.0f * fabsf(p - 0.5f) - 1.0f;
        float s = 0.0f, env = 1.0f;
        switch (inst) {
            case Instrument::PLUCK: s = 0.6f * saw + 0.4f * sq; env = expf(-t * 9.0f); break;
            case Instrument::LEAD:  s = 0.7f * sq + 0.3f * saw; env = fminf(1.0f, t / 0.01f); break;
            case Instrument::BELL:  s = sinf(2 * PI * phase) + 0.35f * sinf(2 * PI * phase2); env = expf(-t * 3.5f); break;
            case Instrument::BASS:  s = 0.8f * tri + 0.25f * sq; env = fminf(1.0f, t / 0.005f); break;
            case Instrument::PAD:   s = saw + (2.0f * (phase3 - floorf(phase3)) - 1.0f); env = fminf(1.0f, t / 0.25f); break;
            case Instrument::STAB:  s = 0.5f * sq + 0.5f * saw; env = expf(-t * 12.0f); break;
        }
        if (t > length) env *= fmaxf(0.0f, 1.0f - (t - length) / release); // Myk slutt
        lp += (s - lp) * a;
        buf[from + i] += lp * env * volume;
    }
}

void addKick(Buffer& buf, float start, float volume) {
    int from = (int)(start * RATE);
    float phase = 0.0f;
    for (int i = 0; i < (int)(0.22f * RATE) && from + i < (int)buf.size(); i++) {
        float t = (float)i / RATE;
        phase += (45.0f + 110.0f * expf(-t * 30.0f)) / RATE;
        buf[from + i] += sinf(2 * PI * phase) * expf(-t * 14.0f) * volume;
    }
}

void addSnare(Buffer& buf, float start, float volume) {
    int from = (int)(start * RATE);
    float lp = 0.0f, phase = 0.0f;
    for (int i = 0; i < (int)(0.18f * RATE) && from + i < (int)buf.size(); i++) {
        float t = (float)i / RATE;
        lp += (noise() - lp) * 0.6f;
        phase += 185.0f / RATE;
        buf[from + i] += (lp * 0.8f + sinf(2 * PI * phase) * 0.4f) * expf(-t * 22.0f) * volume;
    }
}

void addHat(Buffer& buf, float start, float volume) {
    int from = (int)(start * RATE);
    float prev = 0.0f;
    for (int i = 0; i < (int)(0.04f * RATE) && from + i < (int)buf.size(); i++) {
        float t = (float)i / RATE;
        float n = noise();
        buf[from + i] += (n - prev) * 0.5f * expf(-t * 90.0f) * volume; // Høypass: bare den skarpe delen
        prev = n;
    }
}

struct Chord { int root; bool minor; bool major7 = false; };

// Akkordtoner (grunntone, terts, kvint) rundt en gitt oktav
int chordTone(const Chord& c, int index, int octaveBase) {
    int third = c.minor ? 3 : 4;
    int steps[3] = { 0, third, 7 };
    int octave = index / 3;
    return octaveBase + (c.root % 12) + steps[index % 3] + 12 * octave;
}

// Mykt klipp og normalisering så sporet aldri skraper
void finish(Buffer& buf) {
    float peak = 0.0001f;
    for (float& s : buf) { s = tanhf(s * 1.3f); peak = fmaxf(peak, fabsf(s)); }
    for (float& s : buf) s *= 0.85f / peak;
    // Kort inn- og uttoning så loopen ikke klikker
    int fade = RATE / 50;
    for (int i = 0; i < fade && i < (int)buf.size(); i++) {
        float k = (float)i / fade;
        buf[i] *= k;
        buf[buf.size() - 1 - i] *= k;
    }
}

// ---------------------------------------------------------------------
// SPORENE
// ---------------------------------------------------------------------

// Meny: vals i d-moll (3/4), spilledåse-melodi over "umm-pa-pa"
Buffer buildMenu() {
    const float beat = 60.0f / 126.0f;
    const Chord chords[8] = { { 62, true }, { 67, true }, { 60, false }, { 65, false }, { 70, false }, { 67, true }, { 69, false }, { 62, true } };
    const int melody[8][6] = {
        { 74, 0, 77, 0, 76, 74 }, { 70, 0, 74, 0, 72, 70 }, { 72, 0, 76, 0, 74, 72 }, { 69, 0, 72, 0, 77, 76 },
        { 74, 0, 70, 0, 72, 74 }, { 70, 0, 67, 0, 70, 74 }, { 73, 0, 76, 0, 79, 76 }, { 74, 0, 0, 0, 0, 0 },
    };
    const int bars = 16;
    Buffer buf((size_t)(bars * 3 * beat * RATE), 0.0f);
    for (int bar = 0; bar < bars; bar++) {
        const Chord& c = chords[bar % 8];
        float t0 = bar * 3 * beat;
        // Umm (bass) - pa - pa (akkord)
        addNote(buf, t0, beat * 0.9f, chordTone(c, 0, 36), Instrument::BASS, 0.30f);
        for (int b = 1; b < 3; b++)
            for (int k = 0; k < 3; k++) addNote(buf, t0 + b * beat, beat * 0.5f, chordTone(c, k, 60), Instrument::PLUCK, 0.05f);
        addNote(buf, t0, 3 * beat, chordTone(c, 0, 48), Instrument::PAD, 0.035f);
        addNote(buf, t0, 3 * beat, chordTone(c, 2, 48), Instrument::PAD, 0.03f);
        // Melodi (åttendeler); andre gang en oktav høyere og litt svakere
        for (int n = 0; n < 6; n++) {
            int note = melody[bar % 8][n];
            if (note) addNote(buf, t0 + n * beat * 0.5f, beat * 0.9f, note + (bar >= 8 ? 12 : 0), Instrument::BELL, bar >= 8 ? 0.13f : 0.17f);
        }
    }
    finish(buf);
    return buf;
}

// Spill: 140 BPM i d-moll. Første halvdel arpeggio + bass, andre halvdel med melodi.
Buffer buildGame() {
    const float beat = 60.0f / 140.0f;
    const Chord chords[8] = { { 62, true }, { 70, false }, { 60, false }, { 69, true }, { 62, true }, { 70, false }, { 67, true }, { 69, false } };
    const int melody[8][8] = {
        { 69, 0, 74, 0, 77, 76, 74, 72 }, { 74, 0, 70, 0, 74, 77, 76, 74 }, { 72, 0, 76, 0, 79, 77, 76, 72 }, { 76, 0, 69, 0, 72, 76, 74, 72 },
        { 74, 77, 81, 77, 74, 0, 69, 0 }, { 70, 74, 77, 74, 70, 0, 65, 0 }, { 67, 70, 74, 79, 77, 74, 70, 67 }, { 69, 0, 73, 0, 76, 0, 79, 0 },
    };
    const int bars = 16;
    Buffer buf((size_t)(bars * 4 * beat * RATE), 0.0f);
    const int arp[8] = { 0, 1, 2, 3, 2, 1, 0, 1 };
    for (int bar = 0; bar < bars; bar++) {
        const Chord& c = chords[bar % 8];
        float t0 = bar * 4 * beat;
        for (int e = 0; e < 8; e++) {
            float t = t0 + e * beat * 0.5f;
            // Bass: grunntone med oktavhopp
            int bassNote = chordTone(c, 0, 36) + ((e % 4 == 2) ? 12 : 0);
            addNote(buf, t, beat * 0.42f, bassNote, Instrument::BASS, 0.26f);
            // Arpeggio
            addNote(buf, t, beat * 0.4f, chordTone(c, arp[e], 60), Instrument::PLUCK, 0.07f);
            // Trommer
            if (e % 2 == 0) addHat(buf, t, 0.12f);
            else addHat(buf, t, 0.07f);
        }
        for (int b = 0; b < 4; b++) {
            float t = t0 + b * beat;
            if (b == 0 || b == 2) addKick(buf, t, 0.55f);
            if (b == 1 || b == 3) addSnare(buf, t, bar >= 8 ? 0.28f : 0.18f);
        }
        if (bar % 8 == 7) addKick(buf, t0 + 3.5f * beat, 0.45f); // Liten fill før neste runde
        addNote(buf, t0, 4 * beat, chordTone(c, 1, 48), Instrument::PAD, 0.03f);
        if (bar >= 8) {
            for (int n = 0; n < 8; n++) {
                int note = melody[bar % 8][n];
                if (note) addNote(buf, t0 + n * beat * 0.5f, beat * 0.45f, note, Instrument::LEAD, 0.075f);
            }
        }
    }
    finish(buf);
    return buf;
}

// Boss: 160 BPM i c-moll, tunge trommer, pulserende bass og akkordstøt
Buffer buildBoss() {
    const float beat = 60.0f / 160.0f;
    const Chord chords[8] = { { 60, true }, { 68, false }, { 70, false }, { 67, false }, { 60, true }, { 68, false }, { 65, true }, { 67, false } };
    const int melody[8][8] = {
        { 72, 0, 72, 75, 0, 72, 79, 77 }, { 75, 0, 75, 77, 0, 75, 72, 70 }, { 74, 0, 74, 77, 0, 74, 70, 74 }, { 71, 0, 74, 0, 77, 0, 79, 0 },
        { 84, 0, 79, 0, 75, 0, 72, 0 }, { 80, 0, 75, 0, 72, 0, 68, 0 }, { 77, 0, 72, 0, 68, 0, 65, 0 }, { 67, 71, 74, 77, 79, 77, 74, 71 },
    };
    const int bars = 16;
    Buffer buf((size_t)(bars * 4 * beat * RATE), 0.0f);
    for (int bar = 0; bar < bars; bar++) {
        const Chord& c = chords[bar % 8];
        float t0 = bar * 4 * beat;
        for (int s = 0; s < 16; s++) {
            float t = t0 + s * beat * 0.25f;
            addNote(buf, t, beat * 0.2f, chordTone(c, 0, 36), Instrument::BASS, 0.22f);  // Sekstendels-puls
            addHat(buf, t, (s % 2 == 0) ? 0.10f : 0.05f);
        }
        for (int b = 0; b < 4; b++) {
            float t = t0 + b * beat;
            addKick(buf, t, 0.6f);
            if (b == 1 || b == 3) addSnare(buf, t, 0.3f);
            // Akkordstøt på slagene mellom
            for (int k = 0; k < 3; k++) addNote(buf, t + beat * 0.5f, beat * 0.25f, chordTone(c, k, 60), Instrument::STAB, 0.06f);
        }
        addNote(buf, t0, 4 * beat, chordTone(c, 0, 48), Instrument::PAD, 0.04f);
        addNote(buf, t0, 4 * beat, chordTone(c, 2, 48), Instrument::PAD, 0.035f);
        for (int n = 0; n < 8; n++) {
            int note = melody[bar % 8][n];
            if (note) addNote(buf, t0 + n * beat * 0.5f, beat * 0.45f, note, Instrument::LEAD, bar >= 8 ? 0.08f : 0.06f);
        }
    }
    finish(buf);
    return buf;
}

Sound tracks[(int)MusicTrack::COUNT] = {};
bool loaded[(int)MusicTrack::COUNT] = {};
float trackVolume[(int)MusicTrack::COUNT] = {};
MusicTrack wanted = MusicTrack::NONE;
float musicVolume = 0.6f;
bool ready = false;

Sound toSound(const Buffer& buf) {
    std::vector<short> pcm(buf.size());
    for (size_t i = 0; i < buf.size(); i++) pcm[i] = (short)(buf[i] * 32767.0f);
    ::Wave wave{};
    wave.frameCount = (unsigned int)pcm.size();
    wave.sampleRate = RATE;
    wave.sampleSize = 16;
    wave.channels = 1;
    wave.data = pcm.data();
    return LoadSoundFromWave(wave); // Kopierer dataene, så pcm kan slettes etterpå
}

} // namespace

void InitGameMusic() {
    if (!IsAudioDeviceReady()) return;
    tracks[(int)MusicTrack::MENU] = toSound(buildMenu());
    tracks[(int)MusicTrack::GAME] = toSound(buildGame());
    tracks[(int)MusicTrack::BOSS] = toSound(buildBoss());
    for (int i = 1; i < (int)MusicTrack::COUNT; i++) loaded[i] = true;
    ready = true;
}

void UnloadGameMusic() {
    for (int i = 0; i < (int)MusicTrack::COUNT; i++) {
        if (loaded[i]) UnloadSound(tracks[i]);
        loaded[i] = false;
    }
    ready = false;
}

void SetMusicTrack(MusicTrack track) { wanted = track; }

void SetMusicVolume01(float volume) { musicVolume = fmaxf(0.0f, fminf(1.0f, volume)); }

void UpdateGameMusic(float deltaTime) {
    if (!ready) return;
    for (int i = 1; i < (int)MusicTrack::COUNT; i++) {
        // Krysstoning: det ønskede sporet tones opp, de andre ned (ca. 1 sekund)
        float target = (i == (int)wanted) ? 1.0f : 0.0f;
        float step = deltaTime * 1.2f;
        trackVolume[i] += fmaxf(-step, fminf(step, target - trackVolume[i]));

        if (trackVolume[i] > 0.001f) {
            if (!IsSoundPlaying(tracks[i])) PlaySound(tracks[i]); // Starter på nytt = loop
            SetSoundVolume(tracks[i], trackVolume[i] * musicVolume * 0.55f);
        } else if (IsSoundPlaying(tracks[i])) {
            StopSound(tracks[i]);
        }
    }
}
