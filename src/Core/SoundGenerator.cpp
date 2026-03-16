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

Mix_Chunk* SoundGenerator::toChunk(const Sample& s) {
    // Convert to SDL audio format (signed 16-bit, mono)
    size_t bufSize = s.data.size() * sizeof(Sint16);
    Uint8* buf = static_cast<Uint8*>(SDL_malloc(bufSize));
    if (!buf) return nullptr;
    std::memcpy(buf, s.data.data(), bufSize);

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

// ---- Environmental hazard sounds ----

Mix_Chunk* SoundGenerator::fireBurn() {
    // Crackling fire + sizzle
    auto s = generate(0.25f);
    addNoise(s, 0.15f, 0.0f, 0.1f, 0.2f);
    addSine(s, 100, 0.1f, 0.0f, 0, 0.25f);
    addSweep(s, 200, 80, 0.08f, 0.05f, 0.05f, 0.2f);
    return toChunk(s);
}

Mix_Chunk* SoundGenerator::laserHit() {
    // Sharp electric zap
    auto s = generate(0.15f);
    addSweep(s, 2000, 500, 0.2f, 0.1f, 0, 0.1f);
    addSquare(s, 800, 0.15f, 0.0f, 0, 0.12f);
    addNoise(s, 0.1f, 0.0f, 0.05f, 0.15f);
    return toChunk(s);
}

// ---- Void Wyrm boss sounds ----

Mix_Chunk* SoundGenerator::wyrmDive() {
    // Descending screech + whoosh
    auto s = generate(0.3f);
    addSweep(s, 1200, 300, 0.25f, 0.15f, 0, 0.2f);
    addNoise(s, 0.1f, 0.2f, 0, 0.15f);
    addNoise(s, 0.2f, 0.0f, 0.15f, 0.3f);
    addSine(s, 200, 0.1f, 0.0f, 0.1f, 0.3f);
    return toChunk(s);
}

Mix_Chunk* SoundGenerator::wyrmPoison() {
    // Bubbly, hissing gas release
    auto s = generate(0.35f);
    addNoise(s, 0.08f, 0.15f, 0, 0.1f);
    addNoise(s, 0.15f, 0.0f, 0.1f, 0.35f);
    addSine(s, 150, 0.1f, 0.05f, 0, 0.2f);
    addSine(s, 90, 0.08f, 0.0f, 0.1f, 0.35f);
    addSquare(s, 60, 0.05f, 0.0f, 0.15f, 0.3f);
    return toChunk(s);
}

Mix_Chunk* SoundGenerator::wyrmBarrage() {
    // Rapid charging up sound
    auto s = generate(0.25f);
    addSweep(s, 200, 800, 0.1f, 0.2f, 0, 0.15f);
    addSweep(s, 400, 1200, 0.05f, 0.15f, 0.05f, 0.2f);
    addNoise(s, 0.05f, 0.1f, 0, 0.15f);
    addSine(s, 600, 0.15f, 0.0f, 0.15f, 0.25f);
    return toChunk(s);
}

// ---- Combat enhancement sounds ----

Mix_Chunk* SoundGenerator::iceFreeze() {
    // Crystalline freeze: high shimmer descending into crackle
    auto s = generate(0.25f);
    addSweep(s, 1500, 600, 0.2f, 0.1f, 0, 0.12f);
    addSine(s, 800, 0.15f, 0.0f, 0.05f, 0.2f);
    addNoise(s, 0.1f, 0.0f, 0.1f, 0.25f);
    addSquare(s, 300, 0.08f, 0.0f, 0.15f, 0.25f);
    return toChunk(s);
}

Mix_Chunk* SoundGenerator::electricChain() {
    // Rapid electric arcs jumping between targets
    auto s = generate(0.3f);
    for (int i = 0; i < 5; i++) {
        float t0 = i * 0.05f;
        float t1 = t0 + 0.04f;
        addSweep(s, 800 + i * 200, 400 + i * 100, 0.2f, 0.05f, t0, t1);
        addNoise(s, 0.12f, 0.0f, t0, t0 + 0.02f);
    }
    addSquare(s, 500, 0.1f, 0.0f, 0, 0.15f);
    return toChunk(s);
}

Mix_Chunk* SoundGenerator::parry() {
    // Sharp metallic clang - bright impact
    auto s = generate(0.15f);
    addSine(s, 1200, 0.35f, 0.0f, 0, 0.08f);
    addSine(s, 900, 0.25f, 0.0f, 0, 0.12f);
    addNoise(s, 0.2f, 0.0f, 0, 0.04f);
    addSquare(s, 600, 0.1f, 0.0f, 0.02f, 0.1f);
    return toChunk(s);
}

Mix_Chunk* SoundGenerator::parryCounter() {
    // Parry clang + ascending power tone
    auto s = generate(0.25f);
    addSine(s, 1200, 0.3f, 0.0f, 0, 0.08f);
    addNoise(s, 0.15f, 0.0f, 0, 0.04f);
    addSweep(s, 400, 1000, 0.1f, 0.25f, 0.06f, 0.2f);
    addSine(s, 800, 0.2f, 0.0f, 0.15f, 0.25f);
    return toChunk(s);
}

Mix_Chunk* SoundGenerator::chargedAttackRelease() {
    // Deep charge burst: bass thump + bright sweep + impact
    auto s = generate(0.3f);
    addSine(s, 60, 0.3f, 0.0f, 0, 0.15f);
    addSweep(s, 200, 1200, 0.15f, 0.3f, 0, 0.15f);
    addNoise(s, 0.25f, 0.0f, 0.1f, 0.2f);
    addSine(s, 400, 0.2f, 0.0f, 0.15f, 0.3f);
    addSquare(s, 150, 0.15f, 0.0f, 0.05f, 0.2f);
    return toChunk(s);
}

Mix_Chunk* SoundGenerator::criticalHit() {
    // Punchy impact with bright ring
    auto s = generate(0.2f);
    addNoise(s, 0.35f, 0.0f, 0, 0.05f);
    addSine(s, 1000, 0.25f, 0.0f, 0, 0.15f);
    addSine(s, 600, 0.2f, 0.0f, 0.02f, 0.18f);
    addSquare(s, 200, 0.15f, 0.0f, 0, 0.1f);
    return toChunk(s);
}

Mix_Chunk* SoundGenerator::comboMilestone() {
    // Triumphant ascending chime
    auto s = generate(0.3f);
    addSine(s, 600, 0.2f, 0.1f, 0, 0.1f);
    addSine(s, 800, 0.25f, 0.1f, 0.08f, 0.18f);
    addSine(s, 1000, 0.25f, 0.1f, 0.15f, 0.25f);
    addSine(s, 1200, 0.2f, 0.0f, 0.22f, 0.3f);
    return toChunk(s);
}

Mix_Chunk* SoundGenerator::airJuggleLaunch() {
    // Upward whoosh with impact
    auto s = generate(0.2f);
    addSweep(s, 200, 900, 0.25f, 0.1f, 0, 0.12f);
    addNoise(s, 0.15f, 0.0f, 0, 0.06f);
    addSine(s, 500, 0.15f, 0.0f, 0.08f, 0.2f);
    return toChunk(s);
}

Mix_Chunk* SoundGenerator::enemyStun() {
    // Dizzy wobble tone
    auto s = generate(0.25f);
    addSine(s, 400, 0.15f, 0.1f, 0, 0.1f);
    addSine(s, 350, 0.15f, 0.1f, 0.05f, 0.15f);
    addSine(s, 450, 0.1f, 0.0f, 0.1f, 0.25f);
    addSquare(s, 200, 0.08f, 0.0f, 0.05f, 0.2f);
    return toChunk(s);
}

// ---- Player ability sounds ----

Mix_Chunk* SoundGenerator::groundSlam() {
    // Heavy impact: bass thump + earth shake rumble + shockwave whoosh
    auto s = generate(0.4f);
    addSine(s, 40, 0.4f, 0.0f, 0, 0.2f);         // Deep bass impact
    addSweep(s, 200, 40, 0.3f, 0.0f, 0, 0.15f);   // Shockwave
    addNoise(s, 0.25f, 0.0f, 0, 0.1f);             // Impact crack
    addSquare(s, 60, 0.15f, 0.0f, 0.05f, 0.3f);    // Rumble
    addSweep(s, 100, 30, 0.1f, 0.0f, 0.15f, 0.4f); // Sustained rumble
    return toChunk(s);
}

Mix_Chunk* SoundGenerator::riftShieldActivate() {
    // Dimensional hum + crystalline shell forming
    auto s = generate(0.3f);
    addSweep(s, 200, 600, 0.1f, 0.25f, 0, 0.15f);   // Rising activation
    addSine(s, 400, 0.2f, 0.15f, 0.1f, 0.3f);        // Sustained hum
    addSine(s, 600, 0.1f, 0.08f, 0.12f, 0.28f);      // Harmonic shimmer
    addSquare(s, 300, 0.05f, 0.08f, 0.05f, 0.25f);    // Buzzy undertone
    return toChunk(s);
}

Mix_Chunk* SoundGenerator::riftShieldAbsorb() {
    // Soft bassy thud absorbed into shield
    auto s = generate(0.2f);
    addSine(s, 150, 0.3f, 0.0f, 0, 0.1f);           // Impact absorbed
    addSweep(s, 300, 500, 0.15f, 0.0f, 0.05f, 0.2f); // Energy conversion
    addNoise(s, 0.1f, 0.0f, 0, 0.05f);               // Soft crackle
    return toChunk(s);
}

Mix_Chunk* SoundGenerator::riftShieldReflect() {
    // Bright metallic ping + whoosh back
    auto s = generate(0.2f);
    addSine(s, 1000, 0.3f, 0.0f, 0, 0.1f);           // Bright ping
    addSine(s, 800, 0.2f, 0.0f, 0.02f, 0.12f);       // Harmonic
    addSweep(s, 600, 1200, 0.15f, 0.0f, 0.05f, 0.18f); // Reflect whoosh
    addNoise(s, 0.1f, 0.0f, 0, 0.04f);                // Impact snap
    return toChunk(s);
}

Mix_Chunk* SoundGenerator::riftShieldBurst() {
    // Shield overload: charge up + bright explosion
    auto s = generate(0.35f);
    addSweep(s, 100, 400, 0.1f, 0.3f, 0, 0.15f);    // Charge up
    addSweep(s, 400, 1500, 0.3f, 0.0f, 0.15f, 0.3f); // Burst release
    addNoise(s, 0.25f, 0.0f, 0.15f, 0.25f);           // Explosion noise
    addSine(s, 800, 0.2f, 0.0f, 0.2f, 0.35f);        // Ring-out
    return toChunk(s);
}

Mix_Chunk* SoundGenerator::phaseStrikeTeleport() {
    // Fast dimensional blink: down-sweep + up-sweep
    auto s = generate(0.2f);
    addSweep(s, 800, 100, 0.25f, 0.0f, 0, 0.08f);   // Vanish
    addNoise(s, 0.15f, 0.0f, 0, 0.05f);               // Static
    addSweep(s, 100, 900, 0.0f, 0.25f, 0.1f, 0.18f); // Appear
    addSine(s, 500, 0.15f, 0.0f, 0.15f, 0.2f);       // Arrive tone
    return toChunk(s);
}

Mix_Chunk* SoundGenerator::phaseStrikeHit() {
    // Backstab critical: sharp blade + dimensional echo
    auto s = generate(0.25f);
    addNoise(s, 0.35f, 0.0f, 0, 0.04f);               // Sharp impact
    addSweep(s, 500, 100, 0.3f, 0.0f, 0, 0.1f);      // Blade whoosh
    addSine(s, 1200, 0.25f, 0.0f, 0, 0.08f);          // Critical ring
    addSine(s, 800, 0.2f, 0.0f, 0.05f, 0.2f);        // Echo
    addSquare(s, 300, 0.1f, 0.0f, 0.08f, 0.2f);      // Dimensional buzz
    return toChunk(s);
}

// ---- Secret room & event sounds ----

Mix_Chunk* SoundGenerator::breakableWall() {
    auto s = generate(0.4f);
    addNoise(s, 0.4f, 0.0f, 0, 0.06f);
    addSweep(s, 200, 50, 0.3f, 0.0f, 0, 0.15f);
    addNoise(s, 0.1f, 0.0f, 0.05f, 0.3f);
    addSine(s, 80, 0.2f, 0.0f, 0, 0.2f);
    addSquare(s, 60, 0.15f, 0.0f, 0.1f, 0.35f);
    return toChunk(s);
}

Mix_Chunk* SoundGenerator::secretRoomDiscover() {
    auto s = generate(0.6f);
    addSine(s, 400, 0.0f, 0.2f, 0, 0.1f);
    addSine(s, 600, 0.2f, 0.25f, 0.08f, 0.2f);
    addSine(s, 900, 0.25f, 0.0f, 0.15f, 0.45f);
    addSine(s, 1200, 0.15f, 0.0f, 0.25f, 0.55f);
    addNoise(s, 0.05f, 0.0f, 0.1f, 0.5f);
    return toChunk(s);
}

Mix_Chunk* SoundGenerator::shrineActivate() {
    auto s = generate(0.5f);
    addSine(s, 300, 0.3f, 0.0f, 0, 0.4f);
    addSine(s, 600, 0.2f, 0.0f, 0, 0.3f);
    addSweep(s, 200, 400, 0.15f, 0.0f, 0.1f, 0.5f);
    return toChunk(s);
}

Mix_Chunk* SoundGenerator::shrineBlessing() {
    auto s = generate(0.5f);
    addSine(s, 500, 0.25f, 0.0f, 0, 0.4f);
    addSine(s, 630, 0.2f, 0.0f, 0.05f, 0.4f);
    addSine(s, 750, 0.2f, 0.0f, 0.1f, 0.45f);
    addSine(s, 1000, 0.15f, 0.0f, 0.15f, 0.5f);
    return toChunk(s);
}

Mix_Chunk* SoundGenerator::shrineCurse() {
    auto s = generate(0.5f);
    addSweep(s, 500, 100, 0.3f, 0.0f, 0, 0.4f);
    addSine(s, 150, 0.2f, 0.0f, 0.05f, 0.4f);
    addSquare(s, 80, 0.15f, 0.0f, 0.1f, 0.45f);
    addNoise(s, 0.1f, 0.0f, 0.1f, 0.35f);
    return toChunk(s);
}

Mix_Chunk* SoundGenerator::merchantGreet() {
    auto s = generate(0.3f);
    addSine(s, 700, 0.2f, 0.0f, 0, 0.12f);
    addSine(s, 900, 0.25f, 0.0f, 0.1f, 0.25f);
    addSine(s, 1400, 0.1f, 0.0f, 0.1f, 0.25f);
    return toChunk(s);
}

Mix_Chunk* SoundGenerator::anomalySpawn() {
    auto s = generate(0.4f);
    addSweep(s, 200, 800, 0.2f, 0.0f, 0, 0.2f);
    addSweep(s, 800, 200, 0.15f, 0.0f, 0.15f, 0.35f);
    addNoise(s, 0.15f, 0.0f, 0, 0.15f);
    addSquare(s, 150, 0.1f, 0.0f, 0.1f, 0.35f);
    return toChunk(s);
}

// ---- Dimensional Architect boss sounds ----

Mix_Chunk* SoundGenerator::archTileSwap() {
    auto s = generate(0.3f);
    addNoise(s, 0.2f, 0.0f, 0, 0.05f);
    addSweep(s, 400, 200, 0.25f, 0.0f, 0, 0.2f);
    addSquare(s, 150, 0.15f, 0.0f, 0.05f, 0.25f);
    return toChunk(s);
}

Mix_Chunk* SoundGenerator::archConstruct() {
    auto s = generate(0.5f);
    addSweep(s, 100, 600, 0.2f, 0.0f, 0, 0.3f);
    addSine(s, 800, 0.15f, 0.0f, 0.15f, 0.45f);
    addSquare(s, 200, 0.1f, 0.0f, 0.1f, 0.4f);
    addNoise(s, 0.08f, 0.0f, 0.2f, 0.5f);
    return toChunk(s);
}

Mix_Chunk* SoundGenerator::archRiftOpen() {
    auto s = generate(0.6f);
    addSweep(s, 600, 100, 0.3f, 0.0f, 0, 0.3f);
    addSweep(s, 100, 500, 0.2f, 0.0f, 0.2f, 0.5f);
    addNoise(s, 0.15f, 0.0f, 0, 0.2f);
    addSine(s, 250, 0.2f, 0.0f, 0.3f, 0.6f);
    return toChunk(s);
}

Mix_Chunk* SoundGenerator::archCollapse() {
    auto s = generate(0.8f);
    addNoise(s, 0.4f, 0.0f, 0, 0.1f);
    addSweep(s, 300, 30, 0.35f, 0.0f, 0, 0.5f);
    addSine(s, 50, 0.3f, 0.0f, 0, 0.6f);
    addNoise(s, 0.15f, 0.0f, 0.2f, 0.7f);
    addSquare(s, 40, 0.2f, 0.0f, 0.3f, 0.8f);
    return toChunk(s);
}

Mix_Chunk* SoundGenerator::archBeam() {
    auto s = generate(0.4f);
    addSine(s, 500, 0.25f, 0.1f, 0, 0.35f);
    addSine(s, 750, 0.15f, 0.0f, 0.05f, 0.3f);
    addSquare(s, 300, 0.1f, 0.0f, 0, 0.2f);
    addSweep(s, 500, 800, 0.1f, 0.0f, 0.1f, 0.35f);
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

// Dynamic music layers

Mix_Chunk* SoundGenerator::musicRhythm() {
    // Percussive pulse loop - 4 seconds, ~120 BPM (8 beats)
    auto s = generate(4.0f);
    float beatInterval = 0.5f; // 120 BPM

    for (int beat = 0; beat < 8; beat++) {
        float t = beat * beatInterval;
        // Kick drum: low sine sweep down
        addSweep(s, 120.0f, 45.0f, 0.12f, 0.0f, t, t + 0.08f);
        // Off-beat hi-hat (noise burst) on beats 1,3,5,7
        if (beat % 2 == 1) {
            addNoise(s, 0.04f, 0.0f, t, t + 0.03f);
        }
        // Accent on beats 0 and 4 (downbeats)
        if (beat % 4 == 0) {
            addSine(s, 80.0f, 0.08f, 0.0f, t, t + 0.1f);
        }
    }

    return toChunk(s);
}

Mix_Chunk* SoundGenerator::musicIntensity() {
    // Fast aggressive arpeggio loop - 4 seconds
    auto s = generate(4.0f);

    // Rising arpeggio pattern (minor scale feel)
    float notes[] = {130.8f, 155.6f, 196.0f, 233.1f, 261.6f, 233.1f, 196.0f, 155.6f};
    int numNotes = 8;
    float noteLen = 0.125f; // 16th notes at 120 BPM
    int totalSteps = static_cast<int>(4.0f / noteLen);

    for (int i = 0; i < totalSteps; i++) {
        float t = i * noteLen;
        float freq = notes[i % numNotes];
        // Square wave for aggressive edge
        addSquare(s, freq, 0.04f, 0.0f, t, t + noteLen * 0.8f);
        // Subtle sine for body
        addSine(s, freq * 0.5f, 0.03f, 0.0f, t, t + noteLen * 0.6f);
    }

    // Subtle noise bed for grit
    addNoise(s, 0.006f, 0.006f);

    return toChunk(s);
}

Mix_Chunk* SoundGenerator::musicDanger() {
    // Dissonant warning drone - 4 seconds
    auto s = generate(4.0f);

    // Tritone interval (most dissonant)
    addSine(s, 73.4f, 0.07f, 0.07f);   // D2
    addSine(s, 103.8f, 0.05f, 0.05f);  // Ab2 (tritone)

    // Slow warbling alarm
    int samples = static_cast<int>(4.0f * s.sampleRate);
    for (int i = 0; i < samples; i++) {
        float t = static_cast<float>(i) / s.sampleRate;
        // Pulsing alarm: 2 Hz throb
        float throb = (std::sin(2.0f * static_cast<float>(M_PI) * 2.0f * t) + 1.0f) * 0.5f;
        // Warning tone
        float sample = std::sin(2.0f * static_cast<float>(M_PI) * 440.0f * t) * throb * 0.03f;
        // Detuned second for unease
        sample += std::sin(2.0f * static_cast<float>(M_PI) * 466.2f * t) * throb * 0.02f;
        s.data[i] = static_cast<Sint16>(std::clamp(
            static_cast<int>(s.data[i] + sample * 32767), -32767, 32767));
    }

    // Filtered noise sweep
    addNoise(s, 0.015f, 0.015f);

    return toChunk(s);
}

Mix_Chunk* SoundGenerator::musicBoss() {
    // Epic battle theme - 4 seconds, powerful and driving
    auto s = generate(4.0f);

    // Deep power bass (half-time feel)
    float bassHits[] = {0.0f, 1.0f, 2.0f, 2.5f, 3.0f, 3.5f};
    for (float t : bassHits) {
        addSweep(s, 55.0f, 36.7f, 0.15f, 0.02f, t, t + 0.3f);
        addSine(s, 27.5f, 0.08f, 0.0f, t, t + 0.4f);
    }

    // Driving rhythm (double-time kicks)
    for (int i = 0; i < 16; i++) {
        float t = i * 0.25f;
        addSweep(s, 100.0f, 40.0f, 0.1f, 0.0f, t, t + 0.06f);
        // Snare-like noise on 2 and 4
        if (i % 4 == 2) {
            addNoise(s, 0.06f, 0.0f, t, t + 0.05f);
        }
    }

    // Power chord drone (root + fifth)
    addSine(s, 55.0f, 0.06f, 0.06f);   // A1
    addSine(s, 82.4f, 0.04f, 0.04f);   // E2 (fifth)
    addSquare(s, 110.0f, 0.02f, 0.02f); // A2 grit

    // Epic rising sweep every 2 seconds
    addSweep(s, 200.0f, 600.0f, 0.0f, 0.04f, 0.0f, 1.0f);
    addSweep(s, 200.0f, 800.0f, 0.0f, 0.05f, 2.0f, 3.5f);

    return toChunk(s);
}

// Theme-specific ambient accents

Mix_Chunk* SoundGenerator::themeAmbient(int themeId) {
    auto s = generate(4.0f);
    int samples = static_cast<int>(4.0f * s.sampleRate);

    switch (themeId) {
    case 0: // VictorianClockwork - ticking gears, steam hiss
        for (int tick = 0; tick < 8; tick++) {
            float t = tick * 0.5f;
            addSine(s, 2000.0f, 0.03f, 0.0f, t, t + 0.02f); // tick
            addSine(s, 1500.0f, 0.02f, 0.0f, t + 0.25f, t + 0.27f); // tock
        }
        addNoise(s, 0.008f, 0.008f); // steam bed
        addSine(s, 30.0f, 0.03f, 0.03f); // machinery rumble
        break;

    case 1: // DeepOcean - bubbles, pressure, whale calls
        for (int i = 0; i < samples; i++) {
            float t = static_cast<float>(i) / s.sampleRate;
            float bubble = std::sin(2.0f * static_cast<float>(M_PI) * (800.0f + 400.0f * std::sin(t * 3.0f)) * t);
            float env = std::max(0.0f, std::sin(t * 2.5f)) * 0.015f;
            s.data[i] = static_cast<Sint16>(std::clamp(
                static_cast<int>(s.data[i] + bubble * env * 32767), -32767, 32767));
        }
        addSweep(s, 80.0f, 120.0f, 0.02f, 0.02f, 0.5f, 2.5f); // whale call
        addSine(s, 25.0f, 0.04f, 0.04f); // pressure drone
        break;

    case 2: // NeonCity - electronic hum, city buzz
        addSine(s, 60.0f, 0.03f, 0.03f); // power hum
        addSine(s, 120.0f, 0.015f, 0.015f); // harmonic
        for (int i = 0; i < samples; i++) {
            float t = static_cast<float>(i) / s.sampleRate;
            float buzz = std::sin(2.0f * static_cast<float>(M_PI) * 1000.0f * t);
            float lfo = (std::sin(2.0f * static_cast<float>(M_PI) * 0.3f * t) + 1.0f) * 0.5f;
            s.data[i] = static_cast<Sint16>(std::clamp(
                static_cast<int>(s.data[i] + buzz * lfo * 0.008f * 32767), -32767, 32767));
        }
        break;

    case 3: // AncientRuins - wind through stones, distant echoes
        addNoise(s, 0.012f, 0.018f, 0.0f, 2.0f);
        addNoise(s, 0.018f, 0.012f, 2.0f, 4.0f);
        addSine(s, 220.0f, 0.0f, 0.02f, 1.0f, 1.5f); // echo ping
        addSine(s, 220.0f, 0.02f, 0.0f, 1.5f, 2.5f);
        addSine(s, 330.0f, 0.0f, 0.015f, 3.0f, 3.3f);
        addSine(s, 330.0f, 0.015f, 0.0f, 3.3f, 4.0f);
        break;

    case 4: // CrystalCavern - resonant chimes, crystal ring
        addSine(s, 1046.0f, 0.0f, 0.02f, 0.0f, 0.1f); // chime 1
        addSine(s, 1046.0f, 0.02f, 0.0f, 0.1f, 1.5f);
        addSine(s, 1318.0f, 0.0f, 0.015f, 1.5f, 1.6f); // chime 2
        addSine(s, 1318.0f, 0.015f, 0.0f, 1.6f, 3.0f);
        addSine(s, 784.0f, 0.0f, 0.018f, 2.8f, 2.9f); // chime 3
        addSine(s, 784.0f, 0.018f, 0.0f, 2.9f, 4.0f);
        addSine(s, 40.0f, 0.02f, 0.02f); // cave resonance
        break;

    case 5: // BioMechanical - organic pulse, machinery
        for (int i = 0; i < samples; i++) {
            float t = static_cast<float>(i) / s.sampleRate;
            float pulse = (std::sin(2.0f * static_cast<float>(M_PI) * 1.2f * t) + 1.0f) * 0.5f;
            float mech = std::sin(2.0f * static_cast<float>(M_PI) * 45.0f * t) * pulse * 0.03f;
            mech += std::sin(2.0f * static_cast<float>(M_PI) * 90.0f * t) * pulse * 0.015f;
            s.data[i] = static_cast<Sint16>(std::clamp(
                static_cast<int>(s.data[i] + mech * 32767), -32767, 32767));
        }
        addNoise(s, 0.006f, 0.006f);
        break;

    case 6: // FrozenWasteland - howling wind, ice creak
        addNoise(s, 0.01f, 0.025f, 0.0f, 2.0f); // wind rises
        addNoise(s, 0.025f, 0.01f, 2.0f, 4.0f); // wind falls
        addSweep(s, 400.0f, 600.0f, 0.01f, 0.02f, 0.5f, 1.5f); // wind whistle
        addSweep(s, 600.0f, 350.0f, 0.02f, 0.0f, 1.5f, 2.5f);
        addSine(s, 1800.0f, 0.0f, 0.01f, 3.0f, 3.1f); // ice crack
        addSine(s, 1800.0f, 0.01f, 0.0f, 3.1f, 3.3f);
        break;

    case 7: // VolcanicCore - rumble, heat hiss, lava bubble
        addSine(s, 20.0f, 0.05f, 0.05f); // deep rumble
        addSine(s, 30.0f, 0.03f, 0.03f);
        addNoise(s, 0.015f, 0.015f); // heat hiss
        for (int b = 0; b < 3; b++) { // lava bubbles
            float t = 0.8f + b * 1.2f;
            addSweep(s, 60.0f, 200.0f, 0.03f, 0.0f, t, t + 0.15f);
        }
        break;

    case 8: // FloatingIslands - airy wind, open space
        addNoise(s, 0.008f, 0.012f, 0.0f, 2.0f);
        addNoise(s, 0.012f, 0.008f, 2.0f, 4.0f);
        addSine(s, 523.0f, 0.01f, 0.01f); // high airy tone
        addSine(s, 659.0f, 0.008f, 0.008f); // harmony
        break;

    case 9: // VoidRealm - deep distortion, otherworldly
        addSine(s, 22.0f, 0.06f, 0.06f); // sub-void drone
        for (int i = 0; i < samples; i++) {
            float t = static_cast<float>(i) / s.sampleRate;
            float warp = std::sin(2.0f * static_cast<float>(M_PI) * (50.0f + 20.0f * std::sin(t * 0.7f)) * t);
            s.data[i] = static_cast<Sint16>(std::clamp(
                static_cast<int>(s.data[i] + warp * 0.025f * 32767), -32767, 32767));
        }
        addNoise(s, 0.01f, 0.01f);
        break;

    case 10: // SpaceWestern - desert wind, distant twang
        addNoise(s, 0.01f, 0.015f, 0.0f, 2.0f);
        addNoise(s, 0.015f, 0.01f, 2.0f, 4.0f);
        addSweep(s, 300.0f, 280.0f, 0.0f, 0.015f, 1.0f, 1.05f); // twang
        addSweep(s, 280.0f, 200.0f, 0.015f, 0.0f, 1.05f, 1.8f);
        addSine(s, 35.0f, 0.02f, 0.02f); // low desert hum
        break;

    case 11: // Biopunk - bubbling, organic synth
        for (int i = 0; i < samples; i++) {
            float t = static_cast<float>(i) / s.sampleRate;
            float bubble = std::sin(2.0f * static_cast<float>(M_PI) * (600.0f + 300.0f * std::sin(t * 5.0f)) * t);
            float env = std::max(0.0f, std::sin(t * 4.0f)) * 0.012f;
            float organic = std::sin(2.0f * static_cast<float>(M_PI) * 55.0f * t) * 0.025f;
            float lfo = (std::sin(2.0f * static_cast<float>(M_PI) * 0.8f * t) + 1.0f) * 0.5f;
            s.data[i] = static_cast<Sint16>(std::clamp(
                static_cast<int>(s.data[i] + (bubble * env + organic * lfo) * 32767), -32767, 32767));
        }
        break;

    default:
        addNoise(s, 0.01f, 0.01f);
        break;
    }

    return toChunk(s);
}

// Void Sovereign boss sounds

Mix_Chunk* SoundGenerator::voidSovereignOrb() {
    auto s = generate(0.35f);
    addSweep(s, 200.0f, 800.0f, 0.3f, 0.0f);
    addSine(s, 120.0f, 0.2f, 0.0f, 0.0f, 0.15f);
    addNoise(s, 0.08f, 0.0f);
    return toChunk(s);
}

Mix_Chunk* SoundGenerator::voidSovereignSlam() {
    auto s = generate(0.5f);
    addSweep(s, 150.0f, 30.0f, 0.5f, 0.1f);
    addNoise(s, 0.25f, 0.0f);
    addSine(s, 40.0f, 0.3f, 0.0f, 0.05f, 0.4f);
    return toChunk(s);
}

Mix_Chunk* SoundGenerator::voidSovereignTeleport() {
    auto s = generate(0.4f);
    addSweep(s, 1200.0f, 100.0f, 0.2f, 0.0f, 0.0f, 0.2f);
    addSweep(s, 100.0f, 1200.0f, 0.0f, 0.2f, 0.2f, 0.4f);
    addNoise(s, 0.1f, 0.0f);
    return toChunk(s);
}

Mix_Chunk* SoundGenerator::voidSovereignDimLock() {
    auto s = generate(0.6f);
    addSine(s, 80.0f, 0.3f, 0.3f, 0.0f, 0.5f);
    addSine(s, 160.0f, 0.2f, 0.0f, 0.0f, 0.6f);
    addSquare(s, 40.0f, 0.15f, 0.05f);
    addNoise(s, 0.1f, 0.02f);
    return toChunk(s);
}

Mix_Chunk* SoundGenerator::voidSovereignStorm() {
    auto s = generate(0.8f);
    addNoise(s, 0.3f, 0.1f);
    addSweep(s, 60.0f, 200.0f, 0.2f, 0.05f);
    addSine(s, 100.0f, 0.15f, 0.0f);
    return toChunk(s);
}

Mix_Chunk* SoundGenerator::voidSovereignLaser() {
    auto s = generate(0.5f);
    addSine(s, 440.0f, 0.3f, 0.3f, 0.0f, 0.4f);
    addSine(s, 880.0f, 0.15f, 0.15f, 0.0f, 0.4f);
    addSweep(s, 440.0f, 220.0f, 0.2f, 0.0f, 0.3f, 0.5f);
    addNoise(s, 0.05f, 0.0f);
    return toChunk(s);
}

Mix_Chunk* SoundGenerator::loreDiscover() {
    auto s = generate(0.8f);
    // Mystical ascending chime
    addSine(s, 523.0f, 0.0f, 0.25f, 0.0f, 0.15f); // C5
    addSine(s, 523.0f, 0.25f, 0.0f, 0.15f, 0.4f);
    addSine(s, 659.0f, 0.0f, 0.25f, 0.15f, 0.3f); // E5
    addSine(s, 659.0f, 0.25f, 0.0f, 0.3f, 0.55f);
    addSine(s, 784.0f, 0.0f, 0.25f, 0.3f, 0.45f); // G5
    addSine(s, 784.0f, 0.25f, 0.0f, 0.45f, 0.8f);
    // Sparkle
    addSine(s, 1568.0f, 0.05f, 0.0f, 0.4f, 0.8f);
    return toChunk(s);
}

Mix_Chunk* SoundGenerator::heartbeat() {
    // Lub-dub heartbeat: two low-frequency thumps
    auto s = generate(0.45f);
    // First beat (lub): deep thump
    addSine(s, 45.0f, 0.0f, 0.35f, 0.0f, 0.03f);
    addSine(s, 45.0f, 0.35f, 0.0f, 0.03f, 0.15f);
    addSine(s, 90.0f, 0.0f, 0.15f, 0.0f, 0.02f);
    addSine(s, 90.0f, 0.15f, 0.0f, 0.02f, 0.1f);
    // Second beat (dub): slightly lighter
    addSine(s, 50.0f, 0.0f, 0.25f, 0.18f, 0.21f);
    addSine(s, 50.0f, 0.25f, 0.0f, 0.21f, 0.32f);
    addSine(s, 100.0f, 0.0f, 0.1f, 0.18f, 0.2f);
    addSine(s, 100.0f, 0.1f, 0.0f, 0.2f, 0.28f);
    return toChunk(s);
}

// Entropy Incarnate boss sounds

Mix_Chunk* SoundGenerator::entropyPulse() {
    auto s = generate(0.5f);
    // Deep warping pulse: descending sweep + sub-bass rumble
    addSweep(s, 300.0f, 60.0f, 0.4f, 0.1f);
    addSine(s, 50.0f, 0.3f, 0.0f, 0.05f, 0.4f);
    addNoise(s, 0.15f, 0.0f, 0.0f, 0.3f);
    addSine(s, 80.0f, 0.2f, 0.0f, 0.1f, 0.45f);
    return toChunk(s);
}

Mix_Chunk* SoundGenerator::entropyTendril() {
    auto s = generate(0.4f);
    // Eerie homing tendril: slow wobbling tone
    addSweep(s, 180.0f, 400.0f, 0.0f, 0.25f, 0.0f, 0.15f);
    addSweep(s, 400.0f, 250.0f, 0.25f, 0.0f, 0.15f, 0.4f);
    addSine(s, 100.0f, 0.15f, 0.0f, 0.0f, 0.3f);
    return toChunk(s);
}

Mix_Chunk* SoundGenerator::entropyMissile() {
    auto s = generate(0.35f);
    // Sharp missile salvo: ascending burst + crackle
    addSweep(s, 200.0f, 900.0f, 0.35f, 0.0f, 0.0f, 0.15f);
    addNoise(s, 0.12f, 0.0f, 0.05f, 0.25f);
    addSine(s, 150.0f, 0.2f, 0.0f, 0.0f, 0.2f);
    return toChunk(s);
}

Mix_Chunk* SoundGenerator::entropyShatter() {
    auto s = generate(0.6f);
    // Dimension shatter: chaotic crash + dissonant chord
    addSweep(s, 600.0f, 40.0f, 0.5f, 0.1f, 0.0f, 0.3f);
    addNoise(s, 0.3f, 0.0f, 0.0f, 0.4f);
    addSine(s, 55.0f, 0.4f, 0.0f, 0.05f, 0.5f);
    addSine(s, 77.0f, 0.2f, 0.0f, 0.1f, 0.4f); // dissonant overtone
    return toChunk(s);
}

Mix_Chunk* SoundGenerator::chargeReady() {
    auto s = generate(0.2f);
    // Brief ascending shimmer: two quick tones + sparkle
    addSine(s, 880.0f, 0.0f, 0.3f, 0.0f, 0.05f);   // A5 quick rise
    addSine(s, 880.0f, 0.3f, 0.0f, 0.05f, 0.12f);
    addSine(s, 1320.0f, 0.0f, 0.25f, 0.04f, 0.1f);  // E6 shimmer
    addSine(s, 1320.0f, 0.25f, 0.0f, 0.1f, 0.2f);
    addSine(s, 1760.0f, 0.08f, 0.0f, 0.08f, 0.2f);  // A6 sparkle tail
    return toChunk(s);
}
