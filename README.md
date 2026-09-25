# A2E

A2E is a reusable 2D game and simulation engine with an optional first-class autonomous-agent subsystem. It is not a single game and it is not an AI model.

## Current state

Phases 1 through 6 are implemented:

1. Foundation: application loop, fixed and variable timesteps, configuration, logging, scenes, entities, resources, and a Win32 window.
2. World: textures, sprites, tilemaps, layers, a camera, and scene management.
3. Physics: AABB colliders, rigid bodies, layers and masks, triggers, collision events, and fixed substeps.
4. Input: keyboard, mouse, and gamepad abstraction, logical action mapping, and a reusable `PlayerController`.
5. Animation and audio: sprite-sheet animation states and replaceable audio with sounds, music, volume categories, and entity audio sources.
6. Navigation: navigation grids, deterministic A*, dynamic obstacles, and navigation agents.

The autonomous-agent AI (Phase 7+), simulation tooling, procedural generation, and the editor are intentionally not implemented yet. The engine is fully usable without them.

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

With a single-configuration generator such as Ninja, the executables are `build2e_demo.exe` and `build2e_tests.exe`.

### Run the example

```powershell
.\build\Release\a2e_demo.exe
```

A window opens with one rendered entity. Close it to exercise clean shutdown.

The demo world contains a tilemap with interior walls, a movable player, an animated marker, a navigating NPC, and invisible physics boundaries. Use `WASD` or the arrow keys to move the player. Use `+`, `-`, or the mouse wheel to zoom the camera. Hold the left mouse button to move the teal marker. The pink NPC finds a path to the marker around walls and the player. Bumping into a wall plays a short generated tone.

### Run tests

```powershell
ctest --test-dir build -C Release --output-on-failure
```

See [docs/architecture.md](docs/architecture.md), [docs/phase-1.md](docs/phase-1.md), [docs/phase-2.md](docs/phase-2.md), [docs/phase-3.md](docs/phase-3.md), [docs/phase-4.md](docs/phase-4.md), [docs/phase-5.md](docs/phase-5.md), and [docs/phase-6.md](docs/phase-6.md) for the architecture and scope.