#include "InputManager.h"
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <sstream>

InputManager::InputManager()
    : m_gamepad(nullptr)
    , m_distortion(0.0f)
{
    m_keyboardState = SDL_GetKeyboardState(nullptr);
    std::memset(m_prevKeyState, 0, sizeof(m_prevKeyState));
    std::memset(m_currKeyState, 0, sizeof(m_currKeyState));
    std::memset(m_pressedBuffer, 0, sizeof(m_pressedBuffer));
    std::memset(m_releasedBuffer, 0, sizeof(m_releasedBuffer));

    // Open first available gamepad
    for (int i = 0; i < SDL_NumJoysticks(); i++) {
        if (SDL_IsGameController(i)) {
            m_gamepad = SDL_GameControllerOpen(i);
            break;
        }
    }

    setupDefaultBindings();
}

InputManager::~InputManager() {
    if (m_gamepad) SDL_GameControllerClose(m_gamepad);
}

void InputManager::setupDefaultBindings() {
    m_keyBindings[Action::MoveLeft] = SDL_SCANCODE_A;
    m_keyBindings[Action::MoveRight] = SDL_SCANCODE_D;
    m_keyBindings[Action::MoveUp] = SDL_SCANCODE_W;
    m_keyBindings[Action::MoveDown] = SDL_SCANCODE_S;
    m_keyBindings[Action::Jump] = SDL_SCANCODE_SPACE;
    m_keyBindings[Action::Dash] = SDL_SCANCODE_LSHIFT;
    m_keyBindings[Action::Attack] = SDL_SCANCODE_J;
    m_keyBindings[Action::RangedAttack] = SDL_SCANCODE_K;
    m_keyBindings[Action::DimensionSwitch] = SDL_SCANCODE_E;
    m_keyBindings[Action::Interact] = SDL_SCANCODE_F;
    m_keyBindings[Action::Pause] = SDL_SCANCODE_ESCAPE;
    m_keyBindings[Action::Confirm] = SDL_SCANCODE_RETURN;
    m_keyBindings[Action::Cancel] = SDL_SCANCODE_ESCAPE;
    m_keyBindings[Action::MenuUp] = SDL_SCANCODE_UP;
    m_keyBindings[Action::MenuDown] = SDL_SCANCODE_DOWN;
    m_keyBindings[Action::MenuLeft] = SDL_SCANCODE_LEFT;
    m_keyBindings[Action::MenuRight] = SDL_SCANCODE_RIGHT;

    m_padBindings[Action::Jump] = SDL_CONTROLLER_BUTTON_A;
    m_padBindings[Action::Dash] = SDL_CONTROLLER_BUTTON_B;
    m_padBindings[Action::Attack] = SDL_CONTROLLER_BUTTON_X;
    m_padBindings[Action::RangedAttack] = SDL_CONTROLLER_BUTTON_Y;
    m_padBindings[Action::DimensionSwitch] = SDL_CONTROLLER_BUTTON_RIGHTSHOULDER;
    m_padBindings[Action::Interact] = SDL_CONTROLLER_BUTTON_LEFTSHOULDER;
    m_padBindings[Action::Pause] = SDL_CONTROLLER_BUTTON_START;
    m_padBindings[Action::Confirm] = SDL_CONTROLLER_BUTTON_A;
    m_padBindings[Action::Cancel] = SDL_CONTROLLER_BUTTON_B;
    m_padBindings[Action::MenuUp] = SDL_CONTROLLER_BUTTON_DPAD_UP;
    m_padBindings[Action::MenuDown] = SDL_CONTROLLER_BUTTON_DPAD_DOWN;
    m_padBindings[Action::MenuLeft] = SDL_CONTROLLER_BUTTON_DPAD_LEFT;
    m_padBindings[Action::MenuRight] = SDL_CONTROLLER_BUTTON_DPAD_RIGHT;

    // Ability keys
    m_keyBindings[Action::Ability1] = SDL_SCANCODE_1;
    m_keyBindings[Action::Ability2] = SDL_SCANCODE_2;
    m_keyBindings[Action::Ability3] = SDL_SCANCODE_3;
}

void InputManager::update() {
    std::memcpy(m_prevKeyState, m_currKeyState, sizeof(m_currKeyState));
    std::memcpy(m_currKeyState, m_keyboardState, SDL_NUM_SCANCODES);

    // Accumulate press/release events into buffers.
    // These persist until clearPressedBuffers() is called after a fixed update step.
    for (int i = 0; i < SDL_NUM_SCANCODES; i++) {
        if (m_currKeyState[i] && !m_prevKeyState[i]) m_pressedBuffer[i] = 1;
        if (!m_currKeyState[i] && m_prevKeyState[i]) m_releasedBuffer[i] = 1;
    }
}

void InputManager::clearPressedBuffers() {
    std::memset(m_pressedBuffer, 0, sizeof(m_pressedBuffer));
    std::memset(m_releasedBuffer, 0, sizeof(m_releasedBuffer));
}

void InputManager::handleEvent(const SDL_Event& event) {
    if (event.type == SDL_CONTROLLERDEVICEADDED && !m_gamepad) {
        m_gamepad = SDL_GameControllerOpen(event.cdevice.which);
    } else if (event.type == SDL_CONTROLLERDEVICEREMOVED && m_gamepad) {
        SDL_GameControllerClose(m_gamepad);
        m_gamepad = nullptr;
    }
}

bool InputManager::isKeyDown(SDL_Scancode key) const {
    return m_currKeyState[key] != 0;
}

bool InputManager::isKeyPressed(SDL_Scancode key) const {
    return m_pressedBuffer[key] != 0;
}

bool InputManager::isKeyReleased(SDL_Scancode key) const {
    return m_releasedBuffer[key] != 0;
}

bool InputManager::checkBinding(Action action, bool (InputManager::*check)(SDL_Scancode) const) const {
    auto it = m_keyBindings.find(action);
    if (it != m_keyBindings.end() && (this->*check)(it->second)) return true;

    // Check gamepad
    if (m_gamepad) {
        auto pit = m_padBindings.find(action);
        if (pit != m_padBindings.end()) {
            return SDL_GameControllerGetButton(m_gamepad, pit->second);
        }
    }
    return false;
}

bool InputManager::isActionDown(Action action) const {
    // Suit entropy distortion: randomly ignore inputs
    if (m_distortion > 0.5f) {
        float roll = static_cast<float>(std::rand()) / RAND_MAX;
        if (roll < (m_distortion - 0.5f) * 0.3f) return false;
    }
    return checkBinding(action, &InputManager::isKeyDown);
}

bool InputManager::isActionPressed(Action action) const {
    if (m_distortion > 0.5f) {
        float roll = static_cast<float>(std::rand()) / RAND_MAX;
        if (roll < (m_distortion - 0.5f) * 0.3f) return false;
    }
    return checkBinding(action, &InputManager::isKeyPressed);
}

bool InputManager::isActionReleased(Action action) const {
    return checkBinding(action, &InputManager::isKeyReleased);
}

float InputManager::getAxis(Action negative, Action positive) const {
    float value = 0.0f;
    if (isActionDown(negative)) value -= 1.0f;
    if (isActionDown(positive)) value += 1.0f;

    // Gamepad left stick
    if (m_gamepad && value == 0.0f) {
        float axis = getGamepadAxis(SDL_CONTROLLER_AXIS_LEFTX);
        if (std::abs(axis) > 0.2f) value = axis;
    }

    // Entropy distortion: add random drift
    if (m_distortion > 0.75f) {
        float drift = (static_cast<float>(std::rand()) / RAND_MAX - 0.5f) * m_distortion * 0.4f;
        value += drift;
        if (value < -1.0f) value = -1.0f;
        if (value > 1.0f) value = 1.0f;
    }

    return value;
}

float InputManager::getGamepadAxis(SDL_GameControllerAxis axis) const {
    if (!m_gamepad) return 0.0f;
    return static_cast<float>(SDL_GameControllerGetAxis(m_gamepad, axis)) / 32767.0f;
}

void InputManager::rebindKey(Action action, SDL_Scancode newKey) {
    m_keyBindings[action] = newKey;
}

SDL_Scancode InputManager::getKeyForAction(Action action) const {
    auto it = m_keyBindings.find(action);
    if (it != m_keyBindings.end()) return it->second;
    return SDL_SCANCODE_UNKNOWN;
}

bool InputManager::isKeyUsedByOtherAction(SDL_Scancode key, Action exclude) const {
    for (auto& [action, scancode] : m_keyBindings) {
        if (action != exclude && scancode == key) return true;
    }
    return false;
}

Action InputManager::getActionForKey(SDL_Scancode key) const {
    for (auto& [action, scancode] : m_keyBindings) {
        if (scancode == key) return action;
    }
    return Action::MoveLeft; // fallback, should not happen
}

std::string InputManager::getActionName(Action action) {
    switch (action) {
        case Action::MoveLeft:        return "Move Left";
        case Action::MoveRight:       return "Move Right";
        case Action::MoveUp:          return "Look Up";
        case Action::MoveDown:        return "Look Down";
        case Action::Jump:            return "Jump";
        case Action::Dash:            return "Dash";
        case Action::Attack:          return "Melee Attack";
        case Action::RangedAttack:    return "Ranged Attack";
        case Action::DimensionSwitch: return "Dimension Switch";
        case Action::Interact:        return "Interact";
        case Action::Pause:           return "Pause";
        case Action::Ability1:        return "Ground Slam";
        case Action::Ability2:        return "Rift Shield";
        case Action::Ability3:        return "Phase Strike";
        default:                      return "Unknown";
    }
}

const std::vector<Action>& InputManager::getBindableActions() {
    static const std::vector<Action> actions = {
        Action::MoveLeft, Action::MoveRight, Action::MoveUp, Action::MoveDown,
        Action::Jump, Action::Dash,
        Action::Attack, Action::RangedAttack, Action::DimensionSwitch,
        Action::Interact, Action::Pause,
        Action::Ability1, Action::Ability2, Action::Ability3
    };
    return actions;
}

void InputManager::resetToDefaults() {
    setupDefaultBindings();
}

bool InputManager::saveBindings(const std::string& filepath) const {
    std::ofstream file(filepath);
    if (!file.is_open()) return false;

    for (auto& [action, scancode] : m_keyBindings) {
        file << static_cast<int>(action) << " " << static_cast<int>(scancode) << "\n";
    }
    file.close();
    return true;
}

bool InputManager::loadBindings(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) return false;

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        int actionId, scancodeId;
        if (iss >> actionId >> scancodeId) {
            auto action = static_cast<Action>(actionId);
            auto scancode = static_cast<SDL_Scancode>(scancodeId);
            m_keyBindings[action] = scancode;
        }
    }
    file.close();
    return true;
}
