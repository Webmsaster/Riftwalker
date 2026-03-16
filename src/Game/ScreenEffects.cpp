#include "Game/ScreenEffects.h"
#include <cmath>
#include <cstdlib>
#include <SDL2/SDL_ttf.h>

void ScreenEffects::update(float dt) {
    m_time += dt;
    if (m_killFlashTimer > 0) m_killFlashTimer -= dt;
    if (m_bossIntroTimer > 0) m_bossIntroTimer -= dt;
    if (m_rippleTimer > 0) m_rippleTimer -= dt;
    if (m_entropy > 80.0f) m_glitchTimer += dt;
    else m_glitchTimer = 0;
}

void ScreenEffects::render(SDL_Renderer* renderer, int screenW, int screenH) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // 1. Vignette (always, stronger at low HP)
    {
        float strength = 0.15f + (1.0f - m_hpPercent) * 0.25f;
        Uint8 vigA = static_cast<Uint8>(strength * 255);
        int edgeW = screenW / 6;
        int edgeH = screenH / 6;
        // Top
        for (int i = 0; i < edgeH; i++) {
            Uint8 a = static_cast<Uint8>(vigA * (1.0f - static_cast<float>(i) / edgeH));
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, a);
            SDL_Rect r = {0, i, screenW, 1};
            SDL_RenderFillRect(renderer, &r);
        }
        // Bottom
        for (int i = 0; i < edgeH; i++) {
            Uint8 a = static_cast<Uint8>(vigA * (static_cast<float>(i) / edgeH));
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, a);
            SDL_Rect r = {0, screenH - edgeH + i, screenW, 1};
            SDL_RenderFillRect(renderer, &r);
        }
        // Left
        for (int i = 0; i < edgeW; i++) {
            Uint8 a = static_cast<Uint8>(vigA * (1.0f - static_cast<float>(i) / edgeW));
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, a);
            SDL_Rect r = {i, 0, 1, screenH};
            SDL_RenderFillRect(renderer, &r);
        }
        // Right
        for (int i = 0; i < edgeW; i++) {
            Uint8 a = static_cast<Uint8>(vigA * (static_cast<float>(i) / edgeW));
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, a);
            SDL_Rect r = {screenW - edgeW + i, 0, 1, screenH};
            SDL_RenderFillRect(renderer, &r);
        }
    }

    // 2. Low HP pulse (red border, heartbeat rhythm)
    if (m_hpPercent < 0.25f) {
        float heartbeat = std::sin(m_time * 6.0f); // ~1Hz heartbeat
        heartbeat = heartbeat > 0 ? heartbeat : 0;
        float intensity = (0.25f - m_hpPercent) / 0.25f;
        Uint8 a = static_cast<Uint8>(heartbeat * intensity * 80);
        SDL_SetRenderDrawColor(renderer, 200, 0, 0, a);
        int border = 4;
        SDL_Rect top = {0, 0, screenW, border};
        SDL_Rect bot = {0, screenH - border, screenW, border};
        SDL_Rect left = {0, 0, border, screenH};
        SDL_Rect right = {screenW - border, 0, border, screenH};
        SDL_RenderFillRect(renderer, &top);
        SDL_RenderFillRect(renderer, &bot);
        SDL_RenderFillRect(renderer, &left);
        SDL_RenderFillRect(renderer, &right);
    }

    // 3. Kill flash
    if (m_killFlashTimer > 0) {
        float t = m_killFlashTimer / 0.05f;
        Uint8 a = static_cast<Uint8>(t * 30);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, a);
        SDL_Rect full = {0, 0, screenW, screenH};
        SDL_RenderFillRect(renderer, &full);
    }

    // 4. Boss intro cinematic bars + name
    if (m_bossIntroTimer > 0) {
        float t = m_bossIntroTimer;
        float barH = 0;
        float nameAlpha = 0;
        if (t > 1.0f) {
            // Bars growing (1.5s to 1.0s)
            barH = (1.5f - t) / 0.5f * 60.0f;
            barH = std::max(0.0f, std::min(60.0f, barH));
        } else if (t > 0.3f) {
            // Full bars + name
            barH = 60.0f;
            nameAlpha = 1.0f;
        } else {
            // Fading out
            barH = (t / 0.3f) * 60.0f;
            nameAlpha = t / 0.3f;
        }
        int bH = static_cast<int>(barH);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 220);
        SDL_Rect topBar = {0, 0, screenW, bH};
        SDL_Rect botBar = {0, screenH - bH, screenW, bH};
        SDL_RenderFillRect(renderer, &topBar);
        SDL_RenderFillRect(renderer, &botBar);

        // Boss name in center (using basic SDL drawing, no font for simplicity)
        if (nameAlpha > 0 && !m_bossName.empty()) {
            Uint8 na = static_cast<Uint8>(nameAlpha * 200);
            // Simple indicator line under boss area
            SDL_SetRenderDrawColor(renderer, 180, 80, 255, na);
            int lineW = static_cast<int>(m_bossName.size() * 14);
            SDL_Rect nameLine = {screenW / 2 - lineW / 2, screenH / 2 + 20, lineW, 2};
            SDL_RenderFillRect(renderer, &nameLine);
            SDL_Rect nameLinetop = {screenW / 2 - lineW / 2, screenH / 2 - 22, lineW, 2};
            SDL_RenderFillRect(renderer, &nameLinetop);
        }
    }

    // 5. Dimension ripple (sweep + scanlines + color flash)
    if (m_rippleTimer > 0) {
        float t = 1.0f - (m_rippleTimer / 0.5f);

        // Vertical sweep line moving across screen
        int sweepX = static_cast<int>(t * screenW);
        Uint8 sweepA = static_cast<Uint8>((1.0f - t) * 180);
        SDL_SetRenderDrawColor(renderer, 160, 220, 255, sweepA);
        SDL_Rect sweep = {sweepX - 2, 0, 4, screenH};
        SDL_RenderFillRect(renderer, &sweep);
        // Glow around sweep
        SDL_SetRenderDrawColor(renderer, 100, 180, 255, static_cast<Uint8>(sweepA / 3));
        SDL_Rect sweepGlow = {sweepX - 15, 0, 30, screenH};
        SDL_RenderFillRect(renderer, &sweepGlow);

        // Brief full-screen color tint at start
        if (t < 0.3f) {
            Uint8 tintA = static_cast<Uint8>((0.3f - t) / 0.3f * 40);
            SDL_SetRenderDrawColor(renderer, 80, 160, 255, tintA);
            SDL_Rect full = {0, 0, screenW, screenH};
            SDL_RenderFillRect(renderer, &full);
        }

        // Horizontal scanlines spreading from center
        int spread = static_cast<int>(t * screenH / 2);
        Uint8 a = static_cast<Uint8>((1.0f - t) * 50);
        for (int i = 0; i < 12; i++) {
            int offset = spread + i * 3;
            if (offset > screenH / 2) break;
            SDL_SetRenderDrawColor(renderer, 100, 200, 255, static_cast<Uint8>(a * (1.0f - i * 0.08f)));
            SDL_Rect lineUp = {0, screenH / 2 - offset, screenW, 1};
            SDL_Rect lineDown = {0, screenH / 2 + offset, screenW, 1};
            SDL_RenderFillRect(renderer, &lineUp);
            SDL_RenderFillRect(renderer, &lineDown);
        }
    }

    // 6. Void Storm overlay (pulsing purple danger indicator)
    if (m_voidStormActive) {
        float pulse = 0.5f + 0.5f * std::sin(m_time * 8.0f);
        Uint8 a = static_cast<Uint8>(20 + pulse * 35);
        SDL_SetRenderDrawColor(renderer, 120, 0, 180, a);
        SDL_Rect full = {0, 0, screenW, screenH};
        SDL_RenderFillRect(renderer, &full);
        // Danger border
        int border = 6;
        Uint8 ba = static_cast<Uint8>(60 + pulse * 80);
        SDL_SetRenderDrawColor(renderer, 180, 0, 255, ba);
        SDL_Rect top = {0, 0, screenW, border};
        SDL_Rect bot = {0, screenH - border, screenW, border};
        SDL_Rect left = {0, 0, border, screenH};
        SDL_Rect right = {screenW - border, 0, border, screenH};
        SDL_RenderFillRect(renderer, &top);
        SDL_RenderFillRect(renderer, &bot);
        SDL_RenderFillRect(renderer, &left);
        SDL_RenderFillRect(renderer, &right);
    }

    // 7. Entropy glitch
    if (m_entropy > 80.0f && m_glitchTimer > 0) {
        float intensity = (m_entropy - 80.0f) / 20.0f;
        int numGlitches = static_cast<int>(intensity * 5);
        for (int i = 0; i < numGlitches; i++) {
            int gx = std::rand() % screenW;
            int gy = std::rand() % screenH;
            int gw = 10 + std::rand() % 40;
            int gh = 2 + std::rand() % 4;
            Uint8 gr = static_cast<uint8_t>(std::rand() % 256);
            Uint8 gg = static_cast<uint8_t>(std::rand() % 256);
            Uint8 gb = static_cast<uint8_t>(std::rand() % 256);
            Uint8 ga = static_cast<Uint8>(intensity * 40);
            SDL_SetRenderDrawColor(renderer, gr, gg, gb, ga);
            SDL_Rect glitch = {gx, gy, gw, gh};
            SDL_RenderFillRect(renderer, &glitch);
        }
    }
}

void ScreenEffects::triggerKillFlash() {
    m_killFlashTimer = 0.05f;
}

void ScreenEffects::triggerBossIntro(const char* bossName) {
    m_bossIntroTimer = 1.5f;
    m_bossName = bossName;
}

void ScreenEffects::triggerDimensionRipple() {
    m_rippleTimer = 0.5f;
}
