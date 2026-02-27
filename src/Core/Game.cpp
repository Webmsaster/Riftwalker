#include "Game.h"
#include "States/MenuState.h"
#include "States/PlayState.h"
#include "States/PauseState.h"
#include "States/UpgradeState.h"
#include "States/RunSummaryState.h"
#include "States/GameOverState.h"
#include "States/OptionsState.h"
#include "States/DifficultySelectState.h"
#include "States/KeybindingsState.h"
#include "States/AchievementsState.h"
#include "States/ShopState.h"
#include "States/ClassSelectState.h"
#include "States/ChallengeSelectState.h"
#include "States/BestiaryState.h"
#include "States/LoreState.h"
#include "States/EndingState.h"
#include <SDL2/SDL_image.h>
#include <fstream>
#include <sstream>

Game::Game() {}

Game::~Game() {
    shutdown();
}

bool Game::init() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) < 0) {
        SDL_Log("SDL init failed: %s", SDL_GetError());
        return false;
    }

    if (IMG_Init(IMG_INIT_PNG) == 0) {
        SDL_Log("SDL_image init failed: %s", IMG_GetError());
    }

    if (TTF_Init() < 0) {
        SDL_Log("SDL_ttf init failed: %s", TTF_GetError());
    }

    AudioManager::instance().init();

    m_window = std::make_unique<Window>("Riftwalker", SCREEN_WIDTH, SCREEN_HEIGHT);
    ResourceManager::instance().init(m_window->getSDLRenderer());

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
        m_font = TTF_OpenFont(path, 16);
        if (m_font) break;
    }

    if (!m_font) {
        SDL_Log("Warning: No font loaded. Text will not be rendered.");
    }

    // Create states
    m_states[StateID::Menu] = std::make_unique<MenuState>();
    m_states[StateID::Play] = std::make_unique<PlayState>();
    m_states[StateID::Pause] = std::make_unique<PauseState>();
    m_states[StateID::Upgrade] = std::make_unique<UpgradeState>();
    m_states[StateID::RunSummary] = std::make_unique<RunSummaryState>();
    m_states[StateID::GameOver] = std::make_unique<GameOverState>();
    m_states[StateID::Options] = std::make_unique<OptionsState>();
    m_states[StateID::DifficultySelect] = std::make_unique<DifficultySelectState>();
    m_states[StateID::Keybindings] = std::make_unique<KeybindingsState>();
    m_states[StateID::Achievements] = std::make_unique<AchievementsState>();
    m_states[StateID::Shop] = std::make_unique<ShopState>();
    m_states[StateID::ClassSelect] = std::make_unique<ClassSelectState>();
    m_states[StateID::ChallengeSelect] = std::make_unique<ChallengeSelectState>();
    m_states[StateID::Bestiary] = std::make_unique<BestiaryState>();
    m_states[StateID::Lore] = std::make_unique<LoreState>();
    m_states[StateID::Ending] = std::make_unique<EndingState>();

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

    // Start at menu
    changeState(StateID::Menu);

    m_running = true;
    return true;
}

void Game::run() {
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
    }
}

void Game::handleEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            m_running = false;
            return;
        }

        if (event.type == SDL_KEYDOWN && event.key.keysym.scancode == SDL_SCANCODE_F11) {
            m_window->toggleFullscreen();
        }

        m_input.handleEvent(event);

        if (m_currentState) {
            m_currentState->handleEvent(event);
        }
    }
}

void Game::update() {
    // Handle screen transition
    if (m_transition != TransitionState::None) {
        m_transitionTimer += Timer::FIXED_TIMESTEP;
        float progress = m_transitionTimer / m_transitionDuration;
        if (progress > 1.0f) progress = 1.0f;

        if (m_transition == TransitionState::FadeOut) {
            m_transitionAlpha = static_cast<Uint8>(progress * 255);
            if (progress >= 1.0f) {
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
            if (progress >= 1.0f) {
                m_transition = TransitionState::None;
                m_transitionAlpha = 0;
            }
        }
    }

    if (m_currentState) {
        m_currentState->update(Timer::FIXED_TIMESTEP);
    }
}

void Game::render() {
    SDL_Renderer* renderer = m_window->getSDLRenderer();

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    // Render state stack (bottom up for overlay states)
    if (!m_stateStack.empty()) {
        // For pause: render play state underneath
        std::vector<GameState*> stackStates;
        auto tempStack = m_stateStack;
        while (!tempStack.empty()) {
            stackStates.push_back(tempStack.top());
            tempStack.pop();
        }
        // Render from bottom to top
        for (int i = static_cast<int>(stackStates.size()) - 1; i >= 0; i--) {
            stackStates[i]->render(renderer);
        }
    } else if (m_currentState) {
        m_currentState->render(renderer);
    }

    // Transition overlay: rift warp effect
    if (m_transitionAlpha > 0) {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        float progress = static_cast<float>(m_transitionAlpha) / 255.0f;

        // Base dark overlay with dimension color tint
        Uint8 tintR = 40, tintG = 20, tintB = 60; // Purple rift color
        SDL_SetRenderDrawColor(renderer, tintR, tintG, tintB, m_transitionAlpha);
        SDL_Rect full = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
        SDL_RenderFillRect(renderer, &full);

        // Glitch scanlines during transition
        if (progress > 0.2f && progress < 0.95f) {
            int lineCount = static_cast<int>(progress * 15);
            Uint32 ticks = SDL_GetTicks();
            for (int i = 0; i < lineCount; i++) {
                int y = ((i * 7919 + ticks / 50) % SCREEN_HEIGHT);
                int h = 1 + (i % 3);
                int offset = static_cast<int>((((i * 3251 + ticks / 30) % 40) - 20) * progress);
                Uint8 la = static_cast<Uint8>(progress * 120);
                // Alternating purple/cyan lines
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
    }

    SDL_RenderPresent(renderer);
}

void Game::changeState(StateID id) {
    if (m_transition != TransitionState::None) return; // already transitioning
    m_pendingState = id;
    m_pendingPush = false;
    m_transition = TransitionState::FadeOut;
    m_transitionTimer = 0;
}

void Game::pushState(StateID id) {
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
        m_currentState = m_stateStack.top();
        m_stateStack.pop();
    }
}

void Game::loadSaveData() {
    std::ifstream file("riftwalker_save.dat");
    if (file.is_open()) {
        std::stringstream ss;
        ss << file.rdbuf();
        m_upgrades.deserialize(ss.str());
        file.close();
    }
}

void Game::saveSaveData() {
    std::ofstream file("riftwalker_save.dat");
    if (file.is_open()) {
        file << m_upgrades.serialize();
        file.close();
    }
}

void Game::shutdown() {
    m_input.saveBindings("riftwalker_bindings.cfg");
    m_achievements.save("riftwalker_achievements.dat");
    m_lore.save("riftwalker_lore.dat");
    saveSaveData();

    if (m_font) {
        TTF_CloseFont(m_font);
        m_font = nullptr;
    }

    m_states.clear();
    ResourceManager::instance().shutdown();
    AudioManager::instance().shutdown();

    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
}
