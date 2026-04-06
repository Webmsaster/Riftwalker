# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview
Collection of games built with C++17 and SDL2. Currently one active game: **Riftwalker** (roguelike platformer with dimension-shifting mechanics).

**Recent Updates (2026-04-06 massive localization + polish session):**
- **Complete Localization**: 408 EN + 408 DE string pairs — ALL player-facing UI strings use LOC() macro
  - 29 source files with 333 LOC() call sites, 989-line Localization.cpp
  - 19 tutorial hints, 30 gameplay tips (doubled from 15), 3 ending narratives (35 lines)
  - 6 boss names + 8 subtitles, 5 zone names, rift repair/shift prompts, exit hints
  - Death causes, NG+ tiers, element names, mastery tiers, relic tiers, wave cleared
  - Menu panels (daily run, career stats), HUD notifications, version string
  - Gamepad support added to CreditsState (was keyboard-only)
- **Bug Fixes**: weaponKills OOB in HUD.cpp, achievements clipRect+scroll 720p, event banner 720p
- **Mouse UX**: SDL_MOUSEWHEEL (7 states), hover (2 states), right-click back (8 states)
- **Audio UX**: Consistent SFX across all navigation (keyboard, gamepad, scroll) in all states
- **New Features**: Total playtime tracking (persisted), victory count in career stats
- **Credits Polish**: Particles 40→70, 4-color dimensional palette
- **.gitignore**: Added riftwalker_daily.dat + .bak files
- 40 commits, ~40 files changed, +1700 lines

**Previous Updates (2026-04-05 music quality + crossfade + localization session):**
- **Music Synth v2**: Proper MIDI note_on/note_off duration parsing (was guessing), stereo output with per-track panning, Schroeder reverb (4 comb + 2 allpass IIR), chorus for pads/strings, filter envelope modulation for bass/leads, band-limited saw waves
- **Music Crossfade**: `playMusic()` uses `Mix_FadeInMusic(500ms)` / `stopMusic()` uses `Mix_FadeOutMusic(300ms)` — no more hard cuts on zone/boss/state transitions
- **Gameplay Tips**: 15 rotating tips shown during level transition iris wipe (seeded by floor+run for consistency)
- **Localization**: 5 hardcoded HUD strings moved to LOC() (EXIT, LEVEL UP!, LORE DISCOVERED, CHAIN COMPLETE!, RIFT STABILIZED) — 103 EN + 103 DE keys total
- **Music Path Consolidation**: Zone/boss track paths as file-scope constants (was duplicated 3x)
- 3 commits, 10 files changed

**Previous Updates (2026-04-05 music + UI textures + release packaging session):**
- **Music System**: 7 OGG tracks (menu_theme, zone1/2/3, boss_theme, victory, gameover) with MIDI compositions + Python synth renderer
- **Zone-based Music Playback**: Music switches dynamically on zone transitions, boss spawns, boss kills, state changes (menu/gameover/victory/credits)
- **UI Texture System**: 11 textures (panel_dark, panel_light, bar_frame, minimap_frame, 3 buttons, divider, icon_frame, glow_orb, noise) with 9-slice rendering via `UITextures.h`
- **All UI States Textured**: 20+ state files use texture-based panels instead of raw SDL_RenderFillRect — with procedural fallback
- **Stereo Audio**: Fixed Mix_OpenAudio from mono (1) to stereo (2) for proper OGG playback
- **Release Packager**: Updated DLL copying (auto-detect), asset filtering (dev-only exclusion), 52 MB release package
- **Cleanup**: Deleted .bak files, comfyui_temp dirs, unused tool scripts
- 3 commits, 28 files changed, +1200 lines

**Previous Updates (2026-04-03 deep audit + hardening + perf + visual regression session):**
- **weaponKills bounds checks**: 7 unchecked array accesses now bounds-validated (PlayerCombat, PauseState, PlayStateCombatChallenge) — prevented OOB crash on corrupted WeaponID
- **UpgradeSystem deserialize hardened**: Stream state validated after initial extraction; corrupted/truncated saves reset to defaults instead of leaving partial garbage
- **Combat sqrt optimization**: 4 hot-path distance checks replaced with squared-distance comparisons (hit detection, ChainLightning, ChainReaction, ChainThorns) — eliminates sqrt per-entity in nested forEach loops
- **PauseState zone consistency**: Replaced inline zone calculation with shared `getZone()` helper from DimensionShiftBalance.h
- **Visual Regression System Upgraded**: HTML5 side-by-side diff reports (Reference / Actual / Diff), automated baseline capture + pixelmatch pixel-level comparison, `.gitignore` updated for test artifacts
- **Verified clean**: No resource leaks, no compiler warnings, all 6 boss systems correct, all 22 synergies functional, localization complete (98 EN + 98 DE), particle pool pre-allocated (2000)
- 6 commits, 5 files changed

**Previous Updates (2026-04-02 single-frame sprites + save hardening + polish session):**
- **Single-frame sprite system**: Replaced inconsistent multi-frame spritesheets with best-frame extraction + code-driven animations (bob, tilt, squash-stretch, punch, shake, rotation)
- **Frame extraction tool**: `tools/extract_best_frames.py` scores frames by coverage/centering/compactness, saves to `assets/textures/singles/`
- **Atomic save system**: New `SaveUtils.h` — all 9 save points now write to `.tmp`, flush, then rename (crash-safe). Eliminated 3 duplicated `backupFile()` and 3 duplicated `openWithBackupFallback()` functions.
- **Collision grid fix**: `cellHash` changed from `int` to `int64_t` with prime multiplier (overflow prevention at large coords)
- **HUD boss floor fix**: Used `isBossFloor()`/`isMidBossFloor()` instead of `floor%3==0` — distinguishes zone boss (red "BOSS") from mid-boss (orange "MINI-BOSS")
- **Gameplay polish**: Weapon pickup particle burst, dimension-switch-locked SFX, exit-locked SFX+shake
- **Afterimage init**: Default-initialize all primitive members (was garbage values)
- 6 commits, 40+ files changed

**Previous Updates (2026-04-02 deep code audit + hardening session):**
- **AnimationComponent**: Bounds-check currentFrame in getCurrentSrcRect() (OOB crash fix)
- **Null texture guards**: 19 SDL_CreateTextureFromSurface calls now null-checked (PlayStateRenderOverlays, EndingState, LoreState)
- **Game::shutdown**: Added missing save calls for Bestiary, AscensionSystem, ChallengeMode (data loss prevention)
- **wyrm_hunter achievement**: Fixed triggering on wrong bosses (% 2 → % 4, only fires for Void Wyrm)
- **Save deserialization**: Validated bestRoomReached, totalEnemiesKilled, totalRiftsRepaired, milestonesUnlocked on load
- **ChallengeMode load**: Clamped negative values on deserialized scores/times/floors/kills
- **DailyRun legacy load**: Fixed reset that zeroed score/kills after reading v1 leaderboard entries
- **ItemDrop**: Added missing hasComponent<TransformComponent>() check on health orb pickup
- **Level.h**: Initialized m_spawnPoint and m_exitPoint to {0,0}
- **Settings validation**: All loaded settings clamped to valid ranges (volumes 0-1, hudScale 0.5-2, window 640-7680×480-4320)
- 5 commits, 13 files changed

**Previous Updates (2026-04-02 bugfix + polish session):**
- **RunSummaryState**: Fixed NG+ badge, death cause, and grade overlapping at y=344 (dynamic Y cursor)
- **OptionsState**: Fixed 16 items overflowing screen height (itemH 70→64, startY 340→300)
- **PlayerCombat**: Added unique parry-counter particle effects for all 12 weapons (6 were missing)
- **Mute persistence**: Mute state now saved/loaded in riftwalker_settings.cfg
- **Achievement resets**: m_parryCountThisRun, m_tookDamageThisBoss, m_rangedOnlyFloors, m_usedMeleeThisFloor properly reset on quick-restart
- **Localization**: "Resolution" option now localized (EN/DE)
- **Gamepad support**: Relic choice, NPC dialog, and puzzle input now fully gamepad-compatible (D-pad/A/B/Start)
- **PlayerGrapple**: Fixed 2 division-by-zero crash risks in rope constraint + enemy pull
- **Level.cpp**: Added degenerate tile size guard for spike rendering
- 3 commits, 10 files changed

**Previous Updates (2026-04-02 complete 2K UI scaling + polish session):**
- **COMPLETE UI SCALING**: All 35+ files scaled from 1280x720 → 2560x1440 logical resolution
  - Every menu state, overlay, HUD element, title card, and dialog
  - Font sizes doubled for Credits/Lore/Ending (56/32/28px)
  - Boss HP bar, weapon panel, minimap, combo counter all scaled
- Smooth button hover transitions: color lerp + glow fade (m_hoverBlend)
- Staggered button entrance animation on menu load (ease-out slide from left)
- Boss attack animation cycling: Attack1/2/3 based on bossAttackPattern
- Boss enrage animation trigger when isEnraged + Idle state
- AnimationComponent: while-loop frame accumulation (prevents frame skipping)
- Camera.cpp: pixel-perfect SDL_Rect calculation (round-based edge snapping)
- Camera culling margin doubled (128px) + RenderSystem margin (200px)
- Splash screen: larger glow rects, 1.5x press-any-key, 1s skip lock
- ScreenEffects: GameState::SCREEN_WIDTH/HEIGHT instead of hardcoded values
- Level complete iris transition banner scaled for 2K
- Attack buffering: 80ms queue window during cooldown for snappier combat
- Coyote time increased 0.10s → 0.12s, jump release multiplier 0.50 → 0.65
- Small room spawning: min 1 enemy in rooms w<=6/h<=4 (prevents empty rooms)
- Combat UI: all notification popups scaled (achievement, lore, unlock, kill streak)
- Visual effects: speed lines, god rays, exit shafts, depth fog, foreground fog doubled
- Chromatic aberration pixel shift doubled, vignette corner blocks doubled
- Damage indicator edge thickness doubled (32→64px)
- Off-screen enemy indicator size + margin doubled
- Run intro narrative text positions + vignette border scaled
- Boss intro title card: doubled cinematic bars, separator lines, text offsets
- 68 commits, 44 files changed, ~1900 lines total
- Deep-dived into every card internal: Bestiary stat bars/icons/lore, Challenge checkboxes,
  NG+ card text/badges, Difficulty card names/descriptions, Relic selection orbs/labels
- Also scaled: speed lines, god rays, exit shafts, depth fog, foreground fog particles,
  chromatic aberration, vignette corners, void storm border, entropy glitch blocks,
  ambient particles, damage indicators, off-screen enemy arrows, kill streak glow,
  wave clear underlines, event chain progress dots, menu portal effects

**Previous Updates (2026-04-01 high-end visual overhaul session):**
- **COMPLETE SPRITE OVERHAUL**: All 17 enemies + player regenerated via v3 pipeline
  - JuggernautXL v9 SDXL at 768px (no pixel-art LoRA — professional hand-painted style)
  - rembg U²-Net neural background removal (eliminates grey artifacts)
  - Style: Hades/Dead Cells/Vanillaware inspired — bold painterly, dramatic rim light
  - gen_sprites_v3.py: 1527-line production pipeline with 2-pass support
- Entity sprite outlines: 4-direction 1px black offset (Hollow Knight method) on all textured entities
- Enemy rim lights: element/type-specific colored additive outlines (fire=orange, ice=blue, boss=red)
- Multiplicative color grading: SDL_BLENDMODE_MOD for dimension tinting (warm/cool)
- God rays: vertical additive light shafts from rifts (3 bands each) and exit beacon
- Enhanced death sequence: freeze frame → 15% slowmo → multiply desaturation + dark overlay
- Boss zoom transitions: smooth zoom-out on boss appear (2.5→2.2), zoom-in on kill (→2.8)
- Chromatic aberration on player damage: brief RGB split at screen edges
- Player edge glow: dimension-colored additive outline (blue for dim A, red for dim B)
- Level-up celebration: expanding golden ring + 8 radial speed lines + additive flash
- Depth fog bands: 3 horizontal gradient bands between parallax layers
- Enhanced projectile trails: additive glow, 6-segment falloff, sprite-colored

**Previous Updates (2026-04-01 combat feel session):**
- Enemy death ghost effects: white flash → shrink-to-nothing over 0.3s (sprite preserved)
- Graduated hitstop on ALL kills: normal 0.04s, elite 0.06s, mini-boss 0.1s, boss 0.25s
- Kill slow-motion: 0.4s at 40% speed on boss/mini-boss kills, 0.25s on multi-kills
- Weapon impact sparks: directional bright burst at hit point (6 normal, 12 crit)
- Enhanced hit confirmation: stronger white flash + additive overlay + 10% scale pulse
- Improved camera shake: exponential decay + sinusoidal oscillation (smoother, snappier)
- Soft gradient drop shadows (4-band, decreasing alpha vs. flat rectangles)
- Foreground fog particles (8 large semi-transparent drifting rectangles, dimension-tinted)
- Pickup hover animation: sine-wave bob + pulsing additive glow aura
- Enhanced spawn-in: dimensional rift opening (expanding purple ring + bright crack) before entity materializes
- Particle system overhaul: color interpolation (bright→ember), size variation ±30%, gravity on bursts, additive glow layers
- Weapon trail enhancement: white-hot core particles near tip, longer persistence, more particles
- Dash effects: directional speed burst + backward dust cloud + speed line particles + end-puff
- Tile edge highlights: top-left light + bottom-right shadow on tileset tiles, brightness variation per tile
- Rift ambient particles: tripled density with white core sparkle
- Low HP desaturation: gray overlay at <25% HP for visual danger feedback
- 14 files changed, +602/-69 lines

**Previous Updates (2026-04-01 visual overhaul + art refinement session):**
- Native display resolution detection (auto-detects 2560×1440, 4K, etc.)
- Physical window at native res, logical at 1280×720 via SDL_RenderSetLogicalSize
- Camera Zoom 2.5: better balance of sprite detail vs world visibility
- Anisotropic texture filtering (SDL hint "2") + per-texture SDL_SetTextureScaleMode(Linear)
- 4× Real-ESRGAN upscaled sprites with professional transparency cleanup (186 frames fixed)
- Post-processing system: vignette, color grading, ambient particles, bloom/glow (~100 extra draw-calls, zero heap allocs)
- NPC rendering complete overhaul: multi-layer hooded figures with robe gradients, glowing eyes, type-specific details (scrolls, gears, anvils, crystal balls)
- Event rendering complete overhaul: market stalls, shrine pedestals, swirling vortexes, ghost figures, mechanical workbenches, slot machines
- HUD upgrade: gradient backing panels, 3-band gradient bars, specular highlights, animated shimmer
- Font size 20px with light hinting for crisp text
- Sprite quality: 26/29 = GOOD, 3 = NEEDS_FIX (Flyer, GravityWell, Turret — grey artefacts from ComfyUI)
- All 48 files committed in 2 commits (`216c463` + `194ed30`): +2118/-164 lines
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
- **UI Coordinates**: Logical resolution is 2560x1440. ALL pixel values in UI/HUD/overlay code must be designed for this resolution. Never use 720p values. When unsure, use `SCREEN_WIDTH/HEIGHT` relative values or multiply intended 720p value by 2.
- **New UI elements**: Always use SCREEN_WIDTH/HEIGHT relative positioning (percentages, division) instead of hardcoded pixel values where possible.
- New features always via ECS pattern (Component for data + System for logic)
- No heap allocations in game loops (use pools, stack allocation)
- No auto-save of game assets (can be large)
- CMake uses `file(GLOB_RECURSE)` — new .cpp/.h files are picked up after re-running cmake
