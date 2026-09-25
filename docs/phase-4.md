# Phase 4 Scope

Phase 4 completes the input layer with a reusable player controller. Keyboard, mouse, and gamepad abstraction plus logical input mapping were introduced earlier (see `docs/phase-2.md`).

- `PlayerController` is an engine-provided `UpdateSystem` for top-down movement. It reads only logical actions (`move_left`, `move_right`, `move_up`, `move_down`), never physical keys.
- `PlayerController::default_bindings()` maps WASD and the arrow keys. Applications may pass their own `InputMap` or edit `actions()` to rebind, add gamepad buttons, or remove bindings.
- Diagonal input is normalized so movement speed is the same in every direction.
- An optional `GamepadState` can be attached with `set_gamepad`. Gamepad button bindings and the left stick both drive movement; the stick has a small dead zone and is used only when no digital direction is held.
- Entities with a dynamic `RigidBody` are moved by setting velocity, leaving integration and collision resolution to the physics system. Entities without a dynamic body are moved directly through their `Transform`.
- Negative speeds are rejected.

The controller does not know about specific games. Platformer jumping, acceleration curves, and other genre rules remain application code built on the same `InputMap` boundary.

The basic example now uses `PlayerController` instead of an example-owned movement system. Because the demo player has a dynamic body, movement flows through the fixed-rate physics step and resolves against the invisible boundaries.
