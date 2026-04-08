#include "VisualTest.h"
#include "Game.h"
#include "ScreenCapture.h"
#include "../States/GameState.h"
#include <SDL2/SDL.h>
#include <string>

static const char* stateIdName(StateID id) {
    switch (id) {
        case StateID::Splash: return "Splash";
        case StateID::Menu: return "Menu";
        case StateID::Play: return "Play";
        case StateID::Pause: return "Pause";
        case StateID::Upgrade: return "Upgrade";
        case StateID::RunSummary: return "RunSummary";
        case StateID::GameOver: return "GameOver";
        case StateID::Options: return "Options";
        case StateID::ClassSelect: return "ClassSelect";
        case StateID::DifficultySelect: return "DifficultySelect";
        case StateID::NGPlusSelect: return "NGPlusSelect";
        case StateID::Keybindings: return "Keybindings";
        case StateID::Achievements: return "Achievements";
        case StateID::Shop: return "Shop";
        case StateID::ChallengeSelect: return "ChallengeSelect";
        case StateID::Bestiary: return "Bestiary";
        case StateID::Lore: return "Lore";
        case StateID::Ending: return "Ending";
        case StateID::RunHistory: return "RunHistory";
        case StateID::DailyLeaderboard: return "DailyLeaderboard";
        case StateID::Credits: return "Credits";
        case StateID::Tutorial: return "Tutorial";
    }
    return "?";
}

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

    SDL_Log("VisualTest: capture '%s' (state: %s)",
            name, stateIdName(game->getCurrentStateID()));

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
        // Splash auto-transitions to Menu at 2.5s. Wait until we're in Menu
        // (plus a small settle time) rather than injecting a key — otherwise
        // the key would be interpreted as Confirm on the first Menu button.
        if (game->getCurrentStateID() == StateID::Menu && m_phaseFrame >= 60) {
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
            nextPhase(Phase::CaptureDiffSelect);
        }
        break;

    case Phase::CaptureDiffSelect:
        capture(game, "difficulty");
        nextPhase(Phase::SelectDifficulty);
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
        // From menu button 0, navigate down to button 5 (Upgrades)
        // Menu order: 0:NewRun 1:Tutorial 2:DailyRun 3:DailyLeaderboard
        //             4:Challenges 5:Upgrades 6:Bestiary 7:Achievements
        //             8:Lore 9:RunHistory 10:Credits 11:Options 12:Quit
        if (m_phaseFrame == 10) injectKey(SDL_SCANCODE_S);
        if (m_phaseFrame == 15) injectKey(SDL_SCANCODE_S);
        if (m_phaseFrame == 20) injectKey(SDL_SCANCODE_S);
        if (m_phaseFrame == 25) injectKey(SDL_SCANCODE_S);
        if (m_phaseFrame == 30) injectKey(SDL_SCANCODE_S);
        if (m_phaseFrame == 40) injectKey(SDL_SCANCODE_RETURN);
        if (m_phaseFrame >= 45) nextPhase(Phase::WaitUpgrades);
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
        // Navigate to button 7 (Achievements): 7x S then Enter
        if (m_phaseFrame == 90) injectKey(SDL_SCANCODE_S);
        if (m_phaseFrame == 95) injectKey(SDL_SCANCODE_S);
        if (m_phaseFrame == 100) injectKey(SDL_SCANCODE_S);
        if (m_phaseFrame == 105) injectKey(SDL_SCANCODE_S);
        if (m_phaseFrame == 110) injectKey(SDL_SCANCODE_S);
        if (m_phaseFrame == 115) injectKey(SDL_SCANCODE_S);
        if (m_phaseFrame == 120) injectKey(SDL_SCANCODE_S);
        if (m_phaseFrame == 130) injectKey(SDL_SCANCODE_RETURN);
        if (m_phaseFrame >= 135) nextPhase(Phase::WaitAchievements);
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
        if (m_phaseFrame >= 60) {
            // Achievements is pushState, so ESC pops back to menu
            injectKey(SDL_SCANCODE_ESCAPE);
            nextPhase(Phase::NavToTutorial);
        }
        break;

    // ========== TUTORIAL SCREEN ==========

    case Phase::NavToTutorial:
        if (m_phaseFrame < 90) break; // Wait for menu to be back
        // After menu reset, navigate to button 1 (Tutorial)
        if (m_phaseFrame == 90) injectKey(SDL_SCANCODE_S);
        if (m_phaseFrame == 100) injectKey(SDL_SCANCODE_RETURN);
        if (m_phaseFrame >= 105) nextPhase(Phase::WaitTutorial);
        break;

    case Phase::WaitTutorial:
        if (m_phaseFrame >= 90) {
            nextPhase(Phase::CaptureTutorial);
        }
        break;

    case Phase::CaptureTutorial:
        capture(game, "tutorial");
        nextPhase(Phase::BackFromTutorial);
        break;

    case Phase::BackFromTutorial:
        if (m_phaseFrame >= 30) {
            injectKey(SDL_SCANCODE_ESCAPE);
            nextPhase(Phase::NavToOptions);
        }
        break;

    // ========== OPTIONS SCREEN ==========

    case Phase::NavToOptions:
        if (m_phaseFrame < 90) break; // Wait for menu transition
        // From button 0, navigate down to button 11 (Options)
        if (m_phaseFrame == 90)  injectKey(SDL_SCANCODE_S);
        if (m_phaseFrame == 95)  injectKey(SDL_SCANCODE_S);
        if (m_phaseFrame == 100) injectKey(SDL_SCANCODE_S);
        if (m_phaseFrame == 105) injectKey(SDL_SCANCODE_S);
        if (m_phaseFrame == 110) injectKey(SDL_SCANCODE_S);
        if (m_phaseFrame == 115) injectKey(SDL_SCANCODE_S);
        if (m_phaseFrame == 120) injectKey(SDL_SCANCODE_S);
        if (m_phaseFrame == 125) injectKey(SDL_SCANCODE_S);
        if (m_phaseFrame == 130) injectKey(SDL_SCANCODE_S);
        if (m_phaseFrame == 135) injectKey(SDL_SCANCODE_S);
        if (m_phaseFrame == 140) injectKey(SDL_SCANCODE_S);
        if (m_phaseFrame == 150) injectKey(SDL_SCANCODE_RETURN);
        if (m_phaseFrame >= 155) nextPhase(Phase::WaitOptions);
        break;

    case Phase::WaitOptions:
        if (m_phaseFrame >= 90) {
            nextPhase(Phase::CaptureOptions);
        }
        break;

    case Phase::CaptureOptions:
        capture(game, "options");
        nextPhase(Phase::BackFromOptions);
        break;

    case Phase::BackFromOptions:
        if (m_phaseFrame >= 30) {
            injectKey(SDL_SCANCODE_ESCAPE);
            nextPhase(Phase::NavToLore);
        }
        break;

    // ========== LORE SCREEN ==========

    case Phase::NavToLore:
        if (m_phaseFrame < 90) break;
        // Navigate to button 8 (Lore)
        if (m_phaseFrame == 90)  injectKey(SDL_SCANCODE_S);
        if (m_phaseFrame == 95)  injectKey(SDL_SCANCODE_S);
        if (m_phaseFrame == 100) injectKey(SDL_SCANCODE_S);
        if (m_phaseFrame == 105) injectKey(SDL_SCANCODE_S);
        if (m_phaseFrame == 110) injectKey(SDL_SCANCODE_S);
        if (m_phaseFrame == 115) injectKey(SDL_SCANCODE_S);
        if (m_phaseFrame == 120) injectKey(SDL_SCANCODE_S);
        if (m_phaseFrame == 125) injectKey(SDL_SCANCODE_S);
        if (m_phaseFrame == 135) injectKey(SDL_SCANCODE_RETURN);
        if (m_phaseFrame >= 140) nextPhase(Phase::WaitLore);
        break;

    case Phase::WaitLore:
        if (m_phaseFrame >= 90) {
            nextPhase(Phase::CaptureLore);
        }
        break;

    case Phase::CaptureLore:
        capture(game, "lore");
        nextPhase(Phase::BackFromLore);
        break;

    case Phase::BackFromLore:
        if (m_phaseFrame >= 30) {
            injectKey(SDL_SCANCODE_ESCAPE);
            nextPhase(Phase::NavToBestiary);
        }
        break;

    // ========== BESTIARY SCREEN ==========

    case Phase::NavToBestiary:
        if (m_phaseFrame < 90) break;
        // Navigate to button 6 (Bestiary): 6x S then Enter
        if (m_phaseFrame == 90)  injectKey(SDL_SCANCODE_S);
        if (m_phaseFrame == 95)  injectKey(SDL_SCANCODE_S);
        if (m_phaseFrame == 100) injectKey(SDL_SCANCODE_S);
        if (m_phaseFrame == 105) injectKey(SDL_SCANCODE_S);
        if (m_phaseFrame == 110) injectKey(SDL_SCANCODE_S);
        if (m_phaseFrame == 115) injectKey(SDL_SCANCODE_S);
        if (m_phaseFrame == 125) injectKey(SDL_SCANCODE_RETURN);
        if (m_phaseFrame >= 130) nextPhase(Phase::WaitBestiary);
        break;

    case Phase::WaitBestiary:
        if (m_phaseFrame >= 90) nextPhase(Phase::CaptureBestiary);
        break;

    case Phase::CaptureBestiary:
        capture(game, "bestiary");
        nextPhase(Phase::BackFromBestiary);
        break;

    case Phase::BackFromBestiary:
        if (m_phaseFrame >= 30) {
            injectKey(SDL_SCANCODE_ESCAPE);
            nextPhase(Phase::NavToChallenges);
        }
        break;

    // ========== CHALLENGES SCREEN ==========

    case Phase::NavToChallenges:
        if (m_phaseFrame < 90) break;
        // Navigate to button 4 (Challenges): 4x S then Enter
        if (m_phaseFrame == 90)  injectKey(SDL_SCANCODE_S);
        if (m_phaseFrame == 95)  injectKey(SDL_SCANCODE_S);
        if (m_phaseFrame == 100) injectKey(SDL_SCANCODE_S);
        if (m_phaseFrame == 105) injectKey(SDL_SCANCODE_S);
        if (m_phaseFrame == 115) injectKey(SDL_SCANCODE_RETURN);
        if (m_phaseFrame >= 120) nextPhase(Phase::WaitChallenges);
        break;

    case Phase::WaitChallenges:
        if (m_phaseFrame >= 90) nextPhase(Phase::CaptureChallenges);
        break;

    case Phase::CaptureChallenges:
        capture(game, "challenges");
        nextPhase(Phase::BackFromChallenges);
        break;

    case Phase::BackFromChallenges:
        if (m_phaseFrame >= 30) {
            injectKey(SDL_SCANCODE_ESCAPE);
            nextPhase(Phase::NavToCredits);
        }
        break;

    // ========== CREDITS SCREEN ==========

    case Phase::NavToCredits:
        if (m_phaseFrame < 90) break;
        // Navigate to button 10 (Credits): 10x S then Enter
        if (m_phaseFrame == 90)  injectKey(SDL_SCANCODE_S);
        if (m_phaseFrame == 95)  injectKey(SDL_SCANCODE_S);
        if (m_phaseFrame == 100) injectKey(SDL_SCANCODE_S);
        if (m_phaseFrame == 105) injectKey(SDL_SCANCODE_S);
        if (m_phaseFrame == 110) injectKey(SDL_SCANCODE_S);
        if (m_phaseFrame == 115) injectKey(SDL_SCANCODE_S);
        if (m_phaseFrame == 120) injectKey(SDL_SCANCODE_S);
        if (m_phaseFrame == 125) injectKey(SDL_SCANCODE_S);
        if (m_phaseFrame == 130) injectKey(SDL_SCANCODE_S);
        if (m_phaseFrame == 135) injectKey(SDL_SCANCODE_S);
        if (m_phaseFrame == 145) injectKey(SDL_SCANCODE_RETURN);
        if (m_phaseFrame >= 150) nextPhase(Phase::WaitCredits);
        break;

    case Phase::WaitCredits:
        if (m_phaseFrame >= 90) nextPhase(Phase::CaptureCredits);
        break;

    case Phase::CaptureCredits:
        capture(game, "credits");
        nextPhase(Phase::BackFromCredits);
        break;

    case Phase::BackFromCredits:
        if (m_phaseFrame >= 30) {
            injectKey(SDL_SCANCODE_ESCAPE);
            nextPhase(Phase::NavToRunHistory);
        }
        break;

    // ========== RUN HISTORY SCREEN ==========

    case Phase::NavToRunHistory:
        if (m_phaseFrame < 90) break;
        // Navigate to button 9 (Run History): 9x S then Enter
        if (m_phaseFrame == 90)  injectKey(SDL_SCANCODE_S);
        if (m_phaseFrame == 95)  injectKey(SDL_SCANCODE_S);
        if (m_phaseFrame == 100) injectKey(SDL_SCANCODE_S);
        if (m_phaseFrame == 105) injectKey(SDL_SCANCODE_S);
        if (m_phaseFrame == 110) injectKey(SDL_SCANCODE_S);
        if (m_phaseFrame == 115) injectKey(SDL_SCANCODE_S);
        if (m_phaseFrame == 120) injectKey(SDL_SCANCODE_S);
        if (m_phaseFrame == 125) injectKey(SDL_SCANCODE_S);
        if (m_phaseFrame == 130) injectKey(SDL_SCANCODE_S);
        if (m_phaseFrame == 140) injectKey(SDL_SCANCODE_RETURN);
        if (m_phaseFrame >= 145) nextPhase(Phase::WaitRunHistory);
        break;

    case Phase::WaitRunHistory:
        if (m_phaseFrame >= 90) nextPhase(Phase::CaptureRunHistory);
        break;

    case Phase::CaptureRunHistory:
        capture(game, "run_history");
        nextPhase(Phase::BackFromRunHistory);
        break;

    case Phase::BackFromRunHistory:
        if (m_phaseFrame >= 30) {
            injectKey(SDL_SCANCODE_ESCAPE);
            nextPhase(Phase::NavToDailyLB);
        }
        break;

    // ========== DAILY LEADERBOARD SCREEN ==========

    case Phase::NavToDailyLB:
        if (m_phaseFrame < 90) break;
        // Navigate to button 3 (Daily Leaderboard): 3x S then Enter
        if (m_phaseFrame == 90)  injectKey(SDL_SCANCODE_S);
        if (m_phaseFrame == 95)  injectKey(SDL_SCANCODE_S);
        if (m_phaseFrame == 100) injectKey(SDL_SCANCODE_S);
        if (m_phaseFrame == 110) injectKey(SDL_SCANCODE_RETURN);
        if (m_phaseFrame >= 115) nextPhase(Phase::WaitDailyLB);
        break;

    case Phase::WaitDailyLB:
        if (m_phaseFrame >= 90) nextPhase(Phase::CaptureDailyLB);
        break;

    case Phase::CaptureDailyLB:
        capture(game, "daily_leaderboard");
        nextPhase(Phase::BackFromDailyLB);
        break;

    case Phase::BackFromDailyLB:
        if (m_phaseFrame >= 30) {
            injectKey(SDL_SCANCODE_ESCAPE);
            nextPhase(Phase::Done);
        }
        break;

    // ========== COMPLETE ==========

    case Phase::Done:
        SDL_Log("VisualTest: Complete. %d screenshots captured.", m_captureCount);
        return true;
    }

    return false;
}
