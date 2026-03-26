# Riftwalker

**A roguelike dimension-shifting platformer where reality is your weapon — and your prison.**

Shift between two parallel dimensions, fight through 30 floors of procedurally generated chaos, and unravel the mystery of your decaying suit before entropy consumes you.

Built from scratch in C++17 with SDL2. 52,000+ lines of handcrafted code. No engine. No shortcuts.

---

## ✨ Features

- 🌀 **Dimension Shifting** — Switch between Dimension A and B in real-time. Enemies, platforms, and hazards exist in one or both. Master the rift to survive.
- ⚔️ **10 Weapons with Mastery** — 3 melee + 4 ranged weapons, each with mastery tiers that unlock new attack patterns.
- 🧬 **38 Relics, 22 Synergy Combos** — Collect relics that bend the rules. Combine them for devastating synergies.
- 🎭 **4 Classes** — Voidwalker, Berserker, Phantom, Technomancer — each with unique abilities and playstyles.
- 🏰 **30 Floors, 5 Zones** — From corrupted labs to void rifts, each zone escalates the challenge with new enemies and mechanics.
- 👹 **5 Bosses** — Unique boss encounters with multi-phase mechanics and cinematic intro sequences.
- 🔧 **Suit Entropy** — Your suit degrades as you play, distorting controls and abilities. Manage it or embrace the chaos.
- 🔄 **NG+ (5 Tiers)** — Ascension system with progressive difficulty scaling. The void remembers.
- 🎯 **3 Endings** — Your performance shapes the narrative. Discover all three.
- 📜 **NPC Quests & Lore** — 12 discoverable lore fragments and NPC encounters that flesh out the world.
- 📅 **Daily Challenges** — Seeded runs with fixed modifiers. Compete against yourself.
- 🎮 **Full Gamepad Support** — Rebindable controls for keyboard and controller.
- 🇩🇪 **German Localization** — Vollständig auf Deutsch spielbar.

---

## 📸 Screenshots

| Gameplay | Boss Fight | Dimension Shift |
|----------|-----------|-----------------|
| ![Gameplay](screenshots/gameplay.png) | ![Boss](screenshots/boss_fight.png) | ![Shift](screenshots/dimension_shift.png) |

---

## 🔨 Build

**Prerequisites:** CMake 3.16+, a C++17 compiler, and [vcpkg](https://vcpkg.io/) with `SDL2`, `SDL2_image`, `SDL2_mixer`, `SDL2_ttf`.

```bash
git clone https://github.com/yourusername/Riftwalker.git
cd Riftwalker/Riftwalker/build
cmake ..
cmake --build . --config Release
```

Run:
```bash
./Release/Riftwalker.exe
```

---

## 🎮 Controls

| Action | Keyboard | Gamepad |
|--------|----------|---------|
| Move | WASD / Arrow Keys | Left Stick |
| Jump | Space | A |
| Attack | J / K | X |
| Dimension Shift | E | RB |
| Dash | Shift | B |
| Interact | W | Y |
| Pause | Escape | Start |

**Debug Keys:** F3 (debug overlay) · F4 (balance snapshot) · F6 (auto-play bot)

---

## 🏗️ Architecture

ECS (Entity-Component-System) architecture with fixed-timestep game loop. Procedural audio via `SoundGenerator`. Dual rendering pipeline (sprite-based + procedural fallback). 19 game states managed via state stack.

```
src/
├── ECS/           # Entity-Component-System core
├── Components/    # Pure data components
├── Systems/       # Logic systems (Physics, Combat, AI, Render, Particles)
├── States/        # Game states (Play, Menu, Credits, Boss Intro, ...)
└── Game/          # Game systems (Weapons, Classes, Relics, Levels, Entropy)
```

---

## 📝 Credits

**Design, Code & Art** — Solo developed

**Tech Stack** — C++17, SDL2, SDL2_image, SDL2_mixer, SDL2_ttf, CMake, vcpkg

**Tools** — Claude Code (AI pair programming), ComfyUI (asset generation), Tracy Profiler

---

<p align="center"><i>Reality splits. You walk the rift.</i></p>
