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

## Session 2026-04-04 (Final Release Assessment)

### 18. Sprite Aspect Ratio Distortion + Player Speed Bug
- **Pattern**: Sprite rendering used non-uniform scaling; player movement calculations affected
- **Impact**: Visual distortion in sprite rendering, player movement inconsistency
- **Fix**: Corrected aspect ratio handling and player speed calculations
- **Related**: Identified missing music composition and onboarding tutorial as release blockers

## Session 2026-04-09 (Agent-Review Bug Hunt — 11 commits, Phase 2 of 57-commit mega session)

All of these were found via focused code-review agent passes AFTER Phase 1
(perf + localization + defensive hardening, 46 commits) looked saturated.
Every finding was manually verified before fix. The agent found patterns I
would have missed by eye.

### 19. Lore Fragments Unreachable Because forEach Filters Destroyed Entities
- **Pattern**: `forEach + currentHP<=0` check ran AFTER the kill path called `destroy()`. Since `EntityManager::forEach` internally filters out `!isAlive()` entities, the `currentHP<=0` branch was dead code — the enemies were already unreachable by the iteration.
- **Impact**: LoreID::SwarmNature, ::GravityAnomaly, ::MimicDeception were undiscoverable — Bestiary 100% impossible
- **Fix**: Use `Bestiary::getEntry(type).killCount` (updated synchronously on every kill path via `Bestiary::onEnemyKill()`) instead of live entity enumeration
- **File**: PlayStateUpdateCombat.cpp:685-706

### 20. Synergy Pre-Pass Wiped by Main-Loop Reset (Order of Operations)
- **Pattern**: Pre-pass wrote flags on minions, main loop's FIRST action reset them to defaults, main loop's LAST action read the wiped flags
- **Impact**: Summoner enemies' signature minion-buff ability (+20% speed, +15% damage) was completely broken
- **Fix**: Move reset to the collection pass BEFORE the pre-pass write, not inside the main loop body
- **File**: AISystem.cpp — reset moved from ~line 428 to collection loop

### 21. Relic Dead Code (Function Defined But Never Called)
- **Pattern**: `getAbilityCDMultCursed()` and `getAbilityHPCost()` defined but never referenced anywhere. Only the non-cursed TimeDilator branch fired on pickup.
- **Impact**: TimeTax cursed relic had neither the -50% CD upside nor the 5 HP per ability downside — picking it up did nothing
- **Fix**: Extend pickup handler to also fire for TimeTax; add `payTimeTaxCost()` helper at each ability.activate() site
- **Lesson**: When adding a variant feature (like cursed counterpart), grep for ALL usages of the original to ensure the new variant gets the same wiring

### 22. Stat Effects Compounding Because Mutation Without Reset
- **Pattern**: `applyStatEffects()` looped over all relics each call and did `moveSpeed *= speedMult` / `maxHP += hpBonus` on the ALREADY-MUTATED values. Multi-pickup runs saw exponential runaway.
- **Impact**: Two SwiftBoots-like relics → `base * 1.10 * 1.10 = base * 1.21` instead of `base * 1.20`. ChaosCore + TimeDistortion + multiple speed relics → runaway
- **Fix**: Cache `baseMoveSpeed`/`baseMaxHP` on Player at run start (after applyUpgrades). Recalc resets to base before applying the loop's multiplier/bonus
- **Regression-warning**: This fix introduced a NEW bug (#27 below) where GlassCannon HP restriction was bypassed — always cross-check challenge modifiers when changing stat recalc logic

### 23. Accumulator Filled Unconditionally, Consumed Conditionally
- **Pattern**: Boss incremented `eiOverloadHealAccum` every frame at phase>=3. Consumer only fired heal ticks when player entropy > 70, but never reset the accumulator when below. Long fights below threshold built up hidden burst reserve.
- **Impact**: Entropy Incarnate could undo most of a long phase-3 fight in ~3 seconds when entropy briefly crossed 70 (~30% HP burst heal)
- **Fix**: Add else-branch that resets `eiOverloadHealAccum = 0` when entropy below threshold
- **File**: PlayStateUpdate.cpp:525-543

### 24. Function-Static Variables Leaking Across Runs
- **Pattern**: `static float ptRecoveryGraceTimer = 0;` + `static int ptLastDim = 0;` inside `updatePlaytest()` persisted across run boundaries
- **Impact**: Run 2+ in playtest bot occasionally died on floor 1 because fall-rescue was blocked by inherited grace timer; dim disorientation triggered spuriously on first frame
- **Fix**: Reset statics on first frame of each run via `m_playtestRunTimer < 0.1f` guard

### 25. Check-After-Write (Structurally Impossible Condition)
- **Pattern**: `freezeTimer = max(old, 1.5f); if (freezeTimer <= 0.01f) stun(0.15f);` — the max() guarantees the if-branch can never fire
- **Impact**: Ice weapon's brief 0.15s stun on first freeze application was dead code
- **Fix**: Capture `alreadyFrozen` bool BEFORE overwrite, use that for the stun gate
- **File**: CombatSystem.cpp:918-925

### 26. Stat Restriction Bypass by Using Wrong Base Reference
- **Pattern**: `getTile(x, y, dimension)` in laser beam trace used caller's dimension (can be 0) instead of emitter's actual dimension. getTile(0) resolved to m_tilesA regardless of where the laser lived.
- **Impact**: Dimension-B lasers traced through dimension-A walls for any dim-0 entity — walls in B vanished, walls in A incorrectly stopped the beam
- **Fix**: Use `ep.dim` (emitter's own dimension) in the beam traversal loop
- **File**: Level.cpp:896

### 27. Self-Inflicted Regression from Stat Recalc Fix
- **Pattern**: My own fix #22 (cached baseMaxHP) silently nullified the GlassCannon challenge — applyChallengeModifiers set hp.maxHP = 1.0f but not baseMaxHP. Next relic pickup called applyStatEffects which recalculated using the original 200-HP base, fully wiping the 1-HP restriction.
- **Impact**: GlassCannon-challenge player could pick up any relic to get ~200 HP
- **Fix**: In applyChallengeModifiers, also set `m_player->baseMaxHP = cd.playerMaxHP`
- **Lesson**: When introducing a new cached base-value field, grep ALL call sites that override the underlying HP/speed to ensure they also update the cached base

### 28. Orphan Level Tile Committed Before Paired Placement
- **Pattern**: DimSwitch tile written to level at line 491 with `switchPlaced = true`, then gate placement tried for up to 10 attempts. On gate-placement failure, the code skipped outSwitches registration and pairId++, but left the already-committed switch tile orphaned with a variant ID that had no matching gate.
- **Impact**: Some generated levels contained an interactive DimSwitch that, when activated by the player, silently did nothing
- **Fix**: On `!gatePlaced`, overwrite the switch tile back to Empty (rollback)
- **File**: LevelGeneratorPlacement.cpp:499+
- **Pattern-name**: "Commit-before-verify" — always defer committing state until all paired operations succeed, or prepare to roll back

### 29. Entropy Reduction via addEntropy(negative) Gets Multiplied by Resistance
- **Pattern**: `m_entropy.addEntropy(-15.0f)` applies `entropyGainMultiplier * upgradeResistance` to the amount. With EntropyResistance upgrade (0.5), the -15 becomes -7.5
- **Impact**: Scholar NPC reward halved for upgraded players
- **Fix**: Use `m_entropy.reduceEntropy(15.0f)` instead (no multipliers)
- **Rule**: Never pass negative to `addEntropy` — always use `reduceEntropy` for deliberate reductions

### 30b. Base Value Overwritten Instead of Added (passiveDecay wipe)
- **Pattern**: `applyUpgrades()` did `m_entropy.passiveDecay = upgrades.getEntropyDecay() * bonus;` — a full overwrite. At upgrade level 0, `getEntropyDecay()` returns 0, so the 0.15f default from `SuitEntropy::SuitEntropy()` AND the NG+1+ `passiveDecay *= 0.8f` from `startNewRun()` both got silently wiped.
- **Impact**: Players without the EntropyDecay upgrade (all new players, most early-game runs) had ZERO passive entropy decay. Entropy only dropped via rift repairs. This single misplaced `=` instead of `+=` made the "entropy grinds you down" core mechanic harsher than intended for the majority of playtime.
- **Fix**: `passiveDecay = 0.15f + upgrades.getEntropyDecay() * bonus;` + re-apply NG+ modifier at the same call site so both effects compose correctly.
- **Lesson**: Any place that sets a stat to `upgrade * mult` without adding a base should be suspect. Grep for `stat = upgrade.getX()` patterns — they're almost always wrong unless the base is 0.
- **File**: PlayStateRunLifecycle.cpp:652-658

### 30c. Dead Return Fields (MilestoneBonus HP/damage/speed)
- **Pattern**: `UpgradeSystem::checkMilestones()` returned a `MilestoneBonus` struct with 4 fields: `bonusHP`, `bonusDamageMult`, `bonusSpeed`, `bonusShards`. The caller extracted only `bonusShards`. The other 3 fields were silently discarded.
- **Impact**: 5 of 8 milestones granted HP/damage/speed bonuses that NEVER reached the player. Milestones 1 (+10 HP), 2 (x1.05 dmg), 4 (+5% speed), 5 (x1.10 dmg), 6 (+15 HP), 8 (x1.10 dmg) were essentially cosmetic — only the two shard milestones (3, 7) had any effect.
- **Fix**: Added `getAccumulatedMilestoneBonus() const` that walks `1..milestonesUnlocked` and sums non-shard fields. `applyUpgrades()` now composes it into `hp.maxHP`, `moveSpeed`, and melee/ranged damage multipliers.
- **Lesson**: When a function returns a multi-field struct, always grep the caller to verify every field is consumed. Unused return fields are dead code in disguise.
- **File**: UpgradeSystem.cpp + PlayStateRunLifecycle.cpp:925-929

### 30d. Inverted Break Condition in Template Selection Loop
- **Pattern**: `LevelGenerator.cpp` template dim-B loop: break when size difference `> 2`. Intent was to find a DIFFERENT-but-similar template for dim-B. Break-on-large-diff accepted large mismatches eagerly and continued iterating on small ones — the opposite of the intent.
- **Impact**: The loop could assign a dim-B template LARGER than the dim-A template that defined the room's registered `rw`/`rh` bounds. `applyTemplate()` then wrote tiles PAST the registered room rect in dim-2, overpainting adjacent room walls or the gap strip. Corridor connector code then used the dim-A `rw` to pick junctions, causing dim-2 corridors to open into walls.
- **Fix**: Filter candidates to templates with `width <= rw && height <= rh`, take the first valid one.
- **Lesson**: Every break-condition in a size-matching loop should be double-checked for polarity. "Break on good match" vs "break on bad match" are subtle but inverted.
- **File**: LevelGenerator.cpp:116-131

### 30e. BFS Seed Missing One Dimension
- **Pattern**: `LevelGeneratorValidation.cpp` BFS traversal seeded `tryVisit(spawnRoom, 1, 0)` but not `tryVisit(spawnRoom, 2, 0)` even when `spawnValid[2]` was true. Dim-2 was only reachable via a `hasDimensionSwitchAnchor` heuristic hit inside the BFS loop.
- **Impact**: When the spawn room failed the `roomHasSharedSwitchAnchor()` heuristic (possible when the spawn clear tiles don't share an empty tile across both dims), the BFS never visited dim-2 paths at all. `roomReachableInVisitedMask(exitRoomIndex, exitValid)` then reported dim-2-only exits as unreachable, firing `LevelValidationFailure_ExitReachability` on genuinely valid levels. Generator wasted up to 512 retries.
- **Fix**: Seed both dimensions when spawn is valid there.
- **Lesson**: Any graph-traversal BFS/DFS seed code should cover every valid start state, not just the "primary" one.
- **File**: LevelGeneratorValidation.cpp:330-334

### 30. replace_all Collision with Init Line (HUD m_frameTicks self-assignment)
- **Pattern**: Did `replace_all SDL_GetTicks() -> m_frameTicks` across HUD.cpp to convert 23 call sites. The init line was `m_frameTicks = SDL_GetTicks();` — the replace_all also caught the `SDL_GetTicks()` in the init, turning it into `m_frameTicks = m_frameTicks;` (a no-op self-assignment).
- **Impact**: HUD member `m_frameTicks` stayed at 0 for 25+ commits. Every `sin(m_frameTicks * k)` evaluated to `sin(0) = 0`, freezing EVERY HUD pulse animation: HP low-health pulse, XP gold pulse, entropy critical pulse, resonance glow, ability borders, combo counter, finisher prompt, boss minimap dot, rift minimap pulse, player minimap blink, NG+ tier pulse, ~5+ other effects.
- **Fix**: One character change — `m_frameTicks = SDL_GetTicks();`
- **File**: HUD.cpp:349 (regression from commit f4fcef8 — fixed in commit 8a90580)
- **Lesson**: **When using replace_all to propagate a cached value, explicitly exclude the init line.** Use targeted individual edits for the init line, or check the file after replace_all for any `X = X` patterns. Always grep for `(\w+)\s*=\s*\1\s*;` after a large replace_all.
- **Detection shortcut**: `grep -rn '(\w+)\s*=\s*\1\s*;' src/` — this single grep would have caught the bug immediately.

### 30f. Early-Return Kills Every Feature In The Loop Body (Ranged Attacks Skip Everything)
- **Pattern**: `CombatSystem::processAttack()` did `if (combat.currentAttack == AttackType::Ranged) { processRangedAttack(...); return; }` at the top. Everything below the return was a rich melee forEach loop that did: relic damage multiplier, class mult, damage boost pickup, smith dmg mult, weapon mastery, resonance dmg, critical hits, Rift Shield absorb (for enemies attacking player), Reflector mirror-shield (for player attacking reflector), Elite Shielded absorb, ShieldAura -30%, knockback, stun, combo effects, enrage counter.
- **Impact**: FIVE separate gameplay features were silently broken for ranged attacks:
  1. **Ranged damage modifiers**: BloodOrb, VoidCrown, PhaseHunter, WrathOfDimensions, BalanceKeeper, BerserkersCurse, GlassCannon, ChaosCore, BloodPact, ChaosRift buff, class mult, damage boost, blacksmith ranged mult, weapon mastery, resonance — NONE of them scaled ranged damage. A Technomancer with BerserkersCurse at 10% HP (expected +135%) did base weapon damage.
  2. **Rift Shield ability (player)**: shield absorb logic was inside the melee loop — enemy ranged attacks hit the player's HP directly, shield was cosmetic.
  3. **Reflector enemy's mirror-shield**: shield-up cycle was pure visual — player ranged weapons passed through untouched; the "time your shots" tip was meaningless.
  4. **Elite Shielded modifier**: eliteShieldHP absorb was melee-only; ranged builds ignored elite shields entirely.
  5. **Enemy ranged dimDamageMod + Summoner buff**: enemy projectiles didn't scale with dim-B modifier, and Summoner-buffed minions that used ranged weapons ignored their +15% damage buff.
- **Fix** (5 commits): Migrated each feature from the melee forEach loop into the appropriate ranged path. Ranged player damage mults now apply in `processRangedAttack()` at projectile spawn time. Shield/reflect/absorb logic moved into the `createProjectile()` onTrigger lambda (requires caching EntityManager* on CombatSystem so the lambda can spawn reflected projectiles). Enemy ranged modifiers applied at projectile spawn for non-player attackers.
- **Lesson**: **An early return skips every feature after it.** When you see `if (condition) { delegateTo(X); return; }`, grep the rest of the function for every `if (condition)` sub-check and every feature that the sibling path doesn't duplicate. In this case, 5+ checks for `currentAttack == AttackType::Ranged` INSIDE the melee forEach loop were all dead code — and none of the reviews caught it because they looked at *combat* not *early returns*.
- **Detection signal**: Grep for `if ([cond]) { ... return; }` at the top of a function, then grep the rest of the same file for `if ([cond])` — any match is a candidate for dead code that should be in the delegate branch instead.
- **Files**: CombatSystem.cpp:73 (the early return), fixes spread across CombatSystem.cpp / CombatSystem.h / CombatSystemEffects.cpp in commits c3fbc31, 1d07f34, 61e96e0, e1edbff, bc0a341

## Meta-Lessons from the 13-bug Phase 2

1. **Saturation is real but late** — After 46 "clean" commits in Phase 1, focused agent reviews found 13 more real bugs. The "looks done" feeling is misleading for complex gameplay code.
2. **Focused reviews find what eye-reads miss** — Every Phase 2 bug was a pattern my own review passes had walked past. Dedicated per-file-group agent reviews with specific bug-class hints produced consistent hits.
3. **Verify every agent finding** — ~15% of agent findings in Phase 2 were false positives (NPC stage off-by-one, DailyRun prune edge). Blind application would have broken correct code.
4. **Regressions come from your own fixes** — Two self-inflicted regressions in this session: #27 was introduced by #22 (GlassCannon HP bypass from baseMaxHP caching), and #30 was introduced by f4fcef8 (HUD frozen animations from replace_all collision). Grep-the-world before committing any large rewrite or cached field.
5. **Check-after-write is a reliable bug signature** — grep for `max()` followed by `<=` small threshold, `min()` followed by `>=` large threshold.
6. **Order-of-operations bugs love pre-passes** — any time there's a collection pass + main loop pass, check if the main loop resets what the pre-pass wrote.
7. **The "0 findings, saturated" signal is valid — but don't stop too early.** The HUD m_frameTicks regression was found AFTER two consecutive "0 findings, saturated" agent reviews. One more focused review on files I had TOUCHED (not just files I was worried about) caught it. **Files you modified recently deserve a review even after saturation is declared elsewhere.**
8. **Always grep for `X = X` patterns after a large replace_all.** One second of grep would have caught #30 at the time of commit f4fcef8 instead of 26 commits later.
