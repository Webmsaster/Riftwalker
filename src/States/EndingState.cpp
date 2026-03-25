#include "States/EndingState.h"
#include "Core/Game.h"
#include <cmath>
#include <cstdio>

void EndingState::enter() {
    m_fontTitle = TTF_OpenFont("assets/fonts/default.ttf", 32);
    m_fontBody = TTF_OpenFont("assets/fonts/default.ttf", 18);
    m_fontSmall = TTF_OpenFont("assets/fonts/default.ttf", 14);
    m_phase = 0;
    m_phaseTimer = 0.0f;
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
            m_storyScroll += dt * 30.0f;
            // Scroll threshold depends on ending text length
            // healer=19 lines, destroyer=18 lines, speedrunner=14 lines
            // Formula: numLines * 30 + 650 (baseY=600 + buffer)
            float scrollMax = 1220.0f; // default: healer
            if (m_endingType == 1) scrollMax = 1190.0f;      // destroyer
            else if (m_endingType == 2) scrollMax = 1070.0f;  // speedrunner
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
                const char* flashText = "The Rift is sealed.";
                if (m_endingType == 1) flashText = "The Rift is shattered.";
                else if (m_endingType == 2) flashText = "The Rift barely noticed.";
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

            // Ending A: "The Healer" (default)
            const char* linesHealer[] = {
                "You stepped into the Rift seeking answers.",
                "Instead, you found only more questions.",
                "",
                "Each dimension was a fragment of a broken world,",
                "each enemy a shadow of what once was.",
                "",
                "The Void Sovereign was the last chain",
                "binding the Rift to this reality.",
                "",
                "With its defeat, the wound begins to heal.",
                "The dimensions slowly merge back together.",
                "",
                "But you know the truth now:",
                "The Rift was never the enemy.",
                "It was a mirror.",
                "",
                "And mirrors can never truly be shattered.",
                "",
                "Only understood."
            };

            // Ending B: "The Destroyer" (200+ kills)
            const char* linesDestroyer[] = {
                "You came to the Rift with purpose.",
                "Not to understand. Not to heal.",
                "But to destroy.",
                "",
                "Every enemy fell before you,",
                "every dimension trembled at your approach.",
                "",
                "The Void Sovereign didn't stand a chance.",
                "You didn't just seal the Rift --",
                "you shattered it.",
                "",
                "The dimensions merge violently now.",
                "Not healing. Collapsing.",
                "",
                "Perhaps destruction was always the answer.",
                "Or perhaps the Rift needed a gentler hand.",
                "",
                "You'll never know."
            };

            // Ending C: "The Speedrunner" (under 600 seconds)
            const char* linesSpeedrunner[] = {
                "Time bends around those who move through the Rift.",
                "You crossed thirty floors in mere minutes.",
                "",
                "The dimensions barely had time to notice you.",
                "The Void Sovereign barely had time to fight.",
                "",
                "You were a ghost.",
                "A ripple in the fabric of reality.",
                "",
                "The Rift sealed behind you",
                "before it even knew you were there.",
                "",
                "Speed is its own kind of mastery.",
                "But what did you miss along the way?"
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
                float baseY = 600 - m_storyScroll;
                for (int i = 0; i < numLines; i++) {
                    float ly = baseY + i * 30;
                    if (ly < -30 || ly > 620) continue;

                    // Fade at edges
                    float fadeAlpha = 1.0f;
                    if (ly < 80) fadeAlpha = ly / 80.0f;
                    if (ly > 520) fadeAlpha = (620 - ly) / 100.0f;
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
                SDL_Surface* surf = TTF_RenderText_Blended(m_fontSmall, "SPACE to skip", hint);
                if (surf) {
                    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
                    SDL_Rect dst = {SCREEN_WIDTH / 2 - surf->w / 2, 575, surf->w, surf->h};
                    SDL_RenderCopy(renderer, tex, nullptr, &dst);
                    SDL_DestroyTexture(tex);
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
                const char* runTitle = "Run Complete";
                if (m_endingType == 1) runTitle = "Run Complete - The Destroyer";
                else if (m_endingType == 2) runTitle = "Run Complete - The Speedrunner";
                SDL_Surface* surf = TTF_RenderText_Blended(m_fontTitle, runTitle, gold);
                if (surf) {
                    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
                    SDL_Rect dst = {SCREEN_WIDTH / 2 - surf->w / 2, 80, surf->w, surf->h};
                    SDL_RenderCopy(renderer, tex, nullptr, &dst);
                    SDL_DestroyTexture(tex);
                    SDL_FreeSurface(surf);
                }
            }

            if (m_fontBody) {
                char s1[64], s2[64], s3[64], s4[64], s5[64];
                snprintf(s1, sizeof(s1), "Final Score: %d", finalScore);
                snprintf(s2, sizeof(s2), "Enemies Defeated: %d", totalKills);
                snprintf(s3, sizeof(s3), "Max Difficulty: %d", maxDifficulty);
                snprintf(s4, sizeof(s4), "Relics Found: %d", relicsCollected);
                int mins = static_cast<int>(totalTime) / 60;
                int secs = static_cast<int>(totalTime) % 60;
                snprintf(s5, sizeof(s5), "Time: %d:%02d", mins, secs);

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
                        SDL_Rect dst = {SCREEN_WIDTH / 2 - surf->w / 2, 180 + i * 45, surf->w, surf->h};
                        SDL_RenderCopy(renderer, tex, nullptr, &dst);
                        SDL_DestroyTexture(tex);
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
                SDL_Surface* surf = TTF_RenderText_Blended(m_fontSmall, "Press SPACE to continue", hint);
                if (surf) {
                    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
                    SDL_Rect dst = {SCREEN_WIDTH / 2 - surf->w / 2, 500, surf->w, surf->h};
                    SDL_RenderCopy(renderer, tex, nullptr, &dst);
                    SDL_DestroyTexture(tex);
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
                SDL_Surface* surf = TTF_RenderText_Blended(m_fontTitle, "Thank You for Playing", col);
                if (surf) {
                    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
                    SDL_Rect dst = {SCREEN_WIDTH / 2 - surf->w / 2, 200, surf->w, surf->h};
                    SDL_RenderCopy(renderer, tex, nullptr, &dst);
                    SDL_DestroyTexture(tex);
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
                SDL_Surface* surf = TTF_RenderText_Blended(m_fontBody, "The Rift awaits your return... Ascend higher.", col);
                if (surf) {
                    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
                    SDL_Rect dst = {SCREEN_WIDTH / 2 - surf->w / 2, 300, surf->w, surf->h};
                    SDL_RenderCopy(renderer, tex, nullptr, &dst);
                    SDL_DestroyTexture(tex);
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
                SDL_Surface* surf = TTF_RenderText_Blended(m_fontSmall, "Press any key to return to menu", hint);
                if (surf) {
                    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
                    SDL_Rect dst = {SCREEN_WIDTH / 2 - surf->w / 2, 500, surf->w, surf->h};
                    SDL_RenderCopy(renderer, tex, nullptr, &dst);
                    SDL_DestroyTexture(tex);
                    SDL_FreeSurface(surf);
                }
            }
            break;
        }
    }
}

void EndingState::handleEvent(const SDL_Event& event) {
    if (event.type == SDL_KEYDOWN) {
        switch (m_phase) {
            case 0: // Skip flash (only SPACE/RETURN)
                if (event.key.keysym.sym == SDLK_SPACE || event.key.keysym.sym == SDLK_RETURN) {
                    m_phase = 1;
                    m_phaseTimer = 0.0f;
                }
                break;
            case 1: // Skip story
                if (event.key.keysym.sym == SDLK_SPACE) {
                    m_phase = 2;
                    m_phaseTimer = 0.0f;
                }
                break;
            case 2: // Go to thank you
                if (event.key.keysym.sym == SDLK_SPACE && m_phaseTimer > 3.0f) {
                    m_phase = 3;
                    m_phaseTimer = 0.0f;
                }
                break;
            case 3: // Return to menu
                if (m_phaseTimer > 2.0f) {
                    if (game) game->changeState(StateID::Menu);
                }
                break;
        }
    }
}
