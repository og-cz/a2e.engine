#include "a2e/animation.hpp"

#include "a2e/scene.hpp"

#include <stdexcept>
#include <utility>

namespace a2e {

AnimationClip AnimationClip::from_grid(int frame_width, int frame_height, int columns, int first_frame,
                                       int frame_count, double frame_duration, bool loop) {
    if (frame_width <= 0 || frame_height <= 0) throw std::invalid_argument("frame size must be positive");
    if (columns <= 0) throw std::invalid_argument("sprite sheet columns must be positive");
    if (first_frame < 0 || frame_count <= 0) throw std::invalid_argument("frame range is invalid");
    AnimationClip clip;
    clip.loop = loop;
    clip.frames.reserve(static_cast<std::size_t>(frame_count));
    for (int frame = first_frame; frame < first_frame + frame_count; ++frame) {
        clip.frames.push_back({(frame % columns) * frame_width, (frame / columns) * frame_height,
                               frame_width, frame_height, frame_duration});
    }
    clip.validate();
    return clip;
}

double AnimationClip::length() const {
    double total = 0.0;
    for (const auto& frame : frames) total += frame.duration;
    return total;
}

void AnimationClip::validate() const {
    if (frames.empty()) throw std::invalid_argument("animation clip needs at least one frame");
    for (const auto& frame : frames) {
        if (frame.duration <= 0.0) throw std::invalid_argument("animation frame duration must be positive");
        if (frame.source_x < 0 || frame.source_y < 0 || frame.source_width < 0 || frame.source_height < 0) {
            throw std::invalid_argument("animation frame rectangle cannot be negative");
        }
    }
}

void Animator::add_state(const std::string& name, AnimationClip clip) {
    if (name.empty()) throw std::invalid_argument("animation state name cannot be empty");
    clip.validate();
    states_.insert_or_assign(name, std::move(clip));
    if (state_.empty()) play(name);
}

bool Animator::has_state(const std::string& name) const { return states_.find(name) != states_.end(); }

bool Animator::play(const std::string& name, bool restart) {
    if (!has_state(name)) return false;
    if (name == state_ && !restart) return true;
    state_ = name;
    frame_index_ = 0;
    frame_time_ = 0.0;
    finished_ = false;
    return true;
}

bool Animator::advance(double delta_seconds) {
    const auto found = states_.find(state_);
    if (found == states_.end() || paused_ || finished_) return false;
    const auto& clip = found->second;
    frame_time_ += delta_seconds * speed_;
    while (frame_time_ >= clip.frames[frame_index_].duration) {
        frame_time_ -= clip.frames[frame_index_].duration;
        if (frame_index_ + 1 < clip.frames.size()) {
            ++frame_index_;
        } else if (clip.loop) {
            frame_index_ = 0;
        } else {
            frame_time_ = 0.0;
            finished_ = true;
            return true;
        }
    }
    return false;
}

const AnimationFrame* Animator::current_frame() const {
    const auto found = states_.find(state_);
    return found == states_.end() ? nullptr : &found->second.frames[frame_index_];
}

void Animator::set_speed(double speed) {
    if (speed < 0.0) throw std::invalid_argument("animation speed cannot be negative");
    speed_ = speed;
}

void AnimationSystem::update(Scene& scene, const InputState&, double delta_seconds) {
    for (const auto& entity : scene.entities()) {
        if (!entity->active()) continue;
        auto* animator = entity->get_component<Animator>();
        if (!animator) continue;
        const bool just_finished = animator->advance(delta_seconds);
        if (auto* sprite = entity->sprite()) {
            if (const auto* frame = animator->current_frame()) {
                sprite->source_x = frame->source_x;
                sprite->source_y = frame->source_y;
                sprite->source_width = frame->source_width;
                sprite->source_height = frame->source_height;
            }
        }
        if (just_finished && events_) events_->publish(AnimationFinishedEvent{entity->id(), animator->state()});
    }
}

} // namespace a2e
