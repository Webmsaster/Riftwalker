#pragma once
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string>
#include <array>

class ScreenEffects {
public:
    void update(float dt);
    void render(SDL_Renderer* renderer, int screenW, int screenH, TTF_Font* font = nullptr);

    // Post-processing pass (call after world render, before HUD)
    void renderPostProcessing(SDL_Renderer* renderer, int screenW, int screenH,
                              int currentDimension, float dimBlendAlpha);

    // Triggers
    void triggerKillFlash();
    void triggerBossIntro(const char* bossName, const char* subtitle = nullptr);
    void triggerDimensionRipple();
    void cancelBossIntro() { m_bossIntroTimer = 0; m_bossName.clear(); m_bossSubtitle.clear(); }

    // Query
    bool isBossIntroActive() const { return m_bossIntroTimer > 0; }

    // Continuous states
    void setHP(float hpPercent) { m_hpPercent = hpPercent; }
    void setEntropy(float entropy) { m_entropy = entropy; }
    void setVoidStorm(bool active) { m_voidStormActive = active; }

    // Post-processing toggles
    bool vignetteEnabled = true;
    bool colorGradingEnabled = true;
    bool ambientParticlesEnabled = true;
    bool bloomEnabled = true;
    bool dynamicLightingEnabled = true;

    // Dynamic lighting: renders a dark overlay with light circles cut out
    // Call AFTER world/entity rendering, BEFORE post-processing
    void renderDynamicLighting(SDL_Renderer* renderer, int screenW, int screenH);

    // Register light sources (screen-space) — cleared each frame with clearGlowPoints
    struct LightSource {
        float x, y, radius;
        Uint8 r, g, b;
        float intensity; // 0-1
    };
    void registerLight(float screenX, float screenY, float radius, Uint8 r, Uint8 g, Uint8 b, float intensity);

    // Bloom: register bright spots for glow overlay (screen-space coordinates)
    void registerGlowPoint(float screenX, float screenY, float radius, Uint8 r, Uint8 g, Uint8 b, Uint8 intensity);
    void clearGlowPoints();

private:
    float m_time = 0.0f;

    // Vignette (always on)
    float m_hpPercent = 1.0f;

    // Kill flash
    float m_killFlashTimer = 0.0f;

    // Boss intro title card
    static constexpr float kBossIntroDuration = 2.5f;
    float m_bossIntroTimer = 0.0f;
    std::string m_bossName;
    std::string m_bossSubtitle;

    // Dimension ripple
    float m_rippleTimer = 0.0f;

    // Entropy glitch
    float m_entropy = 0.0f;
    float m_glitchTimer = 0.0f;

    // Void Storm
    bool m_voidStormActive = false;

    // --- Post-processing state ---

    // Ambient particles (floating dust motes / dimensional particles)
    struct AmbientParticle {
        float x, y;           // Screen position
        float vx, vy;         // Velocity
        float size;            // 1-3 px
        float alpha;           // Current alpha (20-60)
        float alphaTarget;     // Target alpha for smooth fade
        float lifetime;        // Time until respawn
        Uint8 r, g, b;
    };
    static constexpr int kMaxAmbientParticles = 30;
    std::array<AmbientParticle, kMaxAmbientParticles> m_ambientParticles{};
    bool m_ambientParticlesInitialized = false;
    void initAmbientParticles(int screenW, int screenH);
    void updateAmbientParticles(float dt, int screenW, int screenH, int currentDimension);
    void renderAmbientParticles(SDL_Renderer* renderer);

    // Cinematic vignette (post-processing version — separate from the HP vignette in render())
    void renderVignette(SDL_Renderer* renderer, int screenW, int screenH);

    // Color grading (dimension-based mood tint)
    void renderColorGrading(SDL_Renderer* renderer, int screenW, int screenH,
                            int currentDimension, float dimBlendAlpha);

    // Bloom/glow simulation (soft additive-blend at registered bright points)
    struct GlowPoint {
        float x, y, radius;
        Uint8 r, g, b, intensity;
    };
    static constexpr int kMaxGlowPoints = 32;
    int m_glowPointCount = 0;
    std::array<GlowPoint, kMaxGlowPoints> m_glowPoints{};
    void renderBloom(SDL_Renderer* renderer);

    // Track current dimension for ambient particle colors
    int m_currentDimension = 1;

    // Dynamic lighting state
    static constexpr int kMaxLights = 48;
    int m_lightCount = 0;
    std::array<LightSource, kMaxLights> m_lights{};
};
