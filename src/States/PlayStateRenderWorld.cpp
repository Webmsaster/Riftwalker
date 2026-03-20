// PlayStateRenderWorld.cpp -- Split from PlayStateRender.cpp (world entities, NPCs, events)
#include "PlayState.h"
#include "Core/Game.h"
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

void PlayState::renderLevelCompleteTransition(SDL_Renderer* renderer) {
    if (!m_levelComplete) return;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    float progress = m_levelCompleteTimer / 2.0f; // 0 to 1 over 2 seconds
    if (progress > 1.0f) progress = 1.0f;
    Uint32 ticks = SDL_GetTicks();

    // Growing dark overlay
    Uint8 overlayA = static_cast<Uint8>(progress * 180);
    SDL_SetRenderDrawColor(renderer, 10, 5, 20, overlayA);
    SDL_Rect full = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
    SDL_RenderFillRect(renderer, &full);

    // Expanding rift circle from center
    int maxRadius = 400;
    int radius = static_cast<int>(progress * maxRadius);
    float pulse = 0.7f + 0.3f * std::sin(ticks * 0.01f);
    Uint8 riftA = static_cast<Uint8>((1.0f - progress * 0.5f) * 120 * pulse);

    // Rift ring (multiple concentric rings)
    for (int r = radius - 6; r <= radius + 6; r += 3) {
        if (r < 0) continue;
        for (int angle = 0; angle < 60; angle++) {
            float a = angle * 6.283185f / 60.0f + ticks * 0.002f;
            int px = 640 + static_cast<int>(std::cos(a) * r);
            int py = 360 + static_cast<int>(std::sin(a) * r);
            if (px < 0 || px >= SCREEN_WIDTH || py < 0 || py >= SCREEN_HEIGHT) continue;
            SDL_SetRenderDrawColor(renderer, 180, 100, 255, riftA);
            SDL_Rect dot = {px - 1, py - 1, 3, 3};
            SDL_RenderFillRect(renderer, &dot);
        }
    }

    // Scanlines converging toward center
    if (progress > 0.3f) {
        int lineCount = static_cast<int>((progress - 0.3f) * 20);
        for (int i = 0; i < lineCount; i++) {
            int y = ((i * 4513 + ticks / 40) % SCREEN_HEIGHT);
            Uint8 la = static_cast<Uint8>(progress * 60);
            SDL_SetRenderDrawColor(renderer, 150, 80, 220, la);
            SDL_Rect line = {0, y, SCREEN_WIDTH, 1};
            SDL_RenderFillRect(renderer, &line);
        }
    }

    // Text banner
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, static_cast<Uint8>(180 * std::min(1.0f, progress * 3.0f)));
    SDL_Rect banner = {0, 330, SCREEN_WIDTH, 60};
    SDL_RenderFillRect(renderer, &banner);

    TTF_Font* font = game->getFont();
    if (font) {
        float textAlpha = std::min(1.0f, progress * 3.0f);
        Uint8 ta = static_cast<Uint8>(textAlpha * 255);
        SDL_Color c = {140, 255, 180, ta};
        SDL_Surface* s = TTF_RenderText_Blended(font, "RIFT STABILIZED - Warping to next dimension...", c);
        if (s) {
            SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
            if (t) {
                SDL_Rect r = {640 - s->w / 2, 348, s->w, s->h};
                SDL_RenderCopy(renderer, t, nullptr, &r);
                SDL_DestroyTexture(t);
            }
            SDL_FreeSurface(s);
        }
    }
}

void PlayState::renderRandomEvents(SDL_Renderer* renderer, TTF_Font* font) {
    if (!m_level) return;

    Uint32 ticks = SDL_GetTicks();
    int currentDim = m_dimManager.getCurrentDimension();
    auto& events = m_level->getRandomEvents();

    for (int i = 0; i < static_cast<int>(events.size()); i++) {
        auto& event = events[i];
        if (!event.active || event.used) continue;
        if (event.dimension != 0 && event.dimension != currentDim) continue;

        SDL_FRect worldRect = {event.position.x - 16, event.position.y - 32, 32, 32};
        SDL_Rect sr = m_camera.worldToScreen(worldRect);

        // Skip if off screen
        if (sr.x + sr.w < 0 || sr.x > SCREEN_WIDTH || sr.y + sr.h < 0 || sr.y > SCREEN_HEIGHT) continue;

        float bob = std::sin(ticks * 0.003f + i * 2.0f) * 3.0f;
        sr.y -= static_cast<int>(bob);

        // NPC body
        SDL_Color npcColor;
        switch (event.type) {
            case RandomEventType::Merchant:           npcColor = {80, 200, 80, 255}; break;
            case RandomEventType::Shrine:             npcColor = event.getShrineColor(); break;
            case RandomEventType::DimensionalAnomaly: npcColor = {200, 50, 200, 255}; break;
            case RandomEventType::RiftEcho:           npcColor = {150, 150, 255, 255}; break;
            case RandomEventType::SuitRepairStation:  npcColor = {100, 255, 150, 255}; break;
            case RandomEventType::GamblingRift:       npcColor = {255, 200, 50, 255}; break;
        }

        // Glow
        float glow = 0.5f + 0.5f * std::sin(ticks * 0.004f + i);
        Uint8 ga = static_cast<Uint8>(40 * glow);
        SDL_SetRenderDrawColor(renderer, npcColor.r, npcColor.g, npcColor.b, ga);
        SDL_Rect glowRect = {sr.x - 6, sr.y - 6, sr.w + 12, sr.h + 12};
        SDL_RenderFillRect(renderer, &glowRect);

        // Body
        SDL_SetRenderDrawColor(renderer, npcColor.r, npcColor.g, npcColor.b, 220);
        SDL_RenderFillRect(renderer, &sr);

        // Eye
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 200);
        SDL_Rect eye = {sr.x + sr.w / 2 - 2, sr.y + 8, 4, 4};
        SDL_RenderFillRect(renderer, &eye);

        // Icon on top based on type
        int iconY = sr.y - 6;
        int iconX = sr.x + sr.w / 2;
        SDL_SetRenderDrawColor(renderer, npcColor.r, npcColor.g, npcColor.b, 180);
        switch (event.type) {
            case RandomEventType::Merchant:
                // Coin
                SDL_RenderDrawLine(renderer, iconX - 4, iconY, iconX + 4, iconY);
                SDL_RenderDrawLine(renderer, iconX, iconY - 4, iconX, iconY + 4);
                break;
            case RandomEventType::Shrine:
                // Diamond
                SDL_RenderDrawLine(renderer, iconX, iconY - 5, iconX + 5, iconY);
                SDL_RenderDrawLine(renderer, iconX + 5, iconY, iconX, iconY + 5);
                SDL_RenderDrawLine(renderer, iconX, iconY + 5, iconX - 5, iconY);
                SDL_RenderDrawLine(renderer, iconX - 5, iconY, iconX, iconY - 5);
                break;
            case RandomEventType::DimensionalAnomaly: {
                // Exclamation
                SDL_RenderDrawLine(renderer, iconX, iconY - 5, iconX, iconY + 1);
                SDL_Rect dot = {iconX - 1, iconY + 3, 2, 2};
                SDL_RenderFillRect(renderer, &dot);
                break;
            }
            default:
                // Star
                SDL_RenderDrawLine(renderer, iconX, iconY - 5, iconX, iconY + 5);
                SDL_RenderDrawLine(renderer, iconX - 5, iconY, iconX + 5, iconY);
                break;
        }

        // Interaction hint when near
        if (i == m_nearEventIndex && font) {
            float blink = 0.5f + 0.5f * std::sin(ticks * 0.008f);
            SDL_Color hc = {255, 255, 255, static_cast<Uint8>(200 * blink)};
            char hint[64];
            std::snprintf(hint, sizeof(hint), "[F] %s", event.getName());
            SDL_Surface* hs = TTF_RenderText_Blended(font, hint, hc);
            int nameH = 0;
            if (hs) {
                nameH = hs->h;
                SDL_Texture* ht = SDL_CreateTextureFromSurface(renderer, hs);
                if (ht) {
                    int offy = (event.type == RandomEventType::Shrine) ? -36 : -24;
                    SDL_Rect hr = {sr.x + sr.w / 2 - hs->w / 2, sr.y + offy, hs->w, hs->h};
                    SDL_RenderCopy(renderer, ht, nullptr, &hr);
                    SDL_DestroyTexture(ht);
                }
                SDL_FreeSurface(hs);
            }

            // Shrine: show effect preview below the name
            if (event.type == RandomEventType::Shrine) {
                SDL_Color ec = event.getShrineColor();
                ec.a = static_cast<Uint8>(180 * blink);
                SDL_Surface* es = TTF_RenderText_Blended(font, event.getShrineEffect(), ec);
                if (es) {
                    SDL_Texture* et = SDL_CreateTextureFromSurface(renderer, es);
                    if (et) {
                        SDL_Rect er = {sr.x + sr.w / 2 - es->w / 2, sr.y - 36 + nameH + 2, es->w, es->h};
                        SDL_RenderCopy(renderer, et, nullptr, &er);
                        SDL_DestroyTexture(et);
                    }
                    SDL_FreeSurface(es);
                }
            }
        }
    }
}

void PlayState::renderNPCs(SDL_Renderer* renderer, TTF_Font* font) {
    if (!m_level) return;

    Uint32 ticks = SDL_GetTicks();
    int currentDim = m_dimManager.getCurrentDimension();
    auto& npcs = m_level->getNPCs();

    for (int i = 0; i < static_cast<int>(npcs.size()); i++) {
        auto& npc = npcs[i];
        if (npc.interacted) continue;
        if (npc.dimension != 0 && npc.dimension != currentDim) continue;

        SDL_FRect worldRect = {npc.position.x - 12, npc.position.y - 32, 24, 32};
        SDL_Rect sr = m_camera.worldToScreen(worldRect);

        if (sr.x + sr.w < 0 || sr.x > SCREEN_WIDTH || sr.y + sr.h < 0 || sr.y > SCREEN_HEIGHT) continue;

        float bob = std::sin(ticks * 0.003f + i * 3.0f) * 3.0f;
        sr.y -= static_cast<int>(bob);

        // NPC color by type
        SDL_Color col;
        switch (npc.type) {
            case NPCType::RiftScholar:  col = {100, 180, 255, 255}; break; // Blue
            case NPCType::DimRefugee:   col = {255, 180, 80, 255}; break;  // Orange
            case NPCType::LostEngineer: col = {180, 255, 120, 255}; break; // Green
            case NPCType::EchoOfSelf:   col = {220, 120, 255, 255}; break; // Purple
            case NPCType::Blacksmith:   col = {255, 160, 50, 255}; break;  // Forge orange
            default:                    col = {200, 200, 200, 255}; break;
        }

        // Glow aura
        float glow = 0.5f + 0.5f * std::sin(ticks * 0.004f + i * 2.0f);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, static_cast<Uint8>(35 * glow));
        SDL_Rect glowR = {sr.x - 8, sr.y - 8, sr.w + 16, sr.h + 16};
        SDL_RenderFillRect(renderer, &glowR);

        // Body (hooded figure shape)
        SDL_SetRenderDrawColor(renderer, col.r / 2, col.g / 2, col.b / 2, 220);
        SDL_RenderFillRect(renderer, &sr); // Robe

        // Head (smaller rect on top)
        SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, 240);
        SDL_Rect head = {sr.x + sr.w / 4, sr.y + 2, sr.w / 2, sr.h / 3};
        SDL_RenderFillRect(renderer, &head);

        // Eyes
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 220);
        int eyeY = sr.y + sr.h / 5;
        SDL_Rect leftEye = {sr.x + sr.w / 3 - 2, eyeY, 3, 3};
        SDL_Rect rightEye = {sr.x + sr.w * 2 / 3 - 1, eyeY, 3, 3};
        SDL_RenderFillRect(renderer, &leftEye);
        SDL_RenderFillRect(renderer, &rightEye);

        // Type-specific detail
        switch (npc.type) {
            case NPCType::RiftScholar: {
                SDL_SetRenderDrawColor(renderer, 100, 180, 255, 160);
                SDL_Rect book = {sr.x + sr.w / 2 - 4, sr.y - 12, 8, 6};
                SDL_RenderFillRect(renderer, &book);
                SDL_RenderDrawLine(renderer, sr.x + sr.w / 2, sr.y - 12, sr.x + sr.w / 2, sr.y - 6);
                break;
            }
            case NPCType::DimRefugee: {
                SDL_SetRenderDrawColor(renderer, 255, 200, 60, 180);
                SDL_RenderDrawLine(renderer, sr.x + sr.w / 2, sr.y - 14, sr.x + sr.w / 2 + 5, sr.y - 8);
                SDL_RenderDrawLine(renderer, sr.x + sr.w / 2 + 5, sr.y - 8, sr.x + sr.w / 2, sr.y - 2);
                SDL_RenderDrawLine(renderer, sr.x + sr.w / 2, sr.y - 2, sr.x + sr.w / 2 - 5, sr.y - 8);
                SDL_RenderDrawLine(renderer, sr.x + sr.w / 2 - 5, sr.y - 8, sr.x + sr.w / 2, sr.y - 14);
                break;
            }
            case NPCType::LostEngineer: {
                SDL_SetRenderDrawColor(renderer, 180, 255, 120, 180);
                SDL_RenderDrawLine(renderer, sr.x + sr.w / 2 - 4, sr.y - 12, sr.x + sr.w / 2 + 4, sr.y - 4);
                break;
            }
            case NPCType::EchoOfSelf: {
                SDL_SetRenderDrawColor(renderer, 220, 120, 255, static_cast<Uint8>(100 * glow));
                SDL_Rect mirrorR = {sr.x - 2, sr.y, sr.w + 4, sr.h};
                SDL_RenderDrawRect(renderer, &mirrorR);
                break;
            }
            case NPCType::Blacksmith: {
                // Anvil shape above head
                SDL_SetRenderDrawColor(renderer, 255, 160, 50, 180);
                SDL_Rect anvil = {sr.x + sr.w / 2 - 6, sr.y - 8, 12, 4};
                SDL_RenderFillRect(renderer, &anvil);
                SDL_Rect base = {sr.x + sr.w / 2 - 3, sr.y - 4, 6, 4};
                SDL_RenderFillRect(renderer, &base);
                // Spark particles
                float sparkPhase = std::sin(ticks * 0.008f + i) * 0.5f + 0.5f;
                if (sparkPhase > 0.7f) {
                    SDL_SetRenderDrawColor(renderer, 255, 220, 80, 200);
                    SDL_Rect spark = {sr.x + sr.w / 2 + static_cast<int>(sparkPhase * 8) - 4, sr.y - 14, 2, 2};
                    SDL_RenderFillRect(renderer, &spark);
                }
                break;
            }
            default: break;
        }

        // Interaction hint
        if (i == m_nearNPCIndex && font) {
            float blink = 0.5f + 0.5f * std::sin(ticks * 0.008f);
            SDL_Color hc = {255, 255, 255, static_cast<Uint8>(200 * blink)};
            char hint[64];
            std::snprintf(hint, sizeof(hint), "[F] %s", npc.name);
            SDL_Surface* hs = TTF_RenderText_Blended(font, hint, hc);
            if (hs) {
                SDL_Texture* ht = SDL_CreateTextureFromSurface(renderer, hs);
                if (ht) {
                    SDL_Rect hr = {sr.x + sr.w / 2 - hs->w / 2, sr.y - 28, hs->w, hs->h};
                    SDL_RenderCopy(renderer, ht, nullptr, &hr);
                    SDL_DestroyTexture(ht);
                }
                SDL_FreeSurface(hs);
            }
        }
    }
}

void PlayState::renderNPCDialog(SDL_Renderer* renderer, TTF_Font* font) {
    if (!font || m_nearNPCIndex < 0 || !m_level) return;

    auto& npcs = m_level->getNPCs();
    if (m_nearNPCIndex >= static_cast<int>(npcs.size())) return;
    auto& npc = npcs[m_nearNPCIndex];

    // Semi-transparent overlay
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 150);
    SDL_Rect full = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
    SDL_RenderFillRect(renderer, &full);

    // Dialog box (taller for returning NPCs with longer story text)
    int stage = getNPCStoryStage(npc.type);
    int boxW = 500, boxH = 260 + (stage > 0 ? 30 : 0);
    int boxX = 640 - boxW / 2, boxY = 360 - boxH / 2;

    SDL_SetRenderDrawColor(renderer, 20, 15, 35, 240);
    SDL_Rect box = {boxX, boxY, boxW, boxH};
    SDL_RenderFillRect(renderer, &box);

    // Border
    SDL_SetRenderDrawColor(renderer, 120, 80, 200, 200);
    SDL_RenderDrawRect(renderer, &box);

    // NPC name
    SDL_Color nameColor = {180, 140, 255, 255};
    SDL_Surface* ns = TTF_RenderText_Blended(font, npc.name, nameColor);
    if (ns) {
        SDL_Texture* nt = SDL_CreateTextureFromSurface(renderer, ns);
        if (nt) {
            SDL_Rect nr = {boxX + 20, boxY + 15, ns->w, ns->h};
            SDL_RenderCopy(renderer, nt, nullptr, &nr);
            SDL_DestroyTexture(nt);
        }
        SDL_FreeSurface(ns);
    }

    // Story stage indicator (show returning NPC badge)
    if (stage > 0) {
        const char* stageLabel = (stage >= 2) ? "[Old Friend]" : "[Returning]";
        SDL_Color stageColor = (stage >= 2) ? SDL_Color{255, 215, 80, 200} : SDL_Color{140, 200, 140, 180};
        SDL_Surface* stS = TTF_RenderText_Blended(font, stageLabel, stageColor);
        if (stS) {
            SDL_Texture* stT = SDL_CreateTextureFromSurface(renderer, stS);
            if (stT) {
                SDL_Rect stR = {boxX + boxW - stS->w - 20, boxY + 15, stS->w, stS->h};
                SDL_RenderCopy(renderer, stT, nullptr, &stR);
                SDL_DestroyTexture(stT);
            }
            SDL_FreeSurface(stS);
        }
    }

    // Greeting text (stage-based)
    const char* greeting = NPCSystem::getGreeting(npc.type, stage);
    SDL_Color textColor = {200, 200, 220, 255};
    SDL_Surface* ds = TTF_RenderText_Blended(font, greeting, textColor);
    if (ds) {
        SDL_Texture* dt = SDL_CreateTextureFromSurface(renderer, ds);
        if (dt) {
            SDL_Rect dr = {boxX + 20, boxY + 45, ds->w, ds->h};
            SDL_RenderCopy(renderer, dt, nullptr, &dr);
            SDL_DestroyTexture(dt);
        }
        SDL_FreeSurface(ds);
    }

    // Story line (stage-based, may contain newlines -- render line by line)
    const char* storyText = NPCSystem::getStoryLine(npc.type, stage);
    int lineY = boxY + 70;
    {
        // Split on newline and render each line
        std::string story(storyText);
        size_t pos = 0;
        while (pos < story.size()) {
            size_t nl = story.find('\n', pos);
            std::string line = (nl != std::string::npos) ? story.substr(pos, nl - pos) : story.substr(pos);
            pos = (nl != std::string::npos) ? nl + 1 : story.size();
            if (line.empty()) { lineY += 18; continue; }
            SDL_Surface* ls = TTF_RenderText_Blended(font, line.c_str(), SDL_Color{160, 160, 180, 200});
            if (ls) {
                SDL_Texture* lt = SDL_CreateTextureFromSurface(renderer, ls);
                if (lt) {
                    SDL_Rect lr = {boxX + 20, lineY, ls->w, ls->h};
                    SDL_RenderCopy(renderer, lt, nullptr, &lr);
                    SDL_DestroyTexture(lt);
                }
                SDL_FreeSurface(ls);
            }
            lineY += 18;
        }
    }

    // Dialog options (stage-based) -- positioned below story text
    auto options = NPCSystem::getDialogOptions(npc.type, stage);
    int optY = std::max(lineY + 8, boxY + 110);
    for (int i = 0; i < static_cast<int>(options.size()); i++) {
        bool selected = (i == m_npcDialogChoice);
        SDL_Color oc = selected ? SDL_Color{255, 220, 100, 255} : SDL_Color{180, 180, 200, 200};

        if (selected) {
            SDL_SetRenderDrawColor(renderer, 80, 60, 120, 100);
            SDL_Rect selR = {boxX + 15, optY - 2, boxW - 30, 22};
            SDL_RenderFillRect(renderer, &selR);

            // Arrow
            SDL_SetRenderDrawColor(renderer, 255, 220, 100, 255);
            SDL_RenderDrawLine(renderer, boxX + 20, optY + 8, boxX + 28, optY + 4);
            SDL_RenderDrawLine(renderer, boxX + 28, optY + 4, boxX + 20, optY);
        }

        SDL_Surface* os = TTF_RenderText_Blended(font, options[i], oc);
        if (os) {
            SDL_Texture* ot = SDL_CreateTextureFromSurface(renderer, os);
            if (ot) {
                SDL_Rect or_ = {boxX + 35, optY, os->w, os->h};
                SDL_RenderCopy(renderer, ot, nullptr, &or_);
                SDL_DestroyTexture(ot);
            }
            SDL_FreeSurface(os);
        }
        optY += 26;
    }

    // Controls hint
    SDL_Surface* hs = TTF_RenderText_Blended(font, "[W/S] Navigate  [Enter] Select  [Esc] Close",
                                              SDL_Color{120, 120, 140, 150});
    if (hs) {
        SDL_Texture* ht = SDL_CreateTextureFromSurface(renderer, hs);
        if (ht) {
            SDL_Rect hr = {boxX + boxW / 2 - hs->w / 2, boxY + boxH - 25, hs->w, hs->h};
            SDL_RenderCopy(renderer, ht, nullptr, &hr);
            SDL_DestroyTexture(ht);
        }
        SDL_FreeSurface(hs);
    }
}
