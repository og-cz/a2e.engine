# Phase 2 Scope

Phase 2 currently contains the first world and update boundaries:

- `Camera` converts world coordinates to viewport coordinates and supports position and zoom.
- Camera conversion is available in both directions for future cursor picking and world interaction.
- `Renderable::layer` provides stable ordering for basic 2D drawing.
- `TileMap` stores colored cells, validates coordinates, and is owned by a scene.
- Entities support type-indexed custom components with add, replace, get, and remove operations.
- Entities can be deactivated without destruction; inactive entities are excluded from rendering and physics until reactivated.
- `SceneManager` provides named scene creation, lookup, active-scene switching, destruction, and safe fallback behavior without coupling scenes to a game-specific transition policy.
- `Basic2DRenderer` draws tilemaps before layered entities.
- Renderables with nonzero transform rotation use the backend-neutral polygon primitive; ordinary rectangles retain the fast rectangle path.
- Renderables can be hidden with `visible`; the renderer also culls entities outside the camera viewport using rotation-aware bounds.
- Camera movement policy remains example-owned; the demo maps `+` and `-` to bounded zoom controls through `InputMap`.
- The demo uses `Camera::screen_to_world` to place the marker under the cursor while the left mouse button is held.

- `InputState` stores backend-neutral key state and exposes `is_down`, `pressed`, and `released` queries.
- `InputMap` maps one logical action to one or more physical keys, exposing action-level held, pressed, and released queries.
- `InputMap` can also bind mouse buttons to the same logical actions; the demo uses `place_marker` rather than hard-coding the left button in gameplay logic.
- `InputMap` also supports gamepad button bindings and queries against `GamepadState`, keeping device-specific input behind action names.
- `InputState` also stores mouse position and button transitions; the Win32 backend translates mouse messages without exposing Win32 types to systems.
- `GamepadState` defines portable button and left-stick state for a future backend; no platform-specific gamepad dependency is required yet.
- `UpdateSystem` is an ordered interface for systems that receive the scene, input, and frame delta.

`Application` owns systems and invokes them in registration order after the existing update callback. The Win32 window translates a small initial set of keyboard keys into `InputState`; other backends can provide the same interface without exposing platform types to game systems.

Textures, sprite sheets, physics, navigation, and autonomous-agent behavior remain out of scope until these boundaries have more coverage.