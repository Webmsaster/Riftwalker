#include "Game.h"
#include "Localization.h"
#include "Game/ClassSystem.h"
#include <tracy/Tracy.hpp>
#include "States/MenuState.h"
#include "States/PlayState.h"
#include "States/PauseState.h"
#include "States/UpgradeState.h"
#include "States/RunSummaryState.h"
#include "States/GameOverState.h"
#include "States/OptionsState.h"
#include "States/DifficultySelectState.h"
#include "States/NGPlusSelectState.h"
#include "States/KeybindingsState.h"
#include "States/AchievementsState.h"
#include "States/ShopState.h"
#include "States/ClassSelectState.h"
#include "States/ChallengeSelectState.h"
#include "States/BestiaryState.h"
#include "States/LoreState.h"
#include "States/EndingState.h"
#include "States/RunHistoryState.h"
#include "States/DailyLeaderboardState.h"
#include "States/CreditsState.h"
#include "States/SplashState.h"
#include "States/TutorialState.h"
#include "ScreenCapture.h"
#include "VisualTest.h"
#include "SaveUtils.h"
#include <SDL2/SDL_image.h>
#include <fstream>
#include <sstream>

bool g_screenshotRequested = false;

// Runtime-initialized from display resolution
int Game::WINDOW_WIDTH = 1920;
int Game::WINDOW_HEIGHT = 1080;

Game::Game() {}

Game::~Game() {
    shutdown();
}

bool Game::init() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) < 0) {
        SDL_Log("SDL init failed: %s", SDL_GetError());
        return false;
    }

    // Best-quality filtering for smooth sprite scaling (anisotropic)
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "2");

    // Detect native display resolution for maximum quality
    SDL_DisplayMode displayMode;
    if (SDL_GetDesktopDisplayMode(0, &displayMode) == 0) {
        WINDOW_WIDTH = displayMode.w;
        WINDOW_HEIGHT = displayMode.h;
        SDL_Log("Native display: %dx%d @ %dHz", displayMode.w, displayMode.h, displayMode.refresh_rate);
    } else {
        SDL_Log("Could not detect display mode, using %dx%d", WINDOW_WIDTH, WINDOW_HEIGHT);
    }

    if (IMG_Init(IMG_INIT_PNG) == 0) {
        SDL_Log("SDL_image init failed: %s", IMG_GetError());
    }

    if (TTF_Init() < 0) {
        SDL_Log("SDL_ttf init failed: %s", TTF_GetError());
    }

    AudioManager::instance().init();

    m_window = std::make_unique<Window>("Riftwalker", WINDOW_WIDTH, WINDOW_HEIGHT);
    // Logical resolution: 2560x1440 (all UI coordinates scaled for 2K)
    // Physical window renders at selected/native resolution, SDL scales automatically
    SDL_RenderSetLogicalSize(m_window->getSDLRenderer(), SCREEN_WIDTH, SCREEN_HEIGHT);
    ResourceManager::instance().init(m_window->getSDLRenderer());

    // Generate placeholder sprites if missing — written to assets/textures/placeholders/
    // Safe to call even if real sprites exist; skips files already on disk.
    ResourceManager::instance().ensurePlaceholderTextures();

    // Load font - try several common paths
    const char* fontPaths[] = {
        "assets/fonts/default.ttf",
        "C:/Windows/Fonts/consola.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "C:/Windows/Fonts/segoeui.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
    };

    for (auto path : fontPaths) {
        m_font = TTF_OpenFont(path, 36);
        if (m_font) break;
    }

    if (m_font) {
        // Enable light auto-hinting for crisp text at all sizes
        TTF_SetFontHinting(m_font, TTF_HINTING_LIGHT);
    } else {
        SDL_Log("Warning: No font loaded. Text will not be rendered.");
    }

    // Create states
    m_states[StateID::Splash] = std::make_unique<SplashState>();
    m_states[StateID::Menu] = std::make_unique<MenuState>();
    m_states[StateID::Play] = std::make_unique<PlayState>();
    m_states[StateID::Pause] = std::make_unique<PauseState>();
    m_states[StateID::Upgrade] = std::make_unique<UpgradeState>();
    m_states[StateID::RunSummary] = std::make_unique<RunSummaryState>();
    m_states[StateID::GameOver] = std::make_unique<GameOverState>();
    m_states[StateID::Options] = std::make_unique<OptionsState>();
    m_states[StateID::DifficultySelect] = std::make_unique<DifficultySelectState>();
    m_states[StateID::NGPlusSelect] = std::make_unique<NGPlusSelectState>();
    m_states[StateID::Keybindings] = std::make_unique<KeybindingsState>();
    m_states[StateID::Achievements] = std::make_unique<AchievementsState>();
    m_states[StateID::Shop] = std::make_unique<ShopState>();
    m_states[StateID::ClassSelect] = std::make_unique<ClassSelectState>();
    m_states[StateID::ChallengeSelect] = std::make_unique<ChallengeSelectState>();
    m_states[StateID::Bestiary] = std::make_unique<BestiaryState>();
    m_states[StateID::Lore] = std::make_unique<LoreState>();
    m_states[StateID::Ending] = std::make_unique<EndingState>();
    m_states[StateID::RunHistory] = std::make_unique<RunHistoryState>();
    m_states[StateID::DailyLeaderboard] = std::make_unique<DailyLeaderboardState>();
    m_states[StateID::Credits] = std::make_unique<CreditsState>();
    m_states[StateID::Tutorial] = std::make_unique<TutorialState>();

    for (auto& [id, state] : m_states) {
        state->game = this;
    }

    // Load save data
    loadSaveData();

    // Load achievements
    m_achievements.init();
    m_achievements.load("riftwalker_achievements.dat");

    // Init and load lore
    m_lore.init();
    m_lore.load("riftwalker_lore.dat");

    // Load custom keybindings (falls back to defaults if file missing)
    m_input.loadBindings("riftwalker_bindings.cfg");

    // Start at splash (or directly at play for automated test modes)
    extern bool g_autoSmokeTest;
    extern bool g_autoPlaytest;
    if (g_autoSmokeTest || g_autoPlaytest)
        changeState(StateID::Play);
    else
        changeState(StateID::Splash);

    m_running = true;
    return true;
}

void Game::run() {
    extern bool g_autoScreenshot;
    int frameCount = 0;

    // Visual test mode
    VisualTest visualTest;
    if (g_visualTest) {
        visualTest.init();
    }

    while (m_running) {
        m_timer.update();

        handleEvents();

        // Update input AFTER SDL events are polled
        m_input.update();

        // Fixed timestep for physics
        int steps = 0;
        while (m_timer.shouldStep() && steps < Timer::MAX_STEPS_PER_FRAME) {
            update();
            // Clear buffered press/release after each step so one-shot actions
            // (jump, attack, etc.) only fire once, not once per step
            m_input.clearPressedBuffers();
            steps++;
        }

        render();
        frameCount++;

        // --screenshot: capture after splash screen ends (~7s at 60fps), then exit
        if (g_autoScreenshot && frameCount == 420) {
            g_screenshotRequested = true;
        }
        if (g_autoScreenshot && frameCount == 425) {
            m_running = false;
        }

        // --visual-test: navigate states and capture screenshots
        if (g_visualTest && visualTest.update(frameCount, this)) {
            m_running = false;
        }

        FrameMark;
    }
}

void Game::handleEvents() {
    ZoneScopedN("HandleEvents");
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            m_running = false;
            return;
        }

        if (event.type == SDL_KEYDOWN && event.key.keysym.scancode == SDL_SCANCODE_F11) {
            m_window->toggleFullscreen();
        }

        // F9/F12: Screenshot (works in any state)
        if (event.type == SDL_KEYDOWN &&
            (event.key.keysym.scancode == SDL_SCANCODE_F9 ||
             event.key.keysym.scancode == SDL_SCANCODE_F12)) {
            g_screenshotRequested = true;
        }

        if (event.type == SDL_WINDOWEVENT &&
            event.window.event == SDL_WINDOWEVENT_RESIZED) {
            m_window->onResize(event.window.data1, event.window.data2);
        }

        // Map raw window mouse coordinates to logical (2560x1440) space so all
        // states work correctly regardless of current window size.
        SDL_Renderer* ren = m_window->getSDLRenderer();
        if (event.type == SDL_MOUSEMOTION) {
            float lx, ly;
            SDL_RenderWindowToLogical(ren,
                event.motion.x, event.motion.y, &lx, &ly);
            event.motion.x = static_cast<Sint32>(lx);
            event.motion.y = static_cast<Sint32>(ly);
        } else if (event.type == SDL_MOUSEBUTTONDOWN ||
                   event.type == SDL_MOUSEBUTTONUP) {
            float lx, ly;
            SDL_RenderWindowToLogical(ren,
                event.button.x, event.button.y, &lx, &ly);
            event.button.x = static_cast<Sint32>(lx);
            event.button.y = static_cast<Sint32>(ly);
        }

        m_input.handleEvent(event);

        // Block input to states during transitions to prevent
        // accidental actions while the screen is obscured
        if (m_currentState && m_transition == TransitionState::None) {
            m_currentState->handleEvent(event);
        }
    }
}

void Game::update() {
    ZoneScopedN("GameUpdate");
    // Handle screen transition
    if (m_transition != TransitionState::None) {
        m_transitionTimer += Timer::FIXED_TIMESTEP;
        float rawProgress = m_transitionTimer / m_transitionDuration;
        if (rawProgress > 1.0f) rawProgress = 1.0f;

        // Apply easing for smooth acceleration/deceleration
        float progress = easeInOutCubic(rawProgress);

        if (m_transition == TransitionState::FadeOut) {
            m_transitionAlpha = static_cast<Uint8>(progress * 255);
            if (rawProgress >= 1.0f) {
                // Actually change state now
                if (m_pendingPush) {
                    auto it = m_states.find(m_pendingState);
                    if (it != m_states.end()) {
                        if (m_currentState) m_stateStack.push(m_currentState);
                        m_currentState = it->second.get();
                        m_currentState->enter();
                    }
                } else {
                    while (!m_stateStack.empty()) {
                        m_stateStack.top()->exit();
                        m_stateStack.pop();
                    }
                    if (m_currentState) m_currentState->exit();
                    auto it = m_states.find(m_pendingState);
                    if (it != m_states.end()) {
                        m_currentState = it->second.get();
                        m_currentState->enter();
                    }
                    saveSaveData();
                }
                m_transition = TransitionState::FadeIn;
                m_transitionTimer = 0;
            }
        } else if (m_transition == TransitionState::FadeIn) {
            m_transitionAlpha = static_cast<Uint8>((1.0f - progress) * 255);
            if (rawProgress >= 1.0f) {
                m_transition = TransitionState::None;
                m_transitionAlpha = 0;
            }
        }

        // Block state updates during the last half of fade-out
        // so the old state doesn't process input while invisible
        if (m_transition == TransitionState::FadeOut && rawProgress > 0.5f)
            return;
    }

    if (m_currentState) {
        m_currentState->update(Timer::FIXED_TIMESTEP);
    }
}

void Game::render() {
    ZoneScopedN("GameRender");
    SDL_Renderer* renderer = m_window->getSDLRenderer();

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    // Render state stack (bottom up for overlay states), then current state on top
    if (!m_stateStack.empty()) {
        GameState* stackArr[16];
        int stackSize = 0;
        auto tempStack = m_stateStack;
        while (!tempStack.empty() && stackSize < 16) {
            stackArr[stackSize++] = tempStack.top();
            tempStack.pop();
        }
        // Render from bottom to top
        for (int i = stackSize - 1; i >= 0; i--) {
            stackArr[i]->render(renderer);
        }
        // Render current (top) state last
        if (m_currentState) {
            m_currentState->render(renderer);
        }
    } else if (m_currentState) {
        m_currentState->render(renderer);
    }

    // Transition overlay: style-dependent effect
    if (m_transitionAlpha > 0) {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        float progress = static_cast<float>(m_transitionAlpha) / 255.0f;
        SDL_Rect full = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};

        switch (m_transitionStyle) {
        case TransitionStyle::Rift: {
            // Purple rift dimension warp
            SDL_SetRenderDrawColor(renderer, 40, 20, 60, m_transitionAlpha);
            SDL_RenderFillRect(renderer, &full);

            // Glitch scanlines during mid-transition
            if (progress > 0.2f && progress < 0.95f) {
                int lineCount = static_cast<int>(progress * 12);
                Uint32 ticks = SDL_GetTicks();
                for (int i = 0; i < lineCount; i++) {
                    int y = ((i * 7919 + ticks / 50) % SCREEN_HEIGHT);
                    int h = 1 + (i % 3);
                    int offset = static_cast<int>((((i * 3251 + ticks / 30) % 40) - 20) * progress);
                    Uint8 la = static_cast<Uint8>(progress * 100);
                    if (i % 2 == 0)
                        SDL_SetRenderDrawColor(renderer, 150, 50, 200, la);
                    else
                        SDL_SetRenderDrawColor(renderer, 50, 150, 200, la);
                    SDL_Rect line = {offset, y, SCREEN_WIDTH, h};
                    SDL_RenderFillRect(renderer, &line);
                }
            }

            // Central rift flash at peak
            if (progress > 0.7f) {
                float flashIntensity = (progress - 0.7f) / 0.3f;
                Uint8 fa = static_cast<Uint8>(flashIntensity * 80);
                SDL_SetRenderDrawColor(renderer, 200, 150, 255, fa);
                int fh = static_cast<int>(flashIntensity * 40);
                SDL_Rect flash = {0, SCREEN_HEIGHT / 2 - fh, SCREEN_WIDTH, fh * 2};
                SDL_RenderFillRect(renderer, &flash);
            }
            break;
        }
        case TransitionStyle::Death: {
            // Red death fade with vignette
            Uint8 r = static_cast<Uint8>(60 + 40 * progress);
            SDL_SetRenderDrawColor(renderer, r, 5, 5, m_transitionAlpha);
            SDL_RenderFillRect(renderer, &full);

            // Vignette darkening from edges at high progress
            if (progress > 0.4f) {
                float vignetteStrength = (progress - 0.4f) / 0.6f;
                Uint8 va = static_cast<Uint8>(vignetteStrength * 120);
                // Top/bottom bars closing in
                int barH = static_cast<int>(vignetteStrength * SCREEN_HEIGHT * 0.15f);
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, va);
                SDL_Rect topBar = {0, 0, SCREEN_WIDTH, barH};
                SDL_Rect botBar = {0, SCREEN_HEIGHT - barH, SCREEN_WIDTH, barH};
                SDL_RenderFillRect(renderer, &topBar);
                SDL_RenderFillRect(renderer, &botBar);
            }
            break;
        }
        case TransitionStyle::Subtle: {
            // Simple dark fade for menu transitions
            SDL_SetRenderDrawColor(renderer, 10, 10, 15, m_transitionAlpha);
            SDL_RenderFillRect(renderer, &full);
            break;
        }
        }
    }

    // Screenshot capture (F9/F12 hotkey or playtest auto-trigger)
    if (g_screenshotRequested) {
        g_screenshotRequested = false;
        auto path = ScreenCapture::generateFilename("riftwalker");
        bool ok = ScreenCapture::captureScreenshot(renderer, path);
        SDL_Log("Screenshot %s: %s", ok ? "SAVED" : "FAILED", path.c_str());
    }

    SDL_RenderPresent(renderer);
}

void Game::changeState(StateID id) {
    if (m_transition != TransitionState::None) return; // already transitioning
    m_pendingState = id;
    m_pendingPush = false;
    m_transitionStyle = pickTransitionStyle(id);
    m_transition = TransitionState::FadeOut;
    m_transitionTimer = 0;
}

Game::TransitionStyle Game::pickTransitionStyle(StateID target) const {
    // Death/GameOver gets the red death transition
    if (target == StateID::GameOver || target == StateID::RunSummary)
        return TransitionStyle::Death;
    // Play gets the rift warp (entering a run)
    if (target == StateID::Play)
        return TransitionStyle::Rift;
    // Menu sub-screens get subtle fade
    return TransitionStyle::Subtle;
}

void Game::pushState(StateID id) {
    if (m_transition != TransitionState::None) return; // Block during transition
    // Push is immediate (e.g. pause menu) - no fade needed
    auto it = m_states.find(id);
    if (it != m_states.end()) {
        if (m_currentState) m_stateStack.push(m_currentState);
        m_currentState = it->second.get();
        m_currentState->enter();
    }
}

void Game::popState() {
    if (!m_stateStack.empty()) {
        if (m_currentState) m_currentState->exit();
        saveSaveData(); // Save after state exit to persist shop purchases etc.
        m_currentState = m_stateStack.top();
        m_stateStack.pop();
        if (m_currentState) m_currentState->onResume();
    }
}

void Game::loadSaveData() {
    std::ifstream file = openWithBackupFallback("riftwalker_save.dat");
    if (file.is_open()) {
        std::stringstream ss;
        ss << file.rdbuf();
        m_upgrades.deserialize(ss.str());
        file.close();
    }

    // Load audio/visual settings (with .bak fallback — saveSettings uses atomicSave)
    std::ifstream cfg = openWithBackupFallback("riftwalker_settings.cfg");
    if (cfg.is_open()) {
        std::string key;
        float value;
        while (cfg >> key >> value) {
            if (key == "sfx_volume")           g_sfxVolume      = std::clamp(value, 0.0f, 1.0f);
            else if (key == "music_volume")    g_musicVolume    = std::clamp(value, 0.0f, 1.0f);
            else if (key == "shake_intensity") g_shakeIntensity = std::clamp(value, 0.0f, 2.0f);
            else if (key == "hud_opacity")     g_hudOpacity     = std::clamp(value, 0.0f, 1.0f);
            else if (key == "color_blind")     g_colorBlindMode = std::clamp(static_cast<int>(value), 0, 3);
            else if (key == "hud_scale")       g_hudScale       = std::clamp(value, 0.5f, 2.0f);
            else if (key == "master_volume")   { AudioManager::instance().setMasterVolume(std::clamp(value, 0.0f, 1.0f)); }
            else if (key == "fullscreen")      { if (m_window && static_cast<int>(value) != 0 && !m_window->isFullscreen()) m_window->toggleFullscreen(); }
            else if (key == "rumble")          { m_input.setRumbleEnabled(static_cast<int>(value) != 0); }
            else if (key == "language")        { Localization::instance().setLanguage(static_cast<int>(value) == 1 ? Lang::DE : Lang::EN); }
            else if (key == "muted")           { if (static_cast<int>(value) != 0 && !AudioManager::instance().isMuted()) AudioManager::instance().toggleMute();
                                                   else if (static_cast<int>(value) == 0 && AudioManager::instance().isMuted()) AudioManager::instance().toggleMute(); }
            else if (key == "crt_effect")      { g_crtEffect = (static_cast<int>(value) != 0); }
            else if (key == "quality_preset")  { g_qualityPreset = std::clamp(static_cast<int>(value), 0, 2); applyQualityPreset(); }
            else if (key == "window_width")    { int w = static_cast<int>(value); if (w >= 640 && w <= 7680) WINDOW_WIDTH = w; }
            else if (key == "window_height")   { int h = static_cast<int>(value); if (h >= 480 && h <= 4320) WINDOW_HEIGHT = h; }
            else if (key == "last_class")      { int c = static_cast<int>(value); if (c >= 0 && c < static_cast<int>(PlayerClass::COUNT)) g_selectedClass = static_cast<PlayerClass>(c); }
            else if (key == "last_difficulty")  { int d = static_cast<int>(value); if (d >= 0 && d <= 2) g_selectedDifficulty = static_cast<GameDifficulty>(d); }
        }
        cfg.close();
        // Apply loaded window resolution (before fullscreen check)
        if (m_window && !m_window->isFullscreen()) {
            m_window->setResolution(WINDOW_WIDTH, WINDOW_HEIGHT);
        }
    }
    // Apply loaded volumes to AudioManager
    auto& audio = AudioManager::instance();
    audio.setSFXVolume(static_cast<int>(g_sfxVolume * 128.0f));
    audio.setMusicVolume(static_cast<int>(g_musicVolume * 128.0f));
}

void Game::saveSaveData() {
    std::string data = m_upgrades.serialize();
    atomicSave("riftwalker_save.dat", [&](std::ofstream& f) {
        f << data;
    });

    // Save audio/visual settings
    saveSettings();
}

void Game::saveSettings() {
    atomicSave("riftwalker_settings.cfg", [&](std::ofstream& f) {
        f << "master_volume "   << AudioManager::instance().getMasterVolume() << "\n";
        f << "sfx_volume "      << g_sfxVolume       << "\n";
        f << "music_volume "    << g_musicVolume      << "\n";
        f << "shake_intensity " << g_shakeIntensity   << "\n";
        f << "hud_opacity "     << g_hudOpacity       << "\n";
        f << "color_blind "     << g_colorBlindMode   << "\n";
        f << "hud_scale "       << g_hudScale         << "\n";
        f << "fullscreen "      << (m_window ? (m_window->isFullscreen() ? 1 : 0) : 0) << "\n";
        f << "rumble "           << (m_input.isRumbleEnabled() ? 1 : 0) << "\n";
        f << "language "         << static_cast<int>(Localization::instance().getLanguage()) << "\n";
        f << "muted "             << (AudioManager::instance().isMuted() ? 1 : 0) << "\n";
        f << "crt_effect "       << (g_crtEffect ? 1 : 0) << "\n";
        f << "quality_preset "   << g_qualityPreset << "\n";
        f << "window_width "     << WINDOW_WIDTH  << "\n";
        f << "window_height "    << WINDOW_HEIGHT << "\n";
        f << "last_class "       << static_cast<int>(g_selectedClass) << "\n";
        f << "last_difficulty "  << static_cast<int>(g_selectedDifficulty) << "\n";
    });
}

void Game::shutdown() {
    if (m_shutdownDone) return;
    m_shutdownDone = true;

    m_input.saveBindings("riftwalker_bindings.cfg");
    m_achievements.save("riftwalker_achievements.dat");
    m_lore.save("riftwalker_lore.dat");
    saveSaveData();
    Bestiary::save("bestiary_save.dat");
    AscensionSystem::save("ascension_save.dat");
    ChallengeMode::save("riftwalker_challenges.dat");

    if (m_font) {
        TTF_CloseFont(m_font);
        m_font = nullptr;
    }

    m_currentState = nullptr;
    while (!m_stateStack.empty()) m_stateStack.pop();
    m_states.clear();
    ResourceManager::instance().shutdown();
    AudioManager::instance().shutdown();

    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
}
