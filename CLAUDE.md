# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview
Collection of games built with C++17 and SDL2. Currently one active game: **Riftwalker** (roguelike platformer with dimension-shifting mechanics).

## Build Commands (Riftwalker)
```bash
cd Riftwalker/build
cmake --build . --config Release
start Release/Riftwalker.exe
```

After adding new source files, re-run CMake first:
```bash
cd Riftwalker/build
cmake ..
cmake --build . --config Release
```

Dependencies managed via **vcpkg** (SDL2, SDL2_image, SDL2_mixer, SDL2_ttf).

## Testing
No test framework. Manual testing via debug keys in PlayState:
- **F6** — Smoke test (auto-play bot that exercises movement, combat, dimension switching)
- **F3** — Debug overlay (entity counts, balance stats, performance)
- **F4** — Balance snapshot (writes stats to file for tuning)

Automated playtest system via CLI:
```bash
# Single playtest run
build/Release/Riftwalker.exe --playtest --playtest-runs=3 --playtest-class=voidwalker --playtest-profile=balanced

# Parallel playtests (all classes × all profiles)
python tools/run_playtests.py --quick
```
Profiles: balanced, aggressive, defensive, speedrun. Bot reaches Floor 31 (Victory) in ~1300s.

## Architecture (Riftwalker)

**ECS Pattern** — Components hold data, Systems hold logic, Entities combine them:
- `src/ECS/Component.h` — Base class with compile-time type-ID system (bitset-based, max 32 components)
- `src/ECS/Entity.h` — Template `addComponent<T>()`, `hasComponent<T>()`, `getComponent<T>()`
- `src/ECS/EntityManager.h` — Entity lifecycle and querying
- `src/Components/` — Pure data (PhysicsBody, CombatComponent, AIComponent, SpriteComponent, AnimationComponent, etc.)
- `src/Systems/` — Logic (PhysicsSystem, CollisionSystem, RenderSystem, AISystem, ParticleSystem, CombatSystem)

**Ownership Model** — Two-tier system ownership:
- `Game` (persistent across runs) owns: UpgradeSystem, AchievementSystem, RunBuffSystem, LoreSystem, state machine, font, window
- `PlayState` (per-run) owns: all ECS systems, EntityManager, Player, Level, Camera, DimensionManager, RelicSystem, and all per-run mechanics

**State Machine** — `Game` owns a `map<StateID, unique_ptr<GameState>>` plus a state stack. `changeState()` replaces the current state; `pushState()`/`popState()` for overlay states (e.g. Pause over Play). Screen transitions use fade in/out. 19 states defined in `StateID` enum (`src/States/GameState.h`).

**Game Loop** — Fixed timestep physics in `Game::run()`. Timer controls step rate; InputManager buffers press/release events, cleared via `clearPressedBuffers()` after each fixed step.

**Rendering** — Dual system: sprite-based rendering via `SpriteComponent`/`AnimationComponent` with PNG assets in `assets/textures/`, plus procedural fallback via `SDL_RenderFillRect`/`DrawLine` when textures aren't loaded. `SpriteConfig` (`src/Game/SpriteConfig.h`) sets up sprites and animations per entity type. Audio is fully procedural via `SoundGenerator` (sine waves, noise, sweeps).

**Enemy Factory** — `Enemy` class (`src/Game/Enemy.h`) uses static factory methods: `createWalker()`, `createFlyer()`, `createBoss()`, etc. Stats hardcoded in `Enemy.cpp`. Supports elemental variants, elite modifiers, and mini-boss promotion.

**Dimension System** — Entities have a `dimension` field: 0=both dimensions, 1=dimension A, 2=dimension B. `DimensionManager` handles switching; only entities matching the active dimension (or 0) are updated/rendered.

**Input System** — `InputManager` maps `Action` enums to `SDL_Scancode` via rebindable key bindings. Supports gamepad. `SuitEntropy` can inject input distortion.

**Key Game Systems** (all in `src/Game/`):
- `LevelGenerator` — Procedural level generation with spawn waves, 30-floor zone system
- `SuitEntropy` — Core mechanic (suit degradation affecting gameplay)
- `UpgradeSystem` — Persistent upgrades, serialized to `riftwalker_save.dat`
- `RelicSystem` / `RelicSynergy` — 38 relics with 19 synergy combos
- `WeaponSystem` — 3 melee + 4 ranged weapons with mastery tiers
- `ClassSystem` — 4 classes: Voidwalker, Berserker, Phantom, Technomancer
- `AscensionSystem` — NG+ tiers 1-5 with progressive difficulty
- `LoreSystem` — 12 discoverable lore fragments with in-game triggers
- `DimensionShiftBalance.h` — Per-floor rift counts, enemy scaling, zone multipliers
- `ChallengeMode`, `NPCSystem`, `Bestiary`, `DailyRun`

**Narrative Presentation** (added in `PlayState`):
- Boss intro title cards (2.5s cinematic freeze with name + subtitle)
- Zone transition banners (3s non-blocking overlay)
- Run start narrative (3.5s, different text for NG+)
- Credits screen (`CreditsState`)

**Save Files** (text-based, in working directory):
- `riftwalker_save.dat` — Persistent upgrades
- `riftwalker_achievements.dat` — Achievement progress
- `riftwalker_lore.dat` — Discovered lore
- `riftwalker_bindings.cfg` — Custom keybindings

## Rules
- New features always via ECS pattern (Component for data + System for logic)
- No heap allocations in game loops (use pools, stack allocation)
- No auto-save of game assets (can be large)
- CMake uses `file(GLOB_RECURSE)` — new .cpp/.h files are picked up after re-running cmake
