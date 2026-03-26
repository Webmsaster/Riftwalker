#pragma once
#include <SDL2/SDL.h>
#include <string>

// Automated visual test mode (--visual-test).
// Navigates through game states, captures screenshots at each checkpoint,
// then exits. Uses SDL_PushEvent to inject key inputs.
//
// Checkpoints:
//   01_menu.png         - Main menu (after splash)
//   02_class_select.png - Class selection screen
//   03_gameplay.png     - In-game (Floor 1, after spawning)
//   04_hud_debug.png    - Gameplay with F3 debug overlay
//   05_pause.png        - Pause screen overlay
//
// Screenshots are saved to screenshots/visual_test/

class Game;

class VisualTest {
public:
    void init();
    // Called every frame from Game::run(). Returns true when test is complete.
    bool update(int frameCount, Game* game);

private:
    // Inject a key press event into SDL event queue
    void injectKey(SDL_Scancode scancode);

    // Capture a named screenshot (prefix for the checkpoint)
    void capture(Game* game, const char* name);

    enum class Phase {
        WaitSplash,      // Wait for splash to end
        CaptureMenu,     // Capture main menu
        NavigateToGame,   // Press Enter to start, navigate menus
        WaitClassSelect,  // Wait for class select
        CaptureClassSel,  // Capture class select
        SelectClass,      // Press Enter to select class
        WaitDiffSelect,   // Wait for difficulty select
        SelectDifficulty, // Press Enter on difficulty
        WaitGameplay,     // Wait for gameplay to load
        CaptureGameplay,  // Capture gameplay
        EnableDebug,      // Press F3
        CaptureDebug,     // Capture with debug overlay
        OpenPause,        // Press ESC
        CapturePause,     // Capture pause menu
        Done
    };

    Phase m_phase = Phase::WaitSplash;
    int m_phaseFrame = 0;  // Frames since entering current phase
    int m_captureCount = 0;

    void nextPhase(Phase p) { m_phase = p; m_phaseFrame = 0; }
};

extern bool g_visualTest;
