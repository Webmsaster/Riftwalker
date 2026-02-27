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

Mix_Chunk* SoundGenerator::toChunk(const Sample& s) {
    // Convert to SDL audio format (signed 16-bit, mono)
    size_t bufSize = s.data.size() * sizeof(Sint16);
    Uint8* buf = static_cast<Uint8*>(SDL_malloc(bufSize));
    if (!buf) return nullptr;
    std::memcpy(buf, s.data.data(), bufSize);

    Mix_Chunk* chunk = Mix_QuickLoad_RAW(buf, static_cast<Uint32>(bufSize));
    if (chunk) {
        chunk->allocated = 1; // SDL will free the buffer
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

// ---- Game-specific sounds ----

Mix_Chunk* SoundGenerator::playerJump() {
    auto s = generate(0.15f);
    addSweep(s, 200, 600, 0.25f, 0.0f);
    return toChunk(s);
}

Mix_Chunk* SoundGenerator::playerDash() {
    auto s = generate(0.18f);
    addSweep(s, 150, 800, 0.3f, 0.0f);
    addNoise(s, 0.15f, 0.0f);
    return toChunk(s);
}

Mix_Chunk* SoundGenerator::playerLand() {
    auto s = generate(0.08f);
    addNoise(s, 0.2f, 0.0f);
    addSine(s, 80, 0.15f, 0.0f);
    return toChunk(s);
}

Mix_Chunk* SoundGenerator::meleeSwing() {
    auto s = generate(0.12f);
    addSweep(s, 400, 100, 0.3f, 0.0f);
    addNoise(s, 0.15f, 0.0f);
    return toChunk(s);
}

Mix_Chunk* SoundGenerator::meleeHit() {
    auto s = generate(0.15f);
    addNoise(s, 0.35f, 0.0f, 0, 0.05f);
    addSine(s, 150, 0.25f, 0.0f, 0, 0.15f);
    addSquare(s, 80, 0.15f, 0.0f, 0, 0.1f);
    return toChunk(s);
}

Mix_Chunk* SoundGenerator::rangedShot() {
    auto s = generate(0.15f);
    addSweep(s, 800, 200, 0.25f, 0.0f);
    addNoise(s, 0.1f, 0.0f, 0, 0.05f);
    return toChunk(s);
}

Mix_Chunk* SoundGenerator::enemyHit() {
    auto s = generate(0.1f);
    addNoise(s, 0.3f, 0.0f, 0, 0.04f);
    addSine(s, 200, 0.2f, 0.0f);
    return toChunk(s);
}

Mix_Chunk* SoundGenerator::enemyDeath() {
    auto s = generate(0.3f);
    addSweep(s, 400, 50, 0.35f, 0.0f);
    addNoise(s, 0.2f, 0.0f, 0, 0.1f);
    addSquare(s, 100, 0.15f, 0.0f, 0.05f, 0.3f);
    return toChunk(s);
}

Mix_Chunk* SoundGenerator::playerHurt() {
    auto s = generate(0.2f);
    addSweep(s, 300, 100, 0.35f, 0.0f);
    addNoise(s, 0.2f, 0.05f, 0, 0.08f);
    return toChunk(s);
}

Mix_Chunk* SoundGenerator::playerDeath() {
    auto s = generate(0.6f);
    addSweep(s, 500, 30, 0.4f, 0.0f);
    addNoise(s, 0.25f, 0.0f);
    addSquare(s, 60, 0.0f, 0.2f, 0, 0.1f);
    addSquare(s, 60, 0.2f, 0.0f, 0.1f, 0.6f);
    return toChunk(s);
}

Mix_Chunk* SoundGenerator::dimensionSwitch() {
    auto s = generate(0.25f);
    addSweep(s, 200, 800, 0.2f, 0.0f, 0, 0.12f);
    addSweep(s, 800, 200, 0.2f, 0.0f, 0.12f, 0.25f);
    addNoise(s, 0.1f, 0.0f, 0.05f, 0.15f);
    return toChunk(s);
}

Mix_Chunk* SoundGenerator::riftRepair() {
    auto s = generate(0.4f);
    addSweep(s, 300, 900, 0.3f, 0.1f, 0, 0.2f);
    addSine(s, 900, 0.2f, 0.0f, 0.2f, 0.4f);
    addSine(s, 1200, 0.15f, 0.0f, 0.25f, 0.4f);
    return toChunk(s);
}

Mix_Chunk* SoundGenerator::riftFail() {
    auto s = generate(0.3f);
    addSweep(s, 400, 100, 0.3f, 0.0f);
    addNoise(s, 0.2f, 0.05f, 0, 0.15f);
    return toChunk(s);
}

Mix_Chunk* SoundGenerator::pickup() {
    auto s = generate(0.15f);
    addSine(s, 600, 0.2f, 0.0f, 0, 0.07f);
    addSine(s, 900, 0.25f, 0.0f, 0.05f, 0.15f);
    return toChunk(s);
}

Mix_Chunk* SoundGenerator::menuSelect() {
    auto s = generate(0.06f);
    addSine(s, 500, 0.15f, 0.0f);
    return toChunk(s);
}

Mix_Chunk* SoundGenerator::menuConfirm() {
    auto s = generate(0.12f);
    addSine(s, 500, 0.2f, 0.0f, 0, 0.06f);
    addSine(s, 700, 0.2f, 0.0f, 0.04f, 0.12f);
    return toChunk(s);
}

Mix_Chunk* SoundGenerator::levelComplete() {
    auto s = generate(0.5f);
    addSine(s, 400, 0.25f, 0.15f, 0, 0.12f);
    addSine(s, 500, 0.25f, 0.15f, 0.1f, 0.22f);
    addSine(s, 600, 0.25f, 0.15f, 0.2f, 0.32f);
    addSine(s, 800, 0.3f, 0.0f, 0.3f, 0.5f);
    return toChunk(s);
}

Mix_Chunk* SoundGenerator::collapseWarning() {
    auto s = generate(0.3f);
    addSquare(s, 200, 0.3f, 0.0f, 0, 0.15f);
    addSquare(s, 200, 0.3f, 0.0f, 0.15f, 0.3f);
    return toChunk(s);
}

Mix_Chunk* SoundGenerator::suitEntropyCritical() {
    auto s = generate(0.4f);
    addSquare(s, 150, 0.25f, 0.0f, 0, 0.1f);
    addSquare(s, 120, 0.25f, 0.0f, 0.15f, 0.25f);
    addNoise(s, 0.15f, 0.0f, 0.1f, 0.4f);
    return toChunk(s);
}

Mix_Chunk* SoundGenerator::spikeDamage() {
    auto s = generate(0.15f);
    addNoise(s, 0.3f, 0.0f, 0, 0.05f);
    addSweep(s, 250, 80, 0.2f, 0.0f);
    return toChunk(s);
}

// ---- Boss special attack sounds ----

Mix_Chunk* SoundGenerator::bossMultiShot() {
    // Rapid staccato burst - multiple short tones
    auto s = generate(0.25f);
    for (int i = 0; i < 4; i++) {
        float t0 = i * 0.05f;
        float t1 = t0 + 0.04f;
        addSweep(s, 600 + i * 100, 300 + i * 50, 0.25f, 0.0f, t0, t1);
    }
    addNoise(s, 0.12f, 0.0f, 0, 0.08f);
    return toChunk(s);
}

Mix_Chunk* SoundGenerator::bossShieldBurst() {
    // Deep charge-up then bright burst
    auto s = generate(0.35f);
    // Charging phase: rising low hum
    addSweep(s, 60, 200, 0.1f, 0.3f, 0, 0.2f);
    addSquare(s, 80, 0.05f, 0.15f, 0, 0.2f);
    // Burst phase: bright explosion
    addSweep(s, 400, 1200, 0.35f, 0.0f, 0.2f, 0.35f);
    addNoise(s, 0.25f, 0.0f, 0.2f, 0.3f);
    return toChunk(s);
}

Mix_Chunk* SoundGenerator::bossTeleport() {
    // Fast downward sweep (vanish) + reverse sweep (appear)
    auto s = generate(0.25f);
    // Vanish: high to low
    addSweep(s, 1000, 100, 0.25f, 0.0f, 0, 0.1f);
    addNoise(s, 0.15f, 0.0f, 0, 0.08f);
    // Brief silence gap, then appear: low to high
    addSweep(s, 100, 800, 0.0f, 0.25f, 0.12f, 0.22f);
    addSine(s, 600, 0.15f, 0.0f, 0.2f, 0.25f);
    return toChunk(s);
}

// ---- New enemy sounds ----

Mix_Chunk* SoundGenerator::crawlerDrop() {
    // Fast whoosh downward + thud on impact
    auto s = generate(0.2f);
    addSweep(s, 600, 80, 0.3f, 0.1f, 0, 0.12f);
    addNoise(s, 0.2f, 0.0f, 0.1f, 0.2f);
    addSine(s, 60, 0.25f, 0.0f, 0.12f, 0.2f);
    return toChunk(s);
}

Mix_Chunk* SoundGenerator::summonerSummon() {
    // Dark magical rising hum + crystalline chime
    auto s = generate(0.35f);
    addSweep(s, 80, 300, 0.15f, 0.25f, 0, 0.2f);
    addSquare(s, 120, 0.08f, 0.15f, 0, 0.2f);
    addSine(s, 600, 0.0f, 0.2f, 0.15f, 0.25f);
    addSine(s, 800, 0.2f, 0.0f, 0.25f, 0.35f);
    addNoise(s, 0.05f, 0.0f, 0.1f, 0.25f);
    return toChunk(s);
}

Mix_Chunk* SoundGenerator::sniperTelegraph() {
    // Rising warning tone - builds tension before shot
    auto s = generate(0.3f);
    addSweep(s, 400, 1200, 0.05f, 0.3f, 0, 0.25f);
    addSquare(s, 800, 0.0f, 0.12f, 0.1f, 0.25f);
    addSine(s, 1200, 0.2f, 0.0f, 0.25f, 0.3f);
    return toChunk(s);
}

// ---- Ambient music loops ----

Mix_Chunk* SoundGenerator::ambientDimA() {
    // Cool, ethereal dimension - 4 second loop
    auto s = generate(4.0f);

    // Base drone (low C)
    addSine(s, 65.4f, 0.08f, 0.08f);
    // Fifth harmonic for depth
    addSine(s, 98.0f, 0.04f, 0.04f);
    // High ethereal shimmer
    addSine(s, 523.0f, 0.02f, 0.02f);

    // Pulsing pad - breathes in and out over loop
    float dur = 4.0f;
    int samples = static_cast<int>(dur * s.sampleRate);
    for (int i = 0; i < samples; i++) {
        float t = static_cast<float>(i) / s.sampleRate;
        // Slow LFO modulates a mid-range tone
        float lfo = (std::sin(2.0f * static_cast<float>(M_PI) * 0.5f * t) + 1.0f) * 0.5f;
        float sample = std::sin(2.0f * static_cast<float>(M_PI) * 196.0f * t) * lfo * 0.03f;
        // Add subtle detuned layer
        sample += std::sin(2.0f * static_cast<float>(M_PI) * 197.5f * t) * lfo * 0.02f;
        s.data[i] = static_cast<Sint16>(std::clamp(
            static_cast<int>(s.data[i] + sample * 32767), -32767, 32767));
    }

    // Very subtle noise bed
    addNoise(s, 0.008f, 0.008f);

    return toChunk(s);
}

Mix_Chunk* SoundGenerator::ambientDimB() {
    // Warm, ominous dimension - 4 second loop
    auto s = generate(4.0f);

    // Deeper drone (low A)
    addSine(s, 55.0f, 0.09f, 0.09f);
    // Minor third for tension
    addSine(s, 65.4f, 0.05f, 0.05f);
    // Sub bass rumble
    addSine(s, 36.7f, 0.04f, 0.04f);

    // Ominous pulsing with square wave undertone
    float dur = 4.0f;
    int samples = static_cast<int>(dur * s.sampleRate);
    for (int i = 0; i < samples; i++) {
        float t = static_cast<float>(i) / s.sampleRate;
        // Slower, more menacing LFO
        float lfo = (std::sin(2.0f * static_cast<float>(M_PI) * 0.35f * t) + 1.0f) * 0.5f;
        // Dark pad tone
        float sample = std::sin(2.0f * static_cast<float>(M_PI) * 110.0f * t) * lfo * 0.025f;
        // Gritty square undertone
        float phase = std::fmod(82.4f * t, 1.0f);
        sample += (phase < 0.5f ? 1.0f : -1.0f) * lfo * 0.01f;
        s.data[i] = static_cast<Sint16>(std::clamp(
            static_cast<int>(s.data[i] + sample * 32767), -32767, 32767));
    }

    // Slightly more noise for grit
    addNoise(s, 0.012f, 0.012f);

    return toChunk(s);
}
