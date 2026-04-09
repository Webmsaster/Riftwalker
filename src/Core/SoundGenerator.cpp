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

    for (int i = startSamp; i < endSamp; i++) {
        float t = static_cast<float>(i - startSamp) / (endSamp - startSamp);
        float vol = startVol + (endVol - startVol) * t;
        float phase = std::fmod(freq * i / s.sampleRate, 1.0f);
        float sample = phase < 0.5f ? 1.0f : -1.0f;
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

    for (int i = startSamp; i < endSamp; i++) {
        float t = static_cast<float>(i - startSamp) / (endSamp - startSamp);
        float vol = startVol + (endVol - startVol) * t;
        float sample = (std::rand() / static_cast<float>(RAND_MAX)) * 2.0f - 1.0f;
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
    for (size_t i = 0; i < monoSamples; i++) {
        const Sint16 sample = s.data[i];
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
