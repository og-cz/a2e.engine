#pragma once

#include <cstdint>

namespace a2e {

struct Collider {
    double width = 48.0;
    double height = 48.0;
    std::uint32_t layer = 1;
    std::uint32_t mask = 0xffffffffu;
    bool trigger = false;
};

struct RigidBody {
    double velocity_x = 0.0;
    double velocity_y = 0.0;
    bool dynamic = true;
    double restitution = 0.0;
    double friction = 0.0;
};

} // namespace a2e