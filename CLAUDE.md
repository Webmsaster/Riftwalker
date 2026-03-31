# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview
Collection of games built with C++17 and SDL2. Currently one active game: **Riftwalker** (roguelike platformer with dimension-shifting mechanics).

**Recent Updates (2026-04-01 visual overhaul session):**
- Native display resolution detection (auto-detects 2560×1440, 4K, etc.)
- Physical window at native res, logical at 1280×720 via SDL_RenderSetLogicalSize
- Camera Zoom 2.5: better balance of sprite detail vs world visibility
- Anisotropic texture filtering (SDL hint "2") + per-texture SDL_SetTextureScaleMode(Linear)
- 4× Real-ESRGAN upscaled sprites with professional transparency cleanup (186 frames fixed)
- Post-processing system: vignette, color grading, ambient particles, bloom/glow
- NPC rendering overhaul: layered figures with glow auras, type-specific details (+507 lines)
- Event rendering overhaul: merchants=stalls, shrines=pedestals, anomalies=vortexes, etc.
- HUD upgrade: gradient backing panels, 3-band gradient bars, specular highlights, animated shimmer
- Font size 20px with light hinting for crisp text
- 3 new sprite tools: fix_sprites_pro.py, fix_backgrounds_aggressive.py, fix_grabcut.py

**Previous Updates (2026-03-29/30 refactoring sessions):**
- ChallengeMode persistence: best scores per challenge saved to riftwalker_challenges.dat
- AscensionSystem backup: .bak fallback on load, backup before save
- AISystem synergy pass optimized from O(n²) to O(n*s) with stack-allocated collection
- Vec2 passed by const ref in ~40 hot-path functions (AISystem, Camera, ParticleSystem, CombatSystem, PhysicsBody)
- Vector pre-allocation in RenderSystem (reserve 128) and CombatSystem (reserve 64/32)
- HUD::render() split into renderAbilityBar(), renderCombatOverlay(), renderWeaponPanel() (1053→540 lines)
- GameBalance.h: centralized reference for all 80+ tuned balance constants
- Uint8 overflow safety: clampU8() helper for HUD alpha/color calculations
- Fixed missing TransformComponent check in ChainThorns synergy (crash fix)
- PlayStateRenderOverlays split: combat UI + notifications extracted to PlayStateRenderCombatUI.cpp (882+446 lines)
- AISystem.cpp split: advanced enemies (Shielder-Mimic) extracted to AIEnemyAdvanced.cpp (920+923 lines)
- renderBackground() split: midground layers extracted to renderBackgroundMidground() (290+150 lines)
- Achievement tracking wired up: parry_master (20 parries), no_damage_boss, ranged_only (5 melee-free floors)
- PlayStateUpdate.cpp split: combat helpers extracted to PlayStateUpdateCombat.cpp (561+895 lines)
- Enemy.cpp split: boss/elite factories extracted to EnemyBoss.cpp (912+457 lines)
- 3 new elite modifiers: Phasing (intangible cycle), Regenerating (2 HP/s), Magnetic (projectile deflection)
- 3 new boss attack patterns: Rift Guardian shockwave, Void Wyrm venom spread, Temporal Weaver time bomb
- Credits screen expanded with comprehensive system listing (all 15+ major systems)

**Previous Updates (2026-03-28/29):**
- Added 6 new enemy types: Teleporter, Reflector, Leech, Swarmer, GravityWell, Mimic
- Regenerated all sprites (17 enemies, 6 bosses, player, pickups, tiles)
- Added dedicated Entropy Incarnate boss renderer + HUD name/colors
- Added bestiary preview renderers + silhouettes for all new enemies
- Fixed: Challenge modifiers were never applied (all challenges/mutators broken)
- Fixed: 5 weapons permanently locked (no unlock conditions)
- Fixed: NG+ progress capped at tier 5 (now supports all 10 tiers)
- Fixed: Ascension achievements used floor number instead of NG+ tier
- Added weapon unlock conditions: EntropyScythe, ChainWhip, GravityGauntlet, DimLauncher, RiftCrossbow
- Extended NG+ Select screen UI to show all 11 ascension tiers
- Added frustum culling for entity rendering
- Audio feedback for Reflector shield, GravityWell pull, Mimic reveal

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
- **F9 / F12** — Screenshot (saves to `screenshots/`)

Automated playtest system via CLI:
```bash
# Single playtest run
build/Release/Riftwalker.exe --playtest --playtest-runs=3 --playtest-class=voidwalker --playtest-profile=balanced

# Parallel playtests (all classes × all profiles)
python tools/run_playtests.py --quick

# Screenshot mode (auto-capture + exit)
build/Release/Riftwalker.exe --screenshot
```
Profiles: balanced, aggressive, defensive, speedrun. Bot reaches Floor 31 (Victory) in ~1300s.

**Visual Regression Testing** (implemented):
- `python tools/visual_report.py` — Generate HTML diff reports (Reference / Actual / Diff)
- Playtest-Bot auto-captures floor screenshots (1, 5, 10, 15, 20, 25, 30)
- 7 Visual Checkpoints: Menu, ClassSelect, Gameplay, Debug, Pause, Upgrades, Achievements
- pixelmatch-cpp17 für Pixel-Diffs, CI-Pipeline mit GitHub Actions

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

**State Machine** — `Game` owns a `map<StateID, unique_ptr<GameState>>` plus a state stack. `changeState()` replaces the current state; `pushState()`/`popState()` for overlay states (e.g. Pause over Play). Screen transitions use fade in/out. 21 states defined in `StateID` enum (`src/States/GameState.h`).

**Game Loop** — Fixed timestep physics in `Game::run()`. Timer controls step rate; InputManager buffers press/release events, cleared via `clearPressedBuffers()` after each fixed step.

**Rendering** — Dual system: sprite-based rendering via `SpriteComponent`/`AnimationComponent` with PNG assets in `assets/textures/`, plus procedural fallback via `SDL_RenderFillRect`/`DrawLine` when textures aren't loaded. `SpriteConfig` (`src/Game/SpriteConfig.h`) sets up sprites and animations per entity type. Audio is fully procedural via `SoundGenerator` (sine waves, noise, sweeps).

**Enemy Factory** — `Enemy` class (`src/Game/Enemy.h`) uses static factory methods: `createWalker()`, `createFlyer()`, `createBoss()`, etc. 16 regular enemy types + Boss type. Stats hardcoded in `Enemy.cpp`. Supports elemental variants, elite modifiers, and mini-boss promotion.

**Dimension System** — Entities have a `dimension` field: 0=both dimensions, 1=dimension A, 2=dimension B. `DimensionManager` handles switching; only entities matching the active dimension (or 0) are updated/rendered.

**Input System** — `InputManager` maps `Action` enums to `SDL_Scancode` via rebindable key bindings. Supports gamepad. `SuitEntropy` can inject input distortion.

**Key Game Systems** (all in `src/Game/`):
- `LevelGenerator` — Procedural level generation with spawn waves, 30-floor zone system
- `SuitEntropy` — Core mechanic (suit degradation affecting gameplay)
- `UpgradeSystem` — Persistent upgrades, serialized to `riftwalker_save.dat`
- `RelicSystem` / `RelicSynergy` — 38 relics with 22 synergy combos (13 relic-only + 9 weapon-relic)
- `WeaponSystem` — 6 melee + 6 ranged weapons with mastery tiers
- `ClassSystem` — 4 classes: Voidwalker, Berserker, Phantom, Technomancer
- `AscensionSystem` — NG+ tiers 0-10 with progressive difficulty
- `LoreSystem` — 20 discoverable lore fragments with in-game triggers
- `DimensionShiftBalance.h` — Per-floor rift counts, enemy scaling, zone multipliers
- `ChallengeMode` — 5 challenges + 6 mutators, persistent best scores (riftwalker_challenges.dat)
- `NPCSystem`, `Bestiary`, `DailyRun`
- `GameBalance.h` — Centralized reference for all 80+ tuned balance constants

**Narrative Presentation** (added in `PlayState`):
- Boss intro title cards (2.5s cinematic freeze with name + subtitle)
- Zone transition banners (3s non-blocking overlay)
- Run start narrative (3.5s, different text for NG+)
- Credits screen (`CreditsState`)

**Save Files** (text-based, in working directory):
- `riftwalker_save.dat` — Persistent upgrades, weapon unlocks, class unlocks, run history
- `riftwalker_achievements.dat` — Achievement progress (42 achievements)
- `riftwalker_lore.dat` — Discovered lore (20 fragments)
- `riftwalker_bindings.cfg` — Custom keybindings
- `riftwalker_settings.cfg` — Audio/visual settings
- `bestiary_save.dat` — Enemy discovery & kill counts
- `ascension_save.dat` — NG+ tier and Rift Cores (with .bak backup)
- `riftwalker_challenges.dat` — Challenge mode best scores (with .bak backup)

## Build & Asset Tools
- `tools/fix_transparency.py` — Removes white backgrounds from sprite PNGs (batch processing)
- `tools/upscale_sprites_v2.py` — Upscales sprites 4× using Real-ESRGAN (GPU-accelerated)

## Rules
- New features always via ECS pattern (Component for data + System for logic)
- No heap allocations in game loops (use pools, stack allocation)
- No auto-save of game assets (can be large)
- CMake uses `file(GLOB_RECURSE)` — new .cpp/.h files are picked up after re-running cmake
