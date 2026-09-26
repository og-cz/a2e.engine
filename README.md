# A2E

A2E is a reusable 2D game and simulation engine with an optional first-class autonomous-agent subsystem. It is not a single game and it is not an AI model.

## Current state

Phases 1 through 7 are implemented:

1. Foundation: application loop, fixed and variable timesteps, configuration, logging, scenes, entities, resources, and a Win32 window.
2. World: textures, sprites, tilemaps, layers, a camera, and scene management.
3. Physics: AABB colliders, rigid bodies, layers and masks, triggers, collision events, and fixed substeps.
4. Input: keyboard, mouse, and gamepad abstraction, logical action mapping, and a reusable `PlayerController`.
5. Animation and audio: sprite-sheet animation states and replaceable audio with sounds, music, volume categories, and entity audio sources.
6. Navigation: navigation grids, deterministic A*, dynamic obstacles, and navigation agents.
7. AI foundation (optional): sensors, decaying memory, beliefs, dynamic goals, replaceable decision policies, and validated actions, plus a debug overlay for colliders, paths, and agent perception.

Advanced AI (Phase 8), simulation tooling, procedural generation, and the editor are intentionally not implemented yet. The engine is fully usable without them, and without the AI subsystem.

### Build

Requirements: Windows, a C++17 compiler, and CMake 3.20 or newer.

```powershell
cmake -S . -B build
cmake --build build --config Release
```

With MSYS2 (UCRT64 GCC and Ninja) instead of Visual Studio:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

With a single-configuration generator such as Ninja, the executables are placed directly in `build` (for example `build\a2e_demo.exe`) instead of `build\Release`.

### Run the basic example

```powershell
.\build\Release\a2e_demo.exe
```

The demo world contains a tilemap with interior walls, a movable player, an animated marker, a navigating NPC, and invisible physics boundaries. Use `WASD` or the arrow keys to move the player. Use `+`, `-`, or the mouse wheel to zoom the camera. Hold the left mouse button to move the teal marker. The pink NPC finds a path to the marker around walls and the player. Bumping into a wall plays a short generated tone. Press `F1` to toggle the debug overlay. Close the window to exercise clean shutdown.

### Run the autonomous agents example

```powershell
.\build\Release\a2e_agents_demo.exe
```

A guard patrols a walled room using vision and hearing. It chases you on sight (red), investigates where it last saw or heard you (orange), and otherwise patrols (blue). Move with `WASD` or the arrow keys, press `Space` to make a noise, and press `F1` to toggle the debug overlay showing its vision cone, path, and memories. The guard is built entirely in example code from the engine's generic agent parts.

### Run tests

```powershell
ctest --test-dir build -C Release --output-on-failure
```

This runs the engine tests (`a2e_tests`) and the AI tests (`a2e_ai_tests`).

See [docs/architecture.md](docs/architecture.md), [docs/phase-1.md](docs/phase-1.md), [docs/phase-2.md](docs/phase-2.md), [docs/phase-3.md](docs/phase-3.md), [docs/phase-4.md](docs/phase-4.md), [docs/phase-5.md](docs/phase-5.md), [docs/phase-6.md](docs/phase-6.md), and [docs/phase-7.md](docs/phase-7.md) for the architecture and scope.
