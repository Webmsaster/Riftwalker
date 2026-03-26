#pragma once
#include <SDL2/SDL.h>
#include <string>

// Automated visual test mode (--visual-test).
// Navigates through game states, captures screenshots at each checkpoint,
// then exits. Uses SDL_PushEvent to inject key inputs.
//
// Checkpoints (extended):
//   01_menu.png           - Main menu
//   02_class_select.png   - Class selection screen
//   03_gameplay.png       - In-game (Floor 1)
//   04_debug_overlay.png  - Gameplay with F3 debug overlay
//   05_pause.png          - Pause screen overlay
//   06_upgrades.png       - Upgrades shop (from menu)
//   07_achievements.png   - Achievements screen
//   08_bestiary.png       - Bestiary screen
//
// Screenshots are saved to screenshots/visual_test/

class Game;

class VisualTest {
public:
    void init();
    // Called every frame from Game::run(). Returns true when test is complete.
    bool update(int frameCount, Game* game);

private:
    void injectKey(SDL_Scancode scancode);
    void capture(Game* game, const char* name);

    enum class Phase {
        // --- Core gameplay flow ---
        WaitSplash,
        CaptureMenu,
        NavigateToGame,
        WaitClassSelect,
        CaptureClassSel,
        SelectClass,
        WaitDiffSelect,
        SelectDifficulty,
        WaitGameplay,
        CaptureGameplay,
        EnableDebug,
        CaptureDebug,
        OpenPause,
        CapturePause,
        // --- Return to menu for more screens ---
        ClosePause,         // ESC to resume
        QuitToMenu,         // ESC again, then navigate to Quit to Menu
        WaitMenu2,          // Wait for menu to reload
        // --- Upgrades screen ---
        NavToUpgrades,
        WaitUpgrades,
        CaptureUpgrades,
        BackFromUpgrades,
        // --- Achievements screen ---
        NavToAchievements,
        WaitAchievements,
        CaptureAchievements,
        BackFromAchievements,
        // --- Done ---
        Done
    };

    Phase m_phase = Phase::WaitSplash;
    int m_phaseFrame = 0;
    int m_captureCount = 0;

    void nextPhase(Phase p) { m_phase = p; m_phaseFrame = 0; }
};

extern bool g_visualTest;
