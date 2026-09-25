#pragma once

#include "a2e/input.hpp"
#include "a2e/scene.hpp"
#include "a2e/update_system.hpp"

#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <utility>

namespace a2e {

// Reusable top-down movement driven entirely by logical actions. Entities with a dynamic
// RigidBody are moved through velocity so physics can resolve contacts; other entities
// are moved directly through their Transform.
class PlayerController final : public UpdateSystem {
public:
    static constexpr const char* move_left = "move_left";
    static constexpr const char* move_right = "move_right";
    static constexpr const char* move_up = "move_up";
    static constexpr const char* move_down = "move_down";

    explicit PlayerController(std::uint64_t entity_id, double speed = 180.0,
                              InputMap actions = default_bindings())
        : entity_id_(entity_id), speed_(speed), actions_(std::move(actions)) {
        if (speed < 0.0) throw std::invalid_argument("player speed cannot be negative");
    }

    static InputMap default_bindings() {
        InputMap actions;
        actions.bind(move_left, Key::A);
        actions.bind(move_left, Key::Left);
        actions.bind(move_right, Key::D);
        actions.bind(move_right, Key::Right);
        actions.bind(move_up, Key::W);
        actions.bind(move_up, Key::Up);
        actions.bind(move_down, Key::S);
        actions.bind(move_down, Key::Down);
        return actions;
    }

    void update(Scene& scene, const InputState& input, double delta_seconds) override {
        auto* entity = scene.find_entity(entity_id_);
        if (!entity || !entity->active()) return;

        double direction_x = axis(input, move_left, move_right);
        double direction_y = axis(input, move_up, move_down);
        if (gamepad_ && direction_x == 0.0 && direction_y == 0.0) {
            direction_x = dead_zone(gamepad_->left_stick_x());
            direction_y = dead_zone(gamepad_->left_stick_y());
        }
        const double length = std::sqrt(direction_x * direction_x + direction_y * direction_y);
        if (length > 1.0) {
            direction_x /= length;
            direction_y /= length;
        }

        if (auto* body = entity->rigid_body(); body && body->dynamic) {
            body->velocity_x = direction_x * speed_;
            body->velocity_y = direction_y * speed_;
            return;
        }
        entity->transform().x += direction_x * speed_ * delta_seconds;
        entity->transform().y += direction_y * speed_ * delta_seconds;
    }

    void set_entity(std::uint64_t entity_id) { entity_id_ = entity_id; }
    std::uint64_t entity() const { return entity_id_; }
    void set_speed(double speed) {
        if (speed < 0.0) throw std::invalid_argument("player speed cannot be negative");
        speed_ = speed;
    }
    double speed() const { return speed_; }
    void set_gamepad(const GamepadState* gamepad) { gamepad_ = gamepad; }
    InputMap& actions() { return actions_; }
    const InputMap& actions() const { return actions_; }

private:
    double axis(const InputState& input, const char* negative, const char* positive) const {
        double value = 0.0;
        if (actions_.is_action_down(input, negative) || (gamepad_ && actions_.is_action_down(*gamepad_, negative))) {
            value -= 1.0;
        }
        if (actions_.is_action_down(input, positive) || (gamepad_ && actions_.is_action_down(*gamepad_, positive))) {
            value += 1.0;
        }
        return value;
    }

    static double dead_zone(double value) { return std::abs(value) < 0.2 ? 0.0 : value; }

    std::uint64_t entity_id_;
    double speed_;
    InputMap actions_;
    const GamepadState* gamepad_ = nullptr;
};

} // namespace a2e
