# A2E Phase 1 Architecture

A2E is a reusable 2D engine, not a game and not an AI model. The repository keeps engine code under `engine/` and composition/example code under `examples/`.

## Modules

- `Application` owns lifecycle and the frame schedule.
- `EngineConfig` validates runtime settings.
- `Clock` measures bounded frame delta time.
- `Application::clock()` exposes the clock read-only for diagnostics and future debug overlays.
- `FixedUpdateSystem` provides deterministic simulation updates when `EngineConfig::fixed_update_hz` is enabled; variable systems continue to run once per rendered frame.
- `Scene` owns entity lifetime; `Entity` contains identity, activation state, `Transform`, and optional `Renderable` data. Inactive entities remain available for reactivation but are skipped by rendering and physics.
- `SceneManager` owns named scenes, active-scene selection, destruction, and fallback selection. The current `Application` remains single-scene for compatibility, while future lifecycle work can delegate to the manager.
- Scene and scene-manager lookups provide const overloads for read-only systems and inspection tools.
- `Entity` also provides a type-indexed custom component store, allowing examples and games to attach their own data without modifying engine classes.
- `Camera` owns world-to-screen and screen-to-world conversion without entering game logic; renderers and future mouse-picking systems can share the same math.
- `Renderer` is an interface. `Basic2DRenderer` translates layered renderable scene data into generic `RenderTarget` calls.
- Rendering honors renderable visibility and performs viewport culling before issuing draw calls.
- `InputState` and `InputMap` keep physical keyboard details separate from logical gameplay actions.
- `EventBus` provides synchronous typed publish/subscribe communication for decoupled systems; physics uses it for collision lifecycle events. `subscribe` returns a handle that can explicitly unsubscribe a listener during scene changes or shutdown.
- Physics events expose contact geometry so gameplay does not need to duplicate collider calculations.
- `Window` is both the event/presentation boundary and a render target. Phase 1 supplies a Win32/GDI implementation.
- `ResourceManager` is a typed cache boundary for future assets.
- `Texture` is a resource-owned pixel surface and `Sprite` is entity data that references shared texture resources; renderers consume these through `RenderTarget`.
- Logging is a small replaceable free-function boundary.

## Loop

`Application::run()` creates the configured window, polls events, samples a bounded delta time, invokes the optional update callback, clears and renders the scene, presents, then sleeps for the remainder of the target frame period. `stop()` closes the window and the destructor repeats that cleanup defensively.

## Extensibility

Physics, navigation, input, and autonomous-agent systems can be added as services or update systems that operate on scene data. They can communicate through typed events instead of direct subsystem references. They do not need to know about Win32 or GDI. A future renderer/window backend can implement `Renderer`, `RenderTarget`, and `Window` without changing scene ownership or application lifecycle. Input and ordered update-system interfaces are documented in `docs/phase-2.md`.
