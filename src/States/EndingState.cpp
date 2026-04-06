#include "States/EndingState.h"
#include "Core/Game.h"
#include "Core/AudioManager.h"
#include "Core/Localization.h"
#include "Game/ClassSystem.h"
#include <cmath>
#include <cstdio>

extern PlayerClass g_selectedClass;

void EndingState::enter() {
    m_fontTitle = TTF_OpenFont("assets/fonts/default.ttf", 64);
    m_fontBody = TTF_OpenFont("assets/fonts/default.ttf", 36);
    m_fontSmall = TTF_OpenFont("assets/fonts/default.ttf", 28);
    m_phase = 0;
    m_phaseTimer = 0.0f;

    // Victory music
    AudioManager::instance().stopAmbient();
    AudioManager::instance().stopMusicLayers();
    AudioManager::instance().playMusic("assets/music/victory.ogg");
    m_time = 0.0f;
    m_storyScroll = 0.0f;

    // Determine ending type based on run stats
    // Priority: speedrunner > destroyer > healer (default)
    if (totalTime > 0.0f && totalTime < 600.0f) {
        m_endingType = 2; // Speedrunner: completed in under 10 minutes
        // Speed Demon: complete the game in under 10 minutes
        if (game) game->getAchievements().unlock("speed_demon");
    } else if (totalKills >= 200) {
        m_endingType = 1; // Destroyer: 200+ kills
    } else {
        m_endingType = 0; // Healer: default ending
    }

    // Class mastery achievements
    if (game) {
        auto& ach = game->getAchievements();
        switch (g_selectedClass) {
            case PlayerClass::Voidwalker:   ach.unlock("void_mastery"); break;
            case PlayerClass::Berserker:    ach.unlock("berserk_mastery"); break;
            case PlayerClass::Phantom:      ach.unlock("phantom_mastery"); break;
            case PlayerClass::Technomancer: ach.unlock("tech_mastery"); break;
            default: break;
        }
        // Jack of All Trades: check if all 4 class masteries are unlocked
        if (ach.isUnlocked("void_mastery") && ach.isUnlocked("berserk_mastery") &&
            ach.isUnlocked("phantom_mastery") && ach.isUnlocked("tech_mastery")) {
            ach.unlock("all_classes");
        }
        // Ascension achievements (based on the NG+ tier of this winning run)
        if (ngPlusTier >= 1) ach.unlock("ascension_1");
        if (ngPlusTier >= 5) ach.unlock("ascension_5");
    }
}

void EndingState::exit() {
    if (m_fontTitle) { TTF_CloseFont(m_fontTitle); m_fontTitle = nullptr; }
    if (m_fontBody) { TTF_CloseFont(m_fontBody); m_fontBody = nullptr; }
    if (m_fontSmall) { TTF_CloseFont(m_fontSmall); m_fontSmall = nullptr; }
}

void EndingState::update(float dt) {
    m_time += dt;
    m_phaseTimer += dt;

    switch (m_phase) {
        case 0: // White flash
            if (m_phaseTimer > 2.0f) { m_phase = 1; m_phaseTimer = 0.0f; }
            break;
        case 1: { // Story scroll
            m_storyScroll += dt * 60.0f; // doubled for 2K
            // Scroll threshold depends on ending text length (doubled for 2K)
            // healer=19 lines, destroyer=18 lines, speedrunner=14 lines
            float scrollMax = 2440.0f; // default: healer
            if (m_endingType == 1) scrollMax = 2380.0f;      // destroyer
            else if (m_endingType == 2) scrollMax = 2140.0f;  // speedrunner
            if (m_storyScroll > scrollMax) { m_phase = 2; m_phaseTimer = 0.0f; }
            break;
        }
        case 2: // Stats
            break;
        case 3: // Thank you
            break;
    }
}

void EndingState::render(SDL_Renderer* renderer) {
    switch (m_phase) {
        case 0: { // White flash fading to black
            float t = m_phaseTimer / 2.0f;
            Uint8 brightness = static_cast<Uint8>(255 * std::max(0.0f, 1.0f - t));
            SDL_SetRenderDrawColor(renderer, brightness, brightness, brightness, 255);
            SDL_RenderClear(renderer);

            // "The Rift is sealed" fades in
            if (m_phaseTimer > 0.8f && m_fontTitle) {
                float textAlpha = std::min(1.0f, (m_phaseTimer - 0.8f) / 0.8f);
                SDL_Color col = {
                    static_cast<Uint8>(200 * textAlpha),
                    static_cast<Uint8>(180 * textAlpha),
                    static_cast<Uint8>(255 * textAlpha), 255
                };
                const char* flashText = LOC("ending.sealed");
                if (m_endingType == 1) flashText = LOC("ending.shattered");
                else if (m_endingType == 2) flashText = LOC("ending.barely");
                SDL_Surface* surf = TTF_RenderText_Blended(m_fontTitle, flashText, col);
                if (surf) {
                    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
                    if (tex) {
                    SDL_SetTextureAlphaMod(tex, static_cast<Uint8>(255 * textAlpha));
                    SDL_Rect dst = {SCREEN_WIDTH / 2 - surf->w / 2, 280, surf->w, surf->h};
                    SDL_RenderCopy(renderer, tex, nullptr, &dst);
                    SDL_DestroyTexture(tex);
                    }
                    SDL_FreeSurface(surf);
                }
            }
            break;
        }

        case 1: { // Story scroll
            SDL_SetRenderDrawColor(renderer, 5, 2, 10, 255);
            SDL_RenderClear(renderer);

            // Ending narratives via LOC() keys
            const char* linesHealer[] = {
                LOC("ending.h.0"), LOC("ending.h.1"), "",
                LOC("ending.h.2"), LOC("ending.h.3"), "",
                LOC("ending.h.4"), LOC("ending.h.5"), "",
                LOC("ending.h.6"), LOC("ending.h.7"), "",
                LOC("ending.h.8"), LOC("ending.h.9"),
                LOC("ending.h.10"), "",
                LOC("ending.h.11"), "", LOC("ending.h.12")
            };

            const char* linesDestroyer[] = {
                LOC("ending.d.0"), LOC("ending.d.1"), LOC("ending.d.2"), "",
                LOC("ending.d.3"), LOC("ending.d.4"), "",
                LOC("ending.d.5"), LOC("ending.d.6"), LOC("ending.d.7"), "",
                LOC("ending.d.8"), LOC("ending.d.9"), "",
                LOC("ending.d.10"), LOC("ending.d.11"), "",
                LOC("ending.d.12")
            };

            const char* linesSpeedrunner[] = {
                LOC("ending.s.0"), LOC("ending.s.1"), "",
                LOC("ending.s.2"), LOC("ending.s.3"), "",
                LOC("ending.s.4"), LOC("ending.s.5"), "",
                LOC("ending.s.6"), LOC("ending.s.7"), "",
                LOC("ending.s.8"), LOC("ending.s.9")
            };

            const char** lines;
            int numLines;
            if (m_endingType == 1) {
                lines = linesDestroyer;
                numLines = sizeof(linesDestroyer) / sizeof(linesDestroyer[0]);
            } else if (m_endingType == 2) {
                lines = linesSpeedrunner;
                numLines = sizeof(linesSpeedrunner) / sizeof(linesSpeedrunner[0]);
            } else {
                lines = linesHealer;
                numLines = sizeof(linesHealer) / sizeof(linesHealer[0]);
            }

            if (m_fontBody) {
                float baseY = 1200 - m_storyScroll;
                for (int i = 0; i < numLines; i++) {
                    float ly = baseY + i * 60;
                    if (ly < -60 || ly > 1240) continue;

                    // Fade at edges
                    float fadeAlpha = 1.0f;
                    if (ly < 160) fadeAlpha = ly / 160.0f;
                    if (ly > 1040) fadeAlpha = (1240 - ly) / 200.0f;
                    fadeAlpha = std::max(0.0f, std::min(1.0f, fadeAlpha));

                    SDL_Color col = {
                        static_cast<Uint8>(200 * fadeAlpha),
                        static_cast<Uint8>(190 * fadeAlpha),
                        static_cast<Uint8>(220 * fadeAlpha), 255
                    };
                    if (strlen(lines[i]) == 0) continue;
                    SDL_Surface* surf = TTF_RenderText_Blended(m_fontBody, lines[i], col);
                    if (surf) {
                        SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
                        if (tex) {
                            SDL_SetTextureAlphaMod(tex, static_cast<Uint8>(255 * fadeAlpha));
                            SDL_Rect dst = {SCREEN_WIDTH / 2 - surf->w / 2, static_cast<int>(ly), surf->w, surf->h};
                            SDL_RenderCopy(renderer, tex, nullptr, &dst);
                            SDL_DestroyTexture(tex);
                        }
                        SDL_FreeSurface(surf);
                    }
                }

                // Phase transition handled in update()
            }

            // Hint
            if (m_fontSmall && m_phaseTimer > 3.0f) {
                SDL_Color hint = {60, 50, 80, 255};
                SDL_Surface* surf = TTF_RenderText_Blended(m_fontSmall, LOC("ending.skip"), hint);
                if (surf) {
                    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
                    if (tex) {
                        SDL_Rect dst = {SCREEN_WIDTH / 2 - surf->w / 2, SCREEN_HEIGHT - 60, surf->w, surf->h};
                        SDL_RenderCopy(renderer, tex, nullptr, &dst);
                        SDL_DestroyTexture(tex);
                    }
                    SDL_FreeSurface(surf);
                }
            }
            break;
        }

        case 2: { // Stats
            SDL_SetRenderDrawColor(renderer, 5, 2, 10, 255);
            SDL_RenderClear(renderer);

            float fadeIn = std::min(1.0f, m_phaseTimer / 1.0f);

            if (m_fontTitle) {
                SDL_Color gold = {
                    static_cast<Uint8>(220 * fadeIn),
                    static_cast<Uint8>(190 * fadeIn),
                    static_cast<Uint8>(80 * fadeIn), 255
                };
                const char* runTitle = LOC("ending.run_complete");
                if (m_endingType == 1) runTitle = LOC("ending.run_destroyer");
                else if (m_endingType == 2) runTitle = LOC("ending.run_speedrunner");
                SDL_Surface* surf = TTF_RenderText_Blended(m_fontTitle, runTitle, gold);
                if (surf) {
                    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
                    if (tex) {
                        SDL_Rect dst = {SCREEN_WIDTH / 2 - surf->w / 2, 160, surf->w, surf->h};
                        SDL_RenderCopy(renderer, tex, nullptr, &dst);
                        SDL_DestroyTexture(tex);
                    }
                    SDL_FreeSurface(surf);
                }
            }

            if (m_fontBody) {
                char s1[64], s2[64], s3[64], s4[64], s5[64];
                snprintf(s1, sizeof(s1), LOC("ending.final_score"), finalScore);
                snprintf(s2, sizeof(s2), LOC("ending.enemies"), totalKills);
                snprintf(s3, sizeof(s3), LOC("ending.difficulty"), maxDifficulty);
                snprintf(s4, sizeof(s4), LOC("ending.relics"), relicsCollected);
                int mins = static_cast<int>(totalTime) / 60;
                int secs = static_cast<int>(totalTime) % 60;
                snprintf(s5, sizeof(s5), LOC("ending.time"), mins, secs);

                const char* statLines[] = { s1, s2, s3, s4, s5 };
                for (int i = 0; i < 5; i++) {
                    float lineDelay = 0.5f + i * 0.3f;
                    if (m_phaseTimer < lineDelay) continue;
                    float lineAlpha = std::min(1.0f, (m_phaseTimer - lineDelay) / 0.5f) * fadeIn;

                    SDL_Color col = {
                        static_cast<Uint8>(200 * lineAlpha),
                        static_cast<Uint8>(200 * lineAlpha),
                        static_cast<Uint8>(220 * lineAlpha), 255
                    };
                    SDL_Surface* surf = TTF_RenderText_Blended(m_fontBody, statLines[i], col);
                    if (surf) {
                        SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
                        if (tex) {
                            SDL_Rect dst = {SCREEN_WIDTH / 2 - surf->w / 2, 360 + i * 90, surf->w, surf->h};
                            SDL_RenderCopy(renderer, tex, nullptr, &dst);
                            SDL_DestroyTexture(tex);
                        }
                        SDL_FreeSurface(surf);
                    }
                }
            }

            // Continue hint
            if (m_phaseTimer > 3.0f && m_fontSmall) {
                float pulse = 0.5f + 0.5f * std::sin(m_time * 3.0f);
                SDL_Color hint = {
                    static_cast<Uint8>(150 * pulse),
                    static_cast<Uint8>(130 * pulse),
                    static_cast<Uint8>(180 * pulse), 255
                };
                SDL_Surface* surf = TTF_RenderText_Blended(m_fontSmall, LOC("ending.continue"), hint);
                if (surf) {
                    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
                    if (tex) {
                        SDL_Rect dst = {SCREEN_WIDTH / 2 - surf->w / 2, 1000, surf->w, surf->h};
                        SDL_RenderCopy(renderer, tex, nullptr, &dst);
                        SDL_DestroyTexture(tex);
                    }
                    SDL_FreeSurface(surf);
                }
            }
            break;
        }

        case 3: { // Thank you + Ascension hint
            SDL_SetRenderDrawColor(renderer, 5, 2, 10, 255);
            SDL_RenderClear(renderer);

            float fadeIn = std::min(1.0f, m_phaseTimer / 2.0f);

            if (m_fontTitle) {
                SDL_Color col = {
                    static_cast<Uint8>(180 * fadeIn),
                    static_cast<Uint8>(160 * fadeIn),
                    static_cast<Uint8>(255 * fadeIn), 255
                };
                SDL_Surface* surf = TTF_RenderText_Blended(m_fontTitle, LOC("ending.thank_you"), col);
                if (surf) {
                    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
                    if (tex) {
                        SDL_Rect dst = {SCREEN_WIDTH / 2 - surf->w / 2, 400, surf->w, surf->h};
                        SDL_RenderCopy(renderer, tex, nullptr, &dst);
                        SDL_DestroyTexture(tex);
                    }
                    SDL_FreeSurface(surf);
                }
            }

            if (m_fontBody && m_phaseTimer > 1.5f) {
                float hintAlpha = std::min(1.0f, (m_phaseTimer - 1.5f) / 1.0f);
                SDL_Color col = {
                    static_cast<Uint8>(150 * hintAlpha),
                    static_cast<Uint8>(120 * hintAlpha),
                    static_cast<Uint8>(200 * hintAlpha), 255
                };
                SDL_Surface* surf = TTF_RenderText_Blended(m_fontBody, LOC("ending.return"), col);
                if (surf) {
                    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
                    if (tex) {
                        SDL_Rect dst = {SCREEN_WIDTH / 2 - surf->w / 2, 600, surf->w, surf->h};
                        SDL_RenderCopy(renderer, tex, nullptr, &dst);
                        SDL_DestroyTexture(tex);
                    }
                    SDL_FreeSurface(surf);
                }
            }

            if (m_fontSmall && m_phaseTimer > 3.0f) {
                float pulse = 0.5f + 0.5f * std::sin(m_time * 3.0f);
                SDL_Color hint = {
                    static_cast<Uint8>(100 * pulse),
                    static_cast<Uint8>(80 * pulse),
                    static_cast<Uint8>(140 * pulse), 255
                };
                SDL_Surface* surf = TTF_RenderText_Blended(m_fontSmall, LOC("ending.back"), hint);
                if (surf) {
                    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
                    if (tex) {
                        SDL_Rect dst = {SCREEN_WIDTH / 2 - surf->w / 2, 1000, surf->w, surf->h};
                        SDL_RenderCopy(renderer, tex, nullptr, &dst);
                        SDL_DestroyTexture(tex);
                    }
                    SDL_FreeSurface(surf);
                }
            }
            break;
        }
    }
}

void EndingState::handleEvent(const SDL_Event& event) {
    bool isConfirm = false;
    if (event.type == SDL_KEYDOWN) {
        isConfirm = (event.key.keysym.sym == SDLK_SPACE || event.key.keysym.sym == SDLK_RETURN);
    } else if (event.type == SDL_CONTROLLERBUTTONDOWN) {
        isConfirm = (event.cbutton.button == SDL_CONTROLLER_BUTTON_A ||
                     event.cbutton.button == SDL_CONTROLLER_BUTTON_START);
    }

    if (isConfirm) {
        switch (m_phase) {
            case 0: m_phase = 1; m_phaseTimer = 0.0f; break;
            case 1: m_phase = 2; m_phaseTimer = 0.0f; break;
            case 2:
                if (m_phaseTimer > 3.0f) { m_phase = 3; m_phaseTimer = 0.0f; }
                break;
            case 3:
                if (m_phaseTimer > 2.0f) {
                    if (game) game->changeState(StateID::Menu);
                }
                break;
        }
    }
}
