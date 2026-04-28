#include "OptionsState.h"
#include "Core/Game.h"
#include "Core/AudioManager.h"
#include "Core/InputManager.h"
#include "Core/Localization.h"
#include "Core/Window.h"
#include "UI/UITextures.h"
#include <cmath>
#include <cstdio>
#include <algorithm>
#include <string>

// Available window resolutions (windowed mode only)
struct Resolution { int w, h; const char* label; };
static const Resolution s_resolutions[] = {
    {1280,  720, "1280x720 (720p)"},
    {1600,  900, "1600x900"},
    {1920, 1080, "1920x1080 (1080p)"},
    {2560, 1440, "2560x1440 (2K)"},
    {3840, 2160, "3840x2160 (4K)"},
};
static constexpr int NUM_RESOLUTIONS = sizeof(s_resolutions) / sizeof(s_resolutions[0]);

static int findCurrentResolution() {
    int w = Game::WINDOW_WIDTH, h = Game::WINDOW_HEIGHT;
    for (int i = 0; i < NUM_RESOLUTIONS; i++) {
        if (s_resolutions[i].w == w && s_resolutions[i].h == h) return i;
    }
    // Default to closest match
    for (int i = NUM_RESOLUTIONS - 1; i >= 0; i--) {
        if (s_resolutions[i].w <= w && s_resolutions[i].h <= h) return i;
    }
    return 2; // 1080p fallback
}

// Indices for option items (must match order in enter())
static constexpr int OPT_MASTER   = 0;
static constexpr int OPT_SFX      = 1;
static constexpr int OPT_MUSIC    = 2;
static constexpr int OPT_MUTE     = 3;
static constexpr int OPT_FULLSCR  = 4;
static constexpr int OPT_RESOLUTION = 5;
static constexpr int OPT_SHAKE    = 6;
static constexpr int OPT_HUD_OPAC   = 7;
static constexpr int OPT_RUMBLE     = OPT_HUD_OPAC + 1;
static constexpr int OPT_COLORBLIND = OPT_HUD_OPAC + 2;
static constexpr int OPT_HUDSCALE   = OPT_HUD_OPAC + 3;
static constexpr int OPT_LANGUAGE   = OPT_HUD_OPAC + 4;
static constexpr int OPT_CRT       = OPT_HUD_OPAC + 5;
static constexpr int OPT_QUALITY   = OPT_HUD_OPAC + 6;
static constexpr int OPT_REDUCE_FLASH = OPT_HUD_OPAC + 7;
// Controls / Reset / Back are the last 3 (special buttons)

void OptionsState::enter() {
    auto& audio = AudioManager::instance();

    m_options.clear();
    // Value stored as integer percent; SFX/Music 0-100%, Shake 0-200%, HUD 50-100%
    m_options.push_back({LOC("options.master_volume"), static_cast<int>(audio.getMasterVolume() * 100), 0, 100, 5, false});
    m_options.push_back({LOC("options.sfx_volume"),    static_cast<int>(g_sfxVolume   * 100.0f), 0, 100, 5, false});
    m_options.push_back({LOC("options.music_volume"),  static_cast<int>(g_musicVolume * 100.0f), 0, 100, 5, false});
    m_options.push_back({LOC("options.mute"),          audio.isMuted() ? 1 : 0, 0, 1, 1, true});
    m_options.push_back({LOC("options.fullscreen"),    (game->getWindow() && game->getWindow()->isFullscreen()) ? 1 : 0, 0, 1, 1, true});
    m_options.push_back({LOC("options.resolution"), findCurrentResolution(), 0, NUM_RESOLUTIONS - 1, 1, false});
    m_options.push_back({LOC("options.screen_shake"),  static_cast<int>(g_shakeIntensity * 100.0f), 0, 200, 10, false});
    m_options.push_back({LOC("options.hud_opacity"),   static_cast<int>(g_hudOpacity * 100.0f), 50, 100, 5, false});
    m_options.push_back({LOC("options.rumble"), game->getInput().isRumbleEnabled() ? 1 : 0, 0, 1, 1, true});
    m_options.push_back({LOC("options.colorblind"), g_colorBlindMode, 0, 2, 1, false});
    m_options.push_back({LOC("options.hud_scale"), static_cast<int>(g_hudScale * 100), 75, 150, 25, false});
    m_options.push_back({LOC("options.language"), static_cast<int>(Localization::instance().getLanguage()), 0, 1, 1, false}); // 0=EN, 1=DE
    m_options.push_back({LOC("options.crt_effect"), g_crtEffect ? 1 : 0, 0, 1, 1, true});
    m_options.push_back({LOC("options.quality"), g_qualityPreset, 0, 2, 1, false});
    m_options.push_back({LOC("options.reduce_flashes"), g_reduceFlashes ? 1 : 0, 0, 1, 1, true});
    m_options.push_back({LOC("options.controls"),      0, 0, 0, 0, false}); // special: open keybindings
    m_options.push_back({LOC("options.reset"), 0, 0, 0, 0, false}); // special: reset
    m_options.push_back({LOC("options.back"),          0, 0, 0, 0, false}); // special: back button

    m_selected = 0;
    m_time = 0;
}

void OptionsState::handleEvent(const SDL_Event& event) {
    if (event.type == SDL_KEYDOWN) {
        switch (event.key.keysym.scancode) {
            case SDL_SCANCODE_W: case SDL_SCANCODE_UP:
                m_selected = (m_selected - 1 + static_cast<int>(m_options.size())) % static_cast<int>(m_options.size());
                AudioManager::instance().play(SFX::MenuSelect);
                break;

            case SDL_SCANCODE_S: case SDL_SCANCODE_DOWN:
                m_selected = (m_selected + 1) % static_cast<int>(m_options.size());
                AudioManager::instance().play(SFX::MenuSelect);
                break;

            case SDL_SCANCODE_A: case SDL_SCANCODE_LEFT:
                if (m_selected < static_cast<int>(m_options.size()) - 3) { // not Controls/Reset/Back
                    auto& opt = m_options[m_selected];
                    int prevVal = opt.value;
                    if (opt.isToggle) {
                        opt.value = opt.value ? 0 : 1;
                    } else {
                        opt.value = std::max(opt.minVal, opt.value - opt.step);
                    }
                    applyOption(m_selected);
                    // Play volume preview ping when a volume slider actually changed
                    if (opt.value != prevVal &&
                        (m_selected == OPT_MASTER || m_selected == OPT_SFX || m_selected == OPT_MUSIC)) {
                        AudioManager::instance().play(SFX::VolumePreview);
                    } else {
                        AudioManager::instance().play(SFX::MenuSelect);
                    }
                    game->saveSettings();
                    // Re-enter to refresh labels when language changed
                    if (m_selected == OPT_LANGUAGE && opt.value != prevVal) {
                        int sel = m_selected;
                        enter();
                        m_selected = sel;
                    }
                }
                break;

            case SDL_SCANCODE_D: case SDL_SCANCODE_RIGHT:
                if (m_selected < static_cast<int>(m_options.size()) - 3) { // not Controls/Reset/Back
                    auto& opt = m_options[m_selected];
                    int prevVal = opt.value;
                    if (opt.isToggle) {
                        opt.value = opt.value ? 0 : 1;
                    } else {
                        opt.value = std::min(opt.maxVal, opt.value + opt.step);
                    }
                    applyOption(m_selected);
                    // Play volume preview ping when a volume slider actually changed
                    if (opt.value != prevVal &&
                        (m_selected == OPT_MASTER || m_selected == OPT_SFX || m_selected == OPT_MUSIC)) {
                        AudioManager::instance().play(SFX::VolumePreview);
                    } else {
                        AudioManager::instance().play(SFX::MenuSelect);
                    }
                    game->saveSettings();
                    // Re-enter to refresh labels when language changed
                    if (m_selected == OPT_LANGUAGE && opt.value != prevVal) {
                        int sel = m_selected;
                        enter();
                        m_selected = sel;
                    }
                }
                break;

            case SDL_SCANCODE_RETURN: case SDL_SCANCODE_SPACE:
                if (m_selected == static_cast<int>(m_options.size()) - 1) {
                    // Back button
                    AudioManager::instance().play(SFX::MenuConfirm);
                    game->changeState(StateID::Menu);
                } else if (m_selected == static_cast<int>(m_options.size()) - 2) {
                    // Reset Defaults
                    m_options[OPT_MASTER].value   = 70;  // 70% — safe for headphones
                    m_options[OPT_SFX].value      = 80;  // 80% — clear without being harsh
                    m_options[OPT_MUSIC].value    = 60;  // 60% — music behind SFX
                    m_options[OPT_MUTE].value     = 0;
                    m_options[OPT_FULLSCR].value  = 0;
                    m_options[OPT_RESOLUTION].value = findCurrentResolution();
                    m_options[OPT_SHAKE].value    = 100;
                    m_options[OPT_HUD_OPAC].value = 90;  // 90% — slightly transparent
                    m_options[OPT_RUMBLE].value     = 1;   // Rumble on
                    m_options[OPT_COLORBLIND].value = 0;   // Color blind off
                    m_options[OPT_HUDSCALE].value   = 100; // HUD scale 100%
                    m_options[OPT_LANGUAGE].value   = 0;   // English
                    m_options[OPT_CRT].value        = 0;   // CRT off
                    m_options[OPT_QUALITY].value    = 2;   // High
                    m_options[OPT_REDUCE_FLASH].value = 0;  // Flashes on
                    for (int i = 0; i < static_cast<int>(m_options.size()) - 3; i++) applyOption(i);
                    game->saveSettings();
                    AudioManager::instance().play(SFX::MenuConfirm);
                    // Re-enter to refresh labels after language reset
                    { int sel = m_selected; enter(); m_selected = sel; }
                } else if (m_selected == static_cast<int>(m_options.size()) - 3) {
                    // Controls
                    AudioManager::instance().play(SFX::MenuConfirm);
                    game->changeState(StateID::Keybindings);
                } else if (m_options[m_selected].isToggle) {
                    auto& opt = m_options[m_selected];
                    opt.value = opt.value ? 0 : 1;
                    applyOption(m_selected);
                    AudioManager::instance().play(SFX::MenuConfirm);
                }
                break;

            case SDL_SCANCODE_ESCAPE:
                AudioManager::instance().play(SFX::MenuConfirm);
                game->changeState(StateID::Menu);
                break;

            default: break;
        }
    }

    // Right-click to go back
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_RIGHT) {
        AudioManager::instance().play(SFX::MenuConfirm);
        game->changeState(StateID::Menu);
    }

    // Mouse wheel scrolling
    if (event.type == SDL_MOUSEWHEEL) {
        int total = static_cast<int>(m_options.size());
        if (event.wheel.y > 0) {
            m_selected = (m_selected - 1 + total) % total;
            AudioManager::instance().play(SFX::MenuSelect);
        } else if (event.wheel.y < 0) {
            m_selected = (m_selected + 1) % total;
            AudioManager::instance().play(SFX::MenuSelect);
        }
    }
}

void OptionsState::applyOption(int index) {
    auto& audio = AudioManager::instance();
    switch (index) {
        case OPT_MASTER:
            audio.setMasterVolume(m_options[OPT_MASTER].value / 100.0f);
            break;
        case OPT_SFX:
            g_sfxVolume = m_options[OPT_SFX].value / 100.0f;
            audio.setSFXVolume(static_cast<int>(g_sfxVolume * 128.0f));
            break;
        case OPT_MUSIC:
            g_musicVolume = m_options[OPT_MUSIC].value / 100.0f;
            audio.setMusicVolume(static_cast<int>(g_musicVolume * 128.0f));
            break;
        case OPT_MUTE:
            if ((m_options[OPT_MUTE].value == 1) != audio.isMuted()) {
                audio.toggleMute();
            }
            break;
        case OPT_FULLSCR:
            if (game->getWindow()) {
                bool wantFullscreen = (m_options[OPT_FULLSCR].value == 1);
                if (wantFullscreen != game->getWindow()->isFullscreen()) {
                    game->getWindow()->toggleFullscreen();
                }
            }
            break;
        case OPT_RESOLUTION:
            if (game->getWindow() && !game->getWindow()->isFullscreen()) {
                int idx = m_options[OPT_RESOLUTION].value;
                if (idx >= 0 && idx < NUM_RESOLUTIONS) {
                    game->getWindow()->setResolution(s_resolutions[idx].w, s_resolutions[idx].h);
                    Game::WINDOW_WIDTH = s_resolutions[idx].w;
                    Game::WINDOW_HEIGHT = s_resolutions[idx].h;
                }
            }
            break;
        case OPT_SHAKE:
            g_shakeIntensity = m_options[OPT_SHAKE].value / 100.0f;
            break;
        case OPT_HUD_OPAC:
            g_hudOpacity = m_options[OPT_HUD_OPAC].value / 100.0f;
            break;
        case OPT_RUMBLE: // Controller Rumble
            game->getInputMutable().setRumbleEnabled(m_options[OPT_RUMBLE].value == 1);
            break;
        case OPT_COLORBLIND: // Color Blind Mode
            g_colorBlindMode = m_options[OPT_COLORBLIND].value;
            break;
        case OPT_HUDSCALE: // HUD Scale
            g_hudScale = m_options[OPT_HUDSCALE].value / 100.0f;
            break;
        case OPT_LANGUAGE: // Language
            Localization::instance().setLanguage(m_options[OPT_LANGUAGE].value == 0 ? Lang::EN : Lang::DE);
            // Labels will refresh on next enter() (state re-entry or manual re-enter below)
            break;
        case OPT_CRT: // CRT Scanline Effect
            g_crtEffect = (m_options[OPT_CRT].value == 1);
            break;
        case OPT_QUALITY: // Performance preset
            g_qualityPreset = std::clamp(m_options[OPT_QUALITY].value, 0, 2);
            applyQualityPreset();
            break;
        case OPT_REDUCE_FLASH: // Photosensitivity toggle
            g_reduceFlashes = (m_options[OPT_REDUCE_FLASH].value == 1);
            break;
    }
}

std::string OptionsState::getValueText(int index) const {
    auto& opt = m_options[index];
    if (opt.isToggle) {
        return opt.value ? LOC("options.on") : LOC("options.off");
    }
    // Resolution: show resolution label
    if (index == OPT_RESOLUTION) {
        int idx = opt.value;
        if (idx >= 0 && idx < NUM_RESOLUTIONS) return s_resolutions[idx].label;
        return "Unknown";
    }
    // Language: show language name
    if (index == OPT_LANGUAGE) {
        return opt.value == 0 ? LOC("options.lang_en") : LOC("options.lang_de");
    }
    // Color Blind Mode: show mode name
    if (index == OPT_COLORBLIND) {
        switch (opt.value) {
            case 0: return LOC("options.off");
            case 1: return "Deuteranopia";
            case 2: return "Tritanopia";
            default: return LOC("options.off");
        }
    }
    // HUD Scale: show percentage
    if (index == OPT_HUDSCALE) {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%d%%", opt.value);
        return buf;
    }
    // Quality preset: Low / Medium / High
    if (index == OPT_QUALITY) {
        switch (opt.value) {
            case 0: return LOC("options.quality_low");
            case 1: return LOC("options.quality_med");
            case 2: return LOC("options.quality_high");
            default: return "?";
        }
    }
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%d%%", opt.value);
    return buf;
}

void OptionsState::update(float dt) {
    m_time += dt;

    // Gamepad navigation
    auto& input = game->getInput();
    if (!input.hasGamepad()) return;

    if (input.isActionPressed(Action::MenuUp)) {
        m_selected = (m_selected - 1 + static_cast<int>(m_options.size())) % static_cast<int>(m_options.size());
        AudioManager::instance().play(SFX::MenuSelect);
    }
    if (input.isActionPressed(Action::MenuDown)) {
        m_selected = (m_selected + 1) % static_cast<int>(m_options.size());
        AudioManager::instance().play(SFX::MenuSelect);
    }
    if (input.isActionPressed(Action::MenuLeft)) {
        if (m_selected < static_cast<int>(m_options.size()) - 3) {
            auto& opt = m_options[m_selected];
            int prevVal = opt.value;
            if (opt.isToggle) {
                opt.value = opt.value ? 0 : 1;
            } else {
                opt.value = std::max(opt.minVal, opt.value - opt.step);
            }
            applyOption(m_selected);
            if (opt.value != prevVal &&
                (m_selected == OPT_MASTER || m_selected == OPT_SFX || m_selected == OPT_MUSIC)) {
                AudioManager::instance().play(SFX::VolumePreview);
            } else {
                AudioManager::instance().play(SFX::MenuSelect);
            }
            game->saveSettings();
            if (m_selected == OPT_LANGUAGE && opt.value != prevVal) {
                int sel = m_selected;
                enter();
                m_selected = sel;
            }
        }
    }
    if (input.isActionPressed(Action::MenuRight)) {
        if (m_selected < static_cast<int>(m_options.size()) - 3) {
            auto& opt = m_options[m_selected];
            int prevVal = opt.value;
            if (opt.isToggle) {
                opt.value = opt.value ? 0 : 1;
            } else {
                opt.value = std::min(opt.maxVal, opt.value + opt.step);
            }
            applyOption(m_selected);
            if (opt.value != prevVal &&
                (m_selected == OPT_MASTER || m_selected == OPT_SFX || m_selected == OPT_MUSIC)) {
                AudioManager::instance().play(SFX::VolumePreview);
            } else {
                AudioManager::instance().play(SFX::MenuSelect);
            }
            game->saveSettings();
            if (m_selected == OPT_LANGUAGE && opt.value != prevVal) {
                int sel = m_selected;
                enter();
                m_selected = sel;
            }
        }
    }
    if (input.isActionPressed(Action::Confirm)) {
        if (m_selected == static_cast<int>(m_options.size()) - 1) {
            AudioManager::instance().play(SFX::MenuConfirm);
            game->changeState(StateID::Menu);
        } else if (m_selected == static_cast<int>(m_options.size()) - 2) {
            m_options[OPT_MASTER].value   = 70;
            m_options[OPT_SFX].value      = 80;
            m_options[OPT_MUSIC].value    = 60;
            m_options[OPT_MUTE].value     = 0;
            m_options[OPT_FULLSCR].value  = 0;
            m_options[OPT_RESOLUTION].value = findCurrentResolution();
            m_options[OPT_SHAKE].value    = 100;
            m_options[OPT_HUD_OPAC].value = 90;
            m_options[OPT_RUMBLE].value     = 1;
            m_options[OPT_COLORBLIND].value = 0;
            m_options[OPT_HUDSCALE].value   = 100;
            m_options[OPT_LANGUAGE].value    = 0;
            m_options[OPT_CRT].value         = 0;
            m_options[OPT_QUALITY].value     = 2;
            m_options[OPT_REDUCE_FLASH].value = 0;
            for (int i = 0; i < static_cast<int>(m_options.size()) - 3; i++) applyOption(i);
            game->saveSettings();
            AudioManager::instance().play(SFX::MenuConfirm);
            { int sel = m_selected; enter(); m_selected = sel; }
        } else if (m_selected == static_cast<int>(m_options.size()) - 3) {
            AudioManager::instance().play(SFX::MenuConfirm);
            game->changeState(StateID::Keybindings);
        } else if (m_options[m_selected].isToggle) {
            auto& opt = m_options[m_selected];
            opt.value = opt.value ? 0 : 1;
            applyOption(m_selected);
            AudioManager::instance().play(SFX::MenuConfirm);
        }
    }
    if (input.isActionPressed(Action::Cancel)) {
        AudioManager::instance().play(SFX::MenuConfirm);
        game->changeState(StateID::Menu);
    }
}

void OptionsState::render(SDL_Renderer* renderer) {
    // Dark background
    SDL_SetRenderDrawColor(renderer, 10, 8, 20, 255);
    SDL_RenderClear(renderer);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // Subtle grid
    SDL_SetRenderDrawColor(renderer, 22, 18, 35, 20);
    for (int x = 0; x < SCREEN_WIDTH; x += 160) SDL_RenderDrawLine(renderer, x, 0, x, SCREEN_HEIGHT);
    for (int y = 0; y < SCREEN_HEIGHT; y += 160) SDL_RenderDrawLine(renderer, 0, y, SCREEN_WIDTH, y);

    TTF_Font* font = game->getFont();
    if (!font) return;

    // Title
    {
        SDL_Color c = {140, 100, 220, 255};
        SDL_Surface* s = TTF_RenderUTF8_Blended(font, LOC("options.title"), c);
        if (s) {
            SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
            if (t) {
                int tw = s->w * 2;
                int th = s->h * 2;
                SDL_Rect r = {SCREEN_WIDTH / 2 - tw / 2, 160, tw, th};
                SDL_RenderCopy(renderer, t, nullptr, &r);
                SDL_DestroyTexture(t);
            }
            SDL_FreeSurface(s);
        }
    }

    // Options
    int startY = 300;
    int itemH = 64;
    int cardW = 1000;
    int cardX = SCREEN_WIDTH / 2 - cardW / 2;

    for (int i = 0; i < static_cast<int>(m_options.size()); i++) {
        int y = startY + i * itemH;
        bool selected = (i == m_selected);
        bool isBack = (i == static_cast<int>(m_options.size()) - 1);
        bool isReset = (i == static_cast<int>(m_options.size()) - 2);
        bool isControls = (i == static_cast<int>(m_options.size()) - 3);
        bool isSpecial = isBack || isReset || isControls;

        // Card background
        Uint8 bgA = selected ? 60 : 25;
        SDL_Rect card = {cardX, y, cardW, itemH - 8};
        if (!renderPanelBg(renderer, card, bgA)) {
            SDL_SetRenderDrawColor(renderer, 30, 25, 50, bgA);
            SDL_RenderFillRect(renderer, &card);
        }

        // Selection border
        if (selected) {
            float pulse = 0.6f + 0.4f * std::sin(m_time * 4.0f);
            Uint8 borderA = static_cast<Uint8>(180 * pulse);
            SDL_SetRenderDrawColor(renderer, 120, 80, 200, borderA);
            SDL_RenderDrawRect(renderer, &card);
        }

        // Label
        SDL_Color labelColor = selected ? SDL_Color{220, 200, 255, 255} : SDL_Color{140, 130, 170, 255};
        SDL_Surface* ls = TTF_RenderUTF8_Blended(font, m_options[i].label.c_str(), labelColor);
        if (ls) {
            SDL_Texture* lt = SDL_CreateTextureFromSurface(renderer, ls);
            if (lt) {
                SDL_Rect lr;
                if (isSpecial) {
                    lr = {SCREEN_WIDTH / 2 - ls->w / 2, y + (itemH - 8) / 2 - ls->h / 2, ls->w, ls->h};
                } else {
                    lr = {cardX + 40, y + (itemH - 8) / 2 - ls->h / 2, ls->w, ls->h};
                }
                SDL_RenderCopy(renderer, lt, nullptr, &lr);
                SDL_DestroyTexture(lt);
            }
            SDL_FreeSurface(ls);
        }

        // Value (not for special buttons)
        if (!isSpecial) {
            std::string valText = getValueText(i);

            // Slider bar for non-toggles
            if (!m_options[i].isToggle) {
                int barX = cardX + cardW - 440;
                int barY = y + (itemH - 8) / 2 - 8;
                int barW = 300;
                int barH = 16;

                // Background bar
                SDL_SetRenderDrawColor(renderer, 40, 35, 60, 200);
                SDL_Rect barBg = {barX, barY, barW, barH};
                SDL_RenderFillRect(renderer, &barBg);

                // Filled bar
                float range = static_cast<float>(m_options[i].maxVal - m_options[i].minVal);
                float pct = (range > 0.001f) ? static_cast<float>(m_options[i].value - m_options[i].minVal) / range : 0.0f;
                int fillW = static_cast<int>(barW * pct);
                SDL_SetRenderDrawColor(renderer, 100, 70, 200, 220);
                SDL_Rect barFill = {barX, barY, fillW, barH};
                SDL_RenderFillRect(renderer, &barFill);

                // Knob
                SDL_SetRenderDrawColor(renderer, 180, 150, 255, 255);
                SDL_Rect knob = {barX + fillW - 6, barY - 4, 12, barH + 8};
                SDL_RenderFillRect(renderer, &knob);

                // Arrows
                if (selected) {
                    SDL_Color arrC = {180, 150, 255, 200};
                    SDL_Surface* la = TTF_RenderUTF8_Blended(font, "<", arrC);
                    if (la) {
                        SDL_Texture* lat = SDL_CreateTextureFromSurface(renderer, la);
                        if (lat) {
                            SDL_Rect lar = {barX - 36, barY - 6, la->w, la->h};
                            SDL_RenderCopy(renderer, lat, nullptr, &lar);
                            SDL_DestroyTexture(lat);
                        }
                        SDL_FreeSurface(la);
                    }
                    SDL_Surface* ra = TTF_RenderUTF8_Blended(font, ">", arrC);
                    if (ra) {
                        SDL_Texture* rat = SDL_CreateTextureFromSurface(renderer, ra);
                        if (rat) {
                            SDL_Rect rar = {barX + barW + 12, barY - 6, ra->w, ra->h};
                            SDL_RenderCopy(renderer, rat, nullptr, &rar);
                            SDL_DestroyTexture(rat);
                        }
                        SDL_FreeSurface(ra);
                    }
                }
            }

            // Value text
            SDL_Color valColor = selected ? SDL_Color{200, 180, 255, 255} : SDL_Color{120, 110, 150, 255};
            SDL_Surface* vs = TTF_RenderUTF8_Blended(font, valText.c_str(), valColor);
            if (vs) {
                SDL_Texture* vt = SDL_CreateTextureFromSurface(renderer, vs);
                if (vt) {
                    SDL_Rect vr = {cardX + cardW - 110, y + (itemH - 8) / 2 - vs->h / 2, vs->w, vs->h};
                    SDL_RenderCopy(renderer, vt, nullptr, &vr);
                    SDL_DestroyTexture(vt);
                }
                SDL_FreeSurface(vs);
            }
        }
    }

    // Controls reference removed — redundant with the "Controls" submenu button
    // above (which opens the full Keybindings screen with all actions).

    // Navigation hint
    {
        SDL_Color nc = {120, 120, 140, 180};
        SDL_Surface* ns = TTF_RenderUTF8_Blended(font, LOC("options.nav_hint"), nc);
        if (ns) {
            SDL_Texture* nt = SDL_CreateTextureFromSurface(renderer, ns);
            if (nt) {
                SDL_Rect nr = {SCREEN_WIDTH / 2 - ns->w / 2, SCREEN_HEIGHT - 50, ns->w, ns->h};
                SDL_RenderCopy(renderer, nt, nullptr, &nr);
                SDL_DestroyTexture(nt);
            }
            SDL_FreeSurface(ns);
        }
    }
}
