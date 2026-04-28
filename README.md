# Riftwalker

**A roguelike dimension-shifting platformer where reality is your weapon — and your prison.**

Shift between two parallel dimensions, fight through 30 floors of procedurally generated chaos, and unravel the mystery of your decaying suit before entropy consumes you.

Built from scratch in C++17 with SDL2. ~63 000 lines of handcrafted code. No engine. No shortcuts.

---

## ✨ Features

- 🌀 **Dimension Shifting** — Switch between Dimension A and B in real-time. Enemies, platforms, and hazards exist in one or both. Master the rift to survive.
- ⚔️ **12 Weapons with Mastery** — 6 melee + 6 ranged weapons, each with weapon-specific parry counters and mastery tiers that unlock new attack patterns.
- 🧬 **38 Relics, 25 Synergy Combos** — 13 relic-relic + 12 weapon-relic synergies. Hidden combos transform builds.
- 🎭 **4 Classes** — Voidwalker, Berserker, Phantom, Technomancer — each with unique abilities, signature combo finishers, and class-specific dimension behavior.
- 🏰 **30 Floors, 5 Zones** — From the Fractured Threshold to the Sovereign's Domain. Mid-bosses every 3 floors, zone bosses every 6.
- 👹 **6 Bosses** — Multi-phase encounters: Rift Guardian, Void Wyrm, Dimensional Architect, Temporal Weaver, Void Sovereign, Entropy Incarnate.
- 🔧 **Suit Entropy** — Your suit degrades as you play, distorting controls and abilities. Manage it or embrace the chaos.
- 🔄 **NG+ (Tiers 0-10)** — Ascension system with bracketed difficulty scaling. The void remembers.
- 📜 **NPC Quests & Lore** — 20 discoverable lore fragments and Bestiary entries.
- 📅 **Daily Challenges & Mutators** — 5 challenges + 6 mutators, persistent best scores.
- 🎮 **Full Gamepad Support** — Rebindable controls for keyboard and controller, haptic rumble for impact / heartbeat / parry.
- 🇩🇪 **English + German Localization** — 996 EN + 1225 DE keys, 170 gameplay tips, fully translated tutorial.

---

## ♿ Accessibility & Quality

- **Quality Preset** (Low / Medium / High) — Trade post-FX for FPS. Low runs smoothly on baseline x64/SSE2 hardware.
- **Reduce Flashes** toggle — Photosensitivity-friendly: dampens kill-flash, removes chromatic aberration and full-screen entropy glitch.
- **Casual Difficulty** — +30% HP, +20% damage, -25% damage taken, fewer enemies, more shards. First-time-friendly.
- **Subtle Aim-Assist** — Ranged shots within ~20° of an enemy gently snap toward them.
- **Off-Screen Indicators** — Boss (red), Elite (orange), and active Exit (green) arrows pulse at the screen edge.
- **Color-Blind Modes** — Deuteranopia and Tritanopia palettes.
- **HUD Scale + Opacity** — Adjustable from 75% to 150%.
- **Auto-Pause on Focus Loss** — Alt-Tab safe.
- **9-Page Tutorial** — Walks through Movement, Combat, Dimensions, Progression, Parry, Finisher, Synergy.

---

## 🎮 Controls

| Action | Keyboard | Gamepad |
|--------|----------|---------|
| Move | A/D or Arrow Keys | Left Stick |
| Jump (double, coyote, buffered) | Space | A |
| Dash | Left Shift | B |
| Melee Attack (hold to charge) | J | X |
| Ranged Attack | K | Y |
| Dimension Switch | Q | RB |
| Interact / Combo Finisher | F | LB |
| Pause | Escape | Start |
| Minimap toggle | M | — |
| Quick Retry on death | R | — |
| Screenshot | F9 / F12 | — |
| Fullscreen | F11 | — |
| Debug overlay | F3 | — |

Bindings can be remapped in **Options → Controls**.

---

## 🔨 Build From Source

**Prerequisites:** CMake 3.16+, a C++17 compiler (MSVC 2019+ recommended on Windows), and [vcpkg](https://vcpkg.io/) with `SDL2`, `SDL2_image`, `SDL2_mixer`, `SDL2_ttf`, `tracy`, `pixelmatch-cpp17`.

```bash
git clone <repo-url> Riftwalker
cd Riftwalker/build
cmake ..
cmake --build . --config Release
./Release/Riftwalker.exe
```

**Release flags applied:** `/O2 /Oi /Ot /Oy /GL /Gy /Gw /GS- /fp:fast` + `/LTCG /OPT:REF /OPT:ICF`. Tracy is linked but `TRACY_ENABLE` is undef'd in Release so `ZoneScopedN` macros become no-ops; full profiling is available in Debug + RelWithDebInfo.

After adding new source files, re-run `cmake ..` first.

---

## 🚀 Run the Release Build

A pre-built distribution lives in `dist/Riftwalker-v1.0.0-win64.zip`:

1. Extract the zip anywhere.
2. Double-click `Riftwalker.exe`.

System requirements (minimum, tested):
- Windows 10/11 x64
- SSE2 baseline CPU (any since 2003)
- 200 MB free disk
- 2 GB RAM
- DirectX 11-class GPU (Intel HD 4000+, integrated GPUs OK on **Low** preset)

---

## 🏗️ Architecture

ECS (Entity-Component-System) architecture with a fixed-timestep game loop. Procedural audio via `SoundGenerator`. Dual rendering pipeline (sprite + procedural fallback). 21 game states managed via a state stack with screen-space transitions.

```
src/
├── ECS/           # Entity-Component-System core (depth-indexed nested-forEach pool)
├── Components/    # Pure data: Physics, Combat, Health, AI, Sprite, Animation, Ability...
├── Systems/       # Logic: Physics, Collision, AI, Combat, Render, Particles
├── Core/          # Game / Input / Audio / Window / Camera / Localization
├── States/        # 21 states: Splash, Menu, Play, Pause, Shop, Credits, Tutorial...
├── Game/          # Weapons, Classes, Relics, Levels, Entropy, Bosses, ChallengeMode
└── UI/            # HUD, Buttons, UI textures
```

Logical resolution is **2560×1440** (2K) — all UI/HUD coordinates target that space.

---

## 🧪 Testing

- **Smoke regression** — `ctest -C Release -R smoke_regression_short` (149s automated bot run, must PASS for release)
- **Visual regression** — `ctest -C Release -R visual_regression` (18 screenshots; 13 stable, 5 known timing-variance — see `tests/visual_reference/KNOWN_FAILURES.md`)
- **Manual playtest** — `Riftwalker.exe --playtest --playtest-runs=N --playtest-class=voidwalker --playtest-profile=balanced`
- **Debug bot** — F6 in PlayState (auto-play, exits on death)
- **Bug-pattern catalog** — `.claude/rules/bug-patterns.md` (44 documented patterns + detection heuristics)

---

## 📝 Credits

**Design, Code & Art** — Solo developed (with Claude Code as AI pair programmer)

**Tech Stack** — C++17, SDL2, SDL2_image, SDL2_mixer, SDL2_ttf, CMake, vcpkg

**Tools** — Claude Code (AI pair programming), ComfyUI (asset generation), Tracy Profiler, pixelmatch-cpp17

---

<p align="center"><i>Reality splits. You walk the rift.</i></p>
