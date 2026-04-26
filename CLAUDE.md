# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview
Collection of games built with C++17 and SDL2. Currently one active game: **Riftwalker** (roguelike platformer with dimension-shifting mechanics).

## Current State (2026-04-18)
- **Content**: 12 weapons (12/12 counter-attacks), 4 classes (4/4 combo finishers), 38 relics, 25 synergies (13 relic-relic + 12 weapon-relic, fully localized), 6 bosses, 17 enemy types, NG+ tiers 0–10
- **Localization**: 963 EN + 1192 DE keys, ~162 gameplay tips (EN+DE)
- **Quality**: 0 compiler warnings, visual regression 17/17 PASS, 41 bug patterns documented in `.claude/rules/bug-patterns.md`
- **Codebase**: ~196 files, ~63K LOC, ECS architecture, 2560×1440 logical resolution

**History**: Full session log in `docs/HISTORY.md` (2026-03-28 → 2026-04-18).

**Latest session (2026-04-18, 4 commits, perf-focused):**
- **Render hotpath batching** — 4 targeted optimizations targeting redundant state changes in high-frequency render paths:
  1. RenderSystem: Skip rim light entirely for non-elemental non-elite enemies (rim alpha 18% barely visible at gameplay zoom). NeonCity window SetColor hoisted out of nested loop.
  2. Boss-effects forEach merged (2 entity scans → 1).
  3. HUD minimap: Enemy dots batched under single SetRenderDrawColor (was per-entity).
  4. Level floor pass: Empty-tile grid lines batched under one SetColor (~1000+ state changes/frame saved).
  5. Solid tile gradient: 4 bands → 2 (visually equivalent at gameplay zoom, halves gradient cost per tile, ~400 fewer SetColor+FillRect/frame).
- **Trade-offs (intentional)**: (1) Normal enemies no longer have rim glow (only elites/bosses/elemental), (2) Solid tile gradient slightly less smooth.
- **Cumulative effect**: Meaningful drop in state changes/frame across world, level, and HUD rendering. Visually identical in gameplay (zoom-normalized).

**Previous session (2026-04-12, 8 commits, 40+ fixes):**
- **Gameplay bugs phase** — 8 damage multiplier chain bypasses fixed: Ground Slam, 4 combo finishers, Phase Strike, Shock Trap, Rift Shield burst, GravityGauntlet counter; shared `computePlayerDamageMult()` helper; ItemDrop GlassCannon protection, Blacksmith multiplier persistence, boss enrage race (CombatSystem timesHit exclusion), VoidSovereign/EntropyIncarnate phase timer seeding, UpgradeSystem/AscensionSystem save clamps.
- **Visual polish passes 1–6** — Layout fixes (EndingState, Quest HUD, ClassSelect, Relic overflow), render state leaks (enemy enrage, hit-freeze), DE localization overflow (6 panels), major UI polish (pause transition, boss HP ghost, phase flash, relic pickup), enemy feedback (ability bursts, crit particles, elite slow-mo), visual state leaks, 7 missing enemy death FX, electric chain numbers, AncientWeapon reveal, audio semantics (5 SFX swaps), parry ring visual, quick retry [R], shop card wrap, music system wiring, buff labels, boss phase warnings.
- **Deferred**: Relic bar tooltips, Tutorial expansion (Synergy/Parry/Finisher hints), Colorblind mode.
- All builds clean, 0 compiler warnings. Session ready for production.

**Previous session (2026-04-11, 31 commits):**
- **Electric chain ranged parity**: Element 3 (Electric) 30% chain damage was deferred for ranged — now fires via `m_currentEntities->forEach` in projectile onTrigger lambda. Cost bounded (1x per hit, not per frame).
- **EchoStrike ranged fix**: `rollEchoStrike()` was melee-only (another early-return survivor). Added to projectile onTrigger with PiercingEcho synergy override (35% for RiftCrossbow).
- **3 new weapon-relic synergies** (all 12 weapons now have synergies): GravityThorns (GravityGauntlet + ThornMail, +5 impact DMG), PhantomGrapple (GrapplingHook + PhaseCloak, 2s invis on kill), PiercingEcho (RiftCrossbow + EchoStrike, 35% echo chance). 25 synergies total.
- **Smoke bot static leak (extended)**: Previous fix only reset 2 of 17 statics. Navigation vars (levelTimer, smokeCurrentTarget, stuckTimer, etc.) leaked between runs.
- **DimensionalEcho chain dim fix**: Self-caught regression — new electric chain used `nearby.dim != dimension` where `dimension=0` for DimensionalEcho, filtering out all dim-1/2 enemies.
- **Technomancer playtest bot**: Missing class label + relic scoring for class index 3.
- **5 gameplay tips (145 → 150)**, EN+DE localized. Credits: 22 → 25 synergies.
- **ClassLegacy lore trigger fix**: Triggered on discoveredCount >= 15 instead of checking all 4 class masteries. Now uses all_classes achievement.
- **Dead code cleanup**: Removed m_challengeNoHealing and m_challengeNoDimSwitch (declared, never read).
- **Phantom damage fix**: EchoStrike + GravityThorns now skip if target already dead from primary hit.
- **RNG consistency**: PiercingEcho synergy uses mt19937 via rollChance() instead of std::rand().
- **2 counter-attacks added** (GrapplingHook "Hookshot" + DimLauncher "Dimensional Shatter"): All 12 weapons now have unique parry counters.
- **MaxHPBoost buff overwrite**: +30 HP applied in applyRunBuffs, then overwritten by applyStatEffects resetting from baseMaxHP. Moved to applyUpgrades.
- **CritSurge buff overwrite**: +20% crit replaced achievement crit bonus instead of adding to it.
- **Bug scans all clean**: 5 parallel agents (early-return dead code, check-after-write, takeDamage bypass, SDL resource leaks, systems audit) — 0 false negatives on confirmed fixes.
- **153 gameplay tips** (EN+DE), 25 synergies (fully localized), 12/12 counter-attacks.
- **Synergy localization**: All 25 synergy names + descriptions now use LOC() keys (50 EN + 50 DE). HUD weapon panel renders localized.
- **DE credits fix**: Showed "22 Synergie-Kombos" instead of 25.
- **963 EN + 1192 DE** localization keys total (was 930/1159).
- **161 gameplay tips** (was 145), 8 boss strategy tips added.
- **ShieldAura melee O(n²) → O(1)**: Replaced nested forEach with per-frame cached `hasNearbyShieldAura` flag (already used by ranged path).
- **Temporal Weaver boss phase flicker fix**: Phase assigned every frame without guard — caused flutter at HP boundaries, all timers firing simultaneously on spawn, no transition FX. Added newPhase guard matching all 5 other bosses.
- **Playtest bot synergy-aware scoring**: Simulates adding relic to check if it completes any of 13 relic-only synergies.
- **Boss enrage field dead code**: All 6 bosses wrote `bossEnraged` (never read) instead of `isEnraged` (used by animation + combat speed). Bosses never showed enrage animation or got attack speed boost in late phases. Fixed all 6 + removed dead field.
- **FireAura elite burn bypassing dotDurationMult**: Direct `burnTimer =` assignment skipped `applyBurn()` which applies Elemental Slayer achievement bonus.
- **Technomancer combo finisher** "Overcharge Surge": 140px AoE, 1.5x ranged DMG, 1s stun. Was placeholder-only (particles, no damage). All 4 classes now have unique finishers.
- **Playtest bot** now evaluates all 25 synergies (13 relic-relic + 12 weapon-relic) for optimal relic selection.
- **38 bug patterns** documented in .claude/rules/bug-patterns.md. 162 gameplay tips.
- **Balance + Boss AI audits**: 9 parallel agent scans, damage formulas verified correct, Temporal Weaver phase bug + enrage field bug + FireAura bypass found and fixed.

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
- 18 Visual Checkpoints (including CaptureSplash at frame 120): Menu, ClassSelect, Gameplay, Debug, Pause, Upgrades, Achievements
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
- **UI Coordinates**: Logical resolution is 2560x1440. ALL pixel values in UI/HUD/overlay code must be designed for this resolution. Never use 720p values. When unsure, use `SCREEN_WIDTH/HEIGHT` relative values or multiply intended 720p value by 2.
- **New UI elements**: Always use SCREEN_WIDTH/HEIGHT relative positioning (percentages, division) instead of hardcoded pixel values where possible.
- New features always via ECS pattern (Component for data + System for logic)
- No heap allocations in game loops (use pools, stack allocation)
- No auto-save of game assets (can be large)
- CMake uses `file(GLOB_RECURSE)` — new .cpp/.h files are picked up after re-running cmake
