# Phase 7 Scope

Phase 7 adds the optional AI foundation: a generic autonomous agent built from replaceable parts. Nothing in the engine core depends on it, and no specific agent (guard, hunter, trader) is hard-coded. The engine remains fully usable with the AI subsystem unused.

## Pipeline

`AgentSystem` runs this loop for every active entity with an `Agent` component:

```
World -> Sensors -> Observations -> Memory / Beliefs -> Goals -> Decision Policy -> Validated Action -> World
```

1. **Facing** follows the entity's movement, so vision looks where the agent is going.
2. **Perceive.** Each sensor reports `Observation`s (source, kind, tag, position, time, confidence).
3. **Remember.** Observations go into `AgentMemory`. `AgentDetectedEvent` fires only when a source is newly noticed or had been forgotten.
4. **Evaluate goals.** Dynamic goal priorities are recomputed. `GoalCompletedEvent` fires once when a goal is marked complete.
5. **Decide.** The policy is consulted when the agent is idle or every `decision_interval` seconds. An equivalent proposal (`same_as`) does not restart the running action.
6. **Act.** Proposed actions must pass validation before they start. Every start, success, failure, rejection, and interruption publishes an `AgentActionEvent`.

## Perception (`perception.hpp`)

Agents are not omniscient. They only know what their sensors report.

- `Perceivable` marks entities that sensors may notice, with a tag such as `"player"`. Entities without it are invisible to agents.
- `VisionSensor` sees within a range and field-of-view cone. With a `NavigationGrid`, statically unwalkable cells block line of sight. Dynamic navigation obstacles do not block sight. Confidence falls with distance.
- `HearingSensor` hears `SoundProducedEvent`s whose loudness radius, scaled by sensitivity, reaches the agent. Any system can publish sounds on the `EventBus`, and `AgentSystem` collects them for one update.
- `ProximitySensor` notices nearby entities regardless of facing or walls.
- Applications can implement `Sensor` for smell, touch, or world-event sensors.

## Memory and beliefs (`agent_memory.hpp`)

- `AgentMemory` stores records with a timestamp, confidence (`strength`), and importance. Repeated observations of the same source and kind refresh one record.
- Strength decays over time, and records below the forget threshold are removed. Records at or above the long-term importance threshold become long-term and decay ten times slower.
- Short-term memory has a capacity. When it overflows, the weakest short-term record is dropped.
- `recall` returns matching records newest first, and `latest(tag)` finds the newest record for a tag.
- `Beliefs` stores named facts as `bool`, `double`, `std::string`, or a position, each with a confidence and a time. Beliefs can be wrong; they are what the agent thinks, not what is true.

## Goals, actions, and decisions (`agent.hpp`)

- `Goal` has a name, a priority, a completion flag, and an optional `evaluate` function that recomputes priority from the agent's own state. `Agent::top_goal()` returns the highest-priority incomplete goal.
- `AgentAction` has `validate`, `start`, `update`, and `stop`. Built-in actions are `MoveToAction`, which walks via the entity's `NavigationAgent` and is invalid without one or when the target cell is blocked, and `WaitAction`.
- Applications add world rules with `AgentSystem::set_action_validator`. An action runs only if both its own validation and the application's validator accept it.
- `DecisionPolicy` is the replaceable decision interface. `RulePolicy` is a simple ordered rule list: the first rule whose condition holds and whose factory returns an action wins. Finite-state machines, behavior trees, utility AI, and planners can implement the same interface (Phase 8).
- `AgentStats` counts decisions and started, succeeded, failed, and rejected actions. This is groundwork for learning (Phase 8) and metrics (Phase 9).

`Agent` stores sensors, the policy, and the current action through `shared_ptr` because entity components must be copyable. Copying an `Agent` shares those parts.

## Debug overlay (`debug_renderer.hpp`)

`DebugRenderer` wraps any renderer and draws, on top of the scene: collider outlines (green for solids, yellow for triggers), dynamic navigation obstacles (orange), remaining navigation paths (cyan), agent vision cones (white), and remembered observations (red for seen, orange for heard, sized by strength). Each layer can be switched off through `options()`. The overlay only reads scene data. The Win32 backend now draws polygons without an outline pen, so thin overlay lines keep their color.

## Example

`examples/autonomous_agents.cpp` (`a2e_agents_demo`) builds a guard entirely in example code:

- Vision (230 px, 100 degrees, walls block sight) and hearing.
- Goals: `chase` (1.0 while the player is in view), `investigate` (the strength of the newest player memory times 0.8, so it fades as the memory fades), and `patrol` (constant 0.2).
- A `RulePolicy` that chases, looks around on arriving at the last-known position, investigates, or patrols six points.
- The guard's color shows its top goal: red for chase, orange for investigate, blue for patrol. Being caught resets the player.

Controls: `WASD` or the arrow keys to move, `Space` to make a noise the guard can hear, and `F1` to toggle the debug overlay. The basic example also gains the overlay, hidden by default and toggled with `F1`.

## Current limitations

- Text rendering does not exist yet, so the overlay cannot show FPS, goal names, or memory details as text.
- Hearing ignores walls.
- `MoveToAction` validation checks only the target cell, not full reachability. An unreachable-but-open target fails after starting rather than being rejected.
- Agent communication, behavior trees, utility AI, planning, and learning belong to Phase 8.
