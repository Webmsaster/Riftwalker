#pragma once
#include "GameState.h"
#include <string>
#include <vector>

class OptionsState : public GameState {
public:
    void enter() override;
    void handleEvent(const SDL_Event& event) override;
    void update(float dt) override;
    void render(SDL_Renderer* renderer) override;

private:
    struct OptionItem {
        std::string label;
        int value;      // current value
        int minVal;
        int maxVal;
        int step;
        bool isToggle;  // if true, value is 0 or 1
    };

    std::vector<OptionItem> m_options;
    int m_selected = 0;
    float m_time = 0;

    void applyOption(int index);
    std::string getValueText(int index) const;
};
