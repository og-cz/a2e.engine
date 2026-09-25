#pragma once

#include "a2e/events.hpp"
#include "a2e/update_system.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace a2e {

struct AnimationFrame {
    int source_x = 0;
    int source_y = 0;
    int source_width = 0;
    int source_height = 0;
    double duration = 0.1;
};

struct AnimationClip {
    std::vector<AnimationFrame> frames;
    bool loop = true;

    // Builds a clip from equally sized cells of a sprite sheet, read left to right, top to bottom.
    static AnimationClip from_grid(int frame_width, int frame_height, int columns, int first_frame,
                                   int frame_count, double frame_duration, bool loop = true);
    double length() const;
    void validate() const;
};

// Entity component holding named animation states. Attach with Entity::add_component<Animator>().
class Animator {
public:
    void add_state(const std::string& name, AnimationClip clip);
    bool has_state(const std::string& name) const;
    // Switches state. Playing the current state again keeps its progress unless restart is true.
    bool play(const std::string& name, bool restart = false);
    // Advances time. Returns true only on the update where a non-looping clip finishes.
    bool advance(double delta_seconds);

    const AnimationFrame* current_frame() const;
    const std::string& state() const { return state_; }
    std::size_t frame_index() const { return frame_index_; }
    bool finished() const { return finished_; }
    bool paused() const { return paused_; }
    void set_paused(bool paused) { paused_ = paused; }
    double speed() const { return speed_; }
    void set_speed(double speed);

private:
    std::unordered_map<std::string, AnimationClip> states_;
    std::string state_;
    std::size_t frame_index_ = 0;
    double frame_time_ = 0.0;
    double speed_ = 1.0;
    bool finished_ = false;
    bool paused_ = false;
};

struct AnimationFinishedEvent {
    std::uint64_t entity;
    std::string state;
};

// Advances every active entity's Animator and copies the current frame into its Sprite.
class AnimationSystem final : public UpdateSystem {
public:
    explicit AnimationSystem(EventBus* events = nullptr) : events_(events) {}
    void update(Scene& scene, const InputState& input, double delta_seconds) override;

private:
    EventBus* events_;
};

} // namespace a2e
