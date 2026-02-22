# Riftwalker

## Overview
Roguelike platformer with dimension-switching mechanic. C++17, SDL2, procedural rendering and audio.

## Build
```bash
cd build
cmake --build . --config Release
start Release/Riftwalker.exe
```

If new source files are added, re-run cmake first:
```bash
cd build
cmake ..
cmake --build . --config Release
```

## Architecture
- **ECS Pattern**: Components (data) + Systems (logic) + Entities
- **Procedural Rendering**: No sprites — all visuals drawn with SDL_RenderFillRect/DrawLine
- **Procedural Audio**: All sounds generated via SoundGenerator (sine waves, noise, sweeps)
- **State Machine**: Game states (Menu, Play, Pause, Options, etc.) managed via stack

## Key Directories
- `src/Components/` — ECS data components (PhysicsBody, CombatComponent, AIComponent, etc.)
- `src/Systems/` — ECS systems (RenderSystem, AISystem, CombatSystem, PhysicsSystem, etc.)
- `src/Core/` — Engine core (Game, Window, Camera, InputManager, AudioManager, SoundGenerator)
- `src/Game/` — Game logic (Player, Enemy, Level, LevelGenerator, DimensionManager, etc.)
- `src/States/` — Game states (PlayState, MenuState, PauseState, etc.)
- `src/UI/` — UI elements (HUD, Button)

## Conventions
- New features via ECS: add Component for data, System for logic
- No heap allocations in game loops (use pools, stack allocation)
- All enemy stats hardcoded in Enemy.cpp
- World themes defined in WorldTheme.cpp (12 themes)
- Save system: riftwalker_save.dat (text-based, UpgradeSystem serialization)

## Dependencies (via vcpkg)
SDL2, SDL2_image, SDL2_mixer, SDL2_ttf
