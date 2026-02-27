#pragma once
#include <SDL2/SDL.h>
#include <unordered_map>
#include <string>
#include <vector>

enum class Action {
    MoveLeft, MoveRight, MoveUp, MoveDown, Jump, Dash,
    Attack, RangedAttack, DimensionSwitch,
    Interact, Pause, Confirm, Cancel,
    MenuUp, MenuDown, MenuLeft, MenuRight,
    Ability1, Ability2, Ability3
};

class InputManager {
public:
    InputManager();
    ~InputManager();

    void update();
    void handleEvent(const SDL_Event& event);

    // Keyboard
    bool isKeyDown(SDL_Scancode key) const;
    bool isKeyPressed(SDL_Scancode key) const;  // just pressed this frame
    bool isKeyReleased(SDL_Scancode key) const;

    // Action mapping
    bool isActionDown(Action action) const;
    bool isActionPressed(Action action) const;
    bool isActionReleased(Action action) const;
    float getAxis(Action negative, Action positive) const;

    // Rebinding
    void rebindKey(Action action, SDL_Scancode newKey);
    SDL_Scancode getKeyForAction(Action action) const;
    bool isKeyUsedByOtherAction(SDL_Scancode key, Action exclude) const;
    Action getActionForKey(SDL_Scancode key) const;

    // Utility
    static std::string getActionName(Action action);
    static const std::vector<Action>& getBindableActions();

    // Reset & Persistence
    void resetToDefaults();
    bool saveBindings(const std::string& filepath) const;
    bool loadBindings(const std::string& filepath);

    // Gamepad
    bool hasGamepad() const { return m_gamepad != nullptr; }
    float getGamepadAxis(SDL_GameControllerAxis axis) const;

    // Suit entropy can distort input
    void setInputDistortion(float amount) { m_distortion = amount; }

    // Call after each fixed update step to clear buffered press/release events
    void clearPressedBuffers();

private:
    void setupDefaultBindings();
    bool checkBinding(Action action, bool (InputManager::*check)(SDL_Scancode) const) const;

    const Uint8* m_keyboardState;
    Uint8 m_prevKeyState[SDL_NUM_SCANCODES];
    Uint8 m_currKeyState[SDL_NUM_SCANCODES];

    // Buffered press/release events - persist until consumed by a fixed update step
    Uint8 m_pressedBuffer[SDL_NUM_SCANCODES];
    Uint8 m_releasedBuffer[SDL_NUM_SCANCODES];

    SDL_GameController* m_gamepad;
    std::unordered_map<Action, SDL_Scancode> m_keyBindings;
    std::unordered_map<Action, SDL_GameControllerButton> m_padBindings;

    float m_distortion;
};
