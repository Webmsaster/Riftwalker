#include "BossAI.h"

void AISystem::updateDimensionalArchitect(Entity& entity, float dt, const Vec2& playerPos, EntityManager& entities) {
    auto& ai = entity.getComponent<AIComponent>();
    auto& phys = entity.getComponent<PhysicsBody>();
    auto& transform = entity.getComponent<TransformComponent>();
    auto& hp = entity.getComponent<HealthComponent>();
    Vec2 pos = transform.getCenter();

    float dist = distanceTo(pos, playerPos);
    float hpPct = hp.getPercent();

    // Phase transitions
    bool extraPhase = (AscensionSystem::currentLevel > 0 &&
        AscensionSystem::getLevel(AscensionSystem::currentLevel).bossExtraPhase);
    int newPhase = (extraPhase && hpPct <= 0.15f) ? 4 :
                   (hpPct > 0.66f) ? 1 : (hpPct > 0.33f) ? 2 : 3;
    if (newPhase != ai.bossPhase) {
        ai.bossPhase = newPhase;
        ai.archSwapSize = (newPhase == 1) ? 3 : (newPhase == 2) ? 5 : (newPhase == 3) ? 7 : 9;
        // Reset timers so next attacks use new phase parameters
        ai.archSwapTimer = 1.5f;
        ai.archRiftTimer = 2.0f;
        ai.archConstructTimer = 1.0f;
        ai.archConstructing = false;
        if (m_particles) {
            m_particles->burst(pos, 30, {100, 180, 255, 220}, 200.0f, 6.0f);
        }
        AudioManager::instance().play(SFX::ArchCollapse);
    }

    // Hover animation (floating boss)
    ai.archHoverY += dt * 2.5f;
    if (ai.archHoverY > 6.283185f) ai.archHoverY -= 6.283185f;
    float hoverOffset = std::sin(ai.archHoverY) * 8.0f;
    phys.velocity.y = hoverOffset * 3.0f;
    phys.onGround = false;
    phys.useGravity = false;

    // Facing
    ai.facingRight = playerPos.x > pos.x;

    // Beam angle rotation
    ai.archBeamAngle += dt * 1.5f * ai.bossPhase;
    if (ai.archBeamAngle > 6.283185f) ai.archBeamAngle -= 6.283185f;

    float phaseCooldownMult = 1.0f - (ai.bossPhase - 1) * 0.2f; // 1.0, 0.8, 0.6
    float telegraphTime = 0.6f - (ai.bossPhase - 1) * 0.1f; // 0.6, 0.5, 0.4

    // --- Dimensional Architect Telegraph Processing ---
    if (ai.bossTelegraphTimer > 0) {
        ai.bossTelegraphTimer -= dt;
        phys.velocity.x *= 0.8f;
        phys.velocity.y *= 0.8f;

        float telegraphMax = std::max(0.1f, 0.6f - (ai.bossPhase - 1) * 0.1f);
        float flashRate = 12.0f + (1.0f - ai.bossTelegraphTimer / telegraphMax) * 25.0f;
        float flash = std::sin(ai.bossTelegraphTimer * flashRate) * 0.5f + 0.5f;

        if (entity.hasComponent<SpriteComponent>()) {
            auto& sprite = entity.getComponent<SpriteComponent>();
            switch (ai.bossTelegraphAttack) {
                case 1: sprite.setColor(static_cast<Uint8>(80 + 80 * flash), static_cast<Uint8>(160 + 60 * flash), 255); break; // beam: bright blue
                case 2: sprite.setColor(static_cast<Uint8>(200 + 55 * flash), static_cast<Uint8>(60 * flash), static_cast<Uint8>(120 + 60 * flash)); break; // rift: red-magenta
                case 3: sprite.setColor(static_cast<Uint8>(180 + 75 * flash), static_cast<Uint8>(100 * flash), 255); break; // collapse: deep purple
                default: break;
            }
        }

        if (m_particles) {
            SDL_Color warnColor;
            switch (ai.bossTelegraphAttack) {
                case 1: warnColor = {100, 180, 255, 200}; break;
                case 2: warnColor = {255, 80, 140, 200}; break;
                case 3: warnColor = {200, 120, 255, 200}; break;
                default: warnColor = {255, 255, 255, 200};
            }
            for (int i = 0; i < 2; i++) {
                float angle = (float)(rand() % 628) / 100.0f;
                Vec2 offset = {std::cos(angle) * 35.0f, std::sin(angle) * 35.0f};
                m_particles->burst(pos + offset, 1, warnColor, 60.0f, 3.0f);
            }
        }

        if (ai.bossTelegraphTimer <= 0) {
            switch (ai.bossTelegraphAttack) {
                case 1: {
                    // Construct Beam - execute
                    float damage = entity.getComponent<CombatComponent>().rangedAttack.damage;
                    Vec2 dir = ai.bossTelegraphDir;
                    auto& proj = entities.addEntity("projectile");
                    proj.dimension = entity.dimension;
                    proj.addComponent<TransformComponent>(pos.x - 6, pos.y - 6, 12, 12);
                    auto& ps = proj.addComponent<SpriteComponent>();
                    ps.setColor(80, 160, 255);
                    ps.renderLayer = 5;
                    auto& pp = proj.addComponent<PhysicsBody>();
                    pp.useGravity = false;
                    pp.velocity = dir * 220.0f;
                    auto& pc = proj.addComponent<ColliderComponent>();
                    pc.width = 10; pc.height = 10;
                    pc.layer = LAYER_PROJECTILE;
                    pc.mask = LAYER_TILE | LAYER_PLAYER;
                    pc.type = ColliderType::Trigger;
                    auto* cs = m_combatSystem;
                    pc.onTrigger = [damage, cs](Entity* self, Entity* other) {
                        if (other->hasComponent<HealthComponent>()) {
                            auto& hp = other->getComponent<HealthComponent>();
                            if (!hp.isInvincible()) {
                                hp.takeDamage(damage);
                                if (cs && other->hasComponent<TransformComponent>()) {
                                    bool isPlayer = (other->isPlayer);
                                    Vec2 srcPos = self->hasComponent<TransformComponent>()
                                        ? self->getComponent<TransformComponent>().getCenter() : Vec2{0, 0};
                                    cs->addDamageEvent(other->getComponent<TransformComponent>().getCenter(), damage, isPlayer, false, false, srcPos);
                                }
                            }
                        }
                        self->destroy();
                    };
                    AudioManager::instance().play(SFX::ArchBeam);
                    ai.archConstructTimer = 3.0f * phaseCooldownMult;

                    // Phase 3: Construct Storm extra projectiles alongside beam
                    if (ai.bossPhase >= 3) {
                        float stormDmg = damage * 0.6f;
                        for (int si = -1; si <= 1; si += 2) {
                            float sAngle = std::atan2(dir.y, dir.x) + si * 0.4f;
                            Vec2 sDir = {std::cos(sAngle), std::sin(sAngle)};
                            auto& sproj = entities.addEntity("projectile");
                            sproj.dimension = entity.dimension;
                            sproj.addComponent<TransformComponent>(pos.x - 5, pos.y - 5, 10, 10);
                            auto& sps = sproj.addComponent<SpriteComponent>();
                            sps.setColor(150, 100, 255);
                            sps.renderLayer = 5;
                            auto& spp = sproj.addComponent<PhysicsBody>();
                            spp.useGravity = false;
                            spp.velocity = sDir * 180.0f;
                            auto& spc = sproj.addComponent<ColliderComponent>();
                            spc.width = 8; spc.height = 8;
                            spc.layer = LAYER_PROJECTILE;
                            spc.mask = LAYER_TILE | LAYER_PLAYER;
                            spc.type = ColliderType::Trigger;
                            spc.onTrigger = [stormDmg, cs](Entity* self, Entity* other) {
                                if (other->hasComponent<HealthComponent>()) {
                                    auto& hp = other->getComponent<HealthComponent>();
                                    if (!hp.isInvincible()) {
                                        hp.takeDamage(stormDmg);
                                        if (cs && other->hasComponent<TransformComponent>()) {
                                            bool isPlayer = (other->isPlayer);
                                            Vec2 srcPos = self->hasComponent<TransformComponent>()
                                                ? self->getComponent<TransformComponent>().getCenter() : Vec2{0, 0};
                                            cs->addDamageEvent(other->getComponent<TransformComponent>().getCenter(), stormDmg, isPlayer, false, false, srcPos);
                                        }
                                    }
                                }
                                self->destroy();
                            };
                        }
                        AudioManager::instance().play(SFX::ArchConstruct);
                    }
                    break;
                }
                case 2: {
                    // Rift Zones - execute
                    if (m_level) {
                        Vec2 targetPos = ai.bossTelegraphDir;
                        int ts = m_level->getTileSize();
                        int cx = static_cast<int>(targetPos.x) / ts;
                        int cy = static_cast<int>(targetPos.y) / ts + 2;
                        for (int dx = -1; dx <= 1; dx++) {
                            int tx = cx + dx;
                            int ty = cy;
                            if (!m_level->inBounds(tx, ty)) continue;
                            Tile spikeTile;
                            spikeTile.type = TileType::Spike;
                            spikeTile.color = {200, 60, 120, 255};
                            m_level->setTile(tx, ty, 1, spikeTile);
                            m_level->setTile(tx, ty, 2, spikeTile);
                        }
                    }
                    AudioManager::instance().play(SFX::ArchRiftOpen);
                    if (m_particles) {
                        m_particles->burst(ai.bossTelegraphDir + Vec2{0, 64.0f}, 20, {255, 60, 100, 200}, 100.0f, 5.0f);
                    }
                    ai.archRiftTimer = 6.0f * phaseCooldownMult;
                    break;
                }
                case 3: {
                    // Dimension Collapse - execute
                    if (m_level) {
                        int ts = m_level->getTileSize();
                        int bx = static_cast<int>(pos.x) / ts;
                        int by = static_cast<int>(pos.y) / ts;
                        for (int tx = bx - 10; tx <= bx + 10; tx++) {
                            for (int ty = by - 5; ty <= by + 5; ty++) {
                                if (!m_level->inBounds(tx, ty)) continue;
                                if (std::rand() % 3 == 0) continue;
                                Tile tileA = m_level->getTile(tx, ty, 1);
                                Tile tileB = m_level->getTile(tx, ty, 2);
                                m_level->setTile(tx, ty, 1, tileB);
                                m_level->setTile(tx, ty, 2, tileA);
                            }
                        }
                    }
                    AudioManager::instance().play(SFX::ArchCollapse);
                    if (m_particles) {
                        m_particles->burst(pos, 50, {180, 100, 255, 220}, 300.0f, 8.0f);
                    }
                    if (m_camera) {
                        m_camera->shake(12.0f, 0.6f);
                    }
                    ai.archCollapseTimer = 12.0f * phaseCooldownMult;
                    break;
                }
                default: break;
            }
            ai.bossTelegraphAttack = -1;
        }
        return; // Skip normal AI during telegraph
    }

    if (dist < ai.detectRange || hpPct < 1.0f) {
        // --- Tile Swap Attack (instant, no telegraph) ---
        ai.archSwapTimer -= dt;
        if (ai.archSwapTimer <= 0 && m_level) {
            int ts = m_level->getTileSize();
            int centerTX = static_cast<int>(playerPos.x) / ts;
            int centerTY = static_cast<int>(playerPos.y) / ts;
            int halfW = ai.archSwapSize / 2;
            int halfH = (ai.bossPhase >= 2) ? 2 : 1;

            for (int tx = centerTX - halfW; tx <= centerTX + halfW; tx++) {
                for (int ty = centerTY - halfH; ty <= centerTY + halfH; ty++) {
                    if (!m_level->inBounds(tx, ty)) continue;
                    Tile tileA = m_level->getTile(tx, ty, 1);
                    Tile tileB = m_level->getTile(tx, ty, 2);
                    m_level->setTile(tx, ty, 1, tileB);
                    m_level->setTile(tx, ty, 2, tileA);
                }
            }
            AudioManager::instance().play(SFX::ArchTileSwap);
            if (m_particles) {
                m_particles->burst({playerPos.x, playerPos.y - 20.0f}, 15, {100, 150, 255, 200}, 120.0f, 4.0f);
            }
            ai.archSwapTimer = 4.0f * phaseCooldownMult;
        }

        // --- Construct Beam (telegraphed) ---
        ai.archConstructTimer -= dt;
        if (ai.archConstructTimer <= 0 && dist < 350.0f && ai.bossTelegraphAttack < 0) {
            ai.bossTelegraphTimer = telegraphTime;
            ai.bossTelegraphAttack = 1;
            ai.bossTelegraphDir = (playerPos - pos).normalized();
            AudioManager::instance().play(SFX::SniperTelegraph);
        }

        // --- Dimensional Rift Zones (Phase 2+, telegraphed) ---
        if (ai.bossPhase >= 2) {
            ai.archRiftTimer -= dt;
            if (ai.archRiftTimer <= 0 && m_level && ai.bossTelegraphAttack < 0) {
                ai.bossTelegraphTimer = telegraphTime;
                ai.bossTelegraphAttack = 2;
                ai.bossTelegraphDir = playerPos;
                AudioManager::instance().play(SFX::CollapseWarning);
            }
        }

        // --- Dimension Collapse (Phase 3, telegraphed) ---
        if (ai.bossPhase >= 3) {
            ai.archCollapseTimer -= dt;
            if (ai.archCollapseTimer <= 0 && m_level && ai.bossTelegraphAttack < 0) {
                ai.bossTelegraphTimer = telegraphTime;
                ai.bossTelegraphAttack = 3;
                AudioManager::instance().play(SFX::CollapseWarning);
                if (m_camera) m_camera->shake(4.0f, 0.2f); // Pre-warning shake
            }
        }

        // Movement: slow hover toward player, maintaining distance
        float idealDist = 180.0f;
        if (dist > idealDist + 40.0f) {
            Vec2 dir = (playerPos - pos).normalized();
            phys.velocity.x = dir.x * ai.chaseSpeed;
        } else if (dist < idealDist - 40.0f) {
            Vec2 dir = (pos - playerPos).normalized();
            phys.velocity.x = dir.x * ai.chaseSpeed;
        } else {
            // Strafe slowly
            float strafeDir = std::sin(m_frameTicks * 0.002f) > 0 ? 1.0f : -1.0f;
            phys.velocity.x = strafeDir * ai.patrolSpeed * 0.5f;
        }
    } else {
        // Patrol: slow orbit
        ai.archBeamAngle += dt * 0.8f;
        phys.velocity.x = std::cos(ai.archBeamAngle) * ai.patrolSpeed;
    }

    // Visual color
    if (entity.hasComponent<SpriteComponent>()) {
        auto& sprite = entity.getComponent<SpriteComponent>();
        sprite.flipX = !ai.facingRight;
        float pulse = (std::sin(m_frameTicks * 0.005f * ai.bossPhase) + 1.0f) * 0.5f;
        switch (ai.bossPhase) {
            case 1: sprite.setColor(80, static_cast<Uint8>(150 + 40 * pulse), 255); break;
            case 2: sprite.setColor(static_cast<Uint8>(120 + 40 * pulse), 100, 255); break;
            case 3: sprite.setColor(static_cast<Uint8>(180 + 75 * pulse), static_cast<Uint8>(60 * pulse), 255); break;
            case 4: sprite.setColor(static_cast<Uint8>(220 + 35 * pulse), static_cast<Uint8>(40 * pulse), static_cast<Uint8>(200 + 55 * pulse)); break; // Bright magenta
        }
    }
}
