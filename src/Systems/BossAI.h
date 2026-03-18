#pragma once
// Common includes needed by all boss AI implementations.
// Each BossXxx.cpp implements one AISystem::updateXxx() member function.
// No new classes are introduced; this header simply gathers the shared
// dependencies so individual boss files stay concise.

#include "AISystem.h"
#include "Game/AscensionSystem.h"
#include "Components/AIComponent.h"
#include "Components/TransformComponent.h"
#include "Components/PhysicsBody.h"
#include "Components/CombatComponent.h"
#include "Components/SpriteComponent.h"
#include "Components/AnimationComponent.h"
#include "Components/HealthComponent.h"
#include "Components/ColliderComponent.h"
#include "Systems/ParticleSystem.h"
#include "Systems/CombatSystem.h"
#include "Core/AudioManager.h"
#include "Game/Level.h"
#include "Game/Player.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
