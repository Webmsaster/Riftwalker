# Riftwalker

A roguelike platformer built with C++17 and SDL2, featuring dimension-shifting mechanics, procedural level generation, and fully procedural audio/graphics (no external assets).

## Features

- **Dimension Shifting** - Switch between two parallel dimensions mid-gameplay. Each dimension has unique tile layouts, themes, and enemy placements.
- **Procedural Level Generation** - Rooms are generated from templates and procedural algorithms. Every run is different.
- **10 Enemy Types** - Walker, Flyer, Turret, Charger, Phaser, Exploder, Shielder, Crawler, Summoner, Sniper - each with unique AI behaviors.
- **Boss Fights** - Multi-phase boss with 7 attack patterns including multi-shot fans, shield bursts, and teleport strikes.
- **Rift Puzzles** - Repair dimensional rifts by solving sequence-based puzzles before entropy collapses the level.
- **Procedural Graphics** - All visuals drawn with SDL2 primitives. No sprite sheets needed.
- **Procedural Audio** - Sound effects and ambient music generated from sine waves, sweeps, and noise at runtime.
- **ECS Architecture** - Clean Entity-Component-System design for flexible gameplay logic.
- **Upgrade System** - Permanent upgrades between runs: health, damage, speed, crit chance, dash, and more.
- **In-Run Pickups** - Temporary buffs: shield orbs, speed boosts, damage boosts, health orbs, rift shards.
- **Parallax Backgrounds** - Multi-layer scrolling backgrounds with dimension-specific themes.
- **Screen Shake & Hit Freeze** - Juicy combat feedback with camera shake and hit-stop on impacts.
- **Save System** - Progress persists across sessions (upgrades, best scores, difficulty unlocks).
- **Options Menu** - Configurable audio, controls, and display settings.

## Tech Stack

- **Language:** C++17
- **Graphics/Audio:** SDL2, SDL_mixer, SDL_image, SDL_ttf
- **Build:** CMake
- **Platform:** Windows (easily portable)

## Building

```bash
# Prerequisites: SDL2, SDL2_mixer, SDL2_image, SDL2_ttf development libraries

cd Riftwalker
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

## Running

```bash
./build/Release/Riftwalker.exe
```

## Controls

| Action | Key |
|--------|-----|
| Move | A / D or Arrow Keys |
| Jump | Space |
| Dash | Shift |
| Melee Attack | J |
| Ranged Attack | K |
| Dimension Switch | E |
| Interact | W |
| Pause | Escape |

## Project Structure

```
src/
  Components/   # ECS components (Transform, Physics, Health, AI, Combat, ...)
  Systems/      # ECS systems (AI, Combat, Collision, Particle, Render)
  Core/         # Engine core (Renderer, Input, Audio, Camera, Sound Generator)
  Game/         # Game logic (Player, Enemy, Level Generator, Items, Upgrades)
  States/       # Game states (Menu, Play, Pause, Options, Upgrades, Run Summary)
  ECS/          # Entity-Component-System framework
  UI/           # HUD, menus, floating damage numbers
```

## License

This project is for personal/educational use.
