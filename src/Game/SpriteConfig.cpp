#include "SpriteConfig.h"
#include "Core/ResourceManager.h"
#include <SDL2/SDL.h>

// Disable AI-generated sprite sheets — use procedural rendering instead.
// The ComfyUI sprites are too noisy/detailed for pixel-art style.
// Set to true once proper pixel-art sprite sheets are available.
static constexpr bool kUseEntitySprites = true;

static const float FRAME_DUR = 0.12f;  // Default frame duration
static const float FAST_DUR  = 0.08f;  // Fast animations (attack, hurt)
static const float SLOW_DUR  = 0.18f;  // Slow animations (idle)

const char* SpriteConfig::animStateToName(AnimState state) {
    switch (state) {
        case AnimState::Idle:      return Anim::Idle;
        case AnimState::Run:       return Anim::Run;
        case AnimState::Jump:      return Anim::Jump;
        case AnimState::Fall:      return Anim::Fall;
        case AnimState::Dash:      return Anim::Dash;
        case AnimState::WallSlide: return Anim::WallSlide;
        case AnimState::Attack:    return Anim::Attack;
        case AnimState::Hurt:      return Anim::Hurt;
        case AnimState::Dead:      return Anim::Dead;
        default:                   return Anim::Idle;
    }
}

bool SpriteConfig::setupPlayer(AnimationComponent& anim, SpriteComponent& sprite) {
    if (!kUseEntitySprites) return false; // Use procedural rendering
    auto* tex = ResourceManager::instance().getTexture("assets/textures/player/player.png");
    if (!tex) {
        // Fall back to placeholder — procedural rendering remains active as next fallback
        if (!applyPlayerPlaceholder(sprite)) {
            SDL_Log("Warning: Both texture and placeholder failed for player");
        }
        return false;
    }

    sprite.texture = tex;
    const int fw = 32, fh = 48;

    anim.addSheetAnimation(Anim::Idle,      0, 6, fw, fh, SLOW_DUR, true);
    anim.addSheetAnimation(Anim::Run,       1, 8, fw, fh, FRAME_DUR, true);
    anim.addSheetAnimation(Anim::Jump,      2, 3, fw, fh, FRAME_DUR, false);
    anim.addSheetAnimation(Anim::Fall,      3, 3, fw, fh, FRAME_DUR, true);
    anim.addSheetAnimation(Anim::Dash,      4, 4, fw, fh, FAST_DUR, false);
    anim.addSheetAnimation(Anim::WallSlide, 5, 3, fw, fh, FRAME_DUR, true);
    anim.addSheetAnimation(Anim::Attack,    6, 6, fw, fh, FAST_DUR, false);
    anim.addSheetAnimation(Anim::Hurt,      7, 3, fw, fh, FAST_DUR, false);
    anim.addSheetAnimation(Anim::Dead,      8, 5, fw, fh, FRAME_DUR, false);

    anim.play(Anim::Idle);
    return true;
}

bool SpriteConfig::setupEnemy(AnimationComponent& anim, SpriteComponent& sprite, EnemyType type) {
    if (!kUseEntitySprites) return false; // Use procedural rendering
    std::string path;
    switch (type) {
        case EnemyType::Walker:   path = "assets/textures/enemies/walker.png";   break;
        case EnemyType::Flyer:    path = "assets/textures/enemies/flyer.png";    break;
        case EnemyType::Turret:   path = "assets/textures/enemies/turret.png";   break;
        case EnemyType::Charger:  path = "assets/textures/enemies/charger.png";  break;
        case EnemyType::Phaser:   path = "assets/textures/enemies/phaser.png";   break;
        case EnemyType::Exploder: path = "assets/textures/enemies/exploder.png"; break;
        case EnemyType::Shielder: path = "assets/textures/enemies/shielder.png"; break;
        case EnemyType::Crawler:  path = "assets/textures/enemies/crawler.png";  break;
        case EnemyType::Summoner: path = "assets/textures/enemies/summoner.png"; break;
        case EnemyType::Sniper:     path = "assets/textures/enemies/sniper.png";     break;
        case EnemyType::Teleporter: path = "assets/textures/enemies/teleporter.png"; break;
        case EnemyType::Reflector:  path = "assets/textures/enemies/reflector.png";  break;
        case EnemyType::Leech:      path = "assets/textures/enemies/leech.png";      break;
        default:                    path = "assets/textures/enemies/walker.png";     break;
    }

    auto* tex = ResourceManager::instance().getTexture(path);
    if (!tex) {
        // Fall back to placeholder; procedural rendering is still the final fallback
        if (!applyEnemyPlaceholder(sprite, type)) {
            SDL_Log("Warning: Both texture and placeholder failed for enemy type %d", static_cast<int>(type));
        }
        return false;
    }

    sprite.texture = tex;
    const int fw = 32, fh = 32;

    anim.addSheetAnimation(Anim::Idle,   0, 4, fw, fh, SLOW_DUR, true);
    anim.addSheetAnimation(Anim::Walk,   1, 6, fw, fh, FRAME_DUR, true);
    anim.addSheetAnimation(Anim::Attack, 2, 4, fw, fh, FAST_DUR, false);
    anim.addSheetAnimation(Anim::Hurt,   3, 2, fw, fh, FAST_DUR, false);
    anim.addSheetAnimation(Anim::Dead,   4, 4, fw, fh, FRAME_DUR, false);

    anim.play(Anim::Idle);
    return true;
}

bool SpriteConfig::setupBoss(AnimationComponent& anim, SpriteComponent& sprite, int bossType) {
    if (!kUseEntitySprites) return false; // Use procedural rendering
    std::string path;
    int fw = 64, fh = 64;

    switch (bossType) {
        case 0: path = "assets/textures/bosses/rift_guardian.png"; break;
        case 1: path = "assets/textures/bosses/void_wyrm.png"; break;
        case 2: path = "assets/textures/bosses/dimensional_architect.png"; break;
        case 3: path = "assets/textures/bosses/temporal_weaver.png"; break;
        case 4:
            path = "assets/textures/bosses/void_sovereign.png";
            fw = 96; fh = 96;
            break;
        case 5:
            path = "assets/textures/bosses/entropy_incarnate.png";
            fw = 64; fh = 64;
            break;
        default: path = "assets/textures/bosses/rift_guardian.png"; break;
    }

    auto* tex = ResourceManager::instance().getTexture(path);
    if (!tex) {
        // Fall back to placeholder; procedural rendering is still the final fallback
        if (!applyBossPlaceholder(sprite, bossType)) {
            SDL_Log("Warning: Both texture and placeholder failed for boss type %d", bossType);
        }
        return false;
    }

    sprite.texture = tex;

    anim.addSheetAnimation(Anim::Idle,            0, 6, fw, fh, SLOW_DUR, true);
    anim.addSheetAnimation(Anim::Move,            1, 6, fw, fh, FRAME_DUR, true);
    anim.addSheetAnimation(Anim::Attack1,         2, 6, fw, fh, FAST_DUR, false);
    anim.addSheetAnimation(Anim::Attack2,         3, 6, fw, fh, FAST_DUR, false);
    anim.addSheetAnimation(Anim::Attack3,         4, 6, fw, fh, FAST_DUR, false);
    anim.addSheetAnimation(Anim::PhaseTransition, 5, 8, fw, fh, FRAME_DUR, false);
    anim.addSheetAnimation(Anim::Hurt,            6, 3, fw, fh, FAST_DUR, false);
    anim.addSheetAnimation(Anim::Enrage,          7, 6, fw, fh, FRAME_DUR, false);
    anim.addSheetAnimation(Anim::Dead,            8, 8, fw, fh, FRAME_DUR, false);

    anim.play(Anim::Idle);
    return true;
}

// ---------------------------------------------------------------------------
// Placeholder helpers — called when real sprite sheets are not available.
// Each function sets sprite.texture to a simple colored placeholder PNG.
// Textures are generated by ResourceManager::ensurePlaceholderTextures() on init.
// No AnimationComponent changes are made: the whole placeholder is one static frame.
// ---------------------------------------------------------------------------

static SDL_Texture* loadPlaceholder(const char* filename) {
    std::string path = std::string(SpriteConfig::PLACEHOLDER_DIR) + "/" + filename;
    return ResourceManager::instance().getTexture(path);
}

bool SpriteConfig::applyPlayerPlaceholder(SpriteComponent& sprite) {
    auto* tex = loadPlaceholder("player.png");
    if (!tex) return false;
    sprite.texture = tex;
    sprite.srcRect = {0, 0, 32, 48};
    return true;
}

bool SpriteConfig::applyEnemyPlaceholder(SpriteComponent& sprite, EnemyType type) {
    const char* file = "walker.png"; // default fallback
    int w = 32, h = 32;
    switch (type) {
        case EnemyType::Walker:   file = "walker.png";   break;
        case EnemyType::Flyer:    file = "flyer.png";    break;
        case EnemyType::Turret:   file = "turret.png";   break;
        case EnemyType::Charger:  file = "charger.png";  break;
        case EnemyType::Phaser:   file = "phaser.png";   break;
        case EnemyType::Exploder: file = "exploder.png"; break;
        case EnemyType::Shielder: file = "shielder.png"; break;
        case EnemyType::Crawler:  file = "crawler.png";  h = 24; break;
        case EnemyType::Summoner: file = "summoner.png"; break;
        case EnemyType::Sniper:   file = "sniper.png";   break;
        default: break;
    }
    auto* tex = loadPlaceholder(file);
    if (!tex) return false;
    sprite.texture = tex;
    sprite.srcRect = {0, 0, w, h};
    return true;
}

bool SpriteConfig::applyBossPlaceholder(SpriteComponent& sprite, int bossType) {
    const char* file = "rift_guardian.png";
    int sz = 64;
    switch (bossType) {
        case 0: file = "rift_guardian.png";         sz = 64; break;
        case 1: file = "void_wyrm.png";             sz = 64; break;
        case 2: file = "dimensional_architect.png"; sz = 64; break;
        case 3: file = "temporal_weaver.png";       sz = 64; break;
        case 4: file = "void_sovereign.png";       sz = 96; break;
        case 5: file = "entropy_incarnate.png";    sz = 64; break;
        default: break;
    }
    auto* tex = loadPlaceholder(file);
    if (!tex) return false;
    sprite.texture = tex;
    sprite.srcRect = {0, 0, sz, sz};
    return true;
}

const char* SpriteConfig::getTilePlaceholderPath() {
    return "assets/textures/placeholders/tile_solid.png";
}
