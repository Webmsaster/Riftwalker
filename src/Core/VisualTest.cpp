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

    // ========== CORE GAMEPLAY FLOW ==========

    case Phase::WaitSplash:
        if (frameCount >= 400) {
            injectKey(SDL_SCANCODE_SPACE);
            nextPhase(Phase::CaptureMenu);
        }
        break;

    case Phase::CaptureMenu:
        if (m_phaseFrame >= 120) {
            capture(game, "menu");
            nextPhase(Phase::NavigateToGame);
        }
        break;

    case Phase::NavigateToGame:
        if (m_phaseFrame >= 30) {
            injectKey(SDL_SCANCODE_RETURN); // New Run (button 0)
            nextPhase(Phase::WaitClassSelect);
        }
        break;

    case Phase::WaitClassSelect:
        if (m_phaseFrame >= 120) {
            nextPhase(Phase::CaptureClassSel);
        }
        break;

    case Phase::CaptureClassSel:
        capture(game, "class_select");
        nextPhase(Phase::SelectClass);
        break;

    case Phase::SelectClass:
        if (m_phaseFrame >= 30) {
            injectKey(SDL_SCANCODE_RETURN); // Voidwalker
            nextPhase(Phase::WaitDiffSelect);
        }
        break;

    case Phase::WaitDiffSelect:
        if (m_phaseFrame >= 120) {
            nextPhase(Phase::SelectDifficulty);
        }
        break;

    case Phase::SelectDifficulty:
        if (m_phaseFrame >= 30) {
            injectKey(SDL_SCANCODE_RETURN); // Normal
            nextPhase(Phase::WaitGameplay);
        }
        break;

    case Phase::WaitGameplay:
        if (m_phaseFrame >= 420) { // 7s: wait for run start narrative to fade
            nextPhase(Phase::CaptureGameplay);
        }
        break;

    case Phase::CaptureGameplay:
        capture(game, "gameplay");
        nextPhase(Phase::EnableDebug);
        break;

    case Phase::EnableDebug:
        if (m_phaseFrame >= 15) {
            injectKey(SDL_SCANCODE_F3);
            nextPhase(Phase::CaptureDebug);
        }
        break;

    case Phase::CaptureDebug:
        if (m_phaseFrame >= 30) {
            capture(game, "debug_overlay");
            injectKey(SDL_SCANCODE_F3); // Turn off
            nextPhase(Phase::OpenPause);
        }
        break;

    case Phase::OpenPause:
        if (m_phaseFrame >= 30) {
            injectKey(SDL_SCANCODE_ESCAPE);
            nextPhase(Phase::CapturePause);
        }
        break;

    case Phase::CapturePause:
        if (m_phaseFrame >= 60) {
            capture(game, "pause");
            nextPhase(Phase::ClosePause);
        }
        break;

    // ========== RETURN TO MENU ==========

    case Phase::ClosePause:
        // Navigate to "Quit to Menu" (button 5): press S 5 times then Enter
        if (m_phaseFrame == 15) injectKey(SDL_SCANCODE_S);
        if (m_phaseFrame == 20) injectKey(SDL_SCANCODE_S);
        if (m_phaseFrame == 25) injectKey(SDL_SCANCODE_S);
        if (m_phaseFrame == 30) injectKey(SDL_SCANCODE_S);
        if (m_phaseFrame == 35) injectKey(SDL_SCANCODE_S);
        if (m_phaseFrame == 45) injectKey(SDL_SCANCODE_RETURN); // Quit to Menu
        if (m_phaseFrame >= 50) nextPhase(Phase::WaitMenu2);
        break;

    case Phase::WaitMenu2:
        if (m_phaseFrame >= 120) { // Wait for menu transition
            nextPhase(Phase::NavToUpgrades);
        }
        break;

    // ========== UPGRADES SCREEN ==========

    case Phase::NavToUpgrades:
        // From menu button 0, navigate down to button 4 (Upgrades)
        if (m_phaseFrame == 10) injectKey(SDL_SCANCODE_S);
        if (m_phaseFrame == 15) injectKey(SDL_SCANCODE_S);
        if (m_phaseFrame == 20) injectKey(SDL_SCANCODE_S);
        if (m_phaseFrame == 25) injectKey(SDL_SCANCODE_S);
        if (m_phaseFrame == 35) injectKey(SDL_SCANCODE_RETURN);
        if (m_phaseFrame >= 40) nextPhase(Phase::WaitUpgrades);
        break;

    case Phase::WaitUpgrades:
        if (m_phaseFrame >= 90) {
            nextPhase(Phase::CaptureUpgrades);
        }
        break;

    case Phase::CaptureUpgrades:
        capture(game, "upgrades");
        nextPhase(Phase::BackFromUpgrades);
        break;

    case Phase::BackFromUpgrades:
        if (m_phaseFrame >= 15) {
            injectKey(SDL_SCANCODE_ESCAPE);
            nextPhase(Phase::NavToAchievements);
        }
        break;

    // ========== ACHIEVEMENTS SCREEN ==========

    case Phase::NavToAchievements:
        if (m_phaseFrame < 90) break; // Wait for menu transition
        // After ESC from Upgrades, menu resets to button 0.
        // Navigate to button 6 (Achievements): 6x S then Enter
        if (m_phaseFrame == 90) injectKey(SDL_SCANCODE_S);
        if (m_phaseFrame == 95) injectKey(SDL_SCANCODE_S);
        if (m_phaseFrame == 100) injectKey(SDL_SCANCODE_S);
        if (m_phaseFrame == 105) injectKey(SDL_SCANCODE_S);
        if (m_phaseFrame == 110) injectKey(SDL_SCANCODE_S);
        if (m_phaseFrame == 115) injectKey(SDL_SCANCODE_S);
        if (m_phaseFrame == 125) injectKey(SDL_SCANCODE_RETURN);
        if (m_phaseFrame >= 130) nextPhase(Phase::WaitAchievements);
        break;

    case Phase::WaitAchievements:
        if (m_phaseFrame >= 90) {
            nextPhase(Phase::CaptureAchievements);
        }
        break;

    case Phase::CaptureAchievements:
        capture(game, "achievements");
        nextPhase(Phase::BackFromAchievements);
        break;

    case Phase::BackFromAchievements:
        nextPhase(Phase::Done);
        break;

    // ========== COMPLETE ==========

    case Phase::Done:
        SDL_Log("VisualTest: Complete. %d screenshots captured.", m_captureCount);
        return true;
    }

    return false;
}
