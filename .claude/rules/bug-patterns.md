# Bug Patterns — Riftwalker

## Session 2026-03-29 (22 Commits)

### 1. Challenge Modifiers Not Applied
- **Pattern**: `ChallengeMode::applyChallenge()` called but `applyChallengeModifiers()` never invoked
- **Impact**: All 5 challenges + 6 mutators completely broken
- **Fix**: Call `applyChallengeModifiers()` in `PlayState::initializeChallenge()`

### 2. Weapon Unlock Conditions Missing
- **Pattern**: 5 weapons lack unlock conditions (EntropyScythe, ChainWhip, GravityGauntlet, DimLauncher, RiftCrossbow)
- **Impact**: Weapons unreachable despite existing in codebase
- **Fix**: Add unlock conditions to `WeaponSystem::isWeaponUnlocked()`

### 3. NG+ Progress Capped at Tier 5
- **Pattern**: `g_newGamePlusLevel` hardcoded to max 5 in save/load/UI logic
- **Impact**: Tier 6-10 storage, unlock, and UI blocked
- **Fix**: Remove tier 5 cap, use tier 10 max

### 4. NG+ Tier Unlock Capped at Victory
- **Pattern**: Tier unlock only increments after victory, but stops at tier 5
- **Impact**: Tiers 6-10 never unlocked after win
- **Fix**: Extend tier unlock logic to tier 10

### 5. Ascension Achievements Floor-Based Instead of Tier-Based
- **Pattern**: Achievements use `getCurrentFloor()` instead of `getNG+Tier()`
- **Impact**: "New Game Plus" and "True Riftwalker" unreachable
- **Fix**: Use tier-based logic for ascension achievements

### 6. Entropy Incarnate Lore Fragment Missing
- **Pattern**: Boss kill doesn't trigger lore discovery for Entropy Incarnate
- **Impact**: Lore #20 undiscoverable
- **Fix**: Add lore trigger in boss death handler

### 7. Entropy Incarnate Wrong HUD Name/Color
- **Pattern**: HUD displays "RIFT GUARDIAN" with wrong color instead of "ENTROPY INCARNATE"
- **Impact**: Visual/narrative inconsistency
- **Fix**: Update BossHUD to use correct boss name and Entropy-specific colors

### 8. NG+ Difficulty Infinite Scaling After Tier 10
- **Pattern**: `g_newGamePlusLevel` unbounded in enemy scaling calculations
- **Impact**: Infinite difficulty growth after reaching tier 10
- **Fix**: Clamp scaling at tier 10 max

## Session 2026-04-02 (Deep Code Audit — 4 Commits)

### 9. AnimationComponent OOB Access in getCurrentSrcRect()
- **Pattern**: `frames[currentFrame]` accessed without bounds check
- **Impact**: Potential crash if currentFrame exceeds frames.size()
- **Fix**: Clamp frame index with `std::min(currentFrame, size-1)`

### 10. Null SDL Texture Dereference (19 locations)
- **Pattern**: `SDL_CreateTextureFromSurface()` result used without null check
- **Impact**: Crash under low-memory or renderer error conditions
- **Fix**: Wrap all texture use in `if (tex) { ... }` guard
- **Files**: PlayStateRenderOverlays.cpp (5), EndingState.cpp (7), LoreState.cpp (7)

### 11. Missing Save Calls at Shutdown
- **Pattern**: `Game::shutdown()` only saved Achievements, Lore, Bindings, UpgradeSystem
- **Impact**: Bestiary, AscensionSystem, ChallengeMode data lost on crash/Alt-F4
- **Fix**: Added save calls for all 3 systems in shutdown()

### 12. wyrm_hunter Achievement Wrong Boss Check
- **Pattern**: `bossIdx % 2 == 1` triggers for Void Wyrm AND Temporal Weaver (and others)
- **Impact**: Achievement granted for killing wrong bosses
- **Fix**: Changed to `bossIdx % 4 == 1` matching only Void Wyrm boss type

### 13. DailyRun Legacy Leaderboard Score Loss
- **Pattern**: `e = DailyLeaderboardEntry{}` after reading score/kills in while-loop condition
- **Impact**: All legacy v1 leaderboard entries (after first) loaded with score=0, kills=0
- **Fix**: Removed the reset that overwrote just-read values

### 14. Save Deserialization Without Validation
- **Pattern**: bestRoomReached, totalEnemiesKilled, totalRiftsRepaired, milestonesUnlocked not clamped
- **Impact**: Corrupted save files could inject negative/overflow values into game logic
- **Fix**: Added bounds validation on all deserialized counters

### 15. Missing Counter-Attack Implementations (EntropyScythe, ChainWhip)
- **Pattern**: WeaponSystem::getCounterAttackChance() only handles 6 of 8 weapons
- **Impact**: EntropyScythe and ChainWhip cannot perform counter-attacks despite having chance values
- **Fix**: Added counter-attack handling for both weapons in getCounterAttackChance()

## Session 2026-04-XX (Sprite Sheet Fixes)

### 16. Enemy Sprite Frames Clipped or Misaligned
- **Pattern**: Crawler 128×96 frames cut off Hurt/Dead animations; Swarmer 64×64 barely visible
- **Impact**: Enemy death/damage animations incomplete or invisible, poor visual feedback
- **Fix**: Centered frames in 128×128 cells (Crawler), applied 2× Lanczos upscale (Swarmer)
- **Tool**: Created `tools/fix_sprite_sheets.py` for automated validation and repair of all 25 sprite sheets

### 17. No Sprite Validation System
- **Pattern**: Sprite mismatches (wrong dimensions, frame counts, scaling) not caught before load
- **Impact**: Visual bugs discovered at runtime; no automated quality gate
- **Fix**: Built `fix_sprite_sheets.py` to validate all sprites against SpriteConfig.cpp expectations
