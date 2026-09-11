# A2E

A2E is a reusable 2D game and simulation engine with an optional first-class autonomous-agent subsystem. It is not a single game and it is not an AI model.

## Phase 1

The current implementation provides the Phase 1 foundation, Phase 2 world, scene, input, and composable-component boundaries, a typed cancellable event bus, and a Phase 3 physics boundary with composable colliders, rigid bodies, AABB detection, masks, collision callbacks, material response, and optional fixed substeps. Logical actions can map keyboard, mouse, and gamepad inputs. The demo supports WASD/arrow movement through logical actions. Hunter AI, advanced AI, navigation, and an editor are intentionally not implemented.

### Build

Requirements: Windows, a C++17 compiler, and CMake 3.20 or newer.

```powershell
cmake -S . -B build
cmake --build build --config Release
```

### Run the example

```powershell
.\build\Release\a2e_demo.exe
```

A window opens with one rendered entity. Close it to exercise clean shutdown.

The demo world contains a tilemap, a movable player, a marker, and invisible physics boundaries. Use `WASD` or the arrow keys to move the player. Use `+` and `-` to zoom the camera. Hold the left mouse button to move the teal marker through the logical `place_marker` action.

### Run tests

```powershell
ctest --test-dir build -C Release --output-on-failure
```

See [docs/architecture.md](docs/architecture.md), [docs/phase-1.md](docs/phase-1.md), [docs/phase-2.md](docs/phase-2.md), and [docs/phase-3.md](docs/phase-3.md) for the architecture and scope.