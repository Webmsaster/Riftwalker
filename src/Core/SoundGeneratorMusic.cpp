#include "SoundGenerator.h"
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

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

// ---- Procedural music tracks ----
// Each track is ~8 seconds and designed to loop seamlessly

Mix_Chunk* SoundGenerator::themeMusic(int themeId, int dimension) {
    const float DUR = 8.0f;
    auto s = generate(DUR);

    // Dimension shift: dim 2 plays slightly detuned (+3%) for unsettling feel
    float dimDetune = (dimension == 2) ? 1.03f : 1.0f;
    // Dim 2 also slightly lower volume for contrast
    float dimVol = (dimension == 2) ? 0.85f : 1.0f;

    switch (themeId) {
    case 6: { // FrozenWasteland - slow arpeggios, high pitched, reverb-like decay
        // Pentatonic scale in high register: E5, G5, A5, B5, D6
        float notes[] = {659.3f, 784.0f, 880.0f, 987.8f, 1174.7f};
        float bpm = 80.0f;
        float beatLen = 60.0f / bpm;
        int numBeats = static_cast<int>(DUR / beatLen);

        // Sustained pad drone (E3 + B3)
        addSine(s, 164.8f * dimDetune, 0.03f * dimVol, 0.03f * dimVol);
        addSine(s, 246.9f * dimDetune, 0.02f * dimVol, 0.02f * dimVol);

        // Slow arpeggio with long decay (reverb-like)
        for (int i = 0; i < numBeats; i++) {
            float t = i * beatLen;
            int noteIdx = i % 5;
            float freq = notes[noteIdx] * dimDetune;
            float noteVol = 0.06f * dimVol;
            // Long decay = reverb-like
            addSine(s, freq, 0.0f, noteVol, t, t + 0.05f);
            addSine(s, freq, noteVol, 0.0f, t + 0.05f, std::min(t + beatLen * 2.5f, DUR));
            // Subtle octave shimmer
            addSine(s, freq * 2.0f, 0.0f, noteVol * 0.3f, t, t + 0.03f);
            addSine(s, freq * 2.0f, noteVol * 0.3f, 0.0f, t + 0.03f, std::min(t + beatLen * 1.5f, DUR));
        }

        // Wind noise bed
        addNoise(s, 0.012f * dimVol, 0.012f * dimVol);
        break;
    }

    case 7: { // VolcanicCore - deep bass drums, aggressive rhythm
        float bpm = 140.0f;
        float beatLen = 60.0f / bpm;
        int numBeats = static_cast<int>(DUR / beatLen);

        // Deep bass drone (A1)
        addSine(s, 55.0f * dimDetune, 0.06f * dimVol, 0.06f * dimVol);
        addSquare(s, 27.5f * dimDetune, 0.03f * dimVol, 0.03f * dimVol);

        for (int i = 0; i < numBeats; i++) {
            float t = i * beatLen;
            // Heavy kick on every beat
            addSweep(s, 150.0f * dimDetune, 35.0f, 0.12f * dimVol, 0.0f, t, t + 0.1f);
            // Snare on beats 2,4,6,8...
            if (i % 2 == 1) {
                addNoise(s, 0.08f * dimVol, 0.0f, t, t + 0.06f);
                addSine(s, 200.0f, 0.05f * dimVol, 0.0f, t, t + 0.04f);
            }
            // Aggressive bass note pattern (power fifths)
            if (i % 4 == 0) {
                addSquare(s, 82.4f * dimDetune, 0.05f * dimVol, 0.0f, t, t + beatLen * 1.5f);
            } else if (i % 4 == 2) {
                addSquare(s, 110.0f * dimDetune, 0.05f * dimVol, 0.0f, t, t + beatLen * 1.5f);
            }
            // 16th note hi-hat on every other 16th
            if (i % 2 == 0) {
                float t16 = t + beatLen * 0.5f;
                if (t16 < DUR) addNoise(s, 0.03f * dimVol, 0.0f, t16, t16 + 0.02f);
            }
        }
        // Rumble bed
        addNoise(s, 0.01f * dimVol, 0.01f * dimVol);
        break;
    }

    case 2: { // NeonCity - synth-wave, fast tempo, electronic feel
        float bpm = 128.0f;
        float beatLen = 60.0f / bpm;
        int numBeats = static_cast<int>(DUR / beatLen);

        // Synth bass pattern (A minor): A2, C3, E3, A3
        float bassNotes[] = {110.0f, 130.8f, 164.8f, 220.0f};

        for (int i = 0; i < numBeats; i++) {
            float t = i * beatLen;
            // Four-on-the-floor kick
            addSweep(s, 100.0f, 40.0f, 0.1f * dimVol, 0.0f, t, t + 0.08f);

            // Synth bass arpeggio (16th notes)
            for (int sub = 0; sub < 4; sub++) {
                float st = t + sub * beatLen * 0.25f;
                if (st >= DUR) break;
                float freq = bassNotes[(i * 4 + sub) % 4] * dimDetune;
                addSquare(s, freq, 0.04f * dimVol, 0.0f, st, st + beatLen * 0.2f);
            }

            // Offbeat open hi-hat
            float offbeat = t + beatLen * 0.5f;
            if (offbeat < DUR) {
                addNoise(s, 0.04f * dimVol, 0.0f, offbeat, offbeat + 0.04f);
            }

            // Snare on 2 and 4
            if (i % 2 == 1) {
                addNoise(s, 0.06f * dimVol, 0.0f, t, t + 0.05f);
            }
        }

        // Synth pad (A minor chord: A3, C4, E4)
        addSine(s, 220.0f * dimDetune, 0.025f * dimVol, 0.025f * dimVol);
        addSine(s, 261.6f * dimDetune, 0.02f * dimVol, 0.02f * dimVol);
        addSine(s, 329.6f * dimDetune, 0.018f * dimVol, 0.018f * dimVol);

        // Rising synth sweep every 4 seconds
        addSweep(s, 200.0f * dimDetune, 800.0f * dimDetune, 0.0f, 0.03f * dimVol, 0.0f, 2.0f);
        addSweep(s, 200.0f * dimDetune, 1000.0f * dimDetune, 0.0f, 0.035f * dimVol, 4.0f, 6.0f);
        break;
    }

    case 4: { // CrystalCavern - bell-like tones, ethereal
        // Whole tone scale bells: C5, D5, E5, F#5, G#5, A#5
        float bells[] = {523.3f, 587.3f, 659.3f, 740.0f, 830.6f, 932.3f};
        float bpm = 72.0f;
        float beatLen = 60.0f / bpm;
        int numBeats = static_cast<int>(DUR / beatLen);

        // Deep cave resonance drone
        addSine(s, 65.4f * dimDetune, 0.025f * dimVol, 0.025f * dimVol);
        addSine(s, 130.8f * dimDetune, 0.015f * dimVol, 0.015f * dimVol);

        // Bell-like tones with long ring-out
        for (int i = 0; i < numBeats; i++) {
            float t = i * beatLen;
            int noteIdx = (i * 3 + i / 3) % 6; // Skip pattern for interest
            float freq = bells[noteIdx] * dimDetune;
            float vol = 0.05f * dimVol;
            // Sharp attack, long sine decay = bell character
            addSine(s, freq, 0.0f, vol, t, t + 0.01f);
            addSine(s, freq, vol, 0.0f, t + 0.01f, std::min(t + beatLen * 3.0f, DUR));
            // Inharmonic partial (bell characteristic)
            addSine(s, freq * 2.76f, 0.0f, vol * 0.25f, t, t + 0.01f);
            addSine(s, freq * 2.76f, vol * 0.25f, 0.0f, t + 0.01f, std::min(t + beatLen * 1.5f, DUR));
        }

        // Subtle sparkle noise
        addNoise(s, 0.005f * dimVol, 0.005f * dimVol);
        break;
    }

    case 1: { // DeepOcean - slow ambient, low frequency drones
        // Very slow, ambient feel

        // Sub-bass drone layers
        addSine(s, 36.7f * dimDetune, 0.05f * dimVol, 0.05f * dimVol); // D1
        addSine(s, 55.0f * dimDetune, 0.035f * dimVol, 0.035f * dimVol); // A1

        // Slow LFO-modulated pad
        int samples = static_cast<int>(DUR * s.sampleRate);
        for (int i = 0; i < samples; i++) {
            float t = static_cast<float>(i) / s.sampleRate;
            float lfo = (std::sin(2.0f * static_cast<float>(M_PI) * 0.15f * t) + 1.0f) * 0.5f;
            float sample = std::sin(2.0f * static_cast<float>(M_PI) * 73.4f * dimDetune * t) * lfo * 0.03f * dimVol;
            // Detuned octave above
            sample += std::sin(2.0f * static_cast<float>(M_PI) * 147.5f * dimDetune * t) * lfo * 0.015f * dimVol;
            s.data[i] = static_cast<Sint16>(std::clamp(
                static_cast<int>(s.data[i] + sample * 32767), -32767, 32767));
        }

        // Whale-call-like sweeps
        addSweep(s, 80.0f * dimDetune, 150.0f * dimDetune, 0.0f, 0.025f * dimVol, 1.0f, 3.0f);
        addSweep(s, 150.0f * dimDetune, 80.0f * dimDetune, 0.025f * dimVol, 0.0f, 3.0f, 4.5f);
        addSweep(s, 100.0f * dimDetune, 180.0f * dimDetune, 0.0f, 0.02f * dimVol, 5.5f, 7.0f);
        addSweep(s, 180.0f * dimDetune, 90.0f * dimDetune, 0.02f * dimVol, 0.0f, 7.0f, 8.0f);

        // Pressure noise
        addNoise(s, 0.008f * dimVol, 0.008f * dimVol);
        break;
    }

    case 5: { // BioMechanical - industrial percussion, mechanical rhythm
        float bpm = 110.0f;
        float beatLen = 60.0f / bpm;
        int numBeats = static_cast<int>(DUR / beatLen);

        // Mechanical drone
        addSquare(s, 55.0f * dimDetune, 0.025f * dimVol, 0.025f * dimVol);
        addSine(s, 82.4f * dimDetune, 0.02f * dimVol, 0.02f * dimVol);

        for (int i = 0; i < numBeats; i++) {
            float t = i * beatLen;
            // Industrial kick (metallic)
            addSweep(s, 200.0f, 50.0f, 0.1f * dimVol, 0.0f, t, t + 0.06f);
            addSquare(s, 60.0f, 0.06f * dimVol, 0.0f, t, t + 0.05f);

            // Mechanical clank pattern (irregular)
            if (i % 3 == 1 || i % 5 == 2) {
                addNoise(s, 0.07f * dimVol, 0.0f, t, t + 0.03f);
                addSine(s, 800.0f, 0.03f * dimVol, 0.0f, t, t + 0.02f);
            }

            // Piston-like rhythm (alternating)
            if (i % 2 == 0) {
                float mid = t + beatLen * 0.5f;
                if (mid < DUR) {
                    addSweep(s, 300.0f, 150.0f, 0.04f * dimVol, 0.0f, mid, mid + 0.04f);
                }
            }

            // Grinding gear tone
            if (i % 4 == 0) {
                addSquare(s, 110.0f * dimDetune, 0.04f * dimVol, 0.0f, t, t + beatLen * 2.0f);
            }
        }

        // Steam hiss bed
        addNoise(s, 0.008f * dimVol, 0.008f * dimVol);
        break;
    }

    case 8: { // FloatingIslands - light, airy notes, sparse
        // Major pentatonic: C5, D5, E5, G5, A5
        float notes[] = {523.3f, 587.3f, 659.3f, 784.0f, 880.0f};

        // Airy pad (C major)
        addSine(s, 261.6f * dimDetune, 0.02f * dimVol, 0.02f * dimVol);
        addSine(s, 329.6f * dimDetune, 0.015f * dimVol, 0.015f * dimVol);
        addSine(s, 392.0f * dimDetune, 0.012f * dimVol, 0.012f * dimVol);

        // Sparse melodic notes (only every few beats)
        float times[] = {0.0f, 1.2f, 2.8f, 4.0f, 5.5f, 7.0f};
        int indices[] = {0, 2, 4, 3, 1, 4};
        for (int i = 0; i < 6; i++) {
            float t = times[i];
            float freq = notes[indices[i]] * dimDetune;
            float vol = 0.045f * dimVol;
            addTriangle(s, freq, 0.0f, vol, t, t + 0.03f);
            addTriangle(s, freq, vol, 0.0f, t + 0.03f, std::min(t + 1.5f, DUR));
        }

        // Gentle wind
        addNoise(s, 0.006f * dimVol, 0.01f * dimVol, 0.0f, 4.0f);
        addNoise(s, 0.01f * dimVol, 0.006f * dimVol, 4.0f, 8.0f);
        break;
    }

    case 9: { // VoidRealm - dissonant, unsettling low tones
        // Tritone-based dissonance: A1-Eb2
        addSine(s, 55.0f * dimDetune, 0.05f * dimVol, 0.05f * dimVol);
        addSine(s, 77.8f * dimDetune, 0.04f * dimVol, 0.04f * dimVol); // Tritone
        addSine(s, 36.7f * dimDetune, 0.035f * dimVol, 0.035f * dimVol); // Sub

        // Warping, modulated tones
        int samples = static_cast<int>(DUR * s.sampleRate);
        for (int i = 0; i < samples; i++) {
            float t = static_cast<float>(i) / s.sampleRate;
            // Slow wobble (detuning)
            float wobble = std::sin(2.0f * static_cast<float>(M_PI) * 0.3f * t) * 5.0f;
            float sample = std::sin(2.0f * static_cast<float>(M_PI) * (110.0f + wobble) * dimDetune * t) * 0.025f * dimVol;
            // Dissonant minor second
            sample += std::sin(2.0f * static_cast<float>(M_PI) * (116.5f + wobble * 0.7f) * dimDetune * t) * 0.015f * dimVol;
            s.data[i] = static_cast<Sint16>(std::clamp(
                static_cast<int>(s.data[i] + sample * 32767), -32767, 32767));
        }

        // Scattered unsettling pulses
        for (int p = 0; p < 6; p++) {
            float t = p * 1.3f + 0.2f;
            if (t < DUR) {
                addSweep(s, 200.0f * dimDetune, 50.0f * dimDetune, 0.04f * dimVol, 0.0f, t, std::min(t + 0.8f, DUR));
            }
        }

        // Dark noise floor
        addNoise(s, 0.012f * dimVol, 0.012f * dimVol);
        break;
    }

    case 0: { // VictorianClockwork - organ-like progression
        float bpm = 90.0f;
        float beatLen = 60.0f / bpm;

        // Organ chord progression: Am - Dm - E - Am
        // Each chord = 2 beats = 4 total chords across 8 seconds
        struct Chord { float f1, f2, f3; };
        Chord chords[] = {
            {220.0f, 261.6f, 329.6f},  // Am (A3, C4, E4)
            {146.8f, 174.6f, 220.0f},  // Dm (D3, F3, A3)
            {164.8f, 207.7f, 246.9f},  // E  (E3, G#3, B3)
            {220.0f, 261.6f, 329.6f},  // Am (A3, C4, E4)
        };

        float chordDur = DUR / 4.0f;
        for (int c = 0; c < 4; c++) {
            float t0 = c * chordDur;
            float t1 = t0 + chordDur;
            // Organ tones (square wave for organ character)
            addSquare(s, chords[c].f1 * dimDetune, 0.03f * dimVol, 0.03f * dimVol, t0, t1);
            addSquare(s, chords[c].f2 * dimDetune, 0.025f * dimVol, 0.025f * dimVol, t0, t1);
            addSquare(s, chords[c].f3 * dimDetune, 0.02f * dimVol, 0.02f * dimVol, t0, t1);
            // Sine layer for warmth
            addSine(s, chords[c].f1 * 0.5f * dimDetune, 0.02f * dimVol, 0.02f * dimVol, t0, t1);
        }

        // Clockwork tick rhythm
        int numBeats = static_cast<int>(DUR / beatLen);
        for (int i = 0; i < numBeats; i++) {
            float t = i * beatLen;
            addSine(s, 2000.0f, 0.025f * dimVol, 0.0f, t, t + 0.02f);
            float tock = t + beatLen * 0.5f;
            if (tock < DUR) {
                addSine(s, 1500.0f, 0.015f * dimVol, 0.0f, tock, tock + 0.02f);
            }
        }

        // Subtle gear noise
        addNoise(s, 0.005f * dimVol, 0.005f * dimVol);
        break;
    }

    case 3: { // AncientRuins - tribal drums, pentatonic scale
        float bpm = 100.0f;
        float beatLen = 60.0f / bpm;
        int numBeats = static_cast<int>(DUR / beatLen);

        // Minor pentatonic: A3, C4, D4, E4, G4
        float melody[] = {220.0f, 261.6f, 293.7f, 329.6f, 392.0f};

        // Tribal drum pattern
        for (int i = 0; i < numBeats; i++) {
            float t = i * beatLen;
            // Main drum hit pattern: 1-_-3-_-1-_-3-_ (accent on 1 and 3)
            if (i % 4 == 0) {
                addSweep(s, 120.0f, 50.0f, 0.12f * dimVol, 0.0f, t, t + 0.1f);
                addSine(s, 60.0f, 0.08f * dimVol, 0.0f, t, t + 0.12f);
            } else if (i % 4 == 2) {
                addSweep(s, 100.0f, 45.0f, 0.09f * dimVol, 0.0f, t, t + 0.08f);
            }
            // Shaker on off-beats
            if (i % 2 == 1) {
                addNoise(s, 0.035f * dimVol, 0.0f, t, t + 0.03f);
            }
            // High tom fills
            if (i % 8 == 5 || i % 8 == 7) {
                addSine(s, 300.0f, 0.04f * dimVol, 0.0f, t, t + 0.05f);
            }
        }

        // Pentatonic melody (sparse, call-and-response)
        float melTimes[] = {0.0f, 0.6f, 1.8f, 2.4f, 4.0f, 4.6f, 5.8f, 6.4f};
        int melIdx[] = {0, 2, 4, 3, 0, 1, 3, 2};
        for (int i = 0; i < 8; i++) {
            float t = melTimes[i];
            float freq = melody[melIdx[i]] * dimDetune;
            addTriangle(s, freq, 0.0f, 0.04f * dimVol, t, t + 0.02f);
            addTriangle(s, freq, 0.04f * dimVol, 0.0f, t + 0.02f, std::min(t + 0.5f, DUR));
        }

        // Drone (A2)
        addSine(s, 110.0f * dimDetune, 0.02f * dimVol, 0.02f * dimVol);
        break;
    }

    case 10: { // SpaceWestern - twangy notes, rhythmic
        float bpm = 105.0f;
        float beatLen = 60.0f / bpm;
        int numBeats = static_cast<int>(DUR / beatLen);

        // Twangy guitar-like notes (square wave with fast decay = pluck)
        // Mixolydian scale: A3, B3, C#4, D4, E4, F#4, G4
        float scale[] = {220.0f, 246.9f, 277.2f, 293.7f, 329.6f, 370.0f, 392.0f};

        // Rhythmic pattern: boom-chick-a-boom-chick
        for (int i = 0; i < numBeats; i++) {
            float t = i * beatLen;
            // Bass boom on 1, 3
            if (i % 4 == 0 || i % 4 == 2) {
                addSweep(s, 90.0f, 40.0f, 0.1f * dimVol, 0.0f, t, t + 0.08f);
            }
            // Chick (noise snap)
            if (i % 4 == 1 || i % 4 == 3) {
                addNoise(s, 0.05f * dimVol, 0.0f, t, t + 0.03f);
            }
            // Twangy melody (every 2 beats)
            if (i % 2 == 0) {
                int noteIdx = (i / 2) % 7;
                float freq = scale[noteIdx] * dimDetune;
                // Quick attack, medium decay = pluck
                addSquare(s, freq, 0.05f * dimVol, 0.0f, t, t + beatLen * 0.8f);
                // String vibrato (slight sweep)
                addSweep(s, freq, freq * 1.01f, 0.02f * dimVol, 0.0f, t + 0.05f, std::min(t + beatLen * 0.6f, DUR));
            }
        }

        // Desert wind
        addNoise(s, 0.007f * dimVol, 0.007f * dimVol);
        // Low hum
        addSine(s, 55.0f * dimDetune, 0.015f * dimVol, 0.015f * dimVol);
        break;
    }

    case 11: { // Biopunk - glitchy, irregular beats
        float bpm = 120.0f;
        float beatLen = 60.0f / bpm;
        int totalSteps = static_cast<int>(DUR / (beatLen * 0.25f)); // 16th note grid

        // Glitch bass drone
        addSine(s, 55.0f * dimDetune, 0.03f * dimVol, 0.03f * dimVol);

        // Irregular percussion pattern (pseudo-random from seed)
        int seed = 42;
        for (int i = 0; i < totalSteps; i++) {
            float t = i * beatLen * 0.25f;
            if (t >= DUR) break;

            seed = (seed * 1103515245 + 12345) & 0x7fffffff;
            int r = seed % 100;

            // Kick on some steps (higher probability on downbeats)
            bool isDownbeat = (i % 16 == 0 || i % 16 == 8);
            if (r < (isDownbeat ? 80 : 15)) {
                addSweep(s, 120.0f, 40.0f, 0.09f * dimVol, 0.0f, t, t + 0.06f);
            }

            // Glitch noise burst
            seed = (seed * 1103515245 + 12345) & 0x7fffffff;
            r = seed % 100;
            if (r < 20) {
                float glitchLen = 0.01f + (r % 4) * 0.01f;
                addNoise(s, 0.06f * dimVol, 0.0f, t, t + glitchLen);
            }

            // Random blip tone
            seed = (seed * 1103515245 + 12345) & 0x7fffffff;
            r = seed % 100;
            if (r < 12) {
                float freq = 200.0f + (r * 50.0f) * dimDetune;
                addSquare(s, freq, 0.035f * dimVol, 0.0f, t, t + 0.04f);
            }
        }

        // Organic bubbly modulation
        int samples = static_cast<int>(DUR * s.sampleRate);
        for (int i = 0; i < samples; i++) {
            float t = static_cast<float>(i) / s.sampleRate;
            float bubble = std::sin(2.0f * static_cast<float>(M_PI) * (400.0f + 200.0f * std::sin(t * 7.0f)) * t);
            float env = std::max(0.0f, std::sin(t * 3.5f)) * 0.008f * dimVol;
            s.data[i] = static_cast<Sint16>(std::clamp(
                static_cast<int>(s.data[i] + bubble * env * 32767), -32767, 32767));
        }
        break;
    }

    default: {
        // Fallback: simple ambient loop
        addSine(s, 110.0f * dimDetune, 0.04f * dimVol, 0.04f * dimVol);
        addSine(s, 164.8f * dimDetune, 0.025f * dimVol, 0.025f * dimVol);
        addNoise(s, 0.01f * dimVol, 0.01f * dimVol);
        float bpm = 100.0f;
        float beatLen = 60.0f / bpm;
        int numBeats = static_cast<int>(DUR / beatLen);
        for (int i = 0; i < numBeats; i++) {
            float t = i * beatLen;
            addSweep(s, 100.0f, 40.0f, 0.08f * dimVol, 0.0f, t, t + 0.07f);
        }
        break;
    }
    }

    return toChunk(s);
}

Mix_Chunk* SoundGenerator::bossMusic(int bossPhase) {
    const float DUR = 8.0f;
    auto s = generate(DUR);

    // Phase affects tempo and intensity: 1=building, 2=intense, 3=frantic
    float bpm = 130.0f + bossPhase * 15.0f; // 145, 160, 175
    float beatLen = 60.0f / bpm;
    int numBeats = static_cast<int>(DUR / beatLen);
    float intensity = 0.7f + bossPhase * 0.1f; // 0.8, 0.9, 1.0

    // Power bass drone (A1 + E2, dark fifth)
    addSine(s, 55.0f, 0.06f * intensity, 0.06f * intensity);
    addSine(s, 82.4f, 0.04f * intensity, 0.04f * intensity);
    addSquare(s, 27.5f, 0.025f * intensity, 0.025f * intensity); // Sub rumble

    // Driving drum pattern
    for (int i = 0; i < numBeats; i++) {
        float t = i * beatLen;

        // Heavy kick every beat
        addSweep(s, 180.0f, 35.0f, 0.13f * intensity, 0.0f, t, t + 0.08f);
        addSine(s, 40.0f, 0.08f * intensity, 0.0f, t, t + 0.1f);

        // Snare on 2, 4
        if (i % 2 == 1) {
            addNoise(s, 0.08f * intensity, 0.0f, t, t + 0.05f);
            addSine(s, 250.0f, 0.04f * intensity, 0.0f, t, t + 0.04f);
        }

        // Double-time hi-hat (16th notes)
        for (int sub = 0; sub < 4; sub++) {
            float ht = t + sub * beatLen * 0.25f;
            if (ht >= DUR) break;
            float hatVol = (sub == 0 || sub == 2) ? 0.03f : 0.02f;
            addNoise(s, hatVol * intensity, 0.0f, ht, ht + 0.015f);
        }

        // Aggressive bass riff (minor scale)
        float bassNotes[] = {55.0f, 65.4f, 73.4f, 82.4f, 55.0f, 82.4f, 73.4f, 65.4f};
        float bassFreq = bassNotes[i % 8];
        addSquare(s, bassFreq, 0.05f * intensity, 0.01f * intensity, t, t + beatLen * 0.8f);
    }

    // Phase-specific elements
    if (bossPhase >= 2) {
        // Power chord stabs (phase 2+): every 2 beats
        for (int i = 0; i < numBeats; i += 2) {
            float t = i * beatLen;
            // Root + fifth + octave power chord
            addSquare(s, 110.0f, 0.03f * intensity, 0.0f, t, t + beatLen * 0.6f);
            addSquare(s, 164.8f, 0.025f * intensity, 0.0f, t, t + beatLen * 0.5f);
            addSquare(s, 220.0f, 0.02f * intensity, 0.0f, t, t + beatLen * 0.4f);
        }
    }

    if (bossPhase >= 3) {
        // Frantic ascending arpeggios (phase 3)
        float arpNotes[] = {220.0f, 261.6f, 329.6f, 392.0f, 440.0f, 523.3f, 659.3f, 784.0f};
        float arpLen = beatLen * 0.25f;
        for (int i = 0; i < numBeats * 4; i++) {
            float t = i * arpLen;
            if (t >= DUR) break;
            float freq = arpNotes[i % 8];
            addSine(s, freq, 0.025f * intensity, 0.0f, t, t + arpLen * 0.7f);
        }

        // Warning alarm tone (tritone oscillation)
        int samples = static_cast<int>(DUR * s.sampleRate);
        for (int i = 0; i < samples; i++) {
            float t = static_cast<float>(i) / s.sampleRate;
            float alarm = std::sin(2.0f * static_cast<float>(M_PI) * 3.0f * t); // 3Hz pulse
            float tone = std::sin(2.0f * static_cast<float>(M_PI) * 440.0f * t) * 0.015f * intensity;
            tone *= (alarm > 0.0f) ? 1.0f : 0.3f;
            s.data[i] = static_cast<Sint16>(std::clamp(
                static_cast<int>(s.data[i] + tone * 32767), -32767, 32767));
        }
    }

    // Epic rising sweep every 4 seconds
    addSweep(s, 150.0f, 600.0f, 0.0f, 0.04f * intensity, 0.0f, 1.5f);
    addSweep(s, 150.0f, 800.0f, 0.0f, 0.05f * intensity, 4.0f, 5.5f);

    return toChunk(s);
}
