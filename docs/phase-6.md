# Phase 6 Scope

Phase 6 adds navigation. Navigation is separate from AI: application code or a future agent decides where an entity wants to go, and navigation decides how it gets there. Navigation works without any AI subsystem.

- `NavigationGrid` is a world-space grid with an origin and cell size. Each cell has static walkability and a movement cost (1 is normal ground; larger values are avoided when a cheaper route exists).
- Dynamic obstacles are stored separately from static walkability as per-cell counters. Applications typically call `clear_obstacles()` and re-mark moving blockers with `add_obstacle` or `add_obstacle_rect` each frame without touching the static map.
- `NavigationGrid::from_tilemap` builds a grid aligned with a `TileMap` using an application-supplied predicate, so the engine does not assume what any tile value means.
- `find_path` is a deterministic A* search. It returns the cells from start to goal inclusive, or an empty path when the goal is blocked or unreachable. Diagonal steps are optional (`allow_diagonal`), use an octile heuristic, and never cut past blocked corners. The start cell may be blocked so an agent standing on an obstacle can still leave it. Ties are broken by insertion order, so the same grid always produces the same path.
- `NavigationAgent` is an entity component holding speed, arrival distance, destination, waypoints, and a `NavigationStatus` (`Idle`, `Moving`, `Arrived`, `Unreachable`). Gameplay calls `set_destination` or `stop`.
- `NavigationSystem` plans paths for agents that requested one, follows waypoints, and repaths when any remaining waypoint becomes blocked. It publishes `NavigationArrivedEvent` and `NavigationFailedEvent` when constructed with an `EventBus`.
- Entities with a dynamic `RigidBody` are steered by velocity so physics still resolves contacts; other entities are moved directly through their `Transform`, using any leftover movement budget to continue past reached waypoints in the same update.

Current limitations:

- Paths follow cell centers and are not smoothed; movement looks grid-aligned on open ground.
- Agents do not avoid each other unless the application marks them as obstacles.
- Agents are treated as points. Large agents need a grid with appropriately sized cells or obstacle inflation done by the application.
- After a failed plan, an agent stays `Unreachable` until a new destination is set; retry policy belongs to the application.

The basic example adds two interior walls. They are drawn by the tilemap, solid for physics, and unwalkable for navigation. A pink NPC walks to the teal marker wherever it is placed. The example-owned `PlayerObstacleSystem` marks the player as a dynamic obstacle each frame, and `FollowMarkerSystem` is the small gameplay rule that picks the destination and retries unreachable goals every half second.
