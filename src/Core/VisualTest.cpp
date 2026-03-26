#include "VisualTest.h"
#include "Game.h"
#include "ScreenCapture.h"
#include <SDL2/SDL.h>
#include <string>

bool g_visualTest = false;

void VisualTest::init() {
    m_phase = Phase::WaitSplash;
    m_phaseFrame = 0;
    m_captureCount = 0;
}

void VisualTest::injectKey(SDL_Scancode scancode) {
    SDL_Event down = {};
    down.type = SDL_KEYDOWN;
    down.key.keysym.scancode = scancode;
    down.key.keysym.sym = SDL_GetKeyFromScancode(scancode);
    down.key.state = SDL_PRESSED;
    down.key.repeat = 0;
    SDL_PushEvent(&down);
}

void VisualTest::capture(Game* game, const char* name) {
    m_captureCount++;
    char filename[256];
    std::snprintf(filename, sizeof(filename), "screenshots/visual_test/%02d_%s.png",
                  m_captureCount, name);

    SDL_Renderer* r = game->getRenderer();
    if (r) {
        ScreenCapture::captureScreenshot(r, std::string(filename));
    }
}

bool VisualTest::update(int frameCount, Game* game) {
    m_phaseFrame++;

    switch (m_phase) {

    // --- Wait for splash screen to show "Press any key" ---
    case Phase::WaitSplash:
        if (frameCount >= 400) {
            injectKey(SDL_SCANCODE_SPACE); // Space to dismiss splash (not Enter, to avoid menu activation)
            nextPhase(Phase::CaptureMenu);
        }
        break;

    // --- Wait for menu to fully appear, then capture ---
    case Phase::CaptureMenu:
        if (m_phaseFrame >= 120) { // 2s for transition
            capture(game, "menu");
            nextPhase(Phase::NavigateToGame);
        }
        break;

    // --- Press Enter to select "New Run" ---
    case Phase::NavigateToGame:
        if (m_phaseFrame >= 30) {
            injectKey(SDL_SCANCODE_RETURN);
            nextPhase(Phase::WaitClassSelect);
        }
        break;

    // --- Wait for class select to load ---
    case Phase::WaitClassSelect:
        if (m_phaseFrame >= 120) {
            nextPhase(Phase::CaptureClassSel);
        }
        break;

    case Phase::CaptureClassSel:
        capture(game, "class_select");
        nextPhase(Phase::SelectClass);
        break;

    // --- Select Voidwalker ---
    case Phase::SelectClass:
        if (m_phaseFrame >= 30) {
            injectKey(SDL_SCANCODE_RETURN);
            nextPhase(Phase::WaitDiffSelect);
        }
        break;

    // --- Wait for difficulty select ---
    case Phase::WaitDiffSelect:
        if (m_phaseFrame >= 120) {
            nextPhase(Phase::SelectDifficulty);
        }
        break;

    // --- Select Normal difficulty ---
    case Phase::SelectDifficulty:
        if (m_phaseFrame >= 30) {
            injectKey(SDL_SCANCODE_RETURN);
            nextPhase(Phase::WaitGameplay);
        }
        break;

    // --- Wait for gameplay to fully initialize ---
    case Phase::WaitGameplay:
        if (m_phaseFrame >= 300) { // 5s for level gen + run start narrative
            nextPhase(Phase::CaptureGameplay);
        }
        break;

    case Phase::CaptureGameplay:
        capture(game, "gameplay");
        nextPhase(Phase::EnableDebug);
        break;

    // --- Toggle F3 debug overlay ---
    case Phase::EnableDebug:
        if (m_phaseFrame >= 15) {
            injectKey(SDL_SCANCODE_F3);
            nextPhase(Phase::CaptureDebug);
        }
        break;

    case Phase::CaptureDebug:
        if (m_phaseFrame >= 30) {
            capture(game, "debug_overlay");
            // Turn off debug overlay
            injectKey(SDL_SCANCODE_F3);
            nextPhase(Phase::OpenPause);
        }
        break;

    // --- Open pause screen ---
    case Phase::OpenPause:
        if (m_phaseFrame >= 30) {
            injectKey(SDL_SCANCODE_ESCAPE);
            nextPhase(Phase::CapturePause);
        }
        break;

    case Phase::CapturePause:
        if (m_phaseFrame >= 60) {
            capture(game, "pause");
            nextPhase(Phase::Done);
        }
        break;

    case Phase::Done:
        SDL_Log("VisualTest: Complete. %d screenshots captured.", m_captureCount);
        return true;
    }

    return false;
}
