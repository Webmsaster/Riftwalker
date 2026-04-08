// PlayStateRenderOverlays.cpp -- Split from PlayStateRender.cpp (modal overlays, notifications, tutorial)
#include "PlayState.h"
#include "Core/Game.h"
#include "Core/Localization.h"
#include "Game/Enemy.h"
#include "Components/TransformComponent.h"
#include "Components/HealthComponent.h"
#include "Components/CombatComponent.h"
#include "Components/PhysicsBody.h"
#include "Game/Tile.h"
#include "Components/AIComponent.h"
#include "Components/SpriteComponent.h"
#include "Components/AbilityComponent.h"
#include "Core/AudioManager.h"
#include "Game/AchievementSystem.h"
#include "Game/RelicSystem.h"
#include "Game/RelicSynergy.h"
#include "Game/ClassSystem.h"
#include "Game/LoreSystem.h"
#include "Game/DailyRun.h"
#include "Game/Bestiary.h"
#include "Game/DimensionShiftBalance.h"
#include "Components/RelicComponent.h"
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <algorithm>
#include <string>

void PlayState::renderDeathSequence(SDL_Renderer* renderer) {
    if (!m_playerDying) return;

    float progress = 1.0f - (m_deathSequenceTimer / m_deathSequenceDuration);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // Layer 1: Multiplicative desaturation (drains color toward muted gray)
    {
        float desatProgress = std::min(1.0f, progress * 1.5f);
        Uint8 modVal = static_cast<Uint8>(255 - desatProgress * 80); // 255→175
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_MOD);
        SDL_SetRenderDrawColor(renderer, modVal, modVal, modVal, 255);
        SDL_Rect fs = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
        SDL_RenderFillRect(renderer, &fs);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    }

    // Layer 2: Dark + red overlay (life draining away)
    {
        Uint8 darkA = static_cast<Uint8>(std::min(140.0f, progress * 180.0f));
        SDL_SetRenderDrawColor(renderer, 25, 5, 5, darkA);
        SDL_Rect fs = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
        SDL_RenderFillRect(renderer, &fs);
    }

    // Layer 3: Expanding dark vignette border (red edges closing in)
    int borderW = static_cast<int>(40 + progress * 160);
    Uint8 borderAlpha = static_cast<Uint8>(std::min(220.0f, progress * 270.0f));
    SDL_SetRenderDrawColor(renderer, 140, 10, 0, borderAlpha);
    SDL_Rect top = {0, 0, SCREEN_WIDTH, borderW};
    SDL_Rect bot = {0, SCREEN_HEIGHT - borderW, SCREEN_WIDTH, borderW};
    SDL_Rect lft = {0, 0, borderW, SCREEN_HEIGHT};
    SDL_Rect rgt = {SCREEN_WIDTH - borderW, 0, borderW, SCREEN_HEIGHT};
    SDL_RenderFillRect(renderer, &top);
    SDL_RenderFillRect(renderer, &bot);
    SDL_RenderFillRect(renderer, &lft);
    SDL_RenderFillRect(renderer, &rgt);
    // Softer inner vignette (darker, slightly smaller)
    int innerW = std::max(0, borderW - 30);
    Uint8 innerAlpha = static_cast<Uint8>(std::min(180.0f, progress * 220.0f));
    SDL_SetRenderDrawColor(renderer, 10, 0, 0, innerAlpha);
    SDL_Rect iTop = {0, 0, SCREEN_WIDTH, innerW};
    SDL_Rect iBot = {0, SCREEN_HEIGHT - innerW, SCREEN_WIDTH, innerW};
    SDL_Rect iLft = {0, 0, innerW, SCREEN_HEIGHT};
    SDL_Rect iRgt = {SCREEN_WIDTH - innerW, 0, innerW, SCREEN_HEIGHT};
    SDL_RenderFillRect(renderer, &iTop);
    SDL_RenderFillRect(renderer, &iBot);
    SDL_RenderFillRect(renderer, &iLft);
    SDL_RenderFillRect(renderer, &iRgt);

    // Layer 4: Scanline flicker effect (brief horizontal lines for glitch feel)
    if (progress > 0.1f && progress < 0.9f) {
        Uint8 scanAlpha = static_cast<Uint8>(30 + 40 * std::sin(m_frameTicks * 0.03f));
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, scanAlpha);
        int scanStep = 4 + (std::rand() % 3); // slightly randomized spacing
        for (int y = 0; y < SCREEN_HEIGHT; y += scanStep) {
            SDL_RenderDrawLine(renderer, 0, y, SCREEN_WIDTH, y);
        }
    }

    // "SUIT FAILURE" text (fades in with pulsing glow)
    TTF_Font* font = game->getFont();
    if (font && progress > 0.15f) {
        float textAlpha = std::min(1.0f, (progress - 0.15f) * 2.5f);
        Uint8 ta = static_cast<Uint8>(textAlpha * 255);
        float pulse = 0.6f + 0.4f * std::sin(m_frameTicks * 0.012f);
        Uint8 tr = static_cast<Uint8>(200 * pulse + 55);
        SDL_Color deathColor = {tr, 20, 10, ta};
        SDL_Surface* ds = TTF_RenderText_Blended(font, LOC("gameover.suit_failure"), deathColor);
        if (ds) {
            SDL_Texture* dt = SDL_CreateTextureFromSurface(renderer, ds);
            if (dt) {
                SDL_SetTextureAlphaMod(dt, ta);
                // Center of screen, slight upward drift + subtle shake
                int yOff = static_cast<int>(progress * 20.0f);
                int shakeX = (progress < 0.6f) ? ((std::rand() % 5) - 2) : 0;
                int shakeY = (progress < 0.6f) ? ((std::rand() % 3) - 1) : 0;
                SDL_Rect dr = {SCREEN_WIDTH / 2 - ds->w / 2 + shakeX,
                               SCREEN_HEIGHT / 2 - ds->h / 2 - yOff + shakeY,
                               ds->w, ds->h};
                SDL_RenderCopy(renderer, dt, nullptr, &dr);
                SDL_DestroyTexture(dt);
            }
            SDL_FreeSurface(ds);
        }
    }
}

// renderAchievementNotification, renderLoreNotification -> PlayStateRenderCombatUI.cpp

void PlayState::renderRelicChoice(SDL_Renderer* renderer, TTF_Font* font) {
    if (!font || m_relicChoices.empty()) return;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // Dark overlay
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 160);
    SDL_Rect full = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
    SDL_RenderFillRect(renderer, &full);

    // Title
    {
        SDL_Color tc = {255, 215, 100, 255};
        SDL_Surface* s = TTF_RenderText_Blended(font, LOC("relic.choose"), tc);
        if (s) {
            SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
            if (t) {
                SDL_Rect r = {SCREEN_WIDTH / 2 - s->w / 2, 360, s->w, s->h};
                SDL_RenderCopy(renderer, t, nullptr, &r);
                SDL_DestroyTexture(t);
            }
            SDL_FreeSurface(s);
        }
    }

    // Relic cards
    int cardW = 400;
    int cardH = 520;
    int gap = 60;
    int totalW = static_cast<int>(m_relicChoices.size()) * cardW + (static_cast<int>(m_relicChoices.size()) - 1) * gap;
    int startX = SCREEN_WIDTH / 2 - totalW / 2;
    int cardY = 460;

    for (int i = 0; i < static_cast<int>(m_relicChoices.size()); i++) {
        int cx = startX + i * (cardW + gap);
        bool selected = (i == m_relicChoiceSelected);
        auto& data = RelicSystem::getRelicData(m_relicChoices[i]);

        // Card background
        SDL_SetRenderDrawColor(renderer, 15, 15, 25, 220);
        SDL_Rect cardBg = {cx, cardY, cardW, cardH};
        SDL_RenderFillRect(renderer, &cardBg);

        // Border (glow when selected)
        Uint8 borderA = selected ? 255 : 100;
        SDL_SetRenderDrawColor(renderer, data.glowColor.r, data.glowColor.g, data.glowColor.b, borderA);
        SDL_RenderDrawRect(renderer, &cardBg);
        if (selected) {
            SDL_Rect outer = {cx - 2, cardY - 2, cardW + 4, cardH + 4};
            SDL_RenderDrawRect(renderer, &outer);
        }

        // Relic icon (colored orb)
        int orbX = cx + cardW / 2;
        int orbY = cardY + 100;
        int orbR = 48;
        for (int oy = -orbR; oy <= orbR; oy++) {
            for (int ox = -orbR; ox <= orbR; ox++) {
                if (ox * ox + oy * oy <= orbR * orbR) {
                    float dist = std::sqrt(static_cast<float>(ox * ox + oy * oy)) / orbR;
                    Uint8 a = static_cast<Uint8>((1.0f - dist * 0.5f) * 255);
                    SDL_SetRenderDrawColor(renderer, data.glowColor.r, data.glowColor.g, data.glowColor.b, a);
                    SDL_RenderDrawPoint(renderer, orbX + ox, orbY + oy);
                }
            }
        }

        // Tier text + CURSED label
        bool isCursedRelic = RelicSystem::isCursed(m_relicChoices[i]);
        const char* tierText = LOC("shop.common");
        SDL_Color tierColor = {180, 180, 180, 255};
        if (data.tier == RelicTier::Rare)      { tierText = LOC("shop.rare");      tierColor = {80, 180, 255, 255}; }
        else if (data.tier == RelicTier::Legendary) { tierText = LOC("shop.legendary"); tierColor = {255, 200, 50, 255}; }
        // CURSED override: override tier display with red "CURSED" label
        if (isCursedRelic) { tierText = LOC("relic.cursed"); tierColor = {255, 50, 50, 255}; }
        {
            SDL_Surface* s = TTF_RenderText_Blended(font, tierText, tierColor);
            if (s) {
                SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
                if (t) {
                    SDL_Rect r = {cx + cardW / 2 - s->w / 2, cardY + 180, s->w, s->h};
                    SDL_RenderCopy(renderer, t, nullptr, &r);
                    SDL_DestroyTexture(t);
                }
                SDL_FreeSurface(s);
            }
        }
        // Cursed relics: draw a red skull-mark (X lines) in the top-right corner of the card
        if (isCursedRelic) {
            SDL_SetRenderDrawColor(renderer, 200, 20, 20, 200);
            int mx = cx + cardW - 28, my = cardY + 16;
            SDL_RenderDrawLine(renderer, mx, my, mx + 10, my + 10);
            SDL_RenderDrawLine(renderer, mx + 10, my, mx, my + 10);
            SDL_RenderDrawLine(renderer, mx + 1, my, mx + 11, my + 10);
            SDL_RenderDrawLine(renderer, mx + 11, my, mx + 1, my + 10);
            // Dark red card tint overlay
            SDL_SetRenderDrawColor(renderer, 80, 0, 0, 30);
            SDL_RenderFillRect(renderer, &cardBg);
        }

        // Name (localized relic name)
        char rNameKey[48], rDescKey[48];
        snprintf(rNameKey, sizeof(rNameKey), "relic.%d.name", static_cast<int>(data.id));
        snprintf(rDescKey, sizeof(rDescKey), "relic.%d.desc", static_cast<int>(data.id));
        const char* rLocName = LOC(rNameKey);
        const char* rDisplayName = (strcmp(rLocName, rNameKey) == 0) ? data.name : rLocName;
        const char* rLocDesc = LOC(rDescKey);
        const char* rDisplayDesc = (strcmp(rLocDesc, rDescKey) == 0) ? data.description : rLocDesc;
        {
            SDL_Color nc = {255, 255, 255, 255};
            SDL_Surface* s = TTF_RenderText_Blended(font, rDisplayName, nc);
            if (s) {
                SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
                if (t) {
                    SDL_Rect r = {cx + cardW / 2 - s->w / 2, cardY + 240, s->w, s->h};
                    SDL_RenderCopy(renderer, t, nullptr, &r);
                    SDL_DestroyTexture(t);
                }
                SDL_FreeSurface(s);
            }
        }

        // Description
        {
            SDL_Color dc = {180, 180, 200, 220};
            SDL_Surface* s = TTF_RenderText_Blended(font, rDisplayDesc, dc);
            if (s) {
                SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
                if (t) {
                    SDL_Rect r = {cx + cardW / 2 - s->w / 2, cardY + 320, s->w, s->h};
                    SDL_RenderCopy(renderer, t, nullptr, &r);
                    SDL_DestroyTexture(t);
                }
                SDL_FreeSurface(s);
            }
        }

        // Key hint
        char keyBuf[8];
        std::snprintf(keyBuf, sizeof(keyBuf), "[%d]", i + 1);
        {
            SDL_Color kc = selected ? SDL_Color{255, 255, 255, 255} : SDL_Color{100, 100, 120, 180};
            SDL_Surface* s = TTF_RenderText_Blended(font, keyBuf, kc);
            if (s) {
                SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
                if (t) {
                    SDL_Rect r = {cx + cardW / 2 - s->w / 2, cardY + cardH - 70, s->w, s->h};
                    SDL_RenderCopy(renderer, t, nullptr, &r);
                    SDL_DestroyTexture(t);
                }
                SDL_FreeSurface(s);
            }
        }
    }
}

void PlayState::renderEventChain(SDL_Renderer* renderer, TTF_Font* font) {
    if (m_eventChain.stage <= 0 && m_chainRewardTimer <= 0) return;
    if (!font) return;

    SDL_Color cc = m_eventChain.getColor();

    // Active chain: show progress tracker below the main HUD panel
    if (m_eventChain.stage > 0 && !m_eventChain.completed) {
        int panelX = 30;
        int panelY = 540; // Below HUD (HP/XP/Entropy/Dim/Abilities ~30-500)
        int panelW = 600;
        int panelH = 140;

        // Background
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_Rect bg = {panelX, panelY, panelW, panelH};
        SDL_SetRenderDrawColor(renderer, 20, 20, 30, 180);
        SDL_RenderFillRect(renderer, &bg);

        // Border in chain color
        SDL_SetRenderDrawColor(renderer, cc.r, cc.g, cc.b, 200);
        SDL_RenderDrawRect(renderer, &bg);

        // Chain icon: linked circles
        for (int i = 0; i < m_eventChain.maxStages; i++) {
            int cx = panelX + 40 + i * 56;
            int cy = panelY + 40;
            int r = 14;
            if (i < m_eventChain.stage) {
                // Filled circle for completed stages
                SDL_Rect dot = {cx - r, cy - r, r * 2, r * 2};
                SDL_SetRenderDrawColor(renderer, cc.r, cc.g, cc.b, 255);
                SDL_RenderFillRect(renderer, &dot);
            } else {
                // Hollow for pending
                SDL_Rect dot = {cx - r, cy - r, r * 2, r * 2};
                SDL_SetRenderDrawColor(renderer, cc.r, cc.g, cc.b, 100);
                SDL_RenderDrawRect(renderer, &dot);
            }
            // Link line
            if (i < m_eventChain.maxStages - 1) {
                SDL_SetRenderDrawColor(renderer, cc.r, cc.g, cc.b,
                    static_cast<Uint8>(i < m_eventChain.stage - 1 ? 200 : 60));
                SDL_RenderDrawLine(renderer, cx + r + 1, cy, cx + 56 - r - 1, cy);
            }
        }

        // Chain name
        SDL_Color white = {255, 255, 255, 255};
        SDL_Surface* nameSurf = TTF_RenderText_Blended(font, m_eventChain.getName(), white);
        if (nameSurf) {
            SDL_Texture* nameTex = SDL_CreateTextureFromSurface(renderer, nameSurf);
            if (nameTex) {
                SDL_Rect nameR = {panelX + 220, panelY + 14, nameSurf->w, nameSurf->h};
                SDL_RenderCopy(renderer, nameTex, nullptr, &nameR);
                SDL_DestroyTexture(nameTex);
            }
            SDL_FreeSurface(nameSurf);
        }

        // Stage description
        const char* desc = m_eventChain.getStageDesc();
        if (desc && desc[0]) {
            SDL_Color dimWhite = {200, 200, 200, 200};
            SDL_Surface* descSurf = TTF_RenderText_Blended(font, desc, dimWhite);
            if (descSurf) {
                SDL_Texture* descTex = SDL_CreateTextureFromSurface(renderer, descSurf);
                if (descTex) {
                    int maxW = panelW - 20;
                    int dw = std::min(descSurf->w, maxW);
                    int dh = descSurf->h * dw / std::max(descSurf->w, 1);
                    SDL_Rect descR = {panelX + 14, panelY + 80, dw, dh};
                    SDL_RenderCopy(renderer, descTex, nullptr, &descR);
                    SDL_DestroyTexture(descTex);
                }
                SDL_FreeSurface(descSurf);
            }
        }
    }

    // Stage advance notification (slide-in from top)
    if (m_chainNotifyTimer > 0 && !m_eventChain.completed) {
        float slideT = std::min(m_chainNotifyTimer / 3.5f, 1.0f);
        float slideIn = (slideT > 0.85f) ? (1.0f - slideT) / 0.15f : (slideT < 0.15f ? slideT / 0.15f : 1.0f);
        Uint8 alpha = static_cast<Uint8>(255 * slideIn);

        char stageText[64];
        snprintf(stageText, sizeof(stageText), LOC("hud.chain_stage"), m_eventChain.stage, m_eventChain.maxStages);

        SDL_Color textCol = {cc.r, cc.g, cc.b, alpha};
        SDL_Surface* surf = TTF_RenderText_Blended(font, stageText, textCol);
        if (surf) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
            if (tex) {
                SDL_SetTextureAlphaMod(tex, alpha);
                int tx = (SCREEN_WIDTH - surf->w) / 2;
                int ty = 100 + static_cast<int>((1.0f - slideIn) * -40);
                SDL_Rect r = {tx, ty, surf->w, surf->h};
                SDL_RenderCopy(renderer, tex, nullptr, &r);
                SDL_DestroyTexture(tex);
            }
            SDL_FreeSurface(surf);
        }
    }

    // Completion reward popup (event chain)
    if (m_chainRewardTimer > 0) {
        float fadeT = std::min(m_chainRewardTimer / 4.0f, 1.0f);
        float alpha01 = (fadeT > 0.8f) ? (1.0f - fadeT) / 0.2f : (fadeT < 0.2f ? fadeT / 0.2f : 1.0f);
        Uint8 alpha = static_cast<Uint8>(255 * alpha01);

        // "CHAIN COMPLETE" title
        SDL_Color goldCol = {255, 215, 60, alpha};
        SDL_Surface* titleSurf = TTF_RenderText_Blended(font, LOC("hud.chain_complete"), goldCol);
        if (titleSurf) {
            SDL_Texture* titleTex = SDL_CreateTextureFromSurface(renderer, titleSurf);
            if (titleTex) {
                SDL_SetTextureAlphaMod(titleTex, alpha);
                int tx = (SCREEN_WIDTH - titleSurf->w) / 2;
                int ty = 200;
                SDL_Rect r = {tx, ty, titleSurf->w, titleSurf->h};
                SDL_RenderCopy(renderer, titleTex, nullptr, &r);
                SDL_DestroyTexture(titleTex);
            }
            SDL_FreeSurface(titleSurf);
        }

        // Chain name + reward
        char rewardText[64];
        snprintf(rewardText, sizeof(rewardText), LOC("hud.chain_reward"), m_eventChain.getName(), m_chainRewardShards);
        SDL_Color rewCol = {cc.r, cc.g, cc.b, alpha};
        SDL_Surface* rewSurf = TTF_RenderText_Blended(font, rewardText, rewCol);
        if (rewSurf) {
            SDL_Texture* rewTex = SDL_CreateTextureFromSurface(renderer, rewSurf);
            if (rewTex) {
                SDL_SetTextureAlphaMod(rewTex, alpha);
                int rx = (SCREEN_WIDTH - rewSurf->w) / 2;
                int ry = 250;
                SDL_Rect r = {rx, ry, rewSurf->w, rewSurf->h};
                SDL_RenderCopy(renderer, rewTex, nullptr, &r);
                SDL_DestroyTexture(rewTex);
            }
            SDL_FreeSurface(rewSurf);
        }
    }
}

// renderUnlockNotifications -> PlayStateRenderCombatUI.cpp

void PlayState::renderTutorialHints(SDL_Renderer* renderer, TTF_Font* font) {
    if (!font) return;

    // Only show tutorial hints on the first run; flag is set in startNewRun()
    if (!m_tutorialActive) return;

    // Context-based hint system: show hints when conditions are met
    const char* hint = nullptr;
    const char* keyLabel = nullptr;
    int hintSlot = -1; // which slot to mark done
    bool conditionMet = false;

    // === LEVEL 1: Basic controls (sequential) ===
    if (m_currentDifficulty == 1) {
        if (!m_tutorialHintDone[0]) {
            if (m_tutorialTimer > 1.5f && !m_hasMovedThisRun) {
                hint = LOC("tut.move");
                keyLabel = "WASD";
            }
            if (m_hasMovedThisRun) { conditionMet = true; hintSlot = 0; }
        } else if (!m_tutorialHintDone[1]) {
            hint = LOC("tut.jump");
            keyLabel = "SPACE";
            if (m_hasJumpedThisRun) { conditionMet = true; hintSlot = 1; }
        } else if (!m_tutorialHintDone[2]) {
            hint = LOC("tut.dash");
            keyLabel = "SHIFT";
            if (m_hasDashedThisRun) { conditionMet = true; hintSlot = 2; }
        } else if (!m_tutorialHintDone[3]) {
            bool enemyNear = false;
            if (m_player) {
                Vec2 pp = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
                m_entities.forEach([&](Entity& e) {
                    if (e.isEnemy &&
                        e.hasComponent<TransformComponent>()) {
                        Vec2 ep = e.getComponent<TransformComponent>().getCenter();
                        float d = std::sqrt((pp.x-ep.x)*(pp.x-ep.x) + (pp.y-ep.y)*(pp.y-ep.y));
                        if (d < 250.0f) enemyNear = true;
                    }
                });
            }
            if (enemyNear) {
                hint = LOC("tut.melee");
                keyLabel = "J";
            }
            if (m_hasAttackedThisRun) { conditionMet = true; hintSlot = 3; }
        } else if (!m_tutorialHintDone[4]) {
            hint = LOC("tut.ranged");
            keyLabel = "K";
            if (m_hasRangedThisRun) { conditionMet = true; hintSlot = 4; }
        } else if (!m_tutorialHintDone[5]) {
            hint = LOC("tut.dimension");
            keyLabel = "E";
            if (m_dimManager.getSwitchCount() > 0) { conditionMet = true; hintSlot = 5; }
        }
    }

    // === CONTEXT-BASED HINTS (any level, triggered by situation) ===
    if (!hint) {
        if (!m_tutorialHintDone[16] && m_nearRiftIndex >= 0 && m_level) {
            int requiredDim = m_level->getRiftRequiredDimension(m_nearRiftIndex);
            int currentDim = m_dimManager.getCurrentDimension();
            if (requiredDim > 0 && requiredDim != currentDim) {
                hint = (requiredDim == 2)
                    ? LOC("tut.rift_dimb")
                    : LOC("tut.rift_dima");
                keyLabel = "E";
                hintSlot = 16;
            }
        }
        // Near a rift: explain how to repair
        if (!hint && !m_tutorialHintDone[6] && m_nearRiftIndex >= 0) {
            hint = LOC("tut.rift_repair");
            keyLabel = "F";
            if (riftsRepaired > 0) { conditionMet = true; hintSlot = 6; }
        }
        // Entropy getting high
        else if (!m_tutorialHintDone[7] && !m_shownEntropyWarning && m_entropy.getPercent() > 0.5f) {
            hint = LOC("tut.entropy_warning");
            keyLabel = nullptr;
            m_shownEntropyWarning = true;
            m_tutorialHintShowTimer = 0;
            hintSlot = 7;
            // Auto-dismiss after 6s
            if (m_tutorialHintShowTimer > 6.0f) conditionMet = true;
        }
        // On/near a conveyor
        else if (!m_tutorialHintDone[8] && !m_shownConveyorHint && m_player) {
            auto& pt = m_player->getEntity()->getComponent<TransformComponent>();
            int ts = m_level ? m_level->getTileSize() : 32;
            int fx = static_cast<int>(pt.getCenter().x) / ts;
            int fy = static_cast<int>(pt.position.y + pt.height) / ts;
            int dim = m_dimManager.getCurrentDimension();
            int cdir = 0;
            if (m_level && (m_level->isOnConveyor(fx, fy, dim, cdir) || m_level->isOnConveyor(fx, fy+1, dim, cdir))) {
                hint = LOC("tut.conveyor");
                m_shownConveyorHint = true;
                hintSlot = 8;
                m_tutorialHintShowTimer = 0;
            }
        }
        // Near a dimension-exclusive platform
        else if (!m_tutorialHintDone[9] && !m_shownDimPlatformHint && m_player && m_level) {
            auto& pt = m_player->getEntity()->getComponent<TransformComponent>();
            int ts = m_level->getTileSize();
            int px = static_cast<int>(pt.getCenter().x) / ts;
            int py = static_cast<int>(pt.getCenter().y) / ts;
            int dim = m_dimManager.getCurrentDimension();
            // Check nearby tiles for dim-exclusive platforms
            for (int dy = -2; dy <= 2 && !hint; dy++) {
                for (int dx = -3; dx <= 3 && !hint; dx++) {
                    if (m_level->isDimensionExclusive(px+dx, py+dy, dim)) {
                        hint = LOC("tut.dim_platform");
                        m_shownDimPlatformHint = true;
                        hintSlot = 9;
                        m_tutorialHintShowTimer = 0;
                    }
                }
            }
        }
        // Show wall slide hint if next to wall in air
        else if (!m_tutorialHintDone[10] && !m_shownWallSlideHint && m_player) {
            auto& phys = m_player->getEntity()->getComponent<PhysicsBody>();
            if ((phys.onWallLeft || phys.onWallRight) && !phys.onGround) {
                hint = LOC("tut.wall_slide");
                keyLabel = "SPACE";
                m_shownWallSlideHint = true;
                hintSlot = 10;
                m_tutorialHintShowTimer = 0;
            }
        }
        // Show abilities hint (level 1, after combat basics done)
        else if (!m_tutorialHintDone[11] && m_hasAttackedThisRun && !m_hasUsedAbilityThisRun
                 && m_tutorialTimer > 30.0f) {
            hint = LOC("tut.abilities");
            keyLabel = "1 2 3";
            if (m_hasUsedAbilityThisRun) { conditionMet = true; hintSlot = 11; }
        }
        // Relic choice hint
        else if (!m_tutorialHintDone[12] && !m_shownRelicHint && m_showRelicChoice) {
            hint = LOC("tut.relic_choice");
            m_shownRelicHint = true;
            hintSlot = 12;
            m_tutorialHintShowTimer = 0;
        }
        // Combo hint when first combo reaches 3
        else if (!m_tutorialHintDone[13] && m_player &&
                 m_player->getEntity()->hasComponent<CombatComponent>()) {
            auto& cb = m_player->getEntity()->getComponent<CombatComponent>();
            if (cb.comboCount >= 3) {
                hint = LOC("tut.combo");
                conditionMet = true;
                hintSlot = 13;
            }
        }
        // Exit hint when all rifts repaired
        else if (!m_tutorialHintDone[14] && m_levelComplete && !m_collapsing) {
            hint = LOC("tut.exit");
            conditionMet = false;
            hintSlot = 14;
            m_tutorialHintShowTimer = 0;
        }
        // Weapon switch hint after a few levels
        else if (!m_tutorialHintDone[15] && m_currentDifficulty >= 2 && m_tutorialTimer > 10.0f
                 && m_tutorialHintDone[3]) {
            hint = LOC("tut.weapon_switch");
            hintSlot = 15;
            m_tutorialHintShowTimer = 0;
        }
        // Collapse started hint (escape phase)
        else if (!m_tutorialHintDone[17] && m_collapsing && m_collapseTimer < 3.0f) {
            hint = LOC("tut.collapsing");
            hintSlot = 17;
            m_tutorialHintShowTimer = 0;
        }
    }

    // Auto-dismiss timed hints after showing
    if (hint && hintSlot >= 7 && !conditionMet) {
        if (m_tutorialHintShowTimer > 5.0f) {
            conditionMet = true;
        }
    }

    // Mark completed hints
    if (conditionMet && hintSlot >= 0 && hintSlot < 20) {
        m_tutorialHintDone[hintSlot] = true;
        m_tutorialHintShowTimer = 0;
    }

    if (!hint) return;

    // Fade in over 0.3s
    m_tutorialHintShowTimer += 1.0f / 60.0f;
    float alpha = std::min(1.0f, m_tutorialHintShowTimer / 0.3f);
    Uint8 a = static_cast<Uint8>(alpha * 200);

    // Semi-transparent background
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, static_cast<Uint8>(a * 0.4f));
    SDL_Rect bg = {480, 280, 1600, 72};
    SDL_RenderFillRect(renderer, &bg);

    if (keyLabel) {
        SDL_Surface* hintSurf = TTF_RenderText_Blended(font, hint, {180, 220, 255, a});
        SDL_Surface* keySurf = TTF_RenderText_Blended(font, keyLabel, {255, 255, 255, 255});
        int totalW = (keySurf ? keySurf->w + 16 : 0) + (hintSurf ? hintSurf->w : 0);
        int startX = SCREEN_WIDTH / 2 - totalW / 2;

        if (keySurf) {
            renderKeyBox(renderer, font, keyLabel, startX, 296, a);
            startX += keySurf->w + 16;
            SDL_FreeSurface(keySurf);
        }
        if (hintSurf) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, hintSurf);
            if (tex) {
                SDL_SetTextureAlphaMod(tex, a);
                SDL_Rect dst = {startX, 296, hintSurf->w, hintSurf->h};
                SDL_RenderCopy(renderer, tex, nullptr, &dst);
                SDL_DestroyTexture(tex);
            }
            SDL_FreeSurface(hintSurf);
        }
    } else {
        SDL_Color color = {180, 220, 255, a};
        SDL_Surface* surface = TTF_RenderText_Blended(font, hint, color);
        if (surface) {
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
            if (texture) {
                SDL_SetTextureAlphaMod(texture, a);
                SDL_Rect dst = {SCREEN_WIDTH / 2 - surface->w / 2, 148, surface->w, surface->h};
                SDL_RenderCopy(renderer, texture, nullptr, &dst);
                SDL_DestroyTexture(texture);
            }
            SDL_FreeSurface(surface);
        }
    }
}

void PlayState::renderKeyBox(SDL_Renderer* renderer, TTF_Font* font,
                              const char* key, int x, int y, Uint8 alpha) {
    // Render a key label as a bordered box
    SDL_Surface* surface = TTF_RenderText_Blended(font, key, {255, 255, 255, alpha});
    if (!surface) return;
    int pad = 8;
    SDL_Rect box = {x - pad, y - 4, surface->w + pad * 2, surface->h + 8};
    SDL_SetRenderDrawColor(renderer, 40, 50, 80, static_cast<Uint8>(alpha * 0.8f));
    SDL_RenderFillRect(renderer, &box);
    SDL_SetRenderDrawColor(renderer, 120, 160, 220, alpha);
    SDL_RenderDrawRect(renderer, &box);
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (texture) {
        SDL_SetTextureAlphaMod(texture, alpha);
        SDL_Rect dst = {x, y, surface->w, surface->h};
        SDL_RenderCopy(renderer, texture, nullptr, &dst);
        SDL_DestroyTexture(texture);
    }
    SDL_FreeSurface(surface);
}

void PlayState::renderZoneTransition(SDL_Renderer* renderer, TTF_Font* font) {
    if (!m_zoneTransitionActive || !font) return;

    float t = m_zoneTransitionTimer;
    // Timing: fade in 0–0.5s, hold 0.5–2.5s, fade out 2.5–3.0s
    float alpha;
    if (t < 0.5f) alpha = t / 0.5f;               // fade in
    else if (t < 2.5f) alpha = 1.0f;               // hold
    else alpha = 1.0f - (t - 2.5f) / 0.5f;         // fade out
    alpha = std::clamp(alpha, 0.0f, 1.0f);
    if (alpha <= 0.0f) return;

    Uint8 a = static_cast<Uint8>(alpha * 255);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // Semi-transparent dark bar across upper third of screen
    int barH = SCREEN_HEIGHT / 3;
    int barY = SCREEN_HEIGHT / 6; // Centered in upper half
    SDL_SetRenderDrawColor(renderer, 5, 3, 15, static_cast<Uint8>(alpha * 180));
    SDL_Rect bar = {0, barY, SCREEN_WIDTH, barH};
    SDL_RenderFillRect(renderer, &bar);

    // Subtle purple border lines (top and bottom of bar)
    SDL_SetRenderDrawColor(renderer, 120, 60, 200, static_cast<Uint8>(alpha * 120));
    SDL_RenderDrawLine(renderer, SCREEN_WIDTH / 4, barY, SCREEN_WIDTH * 3 / 4, barY);
    SDL_RenderDrawLine(renderer, SCREEN_WIDTH / 4, barY + barH, SCREEN_WIDTH * 3 / 4, barY + barH);

    int centerX = SCREEN_WIDTH / 2;
    int centerY = barY + barH / 2;

    // "ZONE X" label (small, gold)
    {
        char zoneLbl[16];
        snprintf(zoneLbl, sizeof(zoneLbl), LOC("hud.zone"), m_zoneTransitionNumber);
        SDL_Color gold = {255, 200, 50, a};
        SDL_Surface* s = TTF_RenderText_Blended(font, zoneLbl, gold);
        if (s) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, s);
            if (tex) {
                SDL_SetTextureAlphaMod(tex, a);
                SDL_Rect r = {centerX - s->w / 2, centerY - 76, s->w, s->h};
                SDL_RenderCopy(renderer, tex, nullptr, &r);
                SDL_DestroyTexture(tex);
            }
            SDL_FreeSurface(s);
        }
    }

    // Zone name (large, white, with subtle scale-up via wider dest rect)
    {
        SDL_Color white = {255, 255, 255, a};
        SDL_Surface* s = TTF_RenderText_Blended(font, m_zoneTransitionName.c_str(), white);
        if (s) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, s);
            if (tex) {
                SDL_SetTextureAlphaMod(tex, a);
                // Scale up 2x for large title appearance
                float scale = 2.0f;
                int w = static_cast<int>(s->w * scale);
                int h = static_cast<int>(s->h * scale);
                SDL_Rect r = {centerX - w / 2, centerY - h / 2 - 4, w, h};
                SDL_RenderCopy(renderer, tex, nullptr, &r);
                SDL_DestroyTexture(tex);
            }
            SDL_FreeSurface(s);
        }
    }

    // Tagline (smaller, dim purple)
    {
        SDL_Color purple = {160, 120, 220, static_cast<Uint8>(a * 0.8f)};
        SDL_Surface* s = TTF_RenderText_Blended(font, m_zoneTransitionTagline.c_str(), purple);
        if (s) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, s);
            if (tex) {
                SDL_SetTextureAlphaMod(tex, static_cast<Uint8>(a * 0.8f));
                SDL_Rect r = {centerX - s->w / 2, centerY + 22, s->w, s->h};
                SDL_RenderCopy(renderer, tex, nullptr, &r);
                SDL_DestroyTexture(tex);
            }
            SDL_FreeSurface(s);
        }
    }

    // Decorative horizontal line below tagline
    {
        float lineAlpha = alpha * 0.6f;
        int lineW = static_cast<int>(360 * alpha);
        SDL_SetRenderDrawColor(renderer, 160, 120, 220, static_cast<Uint8>(lineAlpha * 255));
        SDL_Rect line = {centerX - lineW / 2, centerY + 88, lineW, 2};
        SDL_RenderFillRect(renderer, &line);
    }
}

void PlayState::renderRunIntro(SDL_Renderer* renderer, TTF_Font* font) {
    if (!font) return;

    float t = m_runIntroTimer;
    float duration = kRunIntroDuration;
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // Full dark overlay with subtle red/purple tint — fades out in the last 0.5s
    float overlayFade = (t > duration - 0.5f) ? (duration - t) / 0.5f : 1.0f;
    Uint8 bgAlpha = static_cast<Uint8>(overlayFade * 230);
    SDL_SetRenderDrawColor(renderer, 12, 5, 18, bgAlpha); // Dark purple-black
    SDL_Rect full = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
    SDL_RenderFillRect(renderer, &full);

    // Subtle vignette border (deep crimson/purple)
    {
        Uint8 vigA = static_cast<Uint8>(overlayFade * 80);
        SDL_SetRenderDrawColor(renderer, 60, 10, 30, vigA);
        int bw = 80;
        SDL_Rect top = {0, 0, SCREEN_WIDTH, bw};
        SDL_Rect bot = {0, SCREEN_HEIGHT - bw, SCREEN_WIDTH, bw};
        SDL_Rect lft = {0, 0, bw, SCREEN_HEIGHT};
        SDL_Rect rgt = {SCREEN_WIDTH - bw, 0, bw, SCREEN_HEIGHT};
        SDL_RenderFillRect(renderer, &top);
        SDL_RenderFillRect(renderer, &bot);
        SDL_RenderFillRect(renderer, &lft);
        SDL_RenderFillRect(renderer, &rgt);
    }

    int centerX = SCREEN_WIDTH / 2;
    int centerY = SCREEN_HEIGHT / 2;

    bool isNGPlus = (m_ngPlusTier > 0);

    // Line 1: 0.0 - 3.5s (fade in 0-1.0s, hold, fade out in last 0.5s)
    const char* line1 = isNGPlus ? LOC("intro.line1_ng") : LOC("intro.line1");
    if (t > 0.0f) {
        float fadeIn = std::min(1.0f, t / 1.0f);
        float fadeOut = (t > duration - 0.5f) ? (duration - t) / 0.5f : 1.0f;
        float alpha = fadeIn * fadeOut;
        Uint8 a = static_cast<Uint8>(alpha * 255);
        // Dim white with slight purple tint
        SDL_Color color = {180, 170, 200, a};
        SDL_Surface* surf = TTF_RenderText_Blended(font, line1, color);
        if (surf) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
            if (tex) {
                SDL_SetTextureAlphaMod(tex, a);
                SDL_Rect dst = {centerX - surf->w / 2, centerY - 80, surf->w, surf->h};
                SDL_RenderCopy(renderer, tex, nullptr, &dst);
                SDL_DestroyTexture(tex);
            }
            SDL_FreeSurface(surf);
        }
    }

    // Line 2: 1.0 - 3.0s (fade in 1.0-1.5s, hold, fade out 2.5-3.0s)
    if (t > 1.0f) {
        char line2[128];
        if (isNGPlus) {
            snprintf(line2, sizeof(line2), LOC("intro.line2_ng"), m_ngPlusTier);
        } else {
            snprintf(line2, sizeof(line2), "%s", LOC("intro.line2"));
        }
        float fadeIn = std::min(1.0f, (t - 1.0f) / 0.5f);
        float fadeOut = (t > 2.5f) ? std::max(0.0f, (3.0f - t) / 0.5f) : 1.0f;
        float alpha = fadeIn * fadeOut;
        Uint8 a = static_cast<Uint8>(alpha * 220);
        SDL_Color color = {140, 130, 160, a};
        SDL_Surface* surf = TTF_RenderText_Blended(font, line2, color);
        if (surf) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
            if (tex) {
                SDL_SetTextureAlphaMod(tex, a);
                SDL_Rect dst = {centerX - surf->w / 2, centerY - 10, surf->w, surf->h};
                SDL_RenderCopy(renderer, tex, nullptr, &dst);
                SDL_DestroyTexture(tex);
            }
            SDL_FreeSurface(surf);
        }
    }

    // Line 3: 2.5 - 3.5s (fade in 2.5-2.8s, hold, then fade to gameplay)
    if (t > 2.5f) {
        const char* line3 = isNGPlus ? "Begin." : "Survive.";
        float fadeIn = std::min(1.0f, (t - 2.5f) / 0.3f);
        float fadeOut = (t > duration - 0.3f) ? (duration - t) / 0.3f : 1.0f;
        float alpha = fadeIn * fadeOut;
        Uint8 a = static_cast<Uint8>(alpha * 255);
        // Brighter, bolder — slight red warmth
        SDL_Color color = {220, 200, 210, a};
        SDL_Surface* surf = TTF_RenderText_Blended(font, line3, color);
        if (surf) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
            if (tex) {
                SDL_SetTextureAlphaMod(tex, a);
                // Slightly larger via scaled rect
                float scale = 1.5f;
                int w = static_cast<int>(surf->w * scale);
                int h = static_cast<int>(surf->h * scale);
                SDL_Rect dst = {centerX - w / 2, centerY + 70, w, h};
                SDL_RenderCopy(renderer, tex, nullptr, &dst);
                SDL_DestroyTexture(tex);
            }
            SDL_FreeSurface(surf);
        }
    }

    // Atmospheric decorative line (thin, fading purple)
    if (t > 0.8f && t < duration - 0.3f) {
        float lineAlpha = std::min(1.0f, (t - 0.8f) / 0.5f);
        if (t > duration - 0.8f) lineAlpha *= (duration - 0.3f - t) / 0.5f;
        int lineW = static_cast<int>(400 * lineAlpha);
        Uint8 la = static_cast<Uint8>(lineAlpha * 120);
        SDL_SetRenderDrawColor(renderer, 120, 60, 160, la);
        SDL_Rect line = {centerX - lineW / 2, centerY + 140, lineW, 2};
        SDL_RenderFillRect(renderer, &line);
    }
}


// renderKillStreak, renderLevelUp, updateDamageIndicators, renderDamageIndicators,
// renderOffscreenEnemyIndicators -> PlayStateRenderCombatUI.cpp
