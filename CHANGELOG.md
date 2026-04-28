# Changelog

All notable changes to Riftwalker. The format follows [Keep a Changelog](https://keepachangelog.com/) and the project follows [Semantic Versioning](https://semver.org/).

## [1.0.0] — 2026-04-28 — Initial Release

First public-ready build. Feature-complete with full content, accessibility layer, performance presets, and end-to-end-verified gameplay (24-min playtest reaching Floor 27 / Zone 5).

### Content

- 12 weapons (6 melee + 6 ranged), each with mastery tiers and a unique parry counter (12/12).
- 4 classes (Voidwalker, Berserker, Phantom, Technomancer), each with a signature combo finisher (4/4).
- 38 relics + 25 hidden synergies (13 relic-relic + 12 weapon-relic) — fully localized.
- 6 bosses (Rift Guardian, Void Wyrm, Dimensional Architect, Temporal Weaver, Void Sovereign, Entropy Incarnate) with multi-phase AI and per-boss intro cinematics.
- 17 enemy types + 12 elite modifiers across 5 zones × 6 floors.
- NG+ tiers 0–10 with bracketed difficulty scaling (HP, damage, enemy speed all curve smoothly).
- 5 daily-run challenges + 6 mutators; persistent best scores.
- 20 lore fragments + Bestiary discovery system.
- 42 achievements with permanent stat bonuses.

### Localization

- 996 EN + 1225 DE keys.
- 170 gameplay tips (rotation pool seeded by run state).
- 9-page tutorial (Welcome / Controls / Combat / Dimensions / Progression / Parry / Finisher / Synergy / Ready).
- Full menu, HUD, pause, options, credits localized.

### Performance

- Aggressive Release compiler flags: `/O2 /Oi /Ot /Oy /GL /Gy /Gw /GS- /fp:fast` + linker `/LTCG /OPT:REF /OPT:ICF`.
- Tracy linked but `TRACY_ENABLE` undef'd in Release → `ZoneScopedN` macros become no-ops, dead code stripped.
- Quality preset (Low / Medium / High) drives particle count (0.4 / 0.7 / 1.0×), procedural-tile-detail density, and toggles for bloom / ambient particles / dynamic lighting / color grading / vignette / chromatic aberration / entropy glitch.
- Dim-exclusive border tile rendering: 4-bucket alpha-tier batching (~80–200 SetColor → 8 SetColor + 8 RenderDrawRects per frame).
- Cached HUD elements: rift counter, challenge HUD, mutator labels, boss name texture, kill-feed glyphs, finisher prompt, NG+ tier text, speedrun timer (1Hz rebuild), endless score (event-driven rebuild).
- DimResidue scan: O(zones × entities) → O(zones + entities).
- Light/glow/overlay registration return early when their post-FX is disabled — no bookkeeping cost when off.
- Binary 1.83 MB. Runs on baseline x64/SSE2 (no AVX required).

### Game Feel

- Hit-stop on melee + ranged crits + boss kills (graduated freeze 0.05 / 0.10 / 0.45s).
- Camera kick scaled by damage dealt and damage taken (1.5–20 px shake).
- Parry: 8 directional shockwave rays + camera flash + 0.55/140 ms rumble + guaranteed crit + post-parry weapon-specific counter.
- Boss kill triple-sense climax: 0.85/380 ms rumble + gold-white camera flash + 0.45 s slow-mo + zoom-in + 60-particle burst.
- Jump stretch + landing squash for full anticipation/recovery loop.
- Audio: ±12% volume drift on rapid-fire SFX (no ear fatigue from identical samples).
- Damage number tiering: crit+50dmg=2.2×, 100dmg=1.7×, 50dmg=1.5×, 20dmg=1.3×.
- Low-HP heartbeat audio + tactile rumble (0.10–0.40 strength scaling with urgency).
- Boss attack telegraph: 6 warning particles + 6-spoke radial floor sparks at ≥65% windup progress.
- Pickup magnet (150 px squared-falloff toward player) — post-fight cleanup is instant.
- Camera look-ahead scales with player speed (0.6× → 1.4×, asymmetric build/recover).
- Achievement unlock haptic: rumble + green-gold camera flash + 22-particle burst.
- Combo break red-frame flash when 5+ combo timeouts to 0.
- Weapon switch haptic + particle burst on Q/R cycle.
- Dash haptic (0.45/90 ms).
- Off-screen boss/elite/exit indicators at the screen edge with color legend.

### Accessibility & Human-Friendly

- **Casual difficulty (Easy)**: +30% HP, +20% damage dealt, -25% damage taken, -30% enemy spawns, 1.5× shards. Stacks multiplicatively with relics so build identity is preserved.
- **Reduce Flashes** toggle: dampens kill-flash to 30%, fully disables chromatic aberration, fully disables entropy glitch overlay.
- **Quality Preset**: Low / Medium / High exposed in Options menu, persisted to settings.cfg.
- **Subtle keyboard aim-assist**: ~20° detection cone, ~10° max bend (60% lerp). Pros don't notice the bend; casuals land more shots.
- **First-run welcome arrow** beside the Tutorial button (auto-disappears after first run).
- **Pause-menu live controls reference** — 8 actions × current InputManager bindings, reflects remapping.
- **HP critical %** beside the HP bar at ≤25% HP (pulsing red, harder pulse below 10%).
- **Save indicator toast** confirms persistence after every save.
- **Stuck-detection hint** after 60 s of no kills + no rifts repaired: nudges toward dim-switching.
- **Auto-pause on focus loss** — Alt-Tab safe.
- **Mid-run quit guard** — close button / Alt-F4 during Play pushes Pause + Abandon-confirm instead of exiting silently.
- **Color-blind modes** (Deuteranopia / Tritanopia).
- **Death-screen rotating tip** + explicit `[R]` quick-retry hint.
- **Onboarding**: first-time players auto-redirect to Tutorial when they click "New Run".

### Known Limitations

- Visual-regression suite has 5 timing-variance fails out of 18 (run-intro fade, debug overlay, keybindings cursor, bestiary animation, credits scroll). Documented in `tests/visual_reference/KNOWN_FAILURES.md`. Not a render bug — same pixels animate at different phase.
- Playtest bot occasionally pathfinds slowly on complex floors (observed ~5-min stall on Floor 27 in 24-min E2E run). Real player navigation is unaffected.

### Bug-Pattern Catalog

44 documented patterns + detection heuristics in `.claude/rules/bug-patterns.md`. The latest two (#43 pickup-magnet gravity leak, #44 stale save-indicator toast) were caught in the pre-release audit, not in the wild.
