#include "PlayState.h"
#include "Core/Game.h"
#include "Game/Enemy.h"
#include "Components/TransformComponent.h"
#include "Components/HealthComponent.h"
#include "Components/CombatComponent.h"
#include "Components/PhysicsBody.h"
#include "Game/Tile.h"
#include "Components/AIComponent.h"
#include "Core/AudioManager.h"
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <algorithm>

void PlayState::enter() {
    startNewRun();
}

void PlayState::exit() {
    AudioManager::instance().stopAmbient();
    m_entities.clear();
    m_particles.clear();
    m_player.reset();
    m_level.reset();
    m_activePuzzle.reset();
}

void PlayState::startNewRun() {
    m_entities.clear();
    m_particles.clear();

    m_currentDifficulty = 1;
    m_runSeed = std::rand();
    enemiesKilled = 0;
    riftsRepaired = 0;
    roomsCleared = 0;
    shardsCollected = 0;
    m_collapsing = false;
    m_tutorialTimer = 0;
    m_tutorialHintIndex = 0;
    m_tutorialHintShowTimer = 0;
    std::memset(m_tutorialHintDone, 0, sizeof(m_tutorialHintDone));
    m_hasMovedThisRun = false;
    m_hasJumpedThisRun = false;
    m_hasDashedThisRun = false;
    m_hasAttackedThisRun = false;

    // Random theme pair
    auto themes = WorldTheme::getRandomPair(m_runSeed);
    m_themeA = themes.first;
    m_themeB = themes.second;

    m_dimManager = DimensionManager();
    m_dimManager.setDimColors(m_themeA.colors.background, m_themeB.colors.background);
    m_dimManager.particles = &m_particles;
    AudioManager::instance().playAmbient(m_dimManager.getCurrentDimension());

    m_entropy = SuitEntropy();
    m_combatSystem.setParticleSystem(&m_particles);
    m_combatSystem.setCamera(&m_camera);
    m_aiSystem.setParticleSystem(&m_particles);
    m_aiSystem.setCamera(&m_camera);
    m_aiSystem.setCombatSystem(&m_combatSystem);
    m_hitFreezeTimer = 0;
    m_spikeDmgCooldown = 0;

    generateLevel();
}

void PlayState::generateLevel() {
    m_entities.clear();
    m_particles.clear();

    m_levelGen.setThemes(m_themeA, m_themeB);
    m_level = std::make_unique<Level>(m_levelGen.generate(m_currentDifficulty, m_runSeed + m_currentDifficulty));

    // Create player
    m_player = std::make_unique<Player>(m_entities);
    m_player->particles = &m_particles;
    applyUpgrades();

    auto& playerTransform = m_player->getEntity()->getComponent<TransformComponent>();
    playerTransform.position = m_level->getSpawnPoint();

    m_camera.setPosition(playerTransform.getCenter());
    m_camera.setBounds(0, 0, m_level->getPixelWidth(), m_level->getPixelHeight());

    spawnEnemies();

    // Boss every 3 levels
    m_isBossLevel = (m_currentDifficulty % 3 == 0);
    m_bossDefeated = false;
    if (m_isBossLevel) {
        spawnBoss();
    }

    m_levelComplete = false;
    m_levelCompleteTimer = 0;
    m_activePuzzle.reset();
    m_nearRiftIndex = -1;

    // Start ambient music
    AudioManager::instance().playAmbient(m_dimManager.getCurrentDimension());
}

void PlayState::spawnEnemies() {
    auto spawns = m_level->getEnemySpawns();

    // Difficulty modifier: Easy removes 30% of spawns, Hard adds 40% extra
    if (g_selectedDifficulty == GameDifficulty::Easy) {
        int removeCount = static_cast<int>(spawns.size() * 0.3f);
        for (int i = 0; i < removeCount && !spawns.empty(); i++) {
            spawns.pop_back();
        }
    } else if (g_selectedDifficulty == GameDifficulty::Hard) {
        int extraCount = static_cast<int>(spawns.size() * 0.4f);
        for (int i = 0; i < extraCount; i++) {
            auto& base = spawns[std::rand() % spawns.size()];
            Vec2 offset = {static_cast<float>((std::rand() % 100) - 50), 0};
            spawns.push_back({Vec2{base.position.x + offset.x, base.position.y}, base.enemyType, base.dimension});
        }
    }

    // Divide spawns into waves of 3-5 enemies
    m_spawnWaves.clear();
    m_currentWave = 0;
    m_waveTimer = 0;
    m_waveActive = false;

    int waveSize = 3 + (m_currentDifficulty > 3 ? 2 : m_currentDifficulty > 1 ? 1 : 0);
    std::vector<Level::SpawnPoint> wave;
    for (auto& sp : spawns) {
        wave.push_back(sp);
        if (static_cast<int>(wave.size()) >= waveSize) {
            m_spawnWaves.push_back(wave);
            wave.clear();
        }
    }
    if (!wave.empty()) m_spawnWaves.push_back(wave);

    // Spawn first wave immediately
    if (!m_spawnWaves.empty()) {
        for (auto& sp : m_spawnWaves[0]) {
            Enemy::createByType(m_entities, sp.enemyType, sp.position, sp.dimension);
        }
        m_currentWave = 1;
        m_waveActive = true;
        m_waveTimer = m_waveDelay;
    }
}

void PlayState::applyUpgrades() {
    if (!m_player) return;
    auto& upgrades = game->getUpgradeSystem();

    m_player->moveSpeed = 250.0f * upgrades.getMoveSpeedMultiplier();
    m_player->jumpForce = -420.0f * upgrades.getJumpMultiplier();
    m_player->dashCooldown = 0.5f * upgrades.getDashCooldownMultiplier();
    m_player->maxJumps = 2 + upgrades.getExtraJumps();

    auto& hp = m_player->getEntity()->getComponent<HealthComponent>();
    hp.maxHP = 100.0f + upgrades.getMaxHPBonus();
    hp.currentHP = hp.maxHP;
    hp.armor = upgrades.getArmorBonus();

    auto& combat = m_player->getEntity()->getComponent<CombatComponent>();
    combat.meleeAttack.damage = 20.0f * upgrades.getMeleeDamageMultiplier();
    combat.rangedAttack.damage = 12.0f * upgrades.getRangedDamageMultiplier();

    m_dimManager.switchCooldown = 0.5f * upgrades.getSwitchCooldownMultiplier();
    m_entropy.passiveDecay = upgrades.getEntropyDecay();
    m_combatSystem.setCritChance(upgrades.getCritChance());
}

void PlayState::handleEvent(const SDL_Event& event) {
    if (event.type == SDL_KEYDOWN) {
        if (event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
            if (m_activePuzzle && m_activePuzzle->isActive()) {
                m_activePuzzle.reset();
            } else {
                game->pushState(StateID::Pause);
            }
            return;
        }

        // Puzzle input
        if (m_activePuzzle && m_activePuzzle->isActive()) {
            switch (event.key.keysym.scancode) {
                case SDL_SCANCODE_W: case SDL_SCANCODE_UP:    handlePuzzleInput(0); break;
                case SDL_SCANCODE_D: case SDL_SCANCODE_RIGHT: handlePuzzleInput(1); break;
                case SDL_SCANCODE_S: case SDL_SCANCODE_DOWN:  handlePuzzleInput(2); break;
                case SDL_SCANCODE_A: case SDL_SCANCODE_LEFT:  handlePuzzleInput(3); break;
                case SDL_SCANCODE_RETURN: case SDL_SCANCODE_SPACE: handlePuzzleInput(4); break;
                default: break;
            }
            return;
        }
    }
}

void PlayState::update(float dt) {
    if (!m_player || !m_level) return;

    // Hit-freeze: brief pause on melee hit for impact feel
    float freeze = m_combatSystem.consumeHitFreeze();
    if (freeze > 0 && m_hitFreezeTimer <= 0) {
        m_hitFreezeTimer = freeze;
    }
    if (m_hitFreezeTimer > 0) {
        m_hitFreezeTimer -= dt;
        m_camera.update(dt);       // still update camera shake during freeze
        m_particles.update(dt);    // particles keep going
        return;
    }

    // Active puzzle takes priority
    if (m_activePuzzle && m_activePuzzle->isActive()) {
        m_activePuzzle->update(dt);
        return;
    }

    auto& input = game->getInput();

    // Dimension switch
    if (input.isActionPressed(Action::DimensionSwitch)) {
        m_dimManager.switchDimension();
        m_entropy.onDimensionSwitch();
        AudioManager::instance().play(SFX::DimensionSwitch);
        AudioManager::instance().playAmbient(m_dimManager.getCurrentDimension());
    }

    // Forced dimension switch from entropy
    if (m_entropy.shouldForceSwitch()) {
        m_dimManager.switchDimension();
        m_entropy.clearForceSwitch();
        m_camera.shake(8.0f, 0.3f);
        AudioManager::instance().playAmbient(m_dimManager.getCurrentDimension());
    }

    m_dimManager.playerPos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
    m_dimManager.update(dt);

    // Update input distortion from entropy
    game->getInputMutable().setInputDistortion(m_entropy.getInputDistortion());

    // Player update
    m_player->update(dt, input);

    // Physics - pass current dimension so player (dimension=0) collides with correct tiles
    m_physics.update(m_entities, dt, m_level.get(), m_dimManager.getCurrentDimension());

    // AI
    Vec2 playerPos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
    m_aiSystem.update(m_entities, dt, playerPos, m_dimManager.getCurrentDimension());

    // Collision
    m_collision.update(m_entities, m_dimManager.getCurrentDimension());

    // Combat
    m_combatSystem.update(m_entities, dt, m_dimManager.getCurrentDimension());

    // Consume damage events for floating numbers
    auto dmgEvents = m_combatSystem.consumeDamageEvents();
    for (auto& evt : dmgEvents) {
        FloatingDamageNumber num;
        num.position = evt.position;
        num.value = evt.damage;
        num.isPlayerDamage = evt.isPlayerDamage;
        num.isCritical = evt.isCritical;
        num.lifetime = evt.isCritical ? 1.2f : 0.8f;
        num.maxLifetime = num.lifetime;
        m_damageNumbers.push_back(num);
    }
    updateDamageNumbers(dt);

    // Shard magnet: attract pickups toward player
    float magnetRange = game->getUpgradeSystem().getShardMagnetRange();
    if (magnetRange > 14.0f) { // Only if upgrade purchased (base is 14)
        Vec2 pPos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
        m_entities.forEach([&](Entity& e) {
            if (e.getTag().find("pickup") == std::string::npos) return;
            if (!e.hasComponent<TransformComponent>() || !e.hasComponent<PhysicsBody>()) return;

            auto& t = e.getComponent<TransformComponent>();
            Vec2 pickupPos = t.getCenter();
            float dx = pPos.x - pickupPos.x;
            float dy = pPos.y - pickupPos.y;
            float dist = std::sqrt(dx * dx + dy * dy);

            if (dist < magnetRange && dist > 5.0f) {
                float strength = 300.0f * (1.0f - dist / magnetRange);
                auto& phys = e.getComponent<PhysicsBody>();
                phys.velocity.x += (dx / dist) * strength * dt;
                phys.velocity.y += (dy / dist) * strength * dt;
            }
        });
    }

    // Entities
    m_entities.update(dt);
    m_entities.refresh();

    // Check boss defeated
    if (m_isBossLevel && !m_bossDefeated) {
        bool bossAlive = false;
        m_entities.forEach([&](Entity& e) {
            if (e.getTag() == "enemy_boss") bossAlive = true;
        });
        if (!bossAlive) {
            m_bossDefeated = true;
            // Boss kill rewards
            int bossShards = 50 + m_currentDifficulty * 20;
            shardsCollected += bossShards;
            game->getUpgradeSystem().addRiftShards(bossShards);
            AudioManager::instance().play(SFX::LevelComplete);
            m_camera.shake(15.0f, 0.5f);
            Vec2 playerPos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
            m_particles.burst(playerPos, 60, {255, 200, 100, 255}, 350.0f, 6.0f);
        }
    }

    // Spike damage
    m_spikeDmgCooldown -= dt;
    if (m_spikeDmgCooldown <= 0) {
        auto& playerT = m_player->getEntity()->getComponent<TransformComponent>();
        int tileSize = m_level->getTileSize();
        int footX = static_cast<int>(playerT.getCenter().x) / tileSize;
        int footY = static_cast<int>(playerT.position.y + playerT.height - 1) / tileSize;
        int dim = m_dimManager.getCurrentDimension();
        if (m_level->inBounds(footX, footY)) {
            const auto& tile = m_level->getTile(footX, footY, dim);
            if (tile.type == TileType::Spike) {
                auto& playerHP = m_player->getEntity()->getComponent<HealthComponent>();
                playerHP.takeDamage(15.0f);
                m_entropy.addEntropy(5.0f);
                m_spikeDmgCooldown = 0.5f;
                m_camera.shake(6.0f, 0.2f);
                AudioManager::instance().play(SFX::SpikeDamage);
                if (m_player->getEntity()->hasComponent<PhysicsBody>()) {
                    auto& phys = m_player->getEntity()->getComponent<PhysicsBody>();
                    phys.velocity.y = -250.0f;
                }
                m_particles.burst(playerT.getCenter(), 15, {255, 80, 40, 255}, 150.0f, 3.0f);
                m_hud.triggerDamageFlash();
            }
        }
    }

    // HUD flash update
    m_hud.updateFlash(dt);

    // Tutorial timer + action tracking
    m_tutorialTimer += dt;
    if (m_player && m_player->getEntity()->hasComponent<PhysicsBody>()) {
        auto& phys = m_player->getEntity()->getComponent<PhysicsBody>();
        if (std::abs(phys.velocity.x) > 10.0f) m_hasMovedThisRun = true;
        if (!phys.onGround) m_hasJumpedThisRun = true;
    }
    if (m_player && m_player->isDashing) m_hasDashedThisRun = true;
    if (m_player && m_player->getEntity()->hasComponent<CombatComponent>()) {
        if (m_player->getEntity()->getComponent<CombatComponent>().isAttacking) m_hasAttackedThisRun = true;
    }

    // Entropy
    m_entropy.update(dt);
    if (m_entropy.isCritical()) {
        // Suit crash - run over
        AudioManager::instance().play(SFX::SuitEntropyCritical);
        endRun();
        return;
    }

    // Player death
    auto& hp = m_player->getEntity()->getComponent<HealthComponent>();
    if (hp.currentHP <= 0) {
        AudioManager::instance().play(SFX::PlayerDeath);
        endRun();
        return;
    }

    // Check rift interaction
    checkRiftInteraction();

    // Check if player reached exit
    checkExitReached();

    // Camera follow player with look-ahead
    Vec2 camTarget = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
    Vec2 playerVel = m_player->getEntity()->hasComponent<PhysicsBody>() ?
        m_player->getEntity()->getComponent<PhysicsBody>().velocity : Vec2{0, 0};
    m_camera.follow(camTarget, dt, playerVel);
    m_camera.update(dt);

    // Spawn waves
    updateSpawnWaves(dt);

    // Particles
    m_particles.update(dt);

    // Level complete transition
    if (m_levelComplete) {
        m_levelCompleteTimer += dt;
        if (m_levelCompleteTimer >= 2.0f) {
            m_currentDifficulty++;
            roomsCleared++;
            m_runSeed += 100;

            // Pick new theme pair every 3 levels
            if (m_currentDifficulty % 3 == 0) {
                auto themes = WorldTheme::getRandomPair(m_runSeed);
                m_themeA = themes.first;
                m_themeB = themes.second;
                m_dimManager.setDimColors(m_themeA.colors.background, m_themeB.colors.background);
            }

            generateLevel();
        }
    }

    // Collapse mechanic
    if (m_collapsing) {
        m_collapseTimer += dt;
        float urgency = m_collapseTimer / m_collapseMaxTime;
        if (urgency > 0.5f) {
            m_camera.shake(urgency * 3.0f, 0.1f);
        }
        if (m_collapseTimer >= m_collapseMaxTime) {
            endRun();
        }
    }

}

void PlayState::checkRiftInteraction() {
    if (!m_player || !m_level) return;

    Vec2 playerPos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
    auto rifts = m_level->getRiftPositions();

    m_nearRiftIndex = -1;
    for (int i = 0; i < static_cast<int>(rifts.size()); i++) {
        float dx = playerPos.x - rifts[i].x;
        float dy = playerPos.y - rifts[i].y;
        float dist = std::sqrt(dx * dx + dy * dy);
        if (dist < 60.0f) {
            m_nearRiftIndex = i;
            break;
        }
    }

    if (m_nearRiftIndex >= 0 && game->getInput().isActionPressed(Action::Interact)) {
        // Puzzle type progression: early = Timing (easiest), mid = Sequence, late = Alignment
        int puzzleType;
        if (m_currentDifficulty <= 1) {
            puzzleType = 0; // Always Timing for first levels
        } else if (m_currentDifficulty <= 3) {
            puzzleType = (riftsRepaired % 2 == 0) ? 0 : 1; // Mix Timing and Sequence
        } else {
            int roll = std::rand() % 100;
            if (roll < 20) puzzleType = 0;       // 20% Timing
            else if (roll < 55) puzzleType = 1;  // 35% Sequence
            else puzzleType = 2;                 // 45% Alignment
        }
        m_activePuzzle = std::make_unique<RiftPuzzle>(
            static_cast<PuzzleType>(puzzleType), m_currentDifficulty);

        m_activePuzzle->onComplete = [this](bool success) {
            if (success) {
                AudioManager::instance().play(SFX::RiftRepair);
                riftsRepaired++;
                m_entropy.onRiftRepaired();
                shardsCollected += 10 + m_currentDifficulty * 5;
                game->getUpgradeSystem().addRiftShards(10 + m_currentDifficulty * 5);
                game->getUpgradeSystem().totalRiftsRepaired++;

                m_particles.burst(m_dimManager.playerPos, 40,
                    {150, 80, 255, 255}, 250.0f, 6.0f);
                m_camera.shake(5.0f, 0.3f);

                // Start collapse after all rifts repaired
                int totalRifts = static_cast<int>(m_level->getRiftPositions().size());
                if (riftsRepaired >= totalRifts) {
                    m_collapsing = true;
                    m_collapseTimer = 0;
                }
            } else {
                AudioManager::instance().play(SFX::RiftFail);
                m_entropy.addEntropy(10.0f);
                m_camera.shake(3.0f, 0.2f);
            }
            m_activePuzzle.reset();
        };

        m_activePuzzle->activate();
    }
}

void PlayState::checkExitReached() {
    if (!m_player || !m_level || m_levelComplete) return;

    Vec2 playerPos = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
    Vec2 exitPos = m_level->getExitPoint();
    float dx = playerPos.x - exitPos.x;
    float dy = playerPos.y - exitPos.y;
    float dist = std::sqrt(dx * dx + dy * dy);

    if (dist < 50.0f) {
        m_levelComplete = true;
        m_levelCompleteTimer = 0;
        AudioManager::instance().play(SFX::LevelComplete);
        float shardMult = (g_selectedDifficulty == GameDifficulty::Easy) ? 1.5f :
                          (g_selectedDifficulty == GameDifficulty::Hard) ? 0.75f : 1.0f;
        int shards = static_cast<int>((20 + m_currentDifficulty * 10) * shardMult);
        shardsCollected += shards;
        game->getUpgradeSystem().addRiftShards(shards);
    }
}

void PlayState::handlePuzzleInput(int action) {
    if (m_activePuzzle && m_activePuzzle->isActive()) {
        m_activePuzzle->handleInput(action);
    }
}

void PlayState::renderBackground(SDL_Renderer* renderer) {
    auto& bgColor = (m_dimManager.getCurrentDimension() == 1) ?
        m_themeA.colors.background : m_themeB.colors.background;
    SDL_SetRenderDrawColor(renderer, bgColor.r, bgColor.g, bgColor.b, 255);
    SDL_RenderClear(renderer);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    Vec2 camPos = m_camera.getPosition();
    Uint32 ticks = SDL_GetTicks();

    // Layer 1: Distant stars (slow parallax)
    for (int i = 0; i < 60; i++) {
        // Deterministic pseudo-random positions based on index
        int baseX = ((i * 7919 + 104729) % 2560) - 640;
        int baseY = ((i * 6271 + 73856) % 1440) - 360;

        float parallax = 0.05f;
        int sx = baseX - static_cast<int>(camPos.x * parallax) % 2560;
        int sy = baseY - static_cast<int>(camPos.y * parallax) % 1440;
        // Wrap around screen
        sx = ((sx % 1280) + 1280) % 1280;
        sy = ((sy % 720) + 720) % 720;

        float twinkle = 0.5f + 0.5f * std::sin(ticks * 0.002f + i * 1.7f);
        Uint8 brightness = static_cast<Uint8>(30 + 40 * twinkle);
        int size = (i % 3 == 0) ? 2 : 1;
        SDL_SetRenderDrawColor(renderer, brightness, brightness,
                               static_cast<Uint8>(brightness + 30), 255);
        SDL_Rect star = {sx, sy, size, size};
        SDL_RenderFillRect(renderer, &star);
    }

    // Layer 2: Grid lines (medium parallax, gives depth)
    float gridParallax = 0.15f;
    int gridSpacing = 120;
    int gridOffX = static_cast<int>(camPos.x * gridParallax) % gridSpacing;
    int gridOffY = static_cast<int>(camPos.y * gridParallax) % gridSpacing;

    Uint8 gridAlpha = 15;
    SDL_SetRenderDrawColor(renderer,
        static_cast<Uint8>(bgColor.r + 30 > 255 ? 255 : bgColor.r + 30),
        static_cast<Uint8>(bgColor.g + 30 > 255 ? 255 : bgColor.g + 30),
        static_cast<Uint8>(bgColor.b + 30 > 255 ? 255 : bgColor.b + 30),
        gridAlpha);

    for (int x = -gridOffX; x < 1280; x += gridSpacing) {
        SDL_RenderDrawLine(renderer, x, 0, x, 720);
    }
    for (int y = -gridOffY; y < 720; y += gridSpacing) {
        SDL_RenderDrawLine(renderer, 0, y, 1280, y);
    }

    // Layer 3: Floating dimension particles (close parallax)
    auto& dimColor = (m_dimManager.getCurrentDimension() == 1) ?
        m_themeA.colors.solid : m_themeB.colors.solid;
    for (int i = 0; i < 20; i++) {
        float speed = 0.3f + (i % 5) * 0.1f;
        int baseX = ((i * 4513 + 23143) % 1600) - 160;
        int baseY = ((i * 3251 + 51749) % 900) - 90;

        float px = baseX - camPos.x * speed;
        float py = baseY - camPos.y * speed;
        // Gentle floating motion
        py += std::sin(ticks * 0.001f + i * 2.3f) * 15.0f;

        int screenX = ((static_cast<int>(px) % 1280) + 1280) % 1280;
        int screenY = ((static_cast<int>(py) % 720) + 720) % 720;

        int size = 2 + i % 3;
        Uint8 pa = static_cast<Uint8>(20 + 15 * std::sin(ticks * 0.0015f + i));
        SDL_SetRenderDrawColor(renderer, dimColor.r, dimColor.g, dimColor.b, pa);
        SDL_Rect particle = {screenX, screenY, size, size};
        SDL_RenderFillRect(renderer, &particle);
    }
}

void PlayState::render(SDL_Renderer* renderer) {
    // Parallax background
    renderBackground(renderer);

    if (m_level) {
        m_level->render(renderer, m_camera,
                        m_dimManager.getCurrentDimension(),
                        m_dimManager.getBlendAlpha());
    }

    // Render entities
    m_renderSys.render(renderer, m_entities, m_camera,
                       m_dimManager.getCurrentDimension(),
                       m_dimManager.getBlendAlpha());

    // Particles
    m_particles.render(renderer, m_camera);

    // Dimension switch visual effect
    m_dimManager.applyVisualEffect(renderer, 1280, 720);

    // Entropy visual effects
    m_entropy.applyVisualEffects(renderer, 1280, 720);

    // Collapse warning
    if (m_collapsing) {
        float urgency = m_collapseTimer / m_collapseMaxTime;
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        Uint8 a = static_cast<Uint8>(urgency * 100);
        SDL_SetRenderDrawColor(renderer, 255, 30, 0, a);
        SDL_Rect border = {0, 0, 1280, 720};
        SDL_RenderDrawRect(renderer, &border);

        // Collapse timer text
        TTF_Font* font = game->getFont();
        if (font) {
            float remaining = m_collapseMaxTime - m_collapseTimer;
            if (remaining < 0) remaining = 0;
            int secs = static_cast<int>(remaining);
            int tenths = static_cast<int>((remaining - secs) * 10);
            char buf[32];
            std::snprintf(buf, sizeof(buf), "COLLAPSE: %d.%d", secs, tenths);

            // Pulsing red-white text
            Uint8 pulse = static_cast<Uint8>(200 + 55 * std::sin(SDL_GetTicks() * 0.01f));
            SDL_Color tc = {pulse, static_cast<Uint8>(pulse / 4), 0, 255};
            SDL_Surface* ts = TTF_RenderText_Blended(font, buf, tc);
            if (ts) {
                SDL_Texture* tt = SDL_CreateTextureFromSurface(renderer, ts);
                if (tt) {
                    SDL_Rect tr = {640 - ts->w / 2, 50, ts->w, ts->h};
                    SDL_RenderCopy(renderer, tt, nullptr, &tr);
                    SDL_DestroyTexture(tt);
                }
                SDL_FreeSurface(ts);
            }
        }
    }

    // Near rift indicator
    if (m_nearRiftIndex >= 0 && !m_activePuzzle) {
        TTF_Font* font = game->getFont();
        if (font) {
            SDL_Color c = {180, 130, 255, 200};
            SDL_Surface* s = TTF_RenderText_Blended(font, "Press F to repair rift", c);
            if (s) {
                SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
                if (t) {
                    SDL_Rect r = {640 - s->w / 2, 500, s->w, s->h};
                    SDL_RenderCopy(renderer, t, nullptr, &r);
                    SDL_DestroyTexture(t);
                }
                SDL_FreeSurface(s);
            }
        }
    }

    // Off-screen rift direction indicators
    if (m_level && !m_activePuzzle) {
        auto rifts = m_level->getRiftPositions();
        Vec2 camPos = m_camera.getPosition();
        float halfW = 640.0f, halfH = 360.0f;

        for (int i = 0; i < static_cast<int>(rifts.size()); i++) {
            float sx = (rifts[i].x - camPos.x) * m_camera.zoom + halfW;
            float sy = (rifts[i].y - camPos.y) * m_camera.zoom + halfH;

            // Only show if off-screen
            if (sx >= -10 && sx <= 1290 && sy >= -10 && sy <= 730) continue;

            // Clamp to screen edge with margin
            float cx = std::max(25.0f, std::min(1255.0f, sx));
            float cy = std::max(25.0f, std::min(695.0f, sy));

            float pulse = 0.5f + 0.5f * std::sin(SDL_GetTicks() * 0.005f + i * 1.5f);
            Uint8 pa = static_cast<Uint8>(120 + 80 * pulse);

            // Diamond indicator
            SDL_SetRenderDrawColor(renderer, 150, 80, 255, pa);
            SDL_Rect ind = {static_cast<int>(cx) - 5, static_cast<int>(cy) - 5, 10, 10};
            SDL_RenderFillRect(renderer, &ind);

            // Arrow pointing toward rift
            float angle = std::atan2(sy - cy, sx - cx);
            int ax = static_cast<int>(cx + std::cos(angle) * 14);
            int ay = static_cast<int>(cy + std::sin(angle) * 14);
            SDL_SetRenderDrawColor(renderer, 180, 120, 255, pa);
            SDL_RenderDrawLine(renderer, static_cast<int>(cx), static_cast<int>(cy), ax, ay);
        }
    }

    // Puzzle overlay
    if (m_activePuzzle && m_activePuzzle->isActive()) {
        m_activePuzzle->render(renderer, 1280, 720);
    }

    // Level complete message
    if (m_levelComplete) {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 120);
        SDL_Rect overlay = {0, 300, 1280, 120};
        SDL_RenderFillRect(renderer, &overlay);

        TTF_Font* font = game->getFont();
        if (font) {
            SDL_Color c = {100, 255, 100, 255};
            SDL_Surface* s = TTF_RenderText_Blended(font, "LEVEL COMPLETE - Entering next rift...", c);
            if (s) {
                SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
                if (t) {
                    SDL_Rect r = {640 - s->w / 2, 345, s->w, s->h};
                    SDL_RenderCopy(renderer, t, nullptr, &r);
                    SDL_DestroyTexture(t);
                }
                SDL_FreeSurface(s);
            }
        }
    }

    // Tutorial hints (first 3 levels only)
    renderTutorialHints(renderer, game->getFont());

    // Floating damage numbers
    renderDamageNumbers(renderer, game->getFont());

    // Boss HP bar at top of screen
    if (m_isBossLevel && !m_bossDefeated) {
        renderBossHealthBar(renderer, game->getFont());
    }

    // Damage flash overlay
    m_hud.renderFlash(renderer, 1280, 720);

    // HUD (on top of everything)
    m_hud.render(renderer, game->getFont(), m_player.get(), &m_entropy, &m_dimManager,
                 1280, 720, game->getFPS(), game->getUpgradeSystem().getRiftShards());

    // Minimap (bottom right corner)
    m_hud.renderMinimap(renderer, m_level.get(), m_player.get(), &m_dimManager, 1280, 720, &m_entities);
}

void PlayState::updateDamageNumbers(float dt) {
    for (auto& dn : m_damageNumbers) {
        dn.lifetime -= dt;
        dn.position.y -= 60.0f * dt; // Float upward
    }
    // Remove expired
    m_damageNumbers.erase(
        std::remove_if(m_damageNumbers.begin(), m_damageNumbers.end(),
            [](const FloatingDamageNumber& d) { return d.lifetime <= 0; }),
        m_damageNumbers.end());
}

void PlayState::updateSpawnWaves(float dt) {
    if (m_currentWave >= static_cast<int>(m_spawnWaves.size())) return;

    // Count alive enemies
    int aliveEnemies = 0;
    m_entities.forEach([&](Entity& e) {
        if (e.getTag().find("enemy") != std::string::npos) aliveEnemies++;
    });

    // Spawn next wave when: most enemies dead OR timer expired
    m_waveTimer -= dt;
    if (aliveEnemies <= 1 || m_waveTimer <= 0) {
        // Spawn next wave
        for (auto& sp : m_spawnWaves[m_currentWave]) {
            Enemy::createByType(m_entities, sp.enemyType, sp.position, sp.dimension);
        }
        m_currentWave++;
        m_waveTimer = m_waveDelay;

        // Warning SFX for new wave
        AudioManager::instance().play(SFX::CollapseWarning);
    }
}

void PlayState::renderDamageNumbers(SDL_Renderer* renderer, TTF_Font* font) {
    if (!font) return;

    for (auto& dn : m_damageNumbers) {
        float t = dn.lifetime / dn.maxLifetime; // 1.0 → 0.0
        Uint8 alpha = static_cast<Uint8>(255 * t);

        // Color: red for player damage, orange for crits, yellow for normal enemy damage
        SDL_Color color;
        if (dn.isPlayerDamage) {
            color = {255, 60, 40, alpha};
        } else if (dn.isCritical) {
            color = {255, 180, 40, alpha};
        } else {
            color = {255, 240, 100, alpha};
        }

        char buf[32];
        if (dn.isCritical) {
            std::snprintf(buf, sizeof(buf), "CRIT! %.0f", dn.value);
        } else {
            std::snprintf(buf, sizeof(buf), "%.0f", dn.value);
        }

        // Convert world position to screen position
        SDL_FRect worldRect = {dn.position.x - 10, dn.position.y - 8, 20, 16};
        SDL_Rect screenRect = m_camera.worldToScreen(worldRect);

        SDL_Surface* surface = TTF_RenderText_Blended(font, buf, color);
        if (surface) {
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
            if (texture) {
                // Scale up for crits and large damage
                float scale = 1.0f;
                if (dn.isCritical) {
                    scale = 1.5f;
                } else if (dn.value > 20) {
                    scale = 1.3f;
                }
                SDL_Rect dst = {screenRect.x - static_cast<int>(surface->w * scale) / 2,
                               screenRect.y,
                               static_cast<int>(surface->w * scale),
                               static_cast<int>(surface->h * scale)};
                SDL_SetTextureAlphaMod(texture, alpha);
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
    int pad = 4;
    SDL_Rect box = {x - pad, y - 2, surface->w + pad * 2, surface->h + 4};
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

void PlayState::renderTutorialHints(SDL_Renderer* renderer, TTF_Font* font) {
    if (!font || m_currentDifficulty > 3) return;

    // Context-based hint system: show hints when conditions are met
    const char* hint = nullptr;
    const char* keyLabel = nullptr;
    bool conditionMet = false; // True = hint should be dismissed

    if (m_currentDifficulty == 1) {
        if (!m_tutorialHintDone[0]) {
            // Show movement hint after 1.5s of inactivity
            if (m_tutorialTimer > 1.5f && !m_hasMovedThisRun) {
                hint = "to move";
                keyLabel = "WASD";
            }
            if (m_hasMovedThisRun) conditionMet = true;
        } else if (!m_tutorialHintDone[1]) {
            hint = "to jump";
            keyLabel = "SPACE";
            if (m_hasJumpedThisRun) conditionMet = true;
        } else if (!m_tutorialHintDone[2]) {
            hint = "to dash through danger";
            keyLabel = "SHIFT";
            if (m_hasDashedThisRun) conditionMet = true;
        } else if (!m_tutorialHintDone[3]) {
            // Show attack hint when any enemy is nearby
            bool enemyNear = false;
            if (m_player) {
                Vec2 pp = m_player->getEntity()->getComponent<TransformComponent>().getCenter();
                m_entities.forEach([&](Entity& e) {
                    if (e.getTag().find("enemy") != std::string::npos &&
                        e.hasComponent<TransformComponent>()) {
                        Vec2 ep = e.getComponent<TransformComponent>().getCenter();
                        float d = std::sqrt((pp.x-ep.x)*(pp.x-ep.x) + (pp.y-ep.y)*(pp.y-ep.y));
                        if (d < 250.0f) enemyNear = true;
                    }
                });
            }
            if (enemyNear) {
                hint = "melee attack    ";
                keyLabel = "J";
            }
            if (m_hasAttackedThisRun) conditionMet = true;
        } else if (!m_tutorialHintDone[4]) {
            hint = "to switch dimensions - some walls only exist in one!";
            keyLabel = "E";
            if (m_dimManager.getSwitchCount() > 0) conditionMet = true;
        }
    } else if (m_currentDifficulty == 2) {
        if (!m_tutorialHintDone[5]) {
            // Show rift hint when near a rift
            if (m_nearRiftIndex >= 0) {
                hint = "to repair the rift";
                keyLabel = "F";
            } else if (m_tutorialTimer > 5.0f) {
                hint = "Find glowing rifts on the map";
                keyLabel = nullptr;
            }
            if (riftsRepaired > 0) conditionMet = true;
        } else if (!m_tutorialHintDone[6]) {
            if (m_entropy.getPercent() > 0.4f) {
                hint = "Watch your Entropy bar - too much and your suit crashes!";
                keyLabel = nullptr;
                m_tutorialHintShowTimer += 1.0f / 60.0f;
                if (m_tutorialHintShowTimer > 5.0f) conditionMet = true;
            }
        }
    } else if (m_currentDifficulty == 3) {
        if (!m_tutorialHintDone[7]) {
            hint = "Chain attacks for combos - more hits = more damage!";
            keyLabel = nullptr;
            if (m_player && m_player->getEntity()->hasComponent<CombatComponent>()) {
                if (m_player->getEntity()->getComponent<CombatComponent>().comboCount >= 3)
                    conditionMet = true;
            }
            m_tutorialHintShowTimer += 1.0f / 60.0f;
            if (m_tutorialHintShowTimer > 8.0f) conditionMet = true;
        }
    }

    // Mark completed hints
    if (conditionMet) {
        // Find current hint index and mark it
        for (int i = 0; i < 8; i++) {
            if (!m_tutorialHintDone[i]) {
                m_tutorialHintDone[i] = true;
                m_tutorialHintShowTimer = 0;
                break;
            }
        }
    }

    if (!hint) return;

    // Fade in over 0.3s
    m_tutorialHintShowTimer += 1.0f / 60.0f;
    float alpha = std::min(1.0f, m_tutorialHintShowTimer / 0.3f);
    Uint8 a = static_cast<Uint8>(alpha * 200);

    // Semi-transparent background
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, static_cast<Uint8>(a * 0.4f));
    SDL_Rect bg = {240, 140, 800, 36};
    SDL_RenderFillRect(renderer, &bg);

    int textX = 640;
    if (keyLabel) {
        // Render key box + hint text side by side
        SDL_Surface* hintSurf = TTF_RenderText_Blended(font, hint, {180, 220, 255, a});
        SDL_Surface* keySurf = TTF_RenderText_Blended(font, keyLabel, {255, 255, 255, 255});
        int totalW = (keySurf ? keySurf->w + 16 : 0) + (hintSurf ? hintSurf->w : 0);
        int startX = 640 - totalW / 2;

        if (keySurf) {
            renderKeyBox(renderer, font, keyLabel, startX, 148, a);
            startX += keySurf->w + 16;
            SDL_FreeSurface(keySurf);
        }
        if (hintSurf) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, hintSurf);
            if (tex) {
                SDL_SetTextureAlphaMod(tex, a);
                SDL_Rect dst = {startX, 148, hintSurf->w, hintSurf->h};
                SDL_RenderCopy(renderer, tex, nullptr, &dst);
                SDL_DestroyTexture(tex);
            }
            SDL_FreeSurface(hintSurf);
        }
    } else {
        // Just hint text centered
        SDL_Color color = {180, 220, 255, a};
        SDL_Surface* surface = TTF_RenderText_Blended(font, hint, color);
        if (surface) {
            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
            if (texture) {
                SDL_SetTextureAlphaMod(texture, a);
                SDL_Rect dst = {640 - surface->w / 2, 148, surface->w, surface->h};
                SDL_RenderCopy(renderer, texture, nullptr, &dst);
                SDL_DestroyTexture(texture);
            }
            SDL_FreeSurface(surface);
        }
    }
}

void PlayState::spawnBoss() {
    if (!m_level) return;

    // Spawn boss near the center of the level
    Vec2 exitPos = m_level->getExitPoint();
    Vec2 spawnPos = {exitPos.x - 200.0f, exitPos.y - 64.0f};

    int dim = m_dimManager.getCurrentDimension();
    int bossDiff = m_currentDifficulty;
    if (g_selectedDifficulty == GameDifficulty::Hard) bossDiff += 1;
    Enemy::createBoss(m_entities, spawnPos, dim, bossDiff);
}

void PlayState::renderBossHealthBar(SDL_Renderer* renderer, TTF_Font* font) {
    // Find boss entity
    Entity* boss = nullptr;
    m_entities.forEach([&](Entity& e) {
        if (e.getTag() == "enemy_boss") boss = &e;
    });
    if (!boss || !boss->hasComponent<HealthComponent>()) return;

    auto& hp = boss->getComponent<HealthComponent>();
    int bossPhase = 1;
    if (boss->hasComponent<AIComponent>()) {
        bossPhase = boss->getComponent<AIComponent>().bossPhase;
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // Bar background
    int barW = 400;
    int barH = 12;
    int barX = 640 - barW / 2;
    int barY = 20;

    // Dark frame
    SDL_SetRenderDrawColor(renderer, 20, 10, 30, 220);
    SDL_Rect frame = {barX - 2, barY - 2, barW + 4, barH + 4};
    SDL_RenderFillRect(renderer, &frame);

    // Background
    SDL_SetRenderDrawColor(renderer, 50, 25, 50, 200);
    SDL_Rect bg = {barX, barY, barW, barH};
    SDL_RenderFillRect(renderer, &bg);

    // HP fill (color changes per phase)
    float pct = hp.getPercent();
    int fillW = static_cast<int>(barW * pct);
    Uint8 r, g, b;
    switch (bossPhase) {
        case 1: r = 200; g = 50; b = 180; break;
        case 2: r = 220; g = 120; b = 40; break;
        case 3: r = 255; g = 40; b = 40; break;
        default: r = 200; g = 50; b = 180; break;
    }
    SDL_SetRenderDrawColor(renderer, r, g, b, 240);
    SDL_Rect fill = {barX, barY, fillW, barH};
    SDL_RenderFillRect(renderer, &fill);

    // Phase markers at 66% and 33%
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 60);
    SDL_RenderDrawLine(renderer, barX + barW * 2 / 3, barY, barX + barW * 2 / 3, barY + barH);
    SDL_RenderDrawLine(renderer, barX + barW / 3, barY, barX + barW / 3, barY + barH);

    // Boss name
    if (font) {
        const char* bossName = "RIFT GUARDIAN";
        SDL_Color tc = {220, 180, 255, 220};
        SDL_Surface* ts = TTF_RenderText_Blended(font, bossName, tc);
        if (ts) {
            SDL_Texture* tt = SDL_CreateTextureFromSurface(renderer, ts);
            if (tt) {
                SDL_Rect tr = {640 - ts->w / 2, barY + barH + 4, ts->w, ts->h};
                SDL_RenderCopy(renderer, tt, nullptr, &tr);
                SDL_DestroyTexture(tt);
            }
            SDL_FreeSurface(ts);
        }
    }
}

void PlayState::endRun() {
    auto& upgrades = game->getUpgradeSystem();
    upgrades.totalRuns++;
    upgrades.addRunRecord(roomsCleared, enemiesKilled, riftsRepaired,
                          shardsCollected, m_currentDifficulty);
    game->changeState(StateID::RunSummary);
}
