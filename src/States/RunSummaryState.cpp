#include "RunSummaryState.h"
#include "Core/Game.h"
#include "Core/Localization.h"
#include "Game/AchievementSystem.h"
#include "Game/ClassSystem.h"
#include "Game/UpgradeSystem.h"
#include "Game/WeaponSystem.h"
#include "Game/DailyRun.h"
#include "States/DailyLeaderboardState.h"
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <algorithm>

extern bool g_autoSmokeTest;
extern bool g_autoPlaytest;

// Forward declaration (defined below update())
static const char* getGrade(int rooms, int enemies, int rifts);

void RunSummaryState::enter() {
    // Automated modes: auto-skip run summary
    if (g_autoSmokeTest || g_autoPlaytest) {
        game->quit();
        return;
    }

    m_fadeIn = 0;
    m_time = 0;
    m_statsTimer = 0;
    m_gradeAchievementChecked = false;

    // Load today rank for the daily leaderboard display
    m_todayRank = 0;
    if (isDailyRun && dailyScore > 0) {
        DailyRun dr;
        dr.load("riftwalker_daily.dat");
        m_todayRank = dr.getTodayRank(dailyScore);
    }
}

void RunSummaryState::handleEvent(const SDL_Event& event) {
    if (m_fadeIn >= 1.0f) {
        if (event.type == SDL_KEYDOWN) {
            if (event.key.keysym.scancode == SDL_SCANCODE_RETURN ||
                event.key.keysym.scancode == SDL_SCANCODE_SPACE ||
                event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
                game->changeState(StateID::Menu);
            } else if (event.key.keysym.scancode == SDL_SCANCODE_L && isDailyRun) {
                game->pushState(StateID::DailyLeaderboard);
            }
        }
        if (event.type == SDL_CONTROLLERBUTTONDOWN) {
            auto btn = event.cbutton.button;
            if (btn == SDL_CONTROLLER_BUTTON_A || btn == SDL_CONTROLLER_BUTTON_B ||
                btn == SDL_CONTROLLER_BUTTON_START) {
                game->changeState(StateID::Menu);
            }
        }
    }
}

void RunSummaryState::update(float dt) {
    m_time += dt;
    if (m_fadeIn < 1.0f) {
        m_fadeIn += dt * 0.8f;
        if (m_fadeIn > 1.0f) m_fadeIn = 1.0f;
    }
    m_statsTimer += dt;

    // One-shot S-grade achievement unlock (was incorrectly in render())
    if (!m_gradeAchievementChecked) {
        m_gradeAchievementChecked = true;
        const char* grade = getGrade(roomsCleared, enemiesKilled, riftsRepaired);
        if (grade[0] == 'S') {
            game->getAchievements().unlock("grade_s");
        }
    }
}

static const char* getGrade(int rooms, int enemies, int rifts) {
    int score = rooms * 10 + enemies * 3 + rifts * 15;
    if (score >= 80) return "S";
    if (score >= 50) return "A";
    if (score >= 30) return "B";
    if (score >= 15) return "C";
    return "D";
}

static SDL_Color getGradeColor(const char* grade) {
    if (grade[0] == 'S') return {255, 220, 50, 255};
    if (grade[0] == 'A') return {100, 255, 100, 255};
    if (grade[0] == 'B') return {100, 180, 255, 255};
    if (grade[0] == 'C') return {200, 180, 140, 255};
    return {160, 120, 120, 255};
}

void RunSummaryState::render(SDL_Renderer* renderer) {
    SDL_SetRenderDrawColor(renderer, 8, 5, 15, 255);
    SDL_RenderClear(renderer);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // Background particles drifting down
    for (int i = 0; i < 40; i++) {
        float x = static_cast<float>((i * 131 + 47) % SCREEN_WIDTH);
        float baseY = static_cast<float>((i * 89 + 23) % SCREEN_HEIGHT);
        float y = std::fmod(baseY + m_time * (15.0f + (i % 5) * 5.0f), static_cast<float>(SCREEN_HEIGHT));
        float twinkle = 0.5f + 0.5f * std::sin(m_time * 2.0f + i * 1.3f);
        Uint8 a = static_cast<Uint8>(20 * twinkle);
        Uint8 r = (i % 3 == 0) ? static_cast<Uint8>(80) : static_cast<Uint8>(40);
        Uint8 b = (i % 3 != 0) ? static_cast<Uint8>(100) : static_cast<Uint8>(40);
        SDL_SetRenderDrawColor(renderer, r, 20, b, a);
        SDL_Rect p = {static_cast<int>(x), static_cast<int>(y), 2, 2};
        SDL_RenderFillRect(renderer, &p);
    }

    TTF_Font* font = game->getFont();
    if (!font) return;

    Uint8 alpha = static_cast<Uint8>(255 * m_fadeIn);

    // Title with glow
    SDL_Surface* ts = TTF_RenderText_Blended(font, LOC("summary.title"), {200, 80, 80, 255});
    if (ts) {
        SDL_Texture* tt = SDL_CreateTextureFromSurface(renderer, ts);
        if (tt) {
            int tw = ts->w * 2;
            int th = ts->h * 2;
            // Glow
            SDL_SetTextureBlendMode(tt, SDL_BLENDMODE_ADD);
            SDL_SetTextureAlphaMod(tt, static_cast<Uint8>(alpha * 0.15f));
            SDL_Rect glowR = {SCREEN_WIDTH / 2 - tw / 2 - 3, 67, tw + 6, th + 6};
            SDL_RenderCopy(renderer, tt, nullptr, &glowR);
            // Main
            SDL_SetTextureBlendMode(tt, SDL_BLENDMODE_BLEND);
            SDL_SetTextureAlphaMod(tt, alpha);
            SDL_Rect tr = {SCREEN_WIDTH / 2 - tw / 2, 70, tw, th};
            SDL_RenderCopy(renderer, tt, nullptr, &tr);
            SDL_DestroyTexture(tt);
        }
        SDL_FreeSurface(ts);
    }

    // Separator
    SDL_SetRenderDrawColor(renderer, 140, 60, 60, static_cast<Uint8>(alpha * 0.4f));
    SDL_RenderDrawLine(renderer, 350, 145, 930, 145);

    // Run info line (class, weapons, difficulty, time)
    {
        auto& cd = ClassSystem::getData(playerClass);
        int mins = static_cast<int>(runTime) / 60;
        int secs = static_cast<int>(runTime) % 60;
        char infoLine[128];
        std::snprintf(infoLine, sizeof(infoLine), "%s  |  %s / %s  |  Diff %d  |  %d:%02d",
                      cd.name,
                      WeaponSystem::getWeaponName(meleeWeapon),
                      WeaponSystem::getWeaponName(rangedWeapon),
                      difficulty, mins, secs);
        SDL_Color infoColor = {cd.color.r, cd.color.g, cd.color.b, alpha};
        SDL_Surface* is = TTF_RenderText_Blended(font, infoLine, infoColor);
        if (is) {
            SDL_Texture* it = SDL_CreateTextureFromSurface(renderer, is);
            if (it) {
                SDL_SetTextureAlphaMod(it, alpha);
                SDL_Rect ir = {SCREEN_WIDTH / 2 - is->w / 2, 152, is->w, is->h};
                SDL_RenderCopy(renderer, it, nullptr, &ir);
                SDL_DestroyTexture(it);
            }
            SDL_FreeSurface(is);
        }

        // NG+ tier badge (golden, shown below info line when tier > 0)
        if (ngPlusTier > 0) {
            static const char* s_ngpTitles[] = {
                "", "NG+1 CHALLENGER", "NG+2 VETERAN", "NG+3 HARDENED",
                "NG+4 VOID TOUCHED", "NG+5 RIFT SOVEREIGN"
            };
            const char* title = (ngPlusTier >= 1 && ngPlusTier <= 5) ? s_ngpTitles[ngPlusTier] : "";
            float pulse = 0.8f + 0.2f * std::sin(m_time * 4.0f);
            Uint8 ngA = static_cast<Uint8>(alpha * pulse);
            SDL_Color ngC = {255, 210, 40, ngA};
            SDL_Surface* ns = TTF_RenderText_Blended(font, title, ngC);
            if (ns) {
                SDL_Texture* nt = SDL_CreateTextureFromSurface(renderer, ns);
                if (nt) {
                    SDL_SetTextureAlphaMod(nt, ngA);
                    int nw = static_cast<int>(ns->w * 1.1f);
                    int nh = static_cast<int>(ns->h * 1.1f);
                    SDL_Rect nr = {SCREEN_WIDTH / 2 - nw / 2, 172, nw, nh};
                    SDL_RenderCopy(renderer, nt, nullptr, &nr);
                    SDL_DestroyTexture(nt);
                }
                SDL_FreeSurface(ns);
            }
        }
    }

    // Death cause
    if (deathCause > 0) {
        const char* causeText = UpgradeSystem::getDeathCauseName(deathCause);
        SDL_Color causeColor = (deathCause == 5)
            ? SDL_Color{100, 255, 100, alpha}   // Victory: green
            : SDL_Color{255, 120, 80, alpha};    // Death: red-orange
        char causeLine[64];
        std::snprintf(causeLine, sizeof(causeLine), LOC("summary.cause"), causeText);
        SDL_Surface* cs = TTF_RenderText_Blended(font, causeLine, causeColor);
        if (cs) {
            SDL_Texture* ct = SDL_CreateTextureFromSurface(renderer, cs);
            if (ct) {
                SDL_SetTextureAlphaMod(ct, alpha);
                SDL_Rect cr = {SCREEN_WIDTH / 2 - cs->w / 2, 172, cs->w, cs->h};
                SDL_RenderCopy(renderer, ct, nullptr, &cr);
                SDL_DestroyTexture(ct);
            }
            SDL_FreeSurface(cs);
        }
    }

    // Grade
    const char* grade = getGrade(roomsCleared, enemiesKilled, riftsRepaired);
    SDL_Color gradeColor = getGradeColor(grade);
    gradeColor.a = alpha;

    if (m_statsTimer > 0.2f) {
        char gradeText[16];
        std::snprintf(gradeText, sizeof(gradeText), LOC("summary.grade"), grade);
        SDL_Surface* gs = TTF_RenderText_Blended(font, gradeText, gradeColor);
        if (gs) {
            SDL_Texture* gt = SDL_CreateTextureFromSurface(renderer, gs);
            if (gt) {
                SDL_SetTextureAlphaMod(gt, alpha);
                // Render grade larger
                SDL_Rect gr = {SCREEN_WIDTH / 2 - gs->w, 178, gs->w * 2, gs->h * 2};
                SDL_RenderCopy(renderer, gt, nullptr, &gr);
                SDL_DestroyTexture(gt);
            }
            SDL_FreeSurface(gs);
        }
    }

    // Stats (reveal one by one)
    struct Stat { const char* label; int value; SDL_Color barColor; };
    Stat stats[] = {
        {LOC("summary.rooms_cleared"),     roomsCleared,                             {100, 200, 255, 255}},
        {LOC("summary.enemies_defeated"),  enemiesKilled,                            {255, 100,  80, 255}},
        {LOC("summary.rifts_repaired"),    riftsRepaired,                            {180, 100, 255, 255}},
        {LOC("summary.shards_earned"),     shardsEarned,                             {255, 200,  80, 255}},
        {LOC("summary.relics_collected"),  relicsCollected,                          {200, 140, 255, 255}},
        {LOC("summary.best_combo"),        bestCombo,                                {255, 180,  60, 255}},
        {LOC("summary.dim_switches"),      dimensionSwitches,                        { 80, 210, 255, 255}},
        {LOC("summary.damage_taken"),      damageTaken,                              {255,  80, 120, 255}},
        {LOC("summary.aerial_kills"),      aerialKills,                              {180, 255, 140, 255}},
        {LOC("summary.dash_kills"),        dashKills,                                {120, 200, 255, 255}},
        {LOC("summary.total_runs"),        game->getUpgradeSystem().totalRuns,       {150, 150, 180, 255}},
    };

    int statCount = 11;
    int baseY = 250;
    int statH = 38;
    float revealDelay = 0.35f;

    for (int i = 0; i < statCount; i++) {
        float revealTime = 0.5f + i * revealDelay;
        if (m_statsTimer < revealTime) continue;

        float itemAlpha = std::min(1.0f, (m_statsTimer - revealTime) * 3.0f) * m_fadeIn;
        Uint8 ia = static_cast<Uint8>(255 * itemAlpha);

        int y = baseY + i * statH;

        // Background bar
        SDL_SetRenderDrawColor(renderer, 25, 20, 40, static_cast<Uint8>(ia * 0.6f));
        SDL_Rect bg = {300, y, 680, statH - 6};
        SDL_RenderFillRect(renderer, &bg);

        // Left accent line
        SDL_SetRenderDrawColor(renderer, stats[i].barColor.r, stats[i].barColor.g,
                               stats[i].barColor.b, ia);
        SDL_Rect accent = {300, y, 3, statH - 6};
        SDL_RenderFillRect(renderer, &accent);

        // Label
        char text[64];
        std::snprintf(text, sizeof(text), "%s", stats[i].label);
        SDL_Color labelColor = {180, 175, 200, ia};
        SDL_Surface* ls = TTF_RenderText_Blended(font, text, labelColor);
        if (ls) {
            SDL_Texture* lt = SDL_CreateTextureFromSurface(renderer, ls);
            if (lt) {
                SDL_SetTextureAlphaMod(lt, ia);
                SDL_Rect lr = {320, y + 10, ls->w, ls->h};
                SDL_RenderCopy(renderer, lt, nullptr, &lr);
                SDL_DestroyTexture(lt);
            }
            SDL_FreeSurface(ls);
        }

        // Value (animate counting up)
        float countProgress = std::min(1.0f, (m_statsTimer - revealTime) * 2.0f);
        int displayVal = static_cast<int>(stats[i].value * countProgress);
        char valText[32];
        std::snprintf(valText, sizeof(valText), "%d", displayVal);
        SDL_Color valColor = {stats[i].barColor.r, stats[i].barColor.g,
                              stats[i].barColor.b, ia};
        SDL_Surface* vs = TTF_RenderText_Blended(font, valText, valColor);
        if (vs) {
            SDL_Texture* vt = SDL_CreateTextureFromSurface(renderer, vs);
            if (vt) {
                SDL_SetTextureAlphaMod(vt, ia);
                SDL_Rect vr = {930 - vs->w, y + 10, vs->w, vs->h};
                SDL_RenderCopy(renderer, vt, nullptr, &vr);
                SDL_DestroyTexture(vt);
            }
            SDL_FreeSurface(vs);
        }
    }

    // Balance summary (appears after stats)
    bool hasBalanceData = peakDmgRaw > 1.001f || peakSpdRaw > 1.001f
        || voidResProcs > 0 || peakResidueZones > 0 || peakVoidHunger > 0.1f;
    int balanceEndY = baseY + statCount * statH;

    if (hasBalanceData && m_statsTimer > 3.0f) {
        float bAlpha = std::min(1.0f, (m_statsTimer - 3.0f) * 2.0f) * m_fadeIn;
        Uint8 ba = static_cast<Uint8>(255 * bAlpha);

        int by = baseY + statCount * statH + 8;

        // Thin separator
        SDL_SetRenderDrawColor(renderer, 100, 80, 140, static_cast<Uint8>(ba * 0.5f));
        SDL_RenderDrawLine(renderer, 350, by, 930, by);

        // Header
        SDL_Color hdrC = {140, 120, 180, ba};
        SDL_Surface* hs = TTF_RenderText_Blended(font, LOC("summary.balance"), hdrC);
        if (hs) {
            SDL_Texture* ht = SDL_CreateTextureFromSurface(renderer, hs);
            if (ht) {
                SDL_SetTextureAlphaMod(ht, ba);
                SDL_Rect hr = {SCREEN_WIDTH / 2 - hs->w / 2, by + 4, hs->w, hs->h};
                SDL_RenderCopy(renderer, ht, nullptr, &hr);
                SDL_DestroyTexture(ht);
            }
            SDL_FreeSurface(hs);
        }

        // Line 1: DMG & ATK speed
        char line1[96];
        bool dmgCapped = peakDmgRaw > peakDmgClamped + 0.01f;
        bool spdCapped = peakSpdRaw > peakSpdClamped + 0.01f;
        std::snprintf(line1, sizeof(line1), "DMG %.2fx%s  |  ATK %.2fx%s",
                      peakDmgClamped, dmgCapped ? " (CAP)" : "",
                      peakSpdClamped, spdCapped ? " (CAP)" : "");
        SDL_Color l1c = {(dmgCapped || spdCapped) ? (Uint8)255 : (Uint8)160,
                         (dmgCapped || spdCapped) ? (Uint8)140 : (Uint8)155,
                         (dmgCapped || spdCapped) ? (Uint8)100 : (Uint8)180, ba};
        SDL_Surface* l1s = TTF_RenderText_Blended(font, line1, l1c);
        if (l1s) {
            SDL_Texture* l1t = SDL_CreateTextureFromSurface(renderer, l1s);
            if (l1t) {
                SDL_SetTextureAlphaMod(l1t, ba);
                SDL_Rect l1r = {320, by + 22, l1s->w, l1s->h};
                SDL_RenderCopy(renderer, l1t, nullptr, &l1r);
                SDL_DestroyTexture(l1t);
            }
            SDL_FreeSurface(l1s);
        }

        // Line 2: CD Floor, VoidRes, Residue
        char line2[96];
        std::snprintf(line2, sizeof(line2), "CD Floor %.0f%%  |  VoidRes %d  |  Zones %d",
                      cdFloorPercent, voidResProcs, peakResidueZones);
        SDL_Color l2c = {160, 155, 180, ba};
        SDL_Surface* l2s = TTF_RenderText_Blended(font, line2, l2c);
        if (l2s) {
            SDL_Texture* l2t = SDL_CreateTextureFromSurface(renderer, l2s);
            if (l2t) {
                SDL_SetTextureAlphaMod(l2t, ba);
                SDL_Rect l2r = {320, by + 38, l2s->w, l2s->h};
                SDL_RenderCopy(renderer, l2t, nullptr, &l2r);
                SDL_DestroyTexture(l2t);
            }
            SDL_FreeSurface(l2s);
        }

        // Line 3: VoidHunger
        if (peakVoidHunger > 0.1f) {
            char line3[96];
            std::snprintf(line3, sizeof(line3), "VoidHunger %.0f%% final (%.0f%% peak)",
                          finalVoidHunger, peakVoidHunger);
            SDL_Color l3c = {180, 140, 100, ba};
            SDL_Surface* l3s = TTF_RenderText_Blended(font, line3, l3c);
            if (l3s) {
                SDL_Texture* l3t = SDL_CreateTextureFromSurface(renderer, l3s);
                if (l3t) {
                    SDL_SetTextureAlphaMod(l3t, ba);
                    SDL_Rect l3r = {320, by + 54, l3s->w, l3s->h};
                    SDL_RenderCopy(renderer, l3t, nullptr, &l3r);
                    SDL_DestroyTexture(l3t);
                }
                SDL_FreeSurface(l3s);
            }
        }

        balanceEndY = by + 72;
    }

    // NEW RECORD banner
    int recordY = std::max(balanceEndY + 10, baseY + statCount * statH + 20);
    if (isNewRecord && m_statsTimer > 3.3f) {
        float recordAlpha = std::min(1.0f, (m_statsTimer - 3.3f) * 2.0f) * m_fadeIn;
        float pulse = 0.7f + 0.3f * std::sin(m_time * 6.0f);
        Uint8 ra = static_cast<Uint8>(255 * recordAlpha * pulse);

        // Glowing background bar
        SDL_SetRenderDrawColor(renderer, 255, 200, 50, static_cast<Uint8>(ra * 0.2f));
        SDL_Rect recordBg = {300, recordY, 680, 40};
        SDL_RenderFillRect(renderer, &recordBg);

        // Gold border
        SDL_SetRenderDrawColor(renderer, 255, 220, 80, ra);
        SDL_RenderDrawRect(renderer, &recordBg);

        SDL_Color recordColor = {255, 220, 50, ra};
        SDL_Surface* rs = TTF_RenderText_Blended(font, LOC("summary.new_record"), recordColor);
        if (rs) {
            SDL_Texture* rt = SDL_CreateTextureFromSurface(renderer, rs);
            if (rt) {
                SDL_SetTextureAlphaMod(rt, ra);
                int rw = static_cast<int>(rs->w * 1.5f);
                int rh = static_cast<int>(rs->h * 1.5f);
                SDL_Rect rr = {SCREEN_WIDTH / 2 - rw / 2, recordY + 5, rw, rh};
                SDL_RenderCopy(renderer, rt, nullptr, &rr);
                SDL_DestroyTexture(rt);
            }
            SDL_FreeSurface(rs);
        }

        // Sparkle particles around record text
        for (int i = 0; i < 8; i++) {
            float angle = m_time * 2.0f + i * (6.283185f / 8);
            int sx = SCREEN_WIDTH / 2 + static_cast<int>(std::cos(angle) * 180);
            int sy = recordY + 20 + static_cast<int>(std::sin(angle) * 12);
            Uint8 sa = static_cast<Uint8>(ra * (0.5f + 0.5f * std::sin(m_time * 5.0f + i)));
            SDL_SetRenderDrawColor(renderer, 255, 230, 100, sa);
            SDL_Rect spark = {sx - 1, sy - 1, 3, 3};
            SDL_RenderFillRect(renderer, &spark);
        }
    }

    // --- Daily run score panel (top-right corner, same timing as NEW RECORD) ---
    if (isDailyRun && dailyScore > 0 && m_statsTimer > 3.3f) {
        float scoreAlpha = std::min(1.0f, (m_statsTimer - 3.3f) * 2.0f) * m_fadeIn;
        Uint8 sa = static_cast<Uint8>(255 * scoreAlpha);
        float pulse = 0.7f + 0.3f * std::sin(m_time * 4.0f);

        int px = 950, py = 70;
        int pw = 300, ph = 60;

        SDL_SetRenderDrawColor(renderer, 20, 14, 35, static_cast<Uint8>(sa * 0.85f));
        SDL_Rect panelBg = {px, py, pw, ph};
        SDL_RenderFillRect(renderer, &panelBg);

        if (isNewDailyBest) {
            float bpulse = 0.6f + 0.4f * std::sin(m_time * 5.0f);
            SDL_SetRenderDrawColor(renderer, 255, 200, 50, static_cast<Uint8>(sa * bpulse));
        } else {
            SDL_SetRenderDrawColor(renderer, 120, 80, 200, static_cast<Uint8>(sa * 0.6f));
        }
        SDL_RenderDrawRect(renderer, &panelBg);

        // "DAILY SCORE" label
        {
            SDL_Color lc = {180, 160, 220, sa};
            SDL_Surface* dls = TTF_RenderText_Blended(font, LOC("summary.daily_score"), lc);
            if (dls) {
                SDL_Texture* dlt = SDL_CreateTextureFromSurface(renderer, dls);
                if (dlt) {
                    SDL_SetTextureAlphaMod(dlt, sa);
                    SDL_Rect dlr = {px + pw / 2 - dls->w / 2, py + 4, dls->w, dls->h};
                    SDL_RenderCopy(renderer, dlt, nullptr, &dlr);
                    SDL_DestroyTexture(dlt);
                }
                SDL_FreeSurface(dls);
            }
        }

        // Score value (large)
        {
            char dbuf[32];
            std::snprintf(dbuf, sizeof(dbuf), "%d", dailyScore);
            SDL_Color sc2 = isNewDailyBest
                ? SDL_Color{255, 215, 50, sa}
                : SDL_Color{220, 210, 255, sa};
            SDL_Surface* dss = TTF_RenderText_Blended(font, dbuf, sc2);
            if (dss) {
                SDL_Texture* dst = SDL_CreateTextureFromSurface(renderer, dss);
                if (dst) {
                    SDL_SetTextureAlphaMod(dst, sa);
                    int dsw = dss->w * 2, dsh = dss->h * 2;
                    SDL_Rect dsr = {px + pw / 2 - dsw / 2, py + 18, dsw, dsh};
                    SDL_RenderCopy(renderer, dst, nullptr, &dsr);
                    SDL_DestroyTexture(dst);
                }
                SDL_FreeSurface(dss);
            }
        }

        // Rank or NEW BEST line at bottom
        if (isNewDailyBest) {
            Uint8 nba = static_cast<Uint8>(sa * pulse);
            SDL_Color nbc = {255, 230, 80, nba};
            SDL_Surface* nbs = TTF_RenderText_Blended(font, LOC("summary.new_best"), nbc);
            if (nbs) {
                SDL_Texture* nbt = SDL_CreateTextureFromSurface(renderer, nbs);
                if (nbt) {
                    SDL_SetTextureAlphaMod(nbt, nba);
                    SDL_Rect nbr = {px + pw / 2 - nbs->w / 2, py + ph - nbs->h - 3, nbs->w, nbs->h};
                    SDL_RenderCopy(renderer, nbt, nullptr, &nbr);
                    SDL_DestroyTexture(nbt);
                }
                SDL_FreeSurface(nbs);
            }
            // Orbiting sparkles
            for (int si = 0; si < 5; si++) {
                float angle = m_time * 3.0f + si * (6.283185f / 5);
                int spx = px + pw / 2 + static_cast<int>(std::cos(angle) * 140);
                int spy = py + ph / 2 + static_cast<int>(std::sin(angle) * 18);
                Uint8 spa = static_cast<Uint8>(nba * (0.5f + 0.5f * std::sin(m_time * 4.0f + si)));
                SDL_SetRenderDrawColor(renderer, 255, 220, 80, spa);
                SDL_Rect dlspark = {spx - 1, spy - 1, 3, 3};
                SDL_RenderFillRect(renderer, &dlspark);
            }
        } else if (m_todayRank > 0) {
            char rankBuf[32];
            std::snprintf(rankBuf, sizeof(rankBuf), LOC("summary.rank_today"), m_todayRank);
            SDL_Color rc = {160, 200, 140, sa};
            SDL_Surface* drs = TTF_RenderText_Blended(font, rankBuf, rc);
            if (drs) {
                SDL_Texture* drt = SDL_CreateTextureFromSurface(renderer, drs);
                if (drt) {
                    SDL_SetTextureAlphaMod(drt, sa);
                    SDL_Rect drr = {px + pw / 2 - drs->w / 2, py + ph - drs->h - 3, drs->w, drs->h};
                    SDL_RenderCopy(renderer, drt, nullptr, &drr);
                    SDL_DestroyTexture(drt);
                }
                SDL_FreeSurface(drs);
            }
        }
    }

    // Continue prompt
    if (m_fadeIn >= 1.0f && m_statsTimer > 3.8f) {
        float pulse = 0.5f + 0.5f * std::sin(m_time * 4.0f);
        Uint8 pa = static_cast<Uint8>(200 * pulse);

        // Leaderboard hint for daily runs
        if (isDailyRun) {
            SDL_Color lc = {120, 200, 150, pa};
            SDL_Surface* dls = TTF_RenderText_Blended(font, LOC("summary.leaderboard_hint"), lc);
            if (dls) {
                SDL_Texture* dlt = SDL_CreateTextureFromSurface(renderer, dls);
                if (dlt) {
                    SDL_Rect dlr = {SCREEN_WIDTH / 2 - dls->w / 2, 640, dls->w, dls->h};
                    SDL_RenderCopy(renderer, dlt, nullptr, &dlr);
                    SDL_DestroyTexture(dlt);
                }
                SDL_FreeSurface(dls);
            }
        }

        SDL_Color c = {150, 130, 200, pa};
        SDL_Surface* s = TTF_RenderText_Blended(font, LOC("summary.continue"), c);
        if (s) {
            SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
            if (t) {
                SDL_Rect r = {SCREEN_WIDTH / 2 - s->w / 2, 660, s->w, s->h};
                SDL_RenderCopy(renderer, t, nullptr, &r);
                SDL_DestroyTexture(t);
            }
            SDL_FreeSurface(s);
        }
    }
}
