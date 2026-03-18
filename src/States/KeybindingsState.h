#pragma once
#include "GameState.h"
#include "Core/InputManager.h"
#include <string>
#include <vector>

class KeybindingsState : public GameState {
public:
    void enter() override;
    void exit() override;
    void handleEvent(const SDL_Event& event) override;
    void update(float dt) override;
    void render(SDL_Renderer* renderer) override;

private:
    struct BindingItem {
        Action action;
        std::string name;
        SDL_Scancode key;
    };

    std::vector<BindingItem> m_items;
    int m_selected = 0;
    bool m_listening = false;
    float m_time = 0;
    float m_blinkTimer = 0;

    // Total items = bindings + Reset Defaults + Back
    int totalItems() const { return static_cast<int>(m_items.size()) + 2; }
    bool isResetItem(int idx) const { return idx == static_cast<int>(m_items.size()); }
    bool isBackItem(int idx) const { return idx == static_cast<int>(m_items.size()) + 1; }

    void refreshBindings();
    void confirmSelected();
};
