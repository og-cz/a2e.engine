# Phase 5 Scope

Phase 5 adds sprite animation and audio. Both are optional systems that applications register explicitly; neither changes the renderer, physics, or gameplay code.

## Animation

- `AnimationFrame` is a sprite-sheet source rectangle plus a display duration.
- `AnimationClip` is an ordered list of frames with a loop flag. `AnimationClip::from_grid` builds clips from equally sized sprite-sheet cells read left to right, top to bottom.
- `Animator` is an ordinary entity component (`Entity::add_component<Animator>()`) that holds named states. The first state added starts automatically. `play` switches states; replaying the current state keeps its progress unless `restart` is requested. Animators can be paused and time-scaled.
- `AnimationSystem` advances every active entity's `Animator` and copies the current frame into the entity's `Sprite` source rectangle. The renderer is unaware of animation and simply draws the sprite it sees.
- When a non-looping clip ends, the animator holds its last frame and `AnimationSystem` publishes one `AnimationFinishedEvent` when constructed with an `EventBus`.

Animation state policy (for example, choosing between idle and walk) stays in application code.

## Audio

- `AudioClip` stores mono 16-bit PCM samples. `AudioClip::tone` generates a faded sine tone so prototypes can produce sound without asset files. Clips are shared resources and can be registered with `ResourceManager`.
- `AudioBackend` is the replaceable device boundary. `create_audio_backend()` returns a Win32 `waveOut` backend on Windows and falls back to `NullAudioBackend` when no output device exists.
- `NullAudioBackend` is silent and deterministic, which suits tests, headless runs, and future batch simulation.
- `AudioManager` owns a backend and applies master, sound, and music volume. It tracks one music track; starting new music replaces the previous track.
- `AudioSource` is an entity component. Gameplay calls `play()` or `stop()` on the component; `AudioSystem` forwards those requests to the manager, stops sounds on deactivated entities, and lets the backend reclaim finished voices once per frame.

Current limitations:

- Volume is applied when a sound starts. Changing a volume setting affects sounds started afterwards, not sounds already playing.
- Audio is mono and non-spatial. There is no file loading yet; WAV/OGG decoding belongs with the future resource-loading work.
- The Win32 backend opens one `waveOut` stream per sound. This is simple and dependency-free but is not intended for hundreds of simultaneous voices.

The basic example animates the marker from a two-frame sprite sheet and plays a short generated tone when the player first touches a wall. The `AudioManager` is declared before the `Application` so it outlives the systems that reference it.
