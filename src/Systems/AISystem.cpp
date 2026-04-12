#include "AISystem.h"
#include <tracy/Tracy.hpp>
#include "Components/AIComponent.h"
#include "Components/TransformComponent.h"
#include "Components/PhysicsBody.h"
#include "Components/CombatComponent.h"
#include "Components/SpriteComponent.h"
#include "Components/AnimationComponent.h"
#include "Components/HealthComponent.h"
#include "Components/ColliderComponent.h"
#include "Components/RelicComponent.h"
#include "Systems/ParticleSystem.h"
#include "Systems/CombatSystem.h"
#include "Core/AudioManager.h"
#include "Game/Level.h"
#include "Game/SpriteConfig.h"
#include "Game/Player.h"
#include "Game/RelicSystem.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>

void AISystem::attackWindupEffect(Entity& entity, float timer, float windupTime) {
    if (windupTime <= 0) return;
    if (!entity.hasComponent<TransformComponent>()) return;

    float progress = 1.0f - (timer / windupTime); // 0→1 as attack approaches

    // Color pulse: flash from base color toward bright red as attack nears
    if (entity.hasComponent<SpriteComponent>()) {
        auto& sprite = entity.getComponent<SpriteComponent>();
        auto& base = sprite.baseColor;
        float flash = std::sin(progress * 15.0f) * 0.5f + 0.5f;
        float blend = progress * flash; // 0→1, pulsing
        Uint8 r = static_cast<Uint8>(base.r + (255 - base.r) * blend);
        Uint8 g = static_cast<Uint8>(base.g * (1.0f - blend * 0.7f));
        Uint8 b = static_cast<Uint8>(base.b * (1.0f - blend * 0.7f));
        sprite.color = {r, g, b, base.a};
    }

    // Warning particles: red-orange sparks above enemy, intensifying.
    // At 2K, a 1-particle/1.5px burst was invisible — bumped to 3-particle/5px.
    if (m_particles && progress > 0.3f) {
        auto& t = entity.getComponent<TransformComponent>();
        Vec2 aboveHead = {t.getCenter().x, t.position.y - 6.0f};
        float offsetX = (static_cast<float>(std::rand() % 24) - 12.0f);
        aboveHead.x += offsetX;
        SDL_Color warnColor = {255, static_cast<Uint8>(80 + 100 * (1.0f - progress)), 40, 220};
        float size = 4.0f + progress * 3.0f;
        m_particles->burst(aboveHead, 3, warnColor, 30.0f + progress * 60.0f, size);
    }
}

void AISystem::updateEnemyAnimation(Entity& entity) {
    if (!entity.hasComponent<SpriteComponent>() || !entity.hasComponent<AnimationComponent>())
        return;
    auto& sprite = entity.getComponent<SpriteComponent>();
    if (!sprite.texture) return; // No sprite loaded, skip

    auto& ai = entity.getComponent<AIComponent>();
    auto& anim = entity.getComponent<AnimationComponent>();

    // Map AIState to animation name
    const char* animName = Anim::Idle;
    bool isBoss = (ai.enemyType == EnemyType::Boss);

    if (entity.hasComponent<HealthComponent>() && entity.getComponent<HealthComponent>().currentHP <= 0) {
        animName = Anim::Dead;
    } else if (ai.state == AIState::Stunned || ai.state == AIState::Juggled) {
        animName = Anim::Hurt;
    } else if (ai.state == AIState::Attack) {
        if (isBoss) {
            // Cycle through Attack1/2/3 based on boss attack pattern
            switch (ai.bossAttackPattern % 3) {
                case 0: animName = Anim::Attack1; break;
                case 1: animName = Anim::Attack2; break;
                case 2: animName = Anim::Attack3; break;
            }
        } else {
            animName = Anim::Attack;
        }
    } else if (ai.state == AIState::Chase) {
        animName = isBoss ? Anim::Move : Anim::Walk;
    } else if (ai.state == AIState::Patrol) {
        animName = isBoss ? Anim::Move : Anim::Walk;
    } else if (ai.state == AIState::Flee) {
        animName = isBoss ? Anim::Move : Anim::Walk;
    } else {
        animName = Anim::Idle;
    }

    // Boss-specific: hit flash triggers hurt, phase transition, enrage
    if (isBoss && entity.hasComponent<HealthComponent>()) {
        auto& hp = entity.getComponent<HealthComponent>();
        if (hp.invincibilityTimer > hp.invincibilityTime - 0.12f && hp.invincibilityTimer > 0) {
            animName = Anim::Hurt;
        }
        // Enrage animation (brief override on enrage transition)
        if (ai.isEnraged && ai.state == AIState::Idle) {
            animName = Anim::Enrage;
        }
    }

    anim.play(animName);
}

float AISystem::distanceTo(const Vec2& a, const Vec2& b) const {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

float AISystem::distanceToSq(const Vec2& a, const Vec2& b) const {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    return dx * dx + dy * dy;
}

void AISystem::update(EntityManager& entities, float dt, const Vec2& playerPos, int playerDimension) {
    ZoneScopedN("AIUpdate");
    m_frameTicks = SDL_GetTicks();
    auto ents = entities.getEntitiesWithComponent<AIComponent>();

    // --- Synergy pre-pass: Summoner buffs allies, Shielder protects Snipers/Turrets ---
    // O(n) collection pass + O(n*s) application (s = summoner/shielder count, typically 0-3)
    struct SynergySource { Vec2 pos; Entity* entity; };
    SynergySource summoners[8];   // Stack-allocated, max tracked
    SynergySource shielders[8];
    SynergySource sniperTurrets[16];
    int nSummoners = 0, nShielders = 0, nSniperTurrets = 0;

    for (auto* e : ents) {
        if (!e->hasComponent<TransformComponent>() || !e->hasComponent<AIComponent>()) continue;
        if (!e->isAlive()) continue;
        auto& ai = e->getComponent<AIComponent>();
        auto& t = e->getComponent<TransformComponent>();

        // Reset per-frame synergy flags BEFORE the pre-pass writes to them.
        // (Previously this reset was inside the main per-entity loop below,
        // which ran AFTER the pre-pass and silently wiped its results —
        // Summoner speed/damage buffs never applied to minions.)
        ai.synergySummonerBuff = false;
        ai.summonerBuffSpeedMult = 1.0f;
        ai.summonerBuffDamageMult = 1.0f;
        ai.synergyProtectTarget = nullptr;
        ai.hasNearbyShieldAura = false;

        if (ai.enemyType == EnemyType::Summoner && ai.state == AIState::Chase && nSummoners < 8)
            summoners[nSummoners++] = {t.getCenter(), e};
        if (ai.enemyType == EnemyType::Shielder && ai.state != AIState::Patrol && nShielders < 8)
            shielders[nShielders++] = {t.getCenter(), e};
        if ((ai.enemyType == EnemyType::Sniper || ai.enemyType == EnemyType::Turret) && nSniperTurrets < 16)
            sniperTurrets[nSniperTurrets++] = {t.getCenter(), e};
    }

    // Summoner buff: apply to all allies within 120px of any summoner
    if (nSummoners > 0) {
        for (auto* e : ents) {
            if (!e->isAlive() || !e->hasComponent<TransformComponent>() || !e->hasComponent<AIComponent>()) continue;
            auto& t = e->getComponent<TransformComponent>();
            Vec2 pos = t.getCenter();
            for (int i = 0; i < nSummoners; i++) {
                if (summoners[i].entity == e) continue;
                float dist = std::abs(pos.x - summoners[i].pos.x) + std::abs(pos.y - summoners[i].pos.y);
                if (dist < 120.0f) {
                    auto& oai = e->getComponent<AIComponent>();
                    oai.synergySummonerBuff = true;
                    oai.summonerBuffSpeedMult = 1.2f;
                    oai.summonerBuffDamageMult = 1.15f;
                    break; // One buff is enough
                }
            }
        }
    }

    // ShieldAura proximity: mark all enemies within 100px of a ShieldAura elite.
    // This cached flag is consumed by the projectile onTrigger lambda to apply
    // the -30% damage reduction without an expensive inner forEach at hit time.
    for (auto* e : ents) {
        if (!e->isAlive() || !e->hasComponent<AIComponent>()) continue;
        auto& ai = e->getComponent<AIComponent>();
        if (!ai.isElite || ai.eliteMod != EliteModifier::ShieldAura) continue;
        if (!e->hasComponent<TransformComponent>()) continue;
        Vec2 auraPos = e->getComponent<TransformComponent>().getCenter();
        for (auto* other : ents) {
            if (other == e || !other->isAlive()) continue;
            if (!other->isEnemy || !other->hasComponent<AIComponent>()) continue;
            if (!other->hasComponent<TransformComponent>()) continue;
            Vec2 oPos = other->getComponent<TransformComponent>().getCenter();
            float dist = std::abs(oPos.x - auraPos.x) + std::abs(oPos.y - auraPos.y);
            if (dist < 100.0f) {
                other->getComponent<AIComponent>().hasNearbyShieldAura = true;
            }
        }
    }

    // Shielder protect: each shielder finds nearest sniper/turret within 150px
    for (int s = 0; s < nShielders; s++) {
        float bestDist = 999999.0f;
        Entity* bestTarget = nullptr;
        for (int t = 0; t < nSniperTurrets; t++) {
            if (!sniperTurrets[t].entity->isAlive()) continue;
            float dist = std::abs(sniperTurrets[t].pos.x - shielders[s].pos.x)
                       + std::abs(sniperTurrets[t].pos.y - shielders[s].pos.y);
            if (dist < 150.0f && dist < bestDist) {
                bestDist = dist;
                bestTarget = sniperTurrets[t].entity;
            }
        }
        if (bestTarget) {
            shielders[s].entity->getComponent<AIComponent>().synergyProtectTarget = bestTarget;
        }
    }

    for (auto* e : ents) {
        if (!e->hasComponent<TransformComponent>()) continue;
        // Skip player-owned constructs (Technomancer turrets) — handled by PlayState
        if (e->isPlayerTurret) continue;
        auto& ai = e->getComponent<AIComponent>();

        // Spawn animation: skip AI and keep invulnerable while spawning in
        if (ai.spawnTimer > 0) {
            float prevTimer = ai.spawnTimer;
            ai.spawnTimer -= dt;
            if (e->hasComponent<HealthComponent>()) {
                e->getComponent<HealthComponent>().invincibilityTimer = 0.1f;
            }
            // Spawn complete: particle burst in enemy color
            if (ai.spawnTimer <= 0 && m_particles) {
                auto& t = e->getComponent<TransformComponent>();
                SDL_Color col = e->hasComponent<SpriteComponent>()
                    ? e->getComponent<SpriteComponent>().color
                    : SDL_Color{255, 255, 255, 255};
                m_particles->burst(t.getCenter(), 6, col, 80.0f, 2.5f);
            }
            continue;
        }

        // Update stun
        if (ai.state == AIState::Stunned) {
            ai.stunTimer -= dt;
            if (ai.stunTimer <= 0) ai.state = AIState::Patrol;
            continue;
        }

        // Update juggle state
        if (ai.state == AIState::Juggled) {
            ai.juggleTimer -= dt;
            bool landed = false;
            if (e->hasComponent<PhysicsBody>()) {
                landed = e->getComponent<PhysicsBody>().onGround;
            }
            if (ai.juggleTimer <= 0 || landed) {
                // End juggle: brief stun on landing
                ai.stun(0.3f);
                ai.juggleHitCount = 0;
            }
            continue;
        }

        // Elite modifier updates
        if (ai.isElite) {
            ai.eliteGlowTimer += dt;

            // Shielded: regenerate shield (timer starts at 5s)
            if (ai.eliteMod == EliteModifier::Shielded) {
                if (ai.eliteShieldRegenTimer <= 0 && ai.eliteShieldHP >= 30.0f) {
                    ai.eliteShieldRegenTimer = 5.0f; // Initialize timer on first frame
                }
                ai.eliteShieldRegenTimer -= dt;
                if (ai.eliteShieldRegenTimer <= 0 && ai.eliteShieldHP < 30.0f) {
                    ai.eliteShieldHP = 30.0f;
                    ai.eliteShieldRegenTimer = 5.0f;
                    if (m_particles) {
                        auto& t = e->getComponent<TransformComponent>();
                        m_particles->burst(t.getCenter(), 10, {80, 150, 255, 200}, 80.0f, 2.0f);
                    }
                }
            }

            // Teleporter: teleport behind player
            if (ai.eliteMod == EliteModifier::Teleporter && e->hasComponent<TransformComponent>()) {
                ai.eliteTeleportTimer -= dt;
                if (ai.eliteTeleportTimer <= 0) {
                    ai.eliteTeleportTimer = 3.0f;
                    auto& t = e->getComponent<TransformComponent>();
                    float dx = playerPos.x - t.getCenter().x;
                    float behindX = playerPos.x + (dx > 0 ? -60.0f : 60.0f);
                    if (m_particles) {
                        m_particles->burst(t.getCenter(), 12, {150, 80, 220, 255}, 120.0f, 2.0f);
                    }
                    t.position.x = behindX;
                    t.position.y = playerPos.y - t.height / 2;
                    t.prevPosition = t.position; // Sync for interpolation (no smear on teleport)
                    if (m_particles) {
                        m_particles->burst(t.getCenter(), 12, {150, 80, 220, 255}, 120.0f, 2.0f);
                    }
                }
            }

            // Vampiric: heal percentage of damage dealt is handled in CombatSystem

            // Fire Aura: burn player if within 100px
            if (ai.eliteMod == EliteModifier::FireAura && e->hasComponent<TransformComponent>()) {
                auto& t = e->getComponent<TransformComponent>();
                float dist = std::abs(t.getCenter().x - playerPos.x) + std::abs(t.getCenter().y - playerPos.y);
                if (dist < 100.0f && m_player && m_player->getEntity()->hasComponent<HealthComponent>()) {
                    auto& php = m_player->getEntity()->getComponent<HealthComponent>();
                    if (!php.isInvincible()) {
                        m_player->applyBurn(1.0f);
                    }
                }
                // Visual: fire particles around elite
                if (m_particles) {
                    float auraPhase = std::fmod(ai.eliteGlowTimer * 8.0f, 6.28f);
                    float ox = std::cos(auraPhase) * 20.0f;
                    float oy = std::sin(auraPhase) * 12.0f;
                    m_particles->burst({t.getCenter().x + ox, t.getCenter().y + oy}, 1,
                                        {255, 100, 30, 180}, 40.0f, 1.5f);
                }
            }

            // Heal Aura: heal nearby enemies 3 HP/s (checked every 0.5s to avoid O(n^2))
            if (ai.eliteMod == EliteModifier::HealAura && e->hasComponent<TransformComponent>()) {
                ai.healAuraTimer -= dt;
                if (ai.healAuraTimer <= 0) {
                    ai.healAuraTimer = 0.5f;
                    auto& t = e->getComponent<TransformComponent>();
                    Vec2 center = t.getCenter();
                    entities.forEach([&](Entity& other) {
                        if (&other == e || !other.isAlive()) return;
                        if (!other.isEnemy) return;
                        if (!other.hasComponent<TransformComponent>() || !other.hasComponent<HealthComponent>()) return;
                        auto& ot = other.getComponent<TransformComponent>();
                        float dist = std::abs(ot.getCenter().x - center.x) + std::abs(ot.getCenter().y - center.y);
                        if (dist < 120.0f) {
                            other.getComponent<HealthComponent>().heal(1.5f); // 3 HP/s * 0.5s interval
                        }
                    });
                }
                // Visual: green heal particles
                if (m_particles) {
                    auto& t = e->getComponent<TransformComponent>();
                    Vec2 center = t.getCenter();
                    float auraPhase = std::fmod(ai.eliteGlowTimer * 5.0f, 6.28f);
                    float ox = std::cos(auraPhase) * 22.0f;
                    float oy = std::sin(auraPhase) * 14.0f;
                    m_particles->burst({center.x + ox, center.y + oy}, 1,
                                        {60, 230, 80, 160}, 30.0f, 1.5f);
                }
            }

            // Shield Aura: mark handled in CombatSystem (damage reduction for nearby enemies)
            // Visual particles only here
            if (ai.eliteMod == EliteModifier::ShieldAura && e->hasComponent<TransformComponent>() && m_particles) {
                auto& t = e->getComponent<TransformComponent>();
                float auraPhase = std::fmod(ai.eliteGlowTimer * 6.0f, 6.28f);
                float ox = std::cos(auraPhase) * 18.0f;
                float oy = std::sin(auraPhase) * 10.0f;
                m_particles->burst({t.getCenter().x + ox, t.getCenter().y + oy}, 1,
                                    {60, 200, 255, 140}, 35.0f, 1.5f);
            }

            // Phasing: cycle between tangible (4s) and intangible (1.5s)
            if (ai.eliteMod == EliteModifier::Phasing) {
                ai.elitePhasingTimer += dt;
                if (ai.elitePhasingTimer >= 5.5f) ai.elitePhasingTimer = 0;
                bool wasPhasing = ai.elitePhasing;
                ai.elitePhasing = (ai.elitePhasingTimer >= 4.0f);
                // Make invincible while phasing
                if (ai.elitePhasing && e->hasComponent<HealthComponent>()) {
                    e->getComponent<HealthComponent>().invincibilityTimer = 0.1f;
                }
                // Visual: ghost particles while phasing
                if (ai.elitePhasing && m_particles && e->hasComponent<TransformComponent>()) {
                    auto& t = e->getComponent<TransformComponent>();
                    m_particles->burst(t.getCenter(), 1, {220, 220, 255, 100}, 30.0f, 2.0f);
                }
                // Flash on phase transition
                if (ai.elitePhasing != wasPhasing && m_particles && e->hasComponent<TransformComponent>()) {
                    auto& t = e->getComponent<TransformComponent>();
                    m_particles->burst(t.getCenter(), 8, {255, 255, 255, 180}, 100.0f, 2.5f);
                }
                // Dim sprite when phasing
                if (e->hasComponent<SpriteComponent>()) {
                    auto& sprite = e->getComponent<SpriteComponent>();
                    sprite.color.a = ai.elitePhasing ? 80 : 255;
                }
            }

            // Regenerating: heal 2 HP/s
            if (ai.eliteMod == EliteModifier::Regenerating && e->hasComponent<HealthComponent>()) {
                auto& hp = e->getComponent<HealthComponent>();
                if (hp.currentHP > 0 && hp.currentHP < hp.maxHP) {
                    hp.currentHP = std::min(hp.maxHP, hp.currentHP + 2.0f * dt);
                    // Green pulse particles every ~0.5s
                    if (m_particles && e->hasComponent<TransformComponent>()) {
                        if (static_cast<int>(ai.eliteGlowTimer * 2) != static_cast<int>((ai.eliteGlowTimer - dt) * 2)) {
                            auto& t = e->getComponent<TransformComponent>();
                            m_particles->burst(t.getCenter(), 3, {50, 255, 80, 160}, 50.0f, 2.0f);
                        }
                    }
                }
            }

            // Magnetic: handled in CombatSystem (projectile deflection)
            // Visual: magenta ring particles
            if (ai.eliteMod == EliteModifier::Magnetic && m_particles && e->hasComponent<TransformComponent>()) {
                auto& t = e->getComponent<TransformComponent>();
                float auraPhase = std::fmod(ai.eliteGlowTimer * 4.0f, 6.28f);
                float ox = std::cos(auraPhase) * 25.0f;
                float oy = std::sin(auraPhase) * 15.0f;
                m_particles->burst({t.getCenter().x + ox, t.getCenter().y + oy}, 1,
                                    {220, 80, 255, 130}, 25.0f, 1.5f);
            }
        }

        // --- Elemental aura particles (Fire/Ice/Electric variants) ---
        if (ai.element != EnemyElement::None && m_particles && e->hasComponent<TransformComponent>()) {
            auto& t = e->getComponent<TransformComponent>();
            // Emit ~1-2 particles per second, staggered by entity position hash
            int hash = static_cast<int>(t.position.x * 11 + t.position.y * 17) & 0xFFF;
            Uint32 interval = 600; // ~1.7 particles per second
            if (((m_frameTicks + hash) % interval) < static_cast<Uint32>(dt * 1000)) {
                Vec2 center = t.getCenter();
                float ox = static_cast<float>((std::rand() % 20) - 10);
                float oy = static_cast<float>((std::rand() % 16) - 8);

                switch (ai.element) {
                    case EnemyElement::Fire: {
                        // Orange/red particles drifting upward
                        Uint8 g = static_cast<Uint8>(60 + std::rand() % 80); // 60-140
                        SDL_Color fireColor = {255, g, 20, 160};
                        m_particles->directionalBurst(
                            {center.x + ox, center.y + oy}, 1, fireColor,
                            270.0f, 40.0f, 25.0f, 2.0f); // upward
                        break;
                    }
                    case EnemyElement::Ice: {
                        // Cyan/white particles drifting downward
                        Uint8 r = static_cast<Uint8>(150 + std::rand() % 106); // 150-255
                        SDL_Color iceColor = {r, 230, 255, 140};
                        m_particles->directionalBurst(
                            {center.x + ox, center.y + oy}, 1, iceColor,
                            90.0f, 50.0f, 15.0f, 1.8f); // downward
                        break;
                    }
                    case EnemyElement::Electric: {
                        // Yellow spark particles in random directions
                        SDL_Color sparkColor = {255, 255, 60, 180};
                        float randomDir = static_cast<float>(std::rand() % 360);
                        m_particles->directionalBurst(
                            {center.x + ox, center.y + oy}, 1, sparkColor,
                            randomDir, 30.0f, 50.0f, 1.5f); // random burst
                        break;
                    }
                    default: break;
                }
            }
        }

        // Synergy flags (synergySummonerBuff, summonerBuffSpeedMult, etc.)
        // are reset + populated above in the pre-pass — do NOT reset here,
        // or the Summoner buff will be silently wiped before it applies.

        // --- Enrage: after being hit 5 times, speed up attacks ---
        if (ai.timesHit >= 5 && !ai.isEnraged) {
            ai.isEnraged = true;
        }

        // --- Dodge timer decay ---
        if (ai.dodgeCooldown > 0) ai.dodgeCooldown -= dt;
        if (ai.dodgeTimer > 0) {
            ai.dodgeTimer -= dt;
            auto& phys2 = e->getComponent<PhysicsBody>();
            phys2.velocity.x = ai.dodgeDirection.x * 200.0f;
            if (ai.dodgeDirection.y < 0) phys2.velocity.y = ai.dodgeDirection.y * 200.0f;
        }

        // --- Dodge/React logic for certain enemy types ---
        if (m_player && ai.dodgeCooldown <= 0 && ai.dodgeTimer <= 0) {
            auto& t = e->getComponent<TransformComponent>();
            float distToPlayer = distanceTo(t.getCenter(), playerPos);

            // Walker/Shielder/Crawler: dodge sideways when player charges nearby
            if ((ai.enemyType == EnemyType::Walker || ai.enemyType == EnemyType::Shielder ||
                 ai.enemyType == EnemyType::Crawler) && m_player->isChargingAttack && distToPlayer < 100.0f) {
                float dodgeX = (playerPos.x > t.getCenter().x) ? -1.0f : 1.0f;
                ai.dodgeDirection = {dodgeX, 0};
                ai.dodgeTimer = 0.2f;
                ai.dodgeCooldown = 1.5f;
            }
            // Flyer: evade upward when player dashes nearby
            else if (ai.enemyType == EnemyType::Flyer && m_player->isDashing && distToPlayer < 120.0f) {
                ai.dodgeDirection = {0, -1.0f};
                ai.dodgeTimer = 0.2f;
                ai.dodgeCooldown = 1.5f;
            }
        }

        // Dimension behavior modifiers for dim-0 enemies
        if (e->dimension == 0) {
            if (playerDimension == 1) {
                // Dim A (blue/calm): enemies are calmer
                ai.dimSpeedMod = 0.75f;
                ai.dimDamageMod = 0.85f;
                ai.dimDetectMod = 1.3f;
            } else {
                // Dim B (red/hostile): enemies are aggressive
                ai.dimSpeedMod = 1.3f;
                ai.dimDamageMod = 1.25f;
                ai.dimDetectMod = 0.8f;
            }

            // Visual: periodic particle aura indicating dimension state
            if (m_particles && e->hasComponent<TransformComponent>()) {
                auto& t = e->getComponent<TransformComponent>();
                // Emit a small particle every ~2s (stagger by entity position)
                int hash = static_cast<int>(t.position.x * 7 + t.position.y * 13) & 0x7FF;
                if (((m_frameTicks + hash) % 2000) < static_cast<Uint32>(dt * 1000)) {
                    SDL_Color auraColor = (playerDimension == 1)
                        ? SDL_Color{80, 120, 220, 120}   // Blue aura (calm)
                        : SDL_Color{220, 80, 60, 120};   // Red aura (aggressive)
                    m_particles->burst(t.getCenter(), 3, auraColor, 30.0f, 0.8f);
                }
            }
        } else {
            ai.dimSpeedMod = 1.0f;
            ai.dimDamageMod = 1.0f;
            ai.dimDetectMod = 1.0f;
        }

        // Ice weapon freeze slow: 50% speed reduction while frozen
        if (ai.freezeTimer > 0) {
            ai.dimSpeedMod *= 0.5f;
        }

        // Apply summoner buff speed multiplier
        if (ai.synergySummonerBuff) {
            ai.dimSpeedMod *= ai.summonerBuffSpeedMult;
        }

        // Tick aggro alert timer
        if (ai.aggroAlertTimer > 0) ai.aggroAlertTimer -= dt;

        // Track previous state to detect aggro transitions
        AIState prevState = ai.state;

        // Check if enemy is in the same dimension as player
        bool sameDim = (e->dimension == 0 || e->dimension == playerDimension);

        switch (ai.enemyType) {
            case EnemyType::Walker:  updateWalker(*e, dt, sameDim ? playerPos : Vec2{-9999, -9999}); break;
            case EnemyType::Flyer:   updateFlyer(*e, dt, sameDim ? playerPos : Vec2{-9999, -9999}); break;
            case EnemyType::Turret:  updateTurret(*e, dt, sameDim ? playerPos : Vec2{-9999, -9999}); break;
            case EnemyType::Charger: updateCharger(*e, dt, sameDim ? playerPos : Vec2{-9999, -9999}); break;
            case EnemyType::Phaser:   updatePhaser(*e, dt, playerPos, playerDimension); break;
            case EnemyType::Exploder: updateExploder(*e, dt, sameDim ? playerPos : Vec2{-9999, -9999}, entities); break;
            case EnemyType::Shielder: updateShielder(*e, dt, sameDim ? playerPos : Vec2{-9999, -9999}); break;
            case EnemyType::Crawler:  updateCrawler(*e, dt, sameDim ? playerPos : Vec2{-9999, -9999}); break;
            case EnemyType::Summoner: updateSummoner(*e, dt, sameDim ? playerPos : Vec2{-9999, -9999}, entities); break;
            case EnemyType::Sniper:     updateSniper(*e, dt, sameDim ? playerPos : Vec2{-9999, -9999}, entities); break;
            case EnemyType::Teleporter: updateTeleporter(*e, dt, sameDim ? playerPos : Vec2{-9999, -9999}); break;
            case EnemyType::Reflector:  updateReflector(*e, dt, sameDim ? playerPos : Vec2{-9999, -9999}); break;
            case EnemyType::Leech:       updateLeech(*e, dt, sameDim ? playerPos : Vec2{-9999, -9999}); break;
            case EnemyType::Swarmer:     updateSwarmer(*e, dt, sameDim ? playerPos : Vec2{-9999, -9999}); break;
            case EnemyType::GravityWell: updateGravityWell(*e, dt, sameDim ? playerPos : Vec2{-9999, -9999}); break;
            case EnemyType::Mimic:       updateMimic(*e, dt, sameDim ? playerPos : Vec2{-9999, -9999}); break;
            case EnemyType::Boss: {
                int bt = e->getComponent<AIComponent>().bossType;
                if (bt == 5)
                    updateEntropyIncarnate(*e, dt, sameDim ? playerPos : Vec2{-9999, -9999}, entities);
                else if (bt == 4)
                    updateVoidSovereign(*e, dt, sameDim ? playerPos : Vec2{-9999, -9999}, entities);
                else if (bt == 3)
                    updateTemporalWeaver(*e, dt, sameDim ? playerPos : Vec2{-9999, -9999}, entities);
                else if (bt == 2)
                    updateDimensionalArchitect(*e, dt, sameDim ? playerPos : Vec2{-9999, -9999}, entities);
                else if (bt == 1)
                    updateVoidWyrm(*e, dt, sameDim ? playerPos : Vec2{-9999, -9999}, entities);
                else
                    updateBoss(*e, dt, sameDim ? playerPos : Vec2{-9999, -9999}, entities);
                break;
            }
        }

        // Trigger aggro alert when enemy first notices player
        if ((prevState == AIState::Idle || prevState == AIState::Patrol) &&
            (ai.state == AIState::Chase || ai.state == AIState::Attack)) {
            ai.aggroAlertTimer = 0.5f;
        }

        // Drive sprite animation based on AI state
        updateEnemyAnimation(*e);
    }
}

void AISystem::updateWalker(Entity& entity, float dt, const Vec2& playerPos) {
    auto& ai = entity.getComponent<AIComponent>();
    auto& transform = entity.getComponent<TransformComponent>();
    auto& phys = entity.getComponent<PhysicsBody>();
    Vec2 pos = transform.getCenter();
    float distSq = distanceToSq(pos, playerPos);
    float effectiveDetect = ai.detectRange * ai.dimDetectMod;
    float effectiveDetectSq = effectiveDetect * effectiveDetect;
    float effectiveChase = ai.chaseSpeed * ai.dimSpeedMod;

    switch (ai.state) {
        case AIState::Patrol: {
            Vec2 target = ai.patrolForward ? ai.patrolEnd : ai.patrolStart;
            float dirX = (target.x > pos.x) ? 1.0f : -1.0f;
            phys.velocity.x = dirX * ai.patrolSpeed * ai.dimSpeedMod;
            ai.facingRight = dirX > 0;

            if (std::abs(pos.x - target.x) < 5.0f) ai.patrolForward = !ai.patrolForward;
            if (distSq < effectiveDetectSq) ai.state = AIState::Chase;
            break;
        }
        case AIState::Chase: {
            float dirX = (playerPos.x > pos.x) ? 1.0f : -1.0f;
            phys.velocity.x = dirX * effectiveChase;
            ai.facingRight = dirX > 0;

            // Walker Lunge: in Chase when dist 40-80px, lunge forward with small jump
            if (ai.walkerLungeTimer > 0) ai.walkerLungeTimer -= dt;
            if (distSq >= 1600.0f && distSq <= 6400.0f && ai.walkerLungeTimer <= 0 && phys.onGround) {
                phys.velocity.x = dirX * 250.0f;
                phys.velocity.y = -120.0f; // small hop
                ai.walkerLungeTimer = 2.0f;
                if (m_particles) {
                    m_particles->burst(pos, 4, {200, 60, 60, 180}, 60.0f, 1.5f);
                }
            }

            float atkRangeSq = ai.attackRange * ai.attackRange;
            if (distSq < atkRangeSq) {
                ai.state = AIState::Attack;
                ai.attackTimer = ai.attackWindup; // wind-up before first attack
            }
            float loseRangeSq = ai.loseRange * ai.loseRange;
            if (distSq > loseRangeSq) ai.state = AIState::Patrol;
            break;
        }
        case AIState::Attack: {
            ai.attackTimer -= dt;

            // Visual wind-up telegraph: warning particles + color pulse before attack
            float effectiveWindup = ai.attackWindup * (ai.isEnraged ? 0.75f : 1.0f);
            if (ai.attackTimer > 0 && ai.attackTimer < effectiveWindup) {
                attackWindupEffect(entity, ai.attackTimer, effectiveWindup);
            }

            if (ai.attackTimer <= 0) {
                auto& combat = entity.getComponent<CombatComponent>();
                Vec2 dir = {ai.facingRight ? 1.0f : -1.0f, 0.0f};
                combat.startAttack(AttackType::Melee, dir);
                float cooldownMult = ai.isEnraged ? 0.7f : 1.0f;
                ai.attackTimer = ai.attackCooldown * cooldownMult / std::max(0.1f, ai.dimSpeedMod);
                if (entity.hasComponent<SpriteComponent>())
                    entity.getComponent<SpriteComponent>().restoreColor();
            }
            float atkLoseSq = ai.attackRange * 1.5f * ai.attackRange * 1.5f;
            if (distSq > atkLoseSq) ai.state = AIState::Chase;
            break;
        }
        default: break;
    }

    if (entity.hasComponent<SpriteComponent>()) {
        entity.getComponent<SpriteComponent>().flipX = !ai.facingRight;
    }
}

void AISystem::updateFlyer(Entity& entity, float dt, const Vec2& playerPos) {
    auto& ai = entity.getComponent<AIComponent>();
    auto& transform = entity.getComponent<TransformComponent>();
    auto& phys = entity.getComponent<PhysicsBody>();
    Vec2 pos = transform.getCenter();
    float distSq = distanceToSq(pos, playerPos);
    float effectiveDetect = ai.detectRange * ai.dimDetectMod;
    float detectSq = effectiveDetect * effectiveDetect;
    float effectiveChase = ai.chaseSpeed * ai.dimSpeedMod;

    switch (ai.state) {
        case AIState::Patrol: {
            Vec2 target = ai.patrolForward ? ai.patrolEnd : ai.patrolStart;
            Vec2 dir = (target - pos).normalized();
            phys.velocity = dir * ai.patrolSpeed * ai.dimSpeedMod;

            if (distanceToSq(pos, target) < 100.0f) ai.patrolForward = !ai.patrolForward;
            if (distSq < detectSq) {
                ai.state = AIState::Chase;
                ai.attackTimer = ai.attackCooldown * 0.5f; // brief hover before first swoop
            }
            break;
        }
        case AIState::Chase: {
            // Hover above player, then swoop
            Vec2 hoverTarget = {playerPos.x, playerPos.y - ai.flyHeight};
            Vec2 dir = (hoverTarget - pos).normalized();
            phys.velocity = dir * effectiveChase;

            ai.attackTimer -= dt;

            // Visual wind-up telegraph before swoop
            if (ai.attackTimer > 0 && ai.attackTimer < ai.attackWindup && distSq < detectSq) {
                attackWindupEffect(entity, ai.attackTimer, ai.attackWindup);
                phys.velocity = phys.velocity * 0.5f; // slow down while winding up
            }

            if (ai.attackTimer <= 0 && distSq < detectSq) {
                ai.state = AIState::Attack;
                ai.targetPosition = playerPos;
            }
            if (distSq > ai.loseRange * ai.loseRange) ai.state = AIState::Patrol;
            break;
        }
        case AIState::Attack: {
            // Swoop down toward player
            Vec2 dir = (ai.targetPosition - pos).normalized();
            phys.velocity = dir * ai.swoopSpeed * ai.dimSpeedMod;

            if (distanceToSq(pos, ai.targetPosition) < 400.0f || pos.y > ai.targetPosition.y + 30.0f) {
                ai.state = AIState::Chase;
                ai.attackTimer = ai.attackCooldown / std::max(0.1f, ai.dimSpeedMod);
                auto& combat = entity.getComponent<CombatComponent>();
                combat.startAttack(AttackType::Melee, dir);
                if (entity.hasComponent<SpriteComponent>())
                    entity.getComponent<SpriteComponent>().restoreColor();
            }
            break;
        }
        default: break;
    }
}

void AISystem::updateTurret(Entity& entity, float dt, const Vec2& playerPos) {
    auto& ai = entity.getComponent<AIComponent>();
    auto& transform = entity.getComponent<TransformComponent>();
    Vec2 pos = transform.getCenter();
    float detectR = ai.detectRange * ai.dimDetectMod;
    float distSq = distanceToSq(pos, playerPos);

    ai.facingRight = playerPos.x > pos.x;

    if (distSq < detectR * detectR) {
        ai.attackTimer -= dt;
        if (ai.attackTimer <= 0) {
            auto& combat = entity.getComponent<CombatComponent>();
            Vec2 dir = (playerPos - pos).normalized();
            combat.startAttack(AttackType::Ranged, dir);
            ai.attackTimer = ai.attackCooldown / std::max(0.1f, ai.dimSpeedMod);
        }
    }

    if (entity.hasComponent<SpriteComponent>()) {
        entity.getComponent<SpriteComponent>().flipX = !ai.facingRight;
    }
}

void AISystem::updateCharger(Entity& entity, float dt, const Vec2& playerPos) {
    auto& ai = entity.getComponent<AIComponent>();
    auto& transform = entity.getComponent<TransformComponent>();
    auto& phys = entity.getComponent<PhysicsBody>();
    Vec2 pos = transform.getCenter();
    float distSq = distanceToSq(pos, playerPos);
    float effectiveDetect = ai.detectRange * ai.dimDetectMod;

    switch (ai.state) {
        case AIState::Patrol: {
            Vec2 target = ai.patrolForward ? ai.patrolEnd : ai.patrolStart;
            float dirX = (target.x > pos.x) ? 1.0f : -1.0f;
            phys.velocity.x = dirX * ai.patrolSpeed * ai.dimSpeedMod;
            ai.facingRight = dirX > 0;

            if (std::abs(pos.x - target.x) < 5.0f) ai.patrolForward = !ai.patrolForward;
            if (distSq < effectiveDetect * effectiveDetect) {
                ai.state = AIState::Chase;
                ai.attackTimer = ai.chargeWindup / std::max(0.1f, ai.dimSpeedMod);
            }
            break;
        }
        case AIState::Chase: {
            // Wind up before charging
            phys.velocity.x *= 0.9f;
            ai.attackTimer -= dt;

            // Visual warning: flash
            float windupTime = ai.chargeWindup / std::max(0.1f, ai.dimSpeedMod);
            auto& sprite = entity.getComponent<SpriteComponent>();
            sprite.setColor(255, static_cast<Uint8>(120 + 135 * (ai.attackTimer / windupTime)), 40);

            if (ai.attackTimer <= 0) {
                ai.state = AIState::Attack;
                ai.attackTimer = ai.chargeTime;
                ai.facingRight = playerPos.x > pos.x;
            }
            if (distSq > ai.loseRange * ai.loseRange) ai.state = AIState::Patrol;
            break;
        }
        case AIState::Attack: {
            // CHARGE!
            phys.velocity.x = (ai.facingRight ? 1.0f : -1.0f) * ai.chargeSpeed * ai.dimSpeedMod;
            ai.attackTimer -= dt;

            auto& combat = entity.getComponent<CombatComponent>();
            if (!combat.isAttacking) {
                Vec2 dir = {ai.facingRight ? 1.0f : -1.0f, 0.0f};
                combat.startAttack(AttackType::Melee, dir);
            }

            if (ai.attackTimer <= 0) {
                ai.state = AIState::Stunned;
                ai.stunTimer = 1.0f;
                ai.chargerBounced = false;
                phys.velocity.x = 0;
                entity.getComponent<SpriteComponent>().setColor(220, 120, 40);
            } else if (phys.onWallLeft || phys.onWallRight) {
                // Charger Bounce: reverse at 50% speed for one bounce, then stun
                if (!ai.chargerBounced) {
                    ai.chargerBounced = true;
                    ai.facingRight = !ai.facingRight;
                    phys.velocity.x = (ai.facingRight ? 1.0f : -1.0f) * ai.chargeSpeed * 0.5f * ai.dimSpeedMod;
                    if (m_particles) {
                        auto& t2 = entity.getComponent<TransformComponent>();
                        m_particles->burst(t2.getCenter(), 8, {220, 120, 40, 200}, 80.0f, 2.0f);
                    }
                    if (m_camera) m_camera->shake(5.0f, 0.15f);
                    AudioManager::instance().play(SFX::GroundSlam);
                } else {
                    // Already bounced once — stun
                    ai.state = AIState::Stunned;
                    ai.stunTimer = 1.0f;
                    ai.chargerBounced = false;
                    phys.velocity.x = 0;
                    entity.getComponent<SpriteComponent>().setColor(220, 120, 40);
                }
            }
            break;
        }
        default: break;
    }

    if (entity.hasComponent<SpriteComponent>()) {
        entity.getComponent<SpriteComponent>().flipX = !ai.facingRight;
    }
}

void AISystem::updatePhaser(Entity& entity, float dt, const Vec2& playerPos, int playerDim) {
    auto& ai = entity.getComponent<AIComponent>();
    auto& transform = entity.getComponent<TransformComponent>();
    auto& phys = entity.getComponent<PhysicsBody>();
    Vec2 pos = transform.getCenter();

    bool sameDim = (entity.dimension == 0 || entity.dimension == playerDim);
    float distSq = distanceToSq(pos, playerPos);
    float detectSq = ai.detectRange * ai.detectRange;
    float atkSq = ai.attackRange * ai.attackRange;

    // Phase ability
    ai.phaseTimer -= dt;
    if (ai.phaseTimer <= 0 && !sameDim) {
        // Switch to player's dimension
        entity.dimension = playerDim;
        ai.phaseTimer = ai.phaseCooldown;
    } else if (ai.phaseTimer <= 0 && sameDim && distSq < detectSq) {
        // Randomly phase out to confuse player
        if (std::rand() % 100 < 30) {
            entity.dimension = (entity.dimension == 1) ? 2 : 1;
            ai.phaseTimer = ai.phaseCooldown * 0.5f;
        }
    }

    // Visual: phaser flickers between colors
    if (entity.hasComponent<SpriteComponent>()) {
        auto& sprite = entity.getComponent<SpriteComponent>();
        if ((m_frameTicks / 200) % 2 == 0) {
            sprite.setColor(100, 50, 200);
        } else {
            sprite.setColor(200, 50, 100);
        }
    }

    // Movement (similar to walker when in same dimension)
    if (sameDim) {
        if (distSq < detectSq) {
            float dirX = (playerPos.x > pos.x) ? 1.0f : -1.0f;
            phys.velocity.x = dirX * ai.chaseSpeed;
            ai.facingRight = dirX > 0;

            if (distSq < atkSq) {
                ai.attackTimer -= dt;

                // Visual wind-up telegraph
                if (ai.attackTimer > 0 && ai.attackTimer < ai.attackWindup) {
                    attackWindupEffect(entity, ai.attackTimer, ai.attackWindup);
                }

                if (ai.attackTimer <= 0) {
                    auto& combat = entity.getComponent<CombatComponent>();
                    Vec2 dir = {ai.facingRight ? 1.0f : -1.0f, 0.0f};
                    combat.startAttack(AttackType::Melee, dir);
                    ai.attackTimer = ai.attackCooldown;
                    // Phaser has its own flicker colors; don't restore here
                }
            } else {
                // Reset timer for next approach wind-up
                if (ai.attackTimer <= 0) ai.attackTimer = ai.attackWindup;
            }
        } else {
            // Patrol
            Vec2 target = ai.patrolForward ? ai.patrolEnd : ai.patrolStart;
            float dirX = (target.x > pos.x) ? 1.0f : -1.0f;
            phys.velocity.x = dirX * ai.patrolSpeed;
            if (std::abs(pos.x - target.x) < 5.0f) ai.patrolForward = !ai.patrolForward;
        }
    }
}

void AISystem::updateExploder(Entity& entity, float dt, const Vec2& playerPos, EntityManager& entities) {
    auto& ai = entity.getComponent<AIComponent>();
    auto& transform = entity.getComponent<TransformComponent>();
    auto& phys = entity.getComponent<PhysicsBody>();
    Vec2 pos = transform.getCenter();
    float distSq = distanceToSq(pos, playerPos);

    // Explode if HP depleted (killed by combat)
    if (entity.hasComponent<HealthComponent>()) {
        auto& hp = entity.getComponent<HealthComponent>();
        if (hp.currentHP <= 0 && !ai.exploded) {
            explode(entity, entities);
            return;
        }
    }

    float effectiveDetect = ai.detectRange * ai.dimDetectMod;
    float effectiveChase = ai.chaseSpeed * ai.dimSpeedMod;

    switch (ai.state) {
        case AIState::Patrol: {
            Vec2 target = ai.patrolForward ? ai.patrolEnd : ai.patrolStart;
            float dirX = (target.x > pos.x) ? 1.0f : -1.0f;
            phys.velocity.x = dirX * ai.patrolSpeed * ai.dimSpeedMod;
            ai.facingRight = dirX > 0;

            if (std::abs(pos.x - target.x) < 5.0f) ai.patrolForward = !ai.patrolForward;
            if (distSq < effectiveDetect * effectiveDetect) ai.state = AIState::Chase;
            break;
        }
        case AIState::Chase: {
            // Rush toward player at high speed
            float dirX = (playerPos.x > pos.x) ? 1.0f : -1.0f;
            phys.velocity.x = dirX * effectiveChase;
            ai.facingRight = dirX > 0;

            // Explode on contact
            if (distSq < ai.attackRange * ai.attackRange) {
                explode(entity, entities);
                return;
            }
            if (distSq > ai.loseRange * ai.loseRange) ai.state = AIState::Patrol;
            break;
        }
        default: break;
    }

    // Pulsing color when chasing
    if (entity.hasComponent<SpriteComponent>()) {
        auto& sprite = entity.getComponent<SpriteComponent>();
        sprite.flipX = !ai.facingRight;
        if (ai.state == AIState::Chase) {
            float pulse = (std::sin(m_frameTicks * 0.015f) + 1.0f) * 0.5f;
            sprite.setColor(255, static_cast<Uint8>(60 + 80 * pulse), 20);
        }
    }
}

void AISystem::explode(Entity& entity, EntityManager& entities) {
    auto& ai = entity.getComponent<AIComponent>();
    if (ai.exploded) return;
    ai.exploded = true;

    auto& transform = entity.getComponent<TransformComponent>();
    Vec2 center = transform.getCenter();
    int dim = entity.dimension;

    // AoE damage to all nearby entities
    entities.forEach([&](Entity& target) {
        if (&target == &entity) return;
        if (!target.hasComponent<HealthComponent>() || !target.hasComponent<TransformComponent>()) return;
        if (target.dimension != 0 && target.dimension != dim) return;

        Vec2 targetPos = target.getComponent<TransformComponent>().getCenter();
        float dist = distanceTo(center, targetPos);

        if (dist < ai.explodeRadius) {
            float falloff = 1.0f - (dist / std::max(1.0f, ai.explodeRadius));
            auto& hp = target.getComponent<HealthComponent>();
            if (!hp.isInvincible()) {
                float dmg = ai.explodeDamage * falloff;
                // Bug fix: apply defensive relic multiplier when exploder hits player
                if (target.isPlayer && target.hasComponent<RelicComponent>()) {
                    dmg *= RelicSystem::getDamageTakenMult(
                        target.getComponent<RelicComponent>(), target.dimension);
                }
                hp.takeDamage(dmg);
                if (m_combatSystem) {
                    bool isPlayer = (target.isPlayer);
                    m_combatSystem->addDamageEvent(targetPos, dmg, isPlayer, false, false, center);
                }
            }

            if (target.hasComponent<PhysicsBody>()) {
                Vec2 knockDir = (targetPos - center).normalized();
                knockDir.y = -0.5f;
                target.getComponent<PhysicsBody>().velocity += knockDir * 400.0f * falloff;
            }
        }
    });

    // Explosion particles
    if (m_particles) {
        m_particles->burst(center, 40, {255, 120, 30, 255}, 250.0f, 5.0f);
        m_particles->burst(center, 20, {255, 220, 50, 255}, 180.0f, 3.0f);
        m_particles->burst(center, 15, {255, 50, 20, 255}, 300.0f, 6.0f);
    }

    // Sharp burst shake for explosion — high intensity, short duration
    if (m_camera) {
        m_camera->shake(18.0f, 0.15f);
    }

    AudioManager::instance().play(SFX::EnemyDeath);
    entity.destroy();
}

