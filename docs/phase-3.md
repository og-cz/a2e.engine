# Phase 3 Scope

Phase 3 begins with a replaceable physics boundary:

- `Collider` is composable entity data containing an AABB size, collision layer, mask, and trigger flag.
- `RigidBody` is composable entity data containing velocity, dynamic/static state, restitution, and friction.
- `PhysicsSystem` integrates dynamic bodies, applies optional gravity, reports overlapping collider pairs through `CollisionEvent` callbacks, separates solid AABB overlaps, optionally publishes `CollisionEnterEvent` and `CollisionExitEvent` through `EventBus`, and supports opt-in fixed substeps for fast-moving bodies.
- Active collision callbacks and enter events include a normal pointing from the first entity toward the second and the current penetration depth. Exit events retain the entity IDs and trigger flag, with zero geometry because the pair is no longer overlapping.

The legacy callback reports every overlapping pair on each update. Lifecycle events report only contact start and contact end, allowing gameplay systems to react without maintaining their own pair cache. The engine does not hard-code friction, joints, or a third-party physics library.

Current limitations:

- Only axis-aligned boxes are supported.
- Collision detection is pairwise and intended for small Phase 3 scenes.
- Substeps reduce tunneling but are not continuous collision detection; callers choose the maximum step size. Contact lifecycle events are evaluated at each internal step. Joints and broad-phase spatial acceleration are not implemented yet.
- Negative maximum substep sizes are rejected. Contact exits are generated when a previously active pair no longer exists, including after an entity is removed from a scene.

The basic example composes the movement system and physics system in order. The player has a dynamic collider, while four invisible static colliders constrain it to the demo world. These boundaries belong to the example, not to the engine. Contact-enter events are routed through `Application::events()` and logged by the example.

Physics implements `FixedUpdateSystem` and the demo runs it at 60 Hz through the fixed-timestep pipeline. Input and camera systems remain variable-rate systems.
