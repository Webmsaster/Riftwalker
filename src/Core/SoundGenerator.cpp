#include "SoundGenerator.h"
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

SoundGenerator::Sample SoundGenerator::generate(float duration) {
    Sample s;
    s.sampleRate = 44100;
    s.data.resize(static_cast<size_t>(duration * s.sampleRate), 0);
    return s;
}

void SoundGenerator::addSine(Sample& s, float freq, float startVol, float endVol,
                              float startTime, float endTime) {
    if (endTime < 0) endTime = static_cast<float>(s.data.size()) / s.sampleRate;
    int startSamp = static_cast<int>(startTime * s.sampleRate);
    int endSamp = static_cast<int>(endTime * s.sampleRate);
    endSamp = std::min(endSamp, static_cast<int>(s.data.size()));
    if (endSamp <= startSamp) return;

    for (int i = startSamp; i < endSamp; i++) {
        float t = static_cast<float>(i - startSamp) / (endSamp - startSamp);
        float vol = startVol + (endVol - startVol) * t;
        float sample = std::sin(2.0f * static_cast<float>(M_PI) * freq * i / s.sampleRate);
        s.data[i] = static_cast<Sint16>(std::clamp(
            static_cast<int>(s.data[i] + sample * vol * 32767), -32767, 32767));
    }
}

void SoundGenerator::addSquare(Sample& s, float freq, float startVol, float endVol,
                                float startTime, float endTime) {
    if (endTime < 0) endTime = static_cast<float>(s.data.size()) / s.sampleRate;
    int startSamp = static_cast<int>(startTime * s.sampleRate);
    int endSamp = static_cast<int>(endTime * s.sampleRate);
    endSamp = std::min(endSamp, static_cast<int>(s.data.size()));
    if (endSamp <= startSamp) return;

    // Band-limited square: sum of odd harmonics instead of a hard ±1 step.
    // A naive square has infinite harmonics that alias into harsh, buzzy
    // digital "grit". Capping to odd harmonics below ~Nyquist keeps it warm
    // and clean. 4/pi normalizes the partial sum toward unity amplitude.
    const float nyquist = s.sampleRate * 0.45f;
    const float twoPi = 2.0f * static_cast<float>(M_PI);
    for (int i = startSamp; i < endSamp; i++) {
        float t = static_cast<float>(i - startSamp) / (endSamp - startSamp);
        float vol = startVol + (endVol - startVol) * t;
        float theta = twoPi * freq * i / s.sampleRate;
        float sample = 0.0f;
        for (int h = 1; h <= 13 && h * freq < nyquist; h += 2) {
            sample += std::sin(h * theta) / h;
        }
        sample *= 4.0f / static_cast<float>(M_PI);
        s.data[i] = static_cast<Sint16>(std::clamp(
            static_cast<int>(s.data[i] + sample * vol * 32767), -32767, 32767));
    }
}

void SoundGenerator::addNoise(Sample& s, float startVol, float endVol,
                               float startTime, float endTime) {
    if (endTime < 0) endTime = static_cast<float>(s.data.size()) / s.sampleRate;
    int startSamp = static_cast<int>(startTime * s.sampleRate);
    int endSamp = static_cast<int>(endTime * s.sampleRate);
    endSamp = std::min(endSamp, static_cast<int>(s.data.size()));
    if (endSamp <= startSamp) return;

    // One-pole low-pass on the white noise. Raw full-spectrum noise is the
    // hissy/harsh component; rolling off the top octaves turns it into softer
    // "air"/whoosh that sits better under impacts. makeup gain compensates
    // the amplitude the filter removes.
    const float lpCoeff = 0.45f;   // lower = darker
    const float makeup  = 1.5f;
    float lp = 0.0f;
    for (int i = startSamp; i < endSamp; i++) {
        float t = static_cast<float>(i - startSamp) / (endSamp - startSamp);
        float vol = startVol + (endVol - startVol) * t;
        float white = (std::rand() / static_cast<float>(RAND_MAX)) * 2.0f - 1.0f;
        lp += lpCoeff * (white - lp);
        float sample = lp * makeup;
        s.data[i] = static_cast<Sint16>(std::clamp(
            static_cast<int>(s.data[i] + sample * vol * 32767), -32767, 32767));
    }
}

void SoundGenerator::addSweep(Sample& s, float startFreq, float endFreq,
                               float startVol, float endVol,
                               float startTime, float endTime) {
    if (endTime < 0) endTime = static_cast<float>(s.data.size()) / s.sampleRate;
    int startSamp = static_cast<int>(startTime * s.sampleRate);
    int endSamp = static_cast<int>(endTime * s.sampleRate);
    endSamp = std::min(endSamp, static_cast<int>(s.data.size()));
    if (endSamp <= startSamp) return;

    float phase = 0;
    for (int i = startSamp; i < endSamp; i++) {
        float t = static_cast<float>(i - startSamp) / (endSamp - startSamp);
        float vol = startVol + (endVol - startVol) * t;
        float freq = startFreq + (endFreq - startFreq) * t;
        phase += freq / s.sampleRate;
        float sample = std::sin(2.0f * static_cast<float>(M_PI) * phase);
        s.data[i] = static_cast<Sint16>(std::clamp(
            static_cast<int>(s.data[i] + sample * vol * 32767), -32767, 32767));
    }
}

void SoundGenerator::addTriangle(Sample& s, float freq, float startVol, float endVol,
                                  float startTime, float endTime) {
    if (endTime < 0) endTime = static_cast<float>(s.data.size()) / s.sampleRate;
    int startSamp = static_cast<int>(startTime * s.sampleRate);
    int endSamp = static_cast<int>(endTime * s.sampleRate);
    endSamp = std::min(endSamp, static_cast<int>(s.data.size()));
    if (endSamp <= startSamp) return;

    for (int i = startSamp; i < endSamp; i++) {
        float t = static_cast<float>(i - startSamp) / (endSamp - startSamp);
        float vol = startVol + (endVol - startVol) * t;
        float phase = std::fmod(freq * i / s.sampleRate, 1.0f);
        float sample = 4.0f * std::abs(phase - 0.5f) - 1.0f; // Triangle wave
        s.data[i] = static_cast<Sint16>(std::clamp(
            static_cast<int>(s.data[i] + sample * vol * 32767), -32767, 32767));
    }
}

Mix_Chunk* SoundGenerator::toChunk(const Sample& s) {
    // SDL_mixer is opened as stereo (channels=2, AUDIO_S16SYS). Mix_QuickLoad_RAW
    // does NOT convert formats — the raw buffer is treated as being already in
    // the mixer's output format. We generate mono samples, so we must interleave
    // each mono sample into an L/R stereo pair before handing the buffer off.
    //
    // Before fix: mono buffer treated as stereo pairs caused ambient loops and
    // procedural music layers to play at double pitch and half duration.
    const size_t monoSamples = s.data.size();
    const size_t bufSize = monoSamples * 2 * sizeof(Sint16); // stereo interleave
    Uint8* buf = static_cast<Uint8*>(SDL_malloc(bufSize));
    if (!buf) return nullptr;
    Sint16* out = reinterpret_cast<Sint16*>(buf);

    // Anti-click envelope: a few ms of fade at the very start and end removes
    // the pop you get when a waveform begins/ends on a non-zero amplitude —
    // the single biggest source of "cheap" procedural-audio harshness.
    // Skipped for long (>1.2s) music/ambient loops so their loop seam stays
    // seamless (a fade there would pulse on every repeat).
    const float duration = static_cast<float>(monoSamples) / s.sampleRate;
    const bool declick = duration <= 1.2f;
    int atk = 0, rel = 0;
    if (declick) {
        const int quarter = static_cast<int>(monoSamples / 4);
        atk = std::min(quarter, static_cast<int>(0.004f * s.sampleRate)); //  4 ms
        rel = std::min(quarter, static_cast<int>(0.012f * s.sampleRate)); // 12 ms
    }

    for (size_t i = 0; i < monoSamples; i++) {
        float env = 1.0f;
        if (declick) {
            const int fromEnd = static_cast<int>(monoSamples - 1 - i);
            if (static_cast<int>(i) < atk)      env = static_cast<float>(i) / atk;
            else if (fromEnd < rel)             env = static_cast<float>(fromEnd) / rel;
        }
        const Sint16 sample = static_cast<Sint16>(std::lround(s.data[i] * env));
        out[i * 2]     = sample; // L
        out[i * 2 + 1] = sample; // R
    }

    Mix_Chunk* chunk = Mix_QuickLoad_RAW(buf, static_cast<Uint32>(bufSize));
    if (chunk) {
        chunk->allocated = 1; // SDL will free the buffer
    } else {
        SDL_free(buf);
    }
    return chunk;
}

// ---- Waveform primitives ----

Mix_Chunk* SoundGenerator::sineWave(float freq, float duration, int volume) {
    auto s = generate(duration);
    addSine(s, freq, volume / 128.0f, 0, 0, duration);
    return toChunk(s);
}

Mix_Chunk* SoundGenerator::squareWave(float freq, float duration, int volume) {
    auto s = generate(duration);
    addSquare(s, freq, volume / 128.0f, 0, 0, duration);
    return toChunk(s);
}

Mix_Chunk* SoundGenerator::noise(float duration, int volume) {
    auto s = generate(duration);
    addNoise(s, volume / 128.0f, 0, 0, duration);
    return toChunk(s);
}
