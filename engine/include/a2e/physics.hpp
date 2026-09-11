#pragma once

#include "a2e/physics_components.hpp"
#include "a2e/events.hpp"
#include "a2e/update_system.hpp"

#include <cstdint>
#include <functional>
#include <map>
#include <utility>

namespace a2e {

struct CollisionEvent {
    std::uint64_t first_entity;
    std::uint64_t second_entity;
    bool trigger;
    double normal_x;
    double normal_y;
    double penetration;
};

struct CollisionEnterEvent {
    std::uint64_t first_entity;
    std::uint64_t second_entity;
    bool trigger;
    double normal_x;
    double normal_y;
    double penetration;
};

struct CollisionExitEvent {
    std::uint64_t first_entity;
    std::uint64_t second_entity;
    bool trigger;
    double normal_x = 0.0;
    double normal_y = 0.0;
    double penetration = 0.0;
};

class PhysicsSystem final : public UpdateSystem {
public:
    using CollisionCallback = std::function<void(const CollisionEvent&)>;

    explicit PhysicsSystem(CollisionCallback callback = {}, double gravity_y = 0.0,
                           EventBus* events = nullptr, double max_step_seconds = 0.0);
    void update(Scene& scene, const InputState& input, double delta_seconds) override;

private:
    void update_step(Scene& scene, double delta_seconds);

    CollisionCallback callback_;
    double gravity_y_;
    EventBus* events_;
    double max_step_seconds_;
    std::map<std::pair<std::uint64_t, std::uint64_t>, bool> active_contacts_;
};

} // namespace a2e
