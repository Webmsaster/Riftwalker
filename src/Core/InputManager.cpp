#include "InputManager.h"
#include <cstring>
#include <cstdlib>

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
    m_keyBindings[Action::Jump] = SDL_SCANCODE_SPACE;
    m_keyBindings[Action::Dash] = SDL_SCANCODE_LSHIFT;
    m_keyBindings[Action::Attack] = SDL_SCANCODE_J;
    m_keyBindings[Action::RangedAttack] = SDL_SCANCODE_K;
    m_keyBindings[Action::DimensionSwitch] = SDL_SCANCODE_E;
    m_keyBindings[Action::Interact] = SDL_SCANCODE_F;
    m_keyBindings[Action::Pause] = SDL_SCANCODE_ESCAPE;
    m_keyBindings[Action::Confirm] = SDL_SCANCODE_RETURN;
    m_keyBindings[Action::Cancel] = SDL_SCANCODE_ESCAPE;
    m_keyBindings[Action::MenuUp] = SDL_SCANCODE_W;
    m_keyBindings[Action::MenuDown] = SDL_SCANCODE_S;
    m_keyBindings[Action::MenuLeft] = SDL_SCANCODE_A;
    m_keyBindings[Action::MenuRight] = SDL_SCANCODE_D;

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
