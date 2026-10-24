// sound.c - sound effects, synthesized at startup (no audio files, so the web build needs nothing extra)
#include "sound.h"
#include "raylib.h"
#include <math.h>
#include <stdlib.h>

#define RATE 44100
#define PI_F 3.14159265f
#define FLICKS 4                 // aliases of the deal sound, so quick flicks can overlap

static bool ready = false, muted = false;
static Sound sDeal, sDealAlias[FLICKS], sWin, sLose;
static int nextFlick = 0, queued = 0;
static float flickTimer = 0;
static unsigned noiseState = 0x1234567u;

static float Noise(void) {       // white noise in [-1, 1]
    noiseState = noiseState * 1664525u + 1013904223u;
    return (float)(noiseState >> 8) / (float)(1u << 23) - 1.0f;
}

static float *NewBuf(float seconds, int *n) {
    *n = (int)(seconds * RATE);
    return calloc((size_t)*n, sizeof(float));
}

// Normalizes to the given peak and converts to a 16-bit mono Wave that owns its data.
static Wave ToWave(float *buf, int n, float peak) {
    float mx = 1e-6f;
    for (int i = 0; i < n; i++) if (fabsf(buf[i]) > mx) mx = fabsf(buf[i]);
    short *pcm = malloc((size_t)n * sizeof(short));
    for (int i = 0; i < n; i++) pcm[i] = (short)(buf[i] / mx * peak * 32767.0f);
    free(buf);
    return (Wave){ .frameCount = (unsigned)n, .sampleRate = RATE, .sampleSize = 16, .channels = 1, .data = pcm };
}

// A card sliding off the deck and landing on the felt: a short burst of band-passed noise
// with a soft low thump at the end.
static Wave MakeDeal(void) {
    int n;
    float *b = NewBuf(0.09f, &n);
    float lp = 0, hp = 0, prev = 0;
    for (int i = 0; i < n; i++) {
        float t = (float)i / RATE;
        float x = Noise();
        lp += 0.35f * (x - lp);              // low-pass, then high-pass: a papery band around 2-5 kHz
        hp = 0.8f * (hp + lp - prev);
        prev = lp;
        float env = t < 0.004f ? t / 0.004f : expf(-(t - 0.004f) * 55.0f);
        float thump = t > 0.035f ? sinf(2 * PI_F * 140 * t) * expf(-(t - 0.035f) * 90.0f) * 0.35f : 0;
        b[i] = hp * env + thump;
    }
    return ToWave(b, n, 0.55f);
}

// A cheer: a bright major fanfare over a crowd roar (noise in the voice band, swelling and
// fluttering like many people shouting at once).
static Wave MakeWin(void) {
    int n;
    float *b = NewBuf(1.9f, &n);
    static const float notes[4] = { 523.25f, 659.25f, 783.99f, 1046.5f };   // C5 E5 G5 C6
    static const float start[4] = { 0.0f, 0.11f, 0.22f, 0.33f };
    float bp1 = 0, bp2 = 0;                  // state-variable band-pass for the crowd
    for (int i = 0; i < n; i++) {
        float t = (float)i / RATE, s = 0;
        for (int k = 0; k < 4; k++) {
            float u = t - start[k];
            if (u < 0) continue;
            float len = k == 3 ? 1.1f : 0.5f;
            float env = (u < 0.01f ? u / 0.01f : 1) * expf(-u * (k == 3 ? 2.2f : 5.0f)) * (u < len ? 1 : 0);
            float ph = 2 * PI_F * notes[k] * u + (k == 3 ? 0.08f * sinf(2 * PI_F * 5.5f * u) : 0);
            s += env * (0.6f * sinf(ph) + 0.25f * sinf(2 * ph) + 0.12f * sinf(3 * ph));
        }
        float f = 2 * sinf(PI_F * 900.0f / RATE), q = 0.9f;
        float x = Noise();
        bp1 += f * bp2;
        float hi = x - bp1 - q * bp2;
        bp2 += f * hi;
        float swell = t < 0.35f ? t / 0.35f : expf(-(t - 0.35f) * 1.6f);
        float flutter = 0.65f + 0.2f * sinf(2 * PI_F * 6.3f * t) + 0.15f * sinf(2 * PI_F * 9.7f * t + 1.3f);
        b[i] = 0.55f * s + 0.9f * bp2 * swell * flutter;
    }
    return ToWave(b, n, 0.75f);
}

// A losing "wah wah wah waaah": four falling notes on a muted brass tone, the last one wobbling.
static Wave MakeLose(void) {
    int n;
    float *b = NewBuf(2.1f, &n);
    static const float notes[4] = { 293.66f, 277.18f, 261.63f, 246.94f };   // D4 C#4 C4 B3
    static const float start[4] = { 0.0f, 0.38f, 0.76f, 1.14f };
    static const float len[4] = { 0.34f, 0.34f, 0.34f, 0.95f };
    float lp = 0, ph = 0;
    for (int i = 0; i < n; i++) {
        float t = (float)i / RATE, s = 0, freq = 0, env = 0;
        for (int k = 0; k < 4; k++) {
            float u = t - start[k];
            if (u < 0 || u > len[k]) continue;
            freq = notes[k] * (k == 3 ? 1 + 0.025f * sinf(2 * PI_F * 6.0f * u) * fminf(u / 0.3f, 1) : 1);
            env = fminf(u / 0.04f, 1) * fminf((len[k] - u) / 0.06f, 1);
            if (k == 3) env *= 1 - 0.4f * u / len[k];
        }
        if (freq > 0) {
            ph += freq / RATE;
            ph -= floorf(ph);
            s = (2 * ph - 1) * env;              // sawtooth, brassy
        }
        lp += (0.06f + 0.10f * env) * (s - lp);  // the "wah": the mute opens as each note swells
        b[i] = lp;
    }
    return ToWave(b, n, 0.38f);
}

void sndInit(void) {
    InitAudioDevice();
    if (!IsAudioDeviceReady()) return;
    Wave w = MakeDeal();
    sDeal = LoadSoundFromWave(w);
    UnloadWave(w);
    for (int i = 0; i < FLICKS; i++) sDealAlias[i] = LoadSoundAlias(sDeal);
    w = MakeWin();
    sWin = LoadSoundFromWave(w);
    UnloadWave(w);
    w = MakeLose();
    sLose = LoadSoundFromWave(w);
    UnloadWave(w);
    ready = true;
}

void sndClose(void) {
    if (!ready) return;
    for (int i = 0; i < FLICKS; i++) UnloadSoundAlias(sDealAlias[i]);
    UnloadSound(sDeal);
    UnloadSound(sWin);
    UnloadSound(sLose);
    CloseAudioDevice();
    ready = false;
}

void sndUpdate(float dt) {
    if (!ready || queued <= 0) return;
    flickTimer -= dt;
    if (flickTimer > 0) return;
    Sound s = sDealAlias[nextFlick];
    nextFlick = (nextFlick + 1) % FLICKS;
    SetSoundPitch(s, 0.92f + 0.16f * (float)GetRandomValue(0, 100) / 100.0f);   // no two flicks identical
    PlaySound(s);
    queued--;
    flickTimer = 0.075f;
}

void sndDeal(int cards) {
    if (!ready || muted || cards <= 0) return;
    if (queued == 0) flickTimer = 0;
    queued += cards;
}

void sndWin(void) {
    if (!ready || muted) return;
    StopSound(sLose);
    PlaySound(sWin);
}

void sndLose(void) {
    if (!ready || muted) return;
    StopSound(sWin);
    PlaySound(sLose);
}

void sndSetMuted(bool m) {
    muted = m;
    if (!m || !ready) return;
    queued = 0;
    StopSound(sWin);
    StopSound(sLose);
}

bool sndMuted(void) { return muted; }
